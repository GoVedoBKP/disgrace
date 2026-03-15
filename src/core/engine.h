#pragma once

#include <memory>
#include <string>
#include <vector>
#include <atomic>

#include "../sequencer/timing.h"
#include "../sequencer/pattern.h"
#include "../sequencer/note.h"
#include "../mixer/track.h"
#include "../mixer/mixer_bus.h"
#include "../instrument/instrument.h"
#include "../audio/sample_data.h"
#include "undo_stack.h"
#include "transport.h"
#include "key_bindings.h"
#include "../gui/theme.h"
#include "../midi/midi_queue.h"
#include "../midi/midi_input.h"
#include "metronome.h"
#include "masterbus.h"
#include "../util/ringbuffer.h"
#include "engine_command.h"

namespace disgrace_ns
{

struct BlockClipboard
{
    size_t width = 0;
    size_t height = 0;
    ::std::vector<uint8_t> notes;
};

struct SampleClipboard
{
    std::shared_ptr<SampleData> data;
};

class Transport;
class AudioBackend;

class Engine
{
public:
    Engine();
    ~Engine();

    bool initialize();
    void shutdown();
    void save_config();
    void new_project();
    void reinitialize_audio(uint32_t num_ins = 2, uint32_t num_outs = 2,
                            uint32_t num_midi_ins = 1, uint32_t num_midi_outs = 1);
    bool audio_active() const;

    void start();
    void stop();
    void panic();
    void play();
    void play_song();
    void play_pattern();
    void play_from_position(size_t row);
    void auto_seek();

    disgrace_ns::Transport& transport();
    const disgrace_ns::Transport& transport() const;

    void process_audio(const float* const* in_bufs, uint32_t num_ins, float* out_l, float* out_r, size_t nframes);

    double tempo() const;
    uint32_t lpb() const;

    void set_tempo(double);
    void set_lpb(uint32_t);

    const disgrace_ns::Pattern& pattern(size_t) const;
    disgrace_ns::Pattern& pattern(size_t);

    size_t pattern_count() const;
    void clear_patterns() { m_patterns.clear(); }
    void add_pattern(::std::unique_ptr<disgrace_ns::Pattern> pat) { m_patterns.push_back(std::move(pat)); }
    size_t create_pattern();
    size_t copy_pattern(size_t index);
    void resize_pattern(size_t index, size_t new_rows);
    void process_commands();

    ::std::vector<uint8_t> order_list() const;
    void set_order(const ::std::vector<uint8_t>&);

    size_t add_pattern_to_order();
    void remove_pattern_from_order(size_t order_pos);
    size_t copy_pattern_in_order(size_t order_pos);

    size_t m_samples_until_next_tick = 0;
    int    m_current_tick = 0;
    size_t m_current_row = 0;
    ::std::atomic<size_t> m_order_pos{0};
    ::std::atomic<size_t> m_edit_order_pos{0};
    ::std::atomic<size_t> m_order_start{0};
    ::std::atomic<size_t> m_order_end{0};

    void handle_effect_row_start(size_t track_index, const disgrace_ns::TrackEvent& ev);

    size_t current_row() const;
    bool   is_playing() const;
    void set_active_pattern(size_t index);
    size_t active_pattern() const;
    void preview_note(size_t track, uint8_t note, size_t column = 0);
    void stop_preview(size_t track, size_t column = 0);

    disgrace_ns::Pattern& pattern();

    ::std::vector<::std::unique_ptr<disgrace_ns::Instrument>> m_instruments;

    disgrace_ns::Instrument& instrument(size_t index);
    const disgrace_ns::Instrument& instrument(size_t index) const;
    size_t instrument_count() const;
    int get_instrument_index(const disgrace_ns::Instrument* inst) const;
    void add_instrument();
    void add_instrument(::std::unique_ptr<disgrace_ns::Instrument> inst);
    void remove_instrument(size_t index);
    void set_instrument_type(size_t index, InstrumentType type);
    ::std::vector<size_t> m_order;

    void add_order(size_t pattern);
    size_t current_order_pos() const;
    disgrace_ns::UndoStack& undo_stack();
    disgrace_ns::BlockClipboard& clipboard();
    disgrace_ns::SampleClipboard& sample_clipboard() { return m_sample_clipboard; }

    enum class SampleRecordMode { Free, Synced };
    ::std::atomic<bool> m_is_recording_sample{false};
    ::std::atomic<SampleRecordMode> m_recording_sample_mode{SampleRecordMode::Free};
    ::std::atomic<bool> m_recording_synced_active{false};
    ::std::shared_ptr<SampleData> m_recording_sample_data;
    uint32_t m_recording_input_channel = 0;
    bool m_recording_is_mono = false;

    void start_recording_sample(SampleRecordMode mode, uint32_t channel, bool mono);
    void stop_recording_sample();

    disgrace_ns::MidiQueue<disgrace_ns::MidiMessage, 1024> m_midi_queue;
    disgrace_ns::MidiQueue<disgrace_ns::MidiMessage, 1024> m_midi_out_queue;
    disgrace_ns::MidiInput m_midi;

    ::std::atomic<bool> m_record_enabled{false};
    size_t m_record_track{0};

    void enable_record(bool e);
    void set_record_track(size_t t);
    void record_note(uint8_t note, size_t column = 0);
    void set_current_instrument(size_t index);
    void toggle_metronome();
    void set_metronome_enabled(bool enabled);
    disgrace_ns::TransportState transport_state() const;

    disgrace_ns::Metronome m_metronome;

    size_t m_samples_until_next_beat{0};
    ::std::atomic<bool> m_metronome_enabled{true};

    disgrace_ns::MasterBus m_master;
    disgrace_ns::RingBuffer<float, 4096> m_spectral_rb;

    void set_master_gain(float g);
    float master_gain() const;
    float master_meter_l() const;
    float master_meter_r() const;

    struct ExportOptions {
        uint32_t sample_rate = 44100;
        bool separate_tracks = false;
        bool realtime = false;
    };

    std::atomic<float> m_export_progress{0.0f};
    std::atomic<bool>  m_is_exporting{false};

    bool render_to_wav(const ::std::string& path, const ExportOptions& options);
    void process_block(float* l, float* r, size_t nframes, const float* const* in_bufs = nullptr);
    void save_project(const ::std::string& path);
    void load_project(const ::std::string& path);
    void import_audio(const ::std::string& path);
    void handle_midi(uint8_t* data, size_t size);

    void toggle_play();
    void set_loop(bool enable);
    void set_play_position(size_t pattern, size_t row);
    disgrace_ns::Track& track(size_t index);
    const disgrace_ns::Track& track(size_t index) const;
    size_t track_count() const;
    void add_track();
    void remove_track(size_t index);
    void move_track(size_t from, size_t to);

    size_t bus_count() const;
    disgrace_ns::MixerBus& bus(size_t index);
    const disgrace_ns::MixerBus& bus(size_t index) const;
    void add_bus();
    void remove_bus(size_t index);
    void move_bus(size_t from, size_t to);

    uint32_t m_num_ins = 2;
    uint32_t m_num_outs = 2;
    uint32_t m_num_midi_ins = 1;
    uint32_t m_num_midi_outs = 1;

    int m_gui_button_height = 25;
    int m_gui_font_size = 12;
    ThemeType m_gui_theme = ThemeType::Classic;
    unsigned int m_waveform_color = 0x00FF0000;
    unsigned int m_bg_color = 0xCCCCCC00; // Standard FLTK gray
    unsigned int m_fg_color = 0x00000000; // Black
    unsigned int m_button_color = 0xCCCCCC00;
    int m_boxtype = 2; // FL_UP_BOX
    int m_btn_boxtype = 2; // FL_UP_BOX
    int m_label_boxtype = 0; // FL_NO_BOX

    // Tracker colors
    unsigned int m_tracker_bg = 0x1E1E1E00;
    unsigned int m_tracker_text = 0xC8C8C800;
    unsigned int m_tracker_cursor = 0xFFFF0000;
    unsigned int m_tracker_row_highlight = 0x3C3C5000;
    unsigned int m_tracker_lpb_highlight = 0x2D2D3700;
    unsigned int m_tracker_note = 0xB4B4FF00;
    unsigned int m_tracker_sample = 0xB4FFB400;
    unsigned int m_tracker_volume = 0xFFB4B400;
    unsigned int m_tracker_effect = 0xFFFFB400;

    KeyBindings m_key_bindings;

    uint32_t sample_rate() const { return m_sample_rate; }
    int  base_octave() const { return m_base_octave; }
    void set_base_octave(int oct) { m_base_octave = std::max(0, std::min(9, oct)); }
    uint32_t step_size() const { return m_step_size; }
    void set_step_size(uint32_t s) { m_step_size = std::max(1u, std::min(64u, s)); }

    double get_current_time_seconds() const;

    friend class SongSerializer;
    friend class XrnsImporter;

private:
    disgrace_ns::UndoStack m_undo;
    bool m_initialized;
    ::std::unique_ptr<disgrace_ns::AudioBackend> m_backend;
    disgrace_ns::BlockClipboard m_clipboard;
    disgrace_ns::SampleClipboard m_sample_clipboard;
    ::std::atomic<bool> m_playing{false};
    ::std::atomic<bool> m_loop_pattern{false};

    ::std::unique_ptr<disgrace_ns::Transport> m_transport;

    disgrace_ns::Timing m_timing;

    ::std::vector<disgrace_ns::Track> m_tracks;
    ::std::vector<disgrace_ns::MixerBus> m_buses;

    static constexpr size_t MAX_BLOCK = 2048;
    static constexpr size_t MAX_TRACKS_INTERNAL = 64;
    static constexpr size_t MAX_BUSES_INTERNAL = 32;

    float m_mix_l[MAX_BLOCK];
    float m_mix_r[MAX_BLOCK];

    float m_track_l[MAX_TRACKS_INTERNAL][MAX_BLOCK];
    float m_track_r[MAX_TRACKS_INTERNAL][MAX_BLOCK];

    float m_bus_l[MAX_BUSES_INTERNAL][MAX_BLOCK];
    float m_bus_r[MAX_BUSES_INTERNAL][MAX_BLOCK];

    disgrace_ns::RingBuffer<disgrace_ns::EngineCommand, 128> m_cmd_queue;

    ::std::vector<::std::unique_ptr<disgrace_ns::Pattern>> m_patterns;
    ::std::atomic<size_t>  m_active_pattern{0};

    bool write_wav(const ::std::string& path, const ::std::vector<float>& l, const ::std::vector<float>& r, size_t frames, uint32_t sample_rate);

    size_t total_song_samples() const;
    void reset_transport_to_start();
    size_t max_track_latency() const;
    void process_tick();
    void render_block(float* out_l, float* out_r, size_t frames, const float* const* in_bufs = nullptr);
    inline float soft_clip(float x);
    void handle_effect(const disgrace_ns::TrackEvent& ev);
    uint32_t m_sample_rate = 44100;
    int m_base_octave = 4;
    uint32_t m_step_size = 1;
};

} // namespace disgrace_ns
