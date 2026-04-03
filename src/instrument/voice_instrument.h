/*
 * Disgrace - Digital Audio Workstation
 * Copyright (C) 2025  Miroslav Shaltev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include "instrument.h"
#include <map>
#include <vector>
#include <memory>

namespace disgrace_ns {

class Engine;
class VoiceSynthesisWorker;

enum class TTSMode {
    RealTimeEspeak,  // Fast, ~100-200ms per phrase
    OfflineFestival  // Better quality, ~500ms-2s per phrase
};

class VoiceInstrument : public Instrument {
public:
    VoiceInstrument(Engine* engine = nullptr);

    void note_on(uint8_t note, uint8_t velocity, size_t column_index = 0, size_t offset_samples = 0, uint8_t sample_index = 0) override;
    void note_off(size_t column_index = 0) override;
    void panic() override;
    void set_volume(float vol) override;
    void set_pitch(float freq) override;
    void process(float* l, float* r, size_t nframes) override;

    // Voice-specific methods
    void set_text(const std::string& text, size_t column_index = 0);
    std::string get_text(size_t column_index = 0) const;
    
    void set_tts_mode(TTSMode mode) { m_tts_mode = mode; }
    TTSMode tts_mode() const { return m_tts_mode; }
    
    // TTS enhancements
    void set_voice(int voice_index) { m_voice_index = voice_index; }  // 0=default, 1=voice2, etc
    int get_voice() const { return m_voice_index; }
    
    void set_speed(float speed) { m_speed = std::max(0.5f, std::min(2.0f, speed)); }  // 0.5x to 2.0x
    float get_speed() const { return m_speed; }
    
    void set_pitch_accent(float accent) { m_pitch_accent = std::max(0.0f, std::min(1.0f, accent)); }  // 0.0 to 1.0
    float get_pitch_accent() const { return m_pitch_accent; }
    
    // Synthesis
    bool synthesize_text(const std::string& text, float base_freq);
    void clear_cache();
    size_t cache_size() const { return m_audio_cache.size(); }
    
    // Worker thread for batch synthesis
    VoiceSynthesisWorker* get_worker() { return m_worker; }
    void start_synthesis_worker();
    void stop_synthesis_worker();

protected:
    std::unique_ptr<Voice> create_voice() override { return nullptr; }

private:
    Engine* m_engine;
    TTSMode m_tts_mode = TTSMode::RealTimeEspeak;
    float m_volume = 1.0f;
    float m_current_pitch = 1.0f;
    
    // TTS parameters
    int m_voice_index = 0;        // 0=default, 1=voice2, etc
    float m_speed = 1.0f;          // 0.5-2.0 playback speed
    float m_pitch_accent = 0.5f;   // 0.0-1.0 pitch accent (for Festival)
    
    // Worker thread (raw pointer, created/destroyed in start/stop)
    VoiceSynthesisWorker* m_worker = nullptr;
    
    // Text storage per column
    std::array<std::string, 16> m_column_text;
    
    // Audio cache: "{text}@{pitch_factor}" → {samples_left, samples_right}
    // Using pitch in cache key allows pre-shifted audio to be reused
    struct CachedAudio {
        std::vector<float> left;
        std::vector<float> right;
    };
    std::map<std::string, CachedAudio> m_audio_cache;
    
    // Helper to generate cache key including pitch
    std::string make_cache_key(const std::string& text, float pitch_factor) const;
    
    // High-quality pitch shifting using libsamplerate
    bool apply_libsamplerate_pitch(const std::vector<float>& in_left, 
                                   const std::vector<float>& in_right,
                                   float pitch_factor,
                                   std::vector<float>& out_left,
                                   std::vector<float>& out_right);
    
    // Playback state
    std::string m_current_text;
    CachedAudio m_current_audio;
    size_t m_playback_pos = 0;
    bool m_playing = false;
    
    // TTS synthesis helpers
    bool synthesize_with_espeak(const std::string& text, std::vector<float>& out_l, std::vector<float>& out_r);
    bool synthesize_with_festival(const std::string& text, std::vector<float>& out_l, std::vector<float>& out_r);
    bool load_wav_from_file(const std::string& filepath, std::vector<float>& out_l, std::vector<float>& out_r);
};

} // namespace disgrace_ns
