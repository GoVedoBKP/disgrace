#include "engine.h"
#include "config_manager.h"
#include "../audio/audio_backend.h"
#include "../audio/jack_backend.h"
#include "../sequencer/timing.h"
#include "../sequencer/pattern.h"
#include "../mixer/track.h"
#include "../instrument/sample_instrument.h"
#include "../instrument/soundfont_instrument.h"
#include "../instrument/dssi_instrument.h"
#include "../instrument/lv2_instrument.h"
#include "../instrument/midi_instrument.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace disgrace_ns {

Engine::Engine() : m_initialized(false), m_timing(44100) {
    m_transport = std::make_unique<Transport>(*this);
    m_backend = std::make_unique<JackBackend>(this);
    m_export_progress.store(0.0f);
    m_is_exporting.store(false);
}

Engine::~Engine() {
    shutdown();
}

bool Engine::initialize() {
    if (m_initialized) return true;
    
    ConfigManager::instance().load();
    const auto& conf = ConfigManager::instance().config();
    m_num_ins = conf.num_audio_ins;
    m_num_outs = conf.num_audio_outs;
    m_num_midi_ins = conf.num_midi_ins;
    m_num_midi_outs = conf.num_midi_outs;
    m_gui_button_height = conf.gui_button_height;
    m_gui_font_size = conf.gui_font_size;
    m_gui_theme = conf.gui_theme;
    m_waveform_color = conf.waveform_color;
    m_bg_color = conf.bg_color;
    m_fg_color = conf.fg_color;
    m_button_color = conf.button_color;
    m_boxtype = conf.boxtype;
    m_btn_boxtype = conf.btn_boxtype;
    m_label_boxtype = conf.label_boxtype;

    m_tracker_bg = conf.tracker_bg;
    m_tracker_text = conf.tracker_text;
    m_tracker_cursor = conf.tracker_cursor;
    m_tracker_row_highlight = conf.tracker_row_highlight;
    m_tracker_lpb_highlight = conf.tracker_lpb_highlight;
    m_tracker_note = conf.tracker_note;
    m_tracker_sample = conf.tracker_sample;
    m_tracker_volume = conf.tracker_volume;
    m_tracker_effect = conf.tracker_effect;

    new_project();
    if (m_backend->start()) {
        m_initialized = true;
        return true;
    }
    return false;
}

void Engine::save_config() {
    auto& conf = ConfigManager::instance().config();
    conf.num_audio_ins = m_num_ins;
    conf.num_audio_outs = m_num_outs;
    conf.num_midi_ins = m_num_midi_ins;
    conf.num_midi_outs = m_num_midi_outs;
    conf.gui_button_height = m_gui_button_height;
    conf.gui_font_size = m_gui_font_size;
    conf.gui_theme = m_gui_theme;
    conf.waveform_color = m_waveform_color;
    conf.bg_color = m_bg_color;
    conf.fg_color = m_fg_color;
    conf.button_color = m_button_color;
    conf.boxtype = m_boxtype;
    conf.btn_boxtype = m_btn_boxtype;
    conf.label_boxtype = m_label_boxtype;

    conf.tracker_bg = m_tracker_bg;
    conf.tracker_text = m_tracker_text;
    conf.tracker_cursor = m_tracker_cursor;
    conf.tracker_row_highlight = m_tracker_row_highlight;
    conf.tracker_lpb_highlight = m_tracker_lpb_highlight;
    conf.tracker_note = m_tracker_note;
    conf.tracker_sample = m_tracker_sample;
    conf.tracker_volume = m_tracker_volume;
    conf.tracker_effect = m_tracker_effect;

    conf.keyboard_layout = (int)m_key_bindings.get_layout();
    ConfigManager::instance().save();
}

void Engine::shutdown() {
    if (!m_initialized) return;
    save_config();
    m_backend->stop();
    m_initialized = false;
}

void Engine::new_project() {
    m_tracks.clear();
    m_buses.clear();
    m_instruments.clear();
    m_patterns.clear();
    m_order.clear();
    
    // Add one default instrument
    add_instrument();
    
    // Add one default track
    add_track();
    
    // Add one default pattern
    create_pattern();
    
    // Initialize order
    m_order.push_back(0);
    m_order_start.store(0);
    m_order_end.store(0);
    
    m_active_pattern.store(0);
    m_current_row = 0;
    m_current_tick = 0;
    m_edit_order_pos.store(0);
    m_order_pos.store(0);
}

void Engine::reinitialize_audio(uint32_t num_ins, uint32_t num_outs, uint32_t num_midi_ins, uint32_t num_midi_outs) {
    m_backend->stop();
    m_num_ins = num_ins; m_num_outs = num_outs;
    m_num_midi_ins = num_midi_ins; m_num_midi_outs = num_midi_outs;
    m_backend->start();
}

bool Engine::audio_active() const { return m_initialized; }
bool Engine::is_playing() const { return m_playing.load(); }

void Engine::start() { transport().play(); }
void Engine::stop() { 
    transport().stop(); 
    panic();
}

void Engine::panic() {
    for (auto& track : m_tracks) {
        track.panic();
    }
}
void Engine::play() { transport().play(); }

void Engine::play_song() {
    stop();
    m_order_pos.store(0);
    if (!m_order.empty()) {
        set_active_pattern(m_order[0]);
    }
    m_current_row = 0;
    m_current_tick = 0;
    set_loop(false);
    auto_seek();
    start();
}

void Engine::play_pattern() {
    stop();
    m_current_row = 0;
    m_current_tick = 0;
    set_loop(true);
    auto_seek();
    start();
}

void Engine::play_from_position(size_t row) {
    stop();
    m_current_row = row;
    m_current_tick = 0;
    set_loop(true);
    auto_seek();
    start();
}

void Engine::auto_seek() {
    size_t pat_idx = m_active_pattern.load();
    if (pat_idx >= m_patterns.size()) return;
    
    Pattern& pat = *m_patterns[pat_idx];
    size_t target_row = m_current_row;

    for (size_t t = 0; t < m_tracks.size() && t < pat.track_count(); ++t) {
        size_t num_cols = pat.column_count(t);
        for (size_t c = 0; c < num_cols; ++c) {
            int found_row = -1;
            uint8_t found_note = 255;
            uint8_t found_vol = 255;

            for (int r = (int)target_row - 1; r >= 0; --r) {
                const auto& ev = pat.event(t, (size_t)r, c);
                if (ev.note == 254) break; 
                if (ev.note != 255) {
                    found_row = r;
                    found_note = ev.note;
                    found_vol = ev.volume;
                    break; 
                }
            }

            if (found_row != -1 && found_note != 255 && found_note != 254) {
                size_t row_diff = target_row - (size_t)found_row;
                size_t offset_samples = row_diff * m_timing.samples_per_row();
                m_tracks[t].note_on(found_note, found_vol == 255 ? 100 : found_vol, c, offset_samples);
            }
        }
    }
}

void Engine::preview_note(size_t t, uint8_t note, size_t column) {
    if (t < m_tracks.size()) {
        if (note == 254) m_tracks[t].note_off(column);
        else m_tracks[t].note_on(note, 100, column);
    }
}

void Engine::stop_preview(size_t t, size_t column) {
    if (t < m_tracks.size()) m_tracks[t].note_off(column);
}

void Engine::record_note(uint8_t note, size_t column)
{
    size_t row = current_row();
    Pattern& current_pattern = pattern();
    if (m_record_track < current_pattern.track_count()) {
        current_pattern.event(m_record_track, row, column).note = note;
    }
}

void Engine::process_tick() 
{ 
    size_t pat_idx = m_active_pattern.load();
    if (pat_idx >= m_patterns.size()) return;
    Pattern& pat = *m_patterns[pat_idx];

    if (m_current_tick == 0) {
        size_t row = m_current_row;
        for (size_t t = 0; t < m_tracks.size() && t < pat.track_count(); ++t) {
            size_t num_cols = pat.column_count(t);
            for (size_t c = 0; c < num_cols; ++c) {
                TrackEvent& ev = pat.event(t, row, c);
                if (ev.note == 254) {
                    m_tracks[t].note_off(c);
                } else if (ev.note != 255) {
                    m_tracks[t].note_on(ev.note, ev.volume == 255 ? 100 : ev.volume, c, 0, ev.sample_idx); 
                }
                if (c == 0) handle_effect_row_start(t, ev);
            }
        }
    }

    for (size_t t = 0; t < m_tracks.size(); ++t) {
        m_tracks[t].process_tick(m_current_tick);
    }

    m_current_tick++;
    if (m_current_tick >= m_timing.speed()) {
        m_current_tick = 0;
        m_current_row++;
        if (m_current_row >= pat.row_count()) {
            m_current_row = 0;
            if (!transport().m_loop_pattern.load()) {
                size_t next_order_pos = m_order_pos.load() + 1;
                size_t end_pos = m_order_end.load();
                if (end_pos == 0 && !m_order.empty()) end_pos = m_order.size() - 1;

                if (next_order_pos > end_pos) {
                    next_order_pos = m_order_start.load();
                }
                
                if (next_order_pos < m_order.size()) {
                    m_order_pos.store(next_order_pos);
                    set_active_pattern(m_order[next_order_pos]);
                }
            }
        }
    }
}

void Engine::handle_effect_row_start(size_t t, const TrackEvent& ev)
{
    auto& track = m_tracks[t];
    auto process_fx = [&](uint8_t fx, uint8_t param) {
        if (fx == 0) return;
        switch (fx) {
            case 0x0F: if (param < 32) m_timing.set_speed(param); else m_timing.set_bpm(param); break;
            case 0x0C: track.set_volume(param / 64.f); break;
            case 0x0A: track.m_fx_state.vol_slide_up = (param >> 4); track.m_fx_state.vol_slide_down = (param & 0x0F); break;
            case 0x03: track.m_fx_state.porta_speed = param * 0.0005f; track.m_fx_state.porta_active = true; break;
            case 0x0E: { uint8_t sub = param >> 4; uint8_t val = param & 0x0F; if (sub == 0x9) { track.m_fx_state.retrig_ticks = val; track.m_fx_state.retrig_counter = 0; } else if (sub == 0xC) track.m_fx_state.note_cut_tick = val; break; }
        }
    };
    process_fx(ev.effect1, ev.param1);
    process_fx(ev.effect2, ev.param2);
}

void Engine::process_audio(const float* const* in_bufs, uint32_t num_ins, float* out_l, float* out_r, size_t nframes)
{
    if (m_metronome_enabled && transport().state() != TransportState::Stopped)
        m_metronome.process(m_mix_l, m_mix_r, nframes, m_samples_until_next_beat, m_timing.samples_per_beat());

    MidiMessage msg;
    while (m_midi_queue.pop(msg)) {
        uint8_t status = msg.status & 0xF0;
        if (status == 0x90 || status == 0x80) {
            m_tracks[m_record_track].note_on(msg.data1, msg.data2);
            if (m_record_enabled && transport().is_playing()) record_note(msg.data1);
        }
    }

    EngineCommand cmd;
    while (m_cmd_queue.pop(cmd)) {
        switch (cmd.type) {
            case EngineCommandType::Play: m_playing.store(true); break;
            case EngineCommandType::Stop: m_playing.store(false); break;
            case EngineCommandType::SetTempo: m_timing.set_bpm((int)cmd.value); break;
            case EngineCommandType::PlayPattern:
                m_current_row = 0;
                m_current_tick = 0;
                transport().set_loop(true);
                auto_seek();
                m_playing.store(true);
                break;
            case EngineCommandType::ResizePattern:
                if (cmd.index < m_patterns.size()) {
                    printf("DEBUG: Engine: Resizing pattern %zu to %zu rows\n", cmd.index, (size_t)cmd.value);
                    m_patterns[cmd.index]->resize_rows((size_t)cmd.value);
                    if (m_active_pattern.load() == cmd.index) {
                        if (m_current_row >= m_patterns[cmd.index]->row_count()) {
                            m_current_row = 0;
                        }
                    }
                }
                break;
        }
    }

    if (!transport().is_playing()) {
        render_block(out_l, out_r, nframes, in_bufs);
        m_master.process(out_l, out_r, nframes);
        
        if (m_is_exporting.load()) {
            for (size_t i = 0; i < nframes; ++i) { out_l[i] = 0.f; out_r[i] = 0.f; }
        }

        for (size_t i = 0; i < nframes; ++i) m_spectral_rb.push((out_l[i] + out_r[i]) * 0.5f);
        return;
    }
    process_block(out_l, out_r, nframes, in_bufs);

    if (m_is_exporting.load()) {
        for (size_t i = 0; i < nframes; ++i) { out_l[i] = 0.f; out_r[i] = 0.f; }
    }

    for (size_t i = 0; i < nframes; ++i) m_spectral_rb.push((out_l[i] + out_r[i]) * 0.5f);
}

void Engine::process_block(float* l, float* r, size_t nframes, const float* const* in_bufs) {
    size_t processed = 0;
    while (processed < nframes) {
        if (m_samples_until_next_tick == 0) {
            process_tick();
            m_samples_until_next_tick = m_timing.samples_per_tick();
        }
        size_t block = std::min(m_samples_until_next_tick, nframes - processed);
        
        const float* offset_in_bufs[64];
        for(uint32_t i = 0; i < m_num_ins && i < 64; ++i) {
            offset_in_bufs[i] = in_bufs ? in_bufs[i] + processed : nullptr;
        }

        render_block(l + processed, r + processed, block, in_bufs ? offset_in_bufs : nullptr);
        processed += block; m_samples_until_next_tick -= block;
    }
    m_master.process(l, r, nframes);
}

void Engine::render_block(float* out_l, float* out_r, size_t frames, const float* const* in_bufs) {
    if (m_is_recording_sample.load()) {
        bool should_record = false;
        if (m_recording_sample_mode.load() == SampleRecordMode::Free) {
            should_record = true;
        } else {
            if (m_current_row == 0 && m_samples_until_next_tick == m_timing.samples_per_tick()) {
                m_recording_synced_active.store(true);
            }
            if (m_recording_synced_active.load()) {
                should_record = true;
            }
        }

        if (should_record && in_bufs && m_recording_sample_data) {
            uint32_t ch = m_recording_input_channel;
            if (m_recording_is_mono) {
                if (ch < m_num_ins) {
                    for (size_t i = 0; i < frames; ++i) {
                        m_recording_sample_data->left.push_back(in_bufs[ch][i]);
                    }
                }
            } else {
                if (ch < m_num_ins - 1) {
                    for (size_t i = 0; i < frames; ++i) {
                        m_recording_sample_data->left.push_back(in_bufs[ch][i]);
                        m_recording_sample_data->right.push_back(in_bufs[ch+1][i]);
                    }
                } else if (ch < m_num_ins) {
                     for (size_t i = 0; i < frames; ++i) {
                        m_recording_sample_data->left.push_back(in_bufs[ch][i]);
                        m_recording_sample_data->right.push_back(in_bufs[ch][i]);
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < frames; ++i) { out_l[i] = 0.f; out_r[i] = 0.f; }
    
    for (size_t b = 0; b < m_buses.size() && b < MAX_BUSES_INTERNAL; ++b) {
        std::fill(m_bus_l[b], m_bus_l[b] + frames, 0.f);
        std::fill(m_bus_r[b], m_bus_r[b] + frames, 0.f);
    }

    bool any_solo = false;
    for (size_t t = 0; t < m_tracks.size(); ++t) {
        if (m_tracks[t].solo()) { any_solo = true; break; }
    }

    for (size_t t = 0; t < m_tracks.size(); ++t) {
        m_tracks[t].process(m_track_l[t], m_track_r[t], frames, in_bufs);
        
        bool should_play = true;
        if (any_solo) {
            if (!m_tracks[t].solo()) should_play = false;
        } else {
            if (m_tracks[t].muted()) should_play = false;
        }

        if (should_play) {
            int out_idx = m_tracks[t].output_bus();
            if (out_idx >= 0 && (size_t)out_idx < m_buses.size() && (size_t)out_idx < MAX_BUSES_INTERNAL) {
                for (size_t i = 0; i < frames; ++i) {
                    m_bus_l[out_idx][i] += m_track_l[t][i];
                    m_bus_r[out_idx][i] += m_track_r[t][i];
                }
            } else {
                for (size_t i = 0; i < frames; ++i) {
                    out_l[i] += m_track_l[t][i];
                    out_r[i] += m_track_r[t][i];
                }
            }
        }
    }

    for (size_t b = 0; b < m_buses.size() && b < MAX_BUSES_INTERNAL; ++b) {
        m_buses[b].process(m_bus_l[b], m_bus_r[b], frames);
        
        int out_idx = m_buses[b].output_bus();
        if (out_idx >= 0 && (size_t)out_idx < m_buses.size() && (size_t)out_idx < MAX_BUSES_INTERNAL) {
            if ((size_t)out_idx > b) {
                for (size_t i = 0; i < frames; ++i) {
                    m_bus_l[out_idx][i] += m_bus_l[b][i];
                    m_bus_r[out_idx][i] += m_bus_r[b][i];
                }
            } else {
                for (size_t i = 0; i < frames; ++i) {
                    out_l[i] += m_bus_l[b][i];
                    out_r[i] += m_bus_r[b][i];
                }
            }
        } else {
            for (size_t i = 0; i < frames; ++i) {
                out_l[i] += m_bus_l[b][i];
                out_r[i] += m_bus_r[b][i];
            }
        }
    }
}

void Engine::handle_midi(uint8_t* data, size_t size) {
    if (size < 3) return;
    MidiMessage msg; msg.status = data[0]; msg.data1 = data[1]; msg.data2 = data[2];
    m_midi_queue.push(msg);
}

void Engine::set_instrument_type(size_t index, InstrumentType type) {
    if (index >= m_instruments.size()) return;
    if (m_instruments[index]->type() == type) return;
    std::string name = m_instruments[index]->name();
    std::unique_ptr<Instrument> new_inst;
    switch (type) {
        case InstrumentType::Sampler: new_inst = std::make_unique<SampleInstrument>((double)m_sample_rate); break;
        case InstrumentType::SoundFont: new_inst = std::make_unique<SoundFontInstrument>((double)m_sample_rate); break;
        case InstrumentType::Plugin: new_inst = std::make_unique<DSSIInstrument>((double)m_sample_rate); break;
        case InstrumentType::Midi: new_inst = std::make_unique<MidiInstrument>(this); break;
        default: new_inst = std::make_unique<NoneInstrument>(); break;
    }
    new_inst->set_name(name); new_inst->set_type(type);
    for (auto& track : m_tracks) if (track.instrument() == m_instruments[index].get()) track.set_instrument(new_inst.get());
    m_instruments[index] = std::move(new_inst);
}

void Engine::add_track() { 
    m_tracks.emplace_back(); 
    for (auto& pat : m_patterns) {
        pat->resize_tracks(m_tracks.size());
    }
}
void Engine::remove_track(size_t index) { if (index < m_tracks.size()) m_tracks.erase(m_tracks.begin() + index); }
void Engine::move_track(size_t from, size_t to) {
    if (from < m_tracks.size() && to < m_tracks.size()) std::swap(m_tracks[from], m_tracks[to]);
}
size_t Engine::bus_count() const { return m_buses.size(); }
MixerBus& Engine::bus(size_t index) { return m_buses[index]; }
const MixerBus& Engine::bus(size_t index) const { return m_buses[index]; }
void Engine::add_bus() { m_buses.emplace_back(); }
void Engine::remove_bus(size_t index) { if (index < m_buses.size()) m_buses.erase(m_buses.begin() + index); }
void Engine::move_bus(size_t from, size_t to) {
    if (from < m_buses.size() && to < m_buses.size()) std::swap(m_buses[from], m_buses[to]);
}

size_t Engine::track_count() const { return m_tracks.size(); }
Track& Engine::track(size_t index) { return m_tracks[index]; }
const Track& Engine::track(size_t index) const { return m_tracks[index]; }

void Engine::add_instrument() { m_instruments.push_back(std::make_unique<NoneInstrument>()); }
void Engine::add_instrument(::std::unique_ptr<disgrace_ns::Instrument> inst) { m_instruments.push_back(std::move(inst)); }
void Engine::remove_instrument(size_t index) { if (index < m_instruments.size()) m_instruments.erase(m_instruments.begin() + index); }
Instrument& Engine::instrument(size_t index) { return *m_instruments[index]; }
const Instrument& Engine::instrument(size_t index) const { return *m_instruments[index]; }
size_t Engine::instrument_count() const { return m_instruments.size(); }
int Engine::get_instrument_index(const Instrument* inst) const {
    for (size_t i = 0; i < m_instruments.size(); ++i) if (m_instruments[i].get() == inst) return (int)i;
    return -1;
}

size_t Engine::current_row() const { return m_current_row; }
void Engine::set_active_pattern(size_t index) { if (index < m_patterns.size()) m_active_pattern.store(index); }
size_t Engine::active_pattern() const { return m_active_pattern.load(); }
Pattern& Engine::pattern() { return *m_patterns[m_active_pattern.load()]; }
Pattern& Engine::pattern(size_t index) { return *m_patterns[index]; }
const Pattern& Engine::pattern(size_t index) const { return *m_patterns[index]; }
size_t Engine::pattern_count() const { return m_patterns.size(); }

size_t Engine::create_pattern() {
    size_t rows = 64;
    if (!m_patterns.empty()) rows = pattern().row_count();
    m_patterns.push_back(std::make_unique<Pattern>(rows, m_tracks.size()));
    Pattern& p = *m_patterns.back();
    for (size_t i = 0; i < m_tracks.size(); ++i) {
        p.set_column_count(i, 1);
    }
    return m_patterns.size() - 1;
}

size_t Engine::copy_pattern(size_t index) {
    if (index >= m_patterns.size()) return create_pattern();
    m_patterns.push_back(std::make_unique<Pattern>(*m_patterns[index]));
    return m_patterns.size() - 1;
}

void Engine::resize_pattern(size_t index, size_t new_rows) {
    EngineCommand cmd;
    cmd.type = EngineCommandType::ResizePattern;
    cmd.index = index;
    cmd.value = (float)new_rows;
    m_cmd_queue.push(cmd);
    process_commands();
}

void Engine::process_commands() {
    EngineCommand cmd;
    while (m_cmd_queue.pop(cmd)) {
        switch (cmd.type) {
            case EngineCommandType::Play: m_playing.store(true); break;
            case EngineCommandType::Stop: m_playing.store(false); break;
            case EngineCommandType::SetTempo: m_timing.set_bpm((int)cmd.value); break;
            case EngineCommandType::PlayPattern:
                m_current_row = 0;
                m_current_tick = 0;
                transport().set_loop(true);
                auto_seek();
                m_playing.store(true);
                break;
            case EngineCommandType::ResizePattern:
                if (cmd.index < m_patterns.size()) {
                    m_patterns[cmd.index]->resize_rows((size_t)cmd.value);
                    if (m_active_pattern.load() == cmd.index) {
                        if (m_current_row >= m_patterns[cmd.index]->row_count()) {
                            m_current_row = 0;
                        }
                    }
                }
                break;
        }
    }
}

std::vector<uint8_t> Engine::order_list() const {
    std::vector<uint8_t> o; for (auto v : m_order) o.push_back((uint8_t)v);
    return o;
}
void Engine::set_order(const std::vector<uint8_t>& o) {
    m_order.clear(); for (auto v : o) m_order.push_back(v);
}
size_t Engine::add_pattern_to_order() { 
    size_t new_pat = create_pattern();
    m_order.push_back(new_pat); 
    return m_order.size() - 1; 
}
void Engine::remove_pattern_from_order(size_t pos) { if (pos < m_order.size()) m_order.erase(m_order.begin() + pos); }
size_t Engine::copy_pattern_in_order(size_t pos) {
    if (pos < m_order.size()) { 
        size_t new_pat = copy_pattern(m_order[pos]);
        m_order.insert(m_order.begin() + pos + 1, new_pat); 
        return pos + 1; 
    }
    return pos;
}

double Engine::tempo() const { return m_timing.tempo(); }
void Engine::set_tempo(double bpm) { m_timing.set_bpm((int)bpm); }
uint32_t Engine::lpb() const { return m_timing.lpb(); }
void Engine::set_lpb(uint32_t l) { m_timing.set_lpb(l); }

void Engine::toggle_metronome() { m_metronome_enabled = !m_metronome_enabled; }
void Engine::set_metronome_enabled(bool e) { m_metronome_enabled = e; }
void Engine::enable_record(bool e) { m_record_enabled = e; }
TransportState Engine::transport_state() const { return transport().state(); }
void Engine::set_loop(bool e) { transport().set_loop(e); }

void Engine::set_master_gain(float g) { m_master.set_gain(g); }
float Engine::master_gain() const { return m_master.gain(); }
float Engine::master_meter_l() const { return m_master.meter_l(); }
float Engine::master_meter_r() const { return m_master.meter_r(); }
Transport& Engine::transport() { return *m_transport; }
const Transport& Engine::transport() const { return *m_transport; }

size_t Engine::total_song_samples() const { 
    size_t total_rows = 0;
    for (uint8_t pat_idx : m_order) {
        if (pat_idx < m_patterns.size()) {
            total_rows += m_patterns[pat_idx]->row_count();
        }
    }
    // BPM/Speed might change during playback, but this is a base estimate
    return total_rows * m_timing.samples_per_row(); 
}

bool Engine::render_to_wav(const std::string& path, const ExportOptions& opts) {
    if (m_order.empty()) return false;
    
    m_is_exporting.store(true);
    m_export_progress.store(0.0f);
    m_master.m_export_mute.store(true);

    // Save current state
    bool was_playing = m_playing.load();
    size_t old_order_pos = m_order_pos.load();
    size_t old_row = m_current_row;
    size_t old_tick = m_current_tick;
    bool old_loop = transport().m_loop_pattern.load();
    uint32_t old_sr = m_sample_rate;

    // Prepare for rendering
    stop();
    m_sample_rate = opts.sample_rate;
    m_timing.set_sample_rate(opts.sample_rate);

    m_order_pos.store(0);
    set_active_pattern(m_order[0]);
    m_current_row = 0;
    m_current_tick = 0;
    m_samples_until_next_tick = 0;
    transport().set_loop(false);

    // Estimate length
    size_t total_frames = total_song_samples();
    if (total_frames == 0) return false;

    // Buffers for export
    std::vector<float> final_l, final_r;
    std::vector<std::vector<float>> tracks_l, tracks_r;
    
    if (opts.separate_tracks) {
        tracks_l.resize(m_tracks.size());
        tracks_r.resize(m_tracks.size());
    }

    size_t rendered = 0;
    const size_t block_size = 512;
    float bl[block_size], br[block_size];

    m_playing.store(true);

    while (rendered < total_frames && m_playing.load()) {
        size_t to_render = std::min(block_size, total_frames - rendered);
        
        if (opts.realtime) {
            // Realtime export
            m_master.m_recorded_l.clear();
            m_master.m_recorded_r.clear();
            m_master.m_recorded_l.reserve(total_frames);
            m_master.m_recorded_r.reserve(total_frames);
            m_master.m_is_recording.store(true);
            
            start();
            while (transport().is_playing() && m_order_pos.load() < m_order.size()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            m_master.m_is_recording.store(false);
            stop();
            
            final_l = std::move(m_master.m_recorded_l);
            final_r = std::move(m_master.m_recorded_r);
            break;
        }

        process_block(bl, br, to_render, nullptr);
        
        for (size_t i = 0; i < to_render; ++i) {
            final_l.push_back(bl[i]);
            final_r.push_back(br[i]);
        }

        if (opts.separate_tracks) {
            for (size_t t = 0; t < m_tracks.size(); ++t) {
                for (size_t i = 0; i < to_render; ++i) {
                    tracks_l[t].push_back(m_track_l[t][i]);
                    tracks_r[t].push_back(m_track_r[t][i]);
                }
            }
        }

        rendered += to_render;
        m_export_progress.store((float)rendered / (float)total_frames);
        
        if (m_order_pos.load() >= m_order.size() || (m_order_pos.load() == m_order.size()-1 && m_current_row >= pattern().row_count())) {
             break;
        }
    }

    // Save to WAV
    bool success = write_wav(path, final_l, final_r, final_l.size(), opts.sample_rate);

    if (success && opts.separate_tracks) {
        fs::path p(path);
        std::string stem = p.stem().string();
        std::string ext = p.extension().string();
        fs::path parent = p.parent_path();
        
        for (size_t t = 0; t < m_tracks.size(); ++t) {
            std::string filename = stem + "_" + std::to_string(t) + ext;
            fs::path track_path = parent / filename;
            write_wav(track_path.string(), tracks_l[t], tracks_r[t], tracks_l[t].size(), opts.sample_rate);
        }
    }

    // Restore state
    stop();
    m_sample_rate = old_sr;
    m_timing.set_sample_rate(old_sr);
    m_order_pos.store(old_order_pos);
    set_active_pattern(m_order[old_order_pos]);
    m_current_row = old_row;
    m_current_tick = (int)old_tick;
    transport().set_loop(old_loop);
    if (was_playing) start();

    m_is_exporting.store(false);
    m_master.m_export_mute.store(false);
    m_export_progress.store(1.0f);

    return success;
}
inline float Engine::soft_clip(float x) { const float limit = 0.95f; if (x > limit) return limit; if (x < -limit) return -limit; return x; }
UndoStack& Engine::undo_stack() { return m_undo; }
BlockClipboard& Engine::clipboard() { return m_clipboard; }

void Engine::start_recording_sample(SampleRecordMode mode, uint32_t channel, bool mono) {
    m_recording_sample_mode.store(mode);
    m_recording_input_channel = channel;
    m_recording_is_mono = mono;
    m_recording_sample_data = std::make_shared<SampleData>();
    m_recording_sample_data->sample_rate = (int)m_sample_rate;
    m_recording_synced_active.store(false);
    m_is_recording_sample.store(true);
}

void Engine::stop_recording_sample() {
    m_is_recording_sample.store(false);
    m_recording_synced_active.store(false);
}

bool Engine::write_wav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r, size_t frames, uint32_t sample_rate) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    struct {
        char chunkID[4]; uint32_t chunkSize; char format[4];
        char subchunk1ID[4]; uint32_t subchunk1Size; uint16_t audioFormat;
        uint16_t numChannels; uint32_t sampleRate; uint32_t byteRate;
        uint16_t blockAlign; uint16_t bitsPerSample;
        char subchunk2ID[4]; uint32_t subchunk2Size;
    } h;
    
    memcpy(h.chunkID, "RIFF", 4);
    h.chunkSize = 36 + (uint32_t)(frames * 2 * 2);
    memcpy(h.format, "WAVE", 4);
    memcpy(h.subchunk1ID, "fmt ", 4);
    h.subchunk1Size = 16; h.audioFormat = 1; h.numChannels = 2;
    h.sampleRate = sample_rate; h.byteRate = sample_rate * 2 * 2;
    h.blockAlign = 4; h.bitsPerSample = 16;
    memcpy(h.subchunk2ID, "data", 4);
    h.subchunk2Size = (uint32_t)(frames * 2 * 2);

    f.write(reinterpret_cast<char*>(&h), sizeof(h));

    for (size_t i = 0; i < frames; ++i) {
        float sl_f = std::max(-1.f, std::min(1.f, l[i]));
        float sr_f = std::max(-1.f, std::min(1.f, r[i]));
        int16_t sl = static_cast<int16_t>(sl_f * 32767.f);
        int16_t sr = static_cast<int16_t>(sr_f * 32767.f);
        f.write(reinterpret_cast<char*>(&sl), 2);
        f.write(reinterpret_cast<char*>(&sr), 2);
    }
    return true;
}

double Engine::get_current_time_seconds() const {
    size_t total_rows = 0;
    
    if (transport().m_loop_pattern.load()) {
        // Only current pattern time
        total_rows = m_current_row;
    } else {
        // Song time from beginning
        for (size_t i = 0; i < m_order_pos.load() && i < m_order.size(); ++i) {
            size_t pat_idx = m_order[i];
            if (pat_idx < m_patterns.size()) {
                total_rows += m_patterns[pat_idx]->row_count();
            }
        }
        total_rows += m_current_row;
    }
    
    double samples = (double)total_rows * m_timing.samples_per_row();
    samples += (double)m_current_tick * m_timing.samples_per_tick();
    
    // Add sub-tick progress if playing
    if (m_playing.load()) {
        samples += (double)m_timing.samples_per_tick() - (double)m_samples_until_next_tick;
    }
    
    return samples / (double)m_sample_rate;
}

} // namespace disgrace_ns
