#pragma once

#include <memory>
#include <string>
#include <vector>
#include <atomic>

#include "../sequencer/timing.h"
#include "../sequencer/pattern.h"
#include "../sequencer/note.h"
#include "../mixer/track.h"
#include "../instrument/instrument.h"
#include "../audio/sample_data.h"
#include "core/undo_stack.h"
#include "transport.h"
#include "key_bindings.h"
#include "../midi/midi_queue.h"
#include "../midi/midi_input.h"
#include "core/metronome.h"
#include "core/masterbus.h"
#include "../util/ringbuffer.h"
#include "core/engine_command.h"

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
    void new_project();
    void reinitialize_audio(uint32_t num_ins = 2, uint32_t num_outs = 2,
                            uint32_t num_midi_ins = 1, uint32_t num_midi_outs = 1);
    bool audio_active() const;

    void start();
    void stop();
    void play();
    void record();

    disgrace_ns::Transport& transport();

    void process_audio(float* out_l, float* out_r, size_t nframes);

    double tempo() const;
    uint32_t lpb() const;

    void set_tempo(double);
    void set_lpb(uint32_t);

    const disgrace_ns::Pattern& pattern(size_t) const;
    disgrace_ns::Pattern& pattern(size_t);

    size_t pattern_count() const;

    ::std::vector<uint8_t> order_list() const;
    void set_order(const ::std::vector<uint8_t>&);

    size_t add_pattern_to_order();
    void remove_pattern_from_order(size_t order_pos);
    size_t copy_pattern_in_order(size_t order_pos);

    size_t m_samples_until_next_tick = 0;
    int    m_current_tick = 0;
    size_t m_current_row = 0;

    void handle_effect_row_start(size_t track_index, const disgrace_ns::NoteEvent& ev);

    size_t current_row() const;
    bool   is_playing() const;
    void set_active_pattern(size_t index);
    size_t active_pattern() const;
    void preview_note(size_t track, uint8_t note);

    disgrace_ns::Pattern& pattern();

    ::std::vector<::std::unique_ptr<disgrace_ns::Instrument>> m_instruments;

    disgrace_ns::Instrument& instrument(size_t index);
    size_t instrument_count() const;
    int get_instrument_index(disgrace_ns::Instrument* inst) const;
    void add_instrument();
    void remove_instrument(size_t index);
    void set_instrument_type(size_t index, InstrumentType type);
    ::std::vector<size_t> m_order;

    void add_order(size_t pattern);
    size_t current_order_pos() const;
    disgrace_ns::UndoStack& undo_stack();
    disgrace_ns::BlockClipboard& clipboard();
    disgrace_ns::SampleClipboard& sample_clipboard() { return m_sample_clipboard; }

    disgrace_ns::MidiQueue<disgrace_ns::MidiMessage, 1024> m_midi_queue;
    disgrace_ns::MidiInput m_midi;

    ::std::atomic<bool> m_record_enabled{false};
    size_t m_record_track{0};

    void enable_record(bool e);
    void set_record_track(size_t t);
    void record_note(uint8_t note);
    void set_current_instrument(size_t index);
    void toggle_metronome();
    void set_metronome_enabled(bool enabled);
    disgrace_ns::TransportState transport_state() const;

    disgrace_ns::Metronome m_metronome;

    size_t m_samples_until_next_beat{0};
    ::std::atomic<bool> m_metronome_enabled{true};

    disgrace_ns::MasterBus m_master;

    void set_master_gain(float g);
    float master_gain() const;
    float master_meter() const;
    bool render_to_wav(const ::std::string& path);
    void process_block(float* l, float* r, size_t nframes);
    void save_project(const ::std::string& path);
    void handle_midi(uint8_t* data, size_t size);

    void toggle_play();
    void set_loop(bool enable);
    void set_play_position(size_t pattern, size_t row);
    disgrace_ns::Track& track(size_t index);
    size_t track_count() const;
    void add_track();
    void remove_track(size_t index);
    void move_track(size_t from, size_t to);

    uint32_t m_num_ins = 2;
    uint32_t m_num_outs = 2;
    uint32_t m_num_midi_ins = 1;
    uint32_t m_num_midi_outs = 1;

    int m_gui_button_height = 25;
    int m_gui_font_size = 12;
    unsigned int m_waveform_color = 0x00FF0000; // FL_GREEN in hex? Actually FL_GREEN is 63. Let's use 0x40FF4000 for a bright green.

    KeyBindings m_key_bindings;

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

    static constexpr size_t MAX_BLOCK = 2048;

    float m_mix_l[MAX_BLOCK];
    float m_mix_r[MAX_BLOCK];

    float m_track_l[MAX_TRACKS][MAX_BLOCK];
    float m_track_r[MAX_TRACKS][MAX_BLOCK];

    disgrace_ns::RingBuffer<disgrace_ns::EngineCommand, 128> m_cmd_queue;

    ::std::vector<disgrace_ns::Pattern> m_patterns;
    ::std::atomic<size_t>  m_active_pattern{0};

    bool write_wav(const ::std::string& path, const ::std::vector<float>& l, const ::std::vector<float>& r, size_t frames);

    size_t total_song_samples() const;
    void reset_transport_to_start();
    size_t max_track_latency() const;
    void process_tick();
    void render_block(float* out_l, float* out_r, size_t frames);
    inline float soft_clip(float x);
    void handle_effect(const disgrace_ns::NoteEvent& ev);
    uint32_t m_sample_rate = 44100;
};

} // namespace disgrace_ns
