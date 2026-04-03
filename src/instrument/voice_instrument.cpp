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

#include "voice_instrument.h"
#include <cstdlib>
#include <cstring>
#include <sndfile.h>
#include <algorithm>
#include <cmath>

namespace disgrace_ns {

VoiceInstrument::VoiceInstrument(Engine* engine)
    : m_engine(engine)
{
    set_type(InstrumentType::None); // Will be set to Voice later
    set_name("Voice");
}

void VoiceInstrument::note_on(uint8_t note, uint8_t velocity, size_t column_index, size_t, uint8_t) {
    if (velocity == 0) {
        note_off(column_index);
        return;
    }
    
    column_index = column_index % 16;
    
    // Get text for this note
    std::string text = m_column_text[column_index];
    
    // Convert MIDI note to frequency (A4 = 440 Hz, note 69)
    float note_freq = 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
    
    if (!text.empty()) {
        // New text: synthesize it at base pitch (A4 = 440 Hz)
        if (synthesize_text(text, 440.0f)) {
            m_current_text = text;
            m_playback_pos = 0;
            m_current_pitch = note_freq / 440.0f;  // Set pitch shift factor
            m_playing = true;
        }
    } else if (!m_current_text.empty() && m_playing) {
        // No text: pitch shift the current phrase to this note
        m_current_pitch = note_freq / 440.0f;
        m_playback_pos = 0;  // Restart playback at new pitch
    }
}

void VoiceInstrument::note_off(size_t column_index) {
    (void)column_index;
    m_playing = false;
    m_playback_pos = 0;
}

void VoiceInstrument::panic() {
    m_playing = false;
    m_playback_pos = 0;
    m_current_text.clear();
}

void VoiceInstrument::set_volume(float vol) {
    m_volume = std::max(0.0f, std::min(1.0f, vol));
}

void VoiceInstrument::set_pitch(float freq) {
    // Store base pitch for pitch shifting
    m_current_pitch = freq / 440.0f;
}

void VoiceInstrument::set_text(const std::string& text, size_t column_index) {
    column_index = column_index % 16;
    m_column_text[column_index] = text;
}

std::string VoiceInstrument::get_text(size_t column_index) const {
    column_index = column_index % 16;
    return m_column_text[column_index];
}

void VoiceInstrument::process(float* l, float* r, size_t nframes) {
    std::fill(l, l + nframes, 0.0f);
    std::fill(r, r + nframes, 0.0f);
    
    if (!m_playing || m_current_audio.left.empty()) {
        return;
    }
    
    size_t audio_len = m_current_audio.left.size();
    float pitch_factor = m_current_pitch;
    
    // Clamp pitch factor to reasonable range (0.5x to 2x)
    pitch_factor = std::max(0.5f, std::min(2.0f, pitch_factor));
    
    for (size_t i = 0; i < nframes; ++i) {
        if (m_playback_pos >= audio_len * pitch_factor) {
            m_playing = false;
            break;
        }
        
        // Calculate source index with pitch shift
        float src_pos = m_playback_pos / pitch_factor;
        size_t src_idx = (size_t)src_pos;
        
        if (src_idx >= audio_len) {
            m_playing = false;
            break;
        }
        
        // Linear interpolation for smooth pitch shifting
        float frac = src_pos - src_idx;
        size_t next_idx = std::min(src_idx + 1, audio_len - 1);
        
        float left_sample = m_current_audio.left[src_idx] * (1.0f - frac) + m_current_audio.left[next_idx] * frac;
        float right_sample = m_current_audio.right[src_idx] * (1.0f - frac) + m_current_audio.right[next_idx] * frac;
        
        // Apply volume
        l[i] = left_sample * m_volume;
        r[i] = right_sample * m_volume;
        
        m_playback_pos += 1.0f;
    }
}

bool VoiceInstrument::synthesize_text(const std::string& text, float base_freq) {
    if (text.empty()) {
        return false;
    }
    
    // Check cache first
    auto cache_it = m_audio_cache.find(text);
    if (cache_it != m_audio_cache.end()) {
        m_current_audio = cache_it->second;
        return true;
    }
    
    // Synthesize based on mode
    std::vector<float> out_l, out_r;
    
    switch (m_tts_mode) {
        case TTSMode::RealTimeEspeak:
            if (!synthesize_with_espeak(text, out_l, out_r)) {
                return false;
            }
            break;
        case TTSMode::OfflineFestival:
            // Festival not implemented yet
            return synthesize_with_espeak(text, out_l, out_r);
    }
    
    if (out_l.empty()) {
        return false;
    }
    
    // Cache the result
    m_audio_cache[text] = {out_l, out_r};
    m_current_audio = m_audio_cache[text];
    
    return true;
}

bool VoiceInstrument::synthesize_with_espeak(const std::string& text, std::vector<float>& out_l, std::vector<float>& out_r) {
    // Create temp file path
    const char* tmp_wav = "/tmp/disgrace_voice.wav";
    
    // Escape quotes in text for shell command
    std::string escaped_text = text;
    size_t pos = 0;
    while ((pos = escaped_text.find('"', pos)) != std::string::npos) {
        escaped_text.replace(pos, 1, "\\\"");
        pos += 2;
    }
    
    // Build espeak command: espeak-ng -w output.wav "text"
    std::string cmd = "espeak-ng -w \"" + std::string(tmp_wav) + "\" \"" + escaped_text + "\" 2>/dev/null";
    
    int ret = system(cmd.c_str());
    if (ret != 0) {
        return false;
    }
    
    // Load WAV file
    if (!load_wav_from_file(tmp_wav, out_l, out_r)) {
        return false;
    }
    
    // Clean up
    remove(tmp_wav);
    
    return true;
}

bool VoiceInstrument::load_wav_from_file(const std::string& filepath, std::vector<float>& out_l, std::vector<float>& out_r) {
    out_l.clear();
    out_r.clear();
    
    SF_INFO sf_info = {};
    SNDFILE* file = sf_open(filepath.c_str(), SFM_READ, &sf_info);
    if (!file) {
        return false;
    }
    
    // Read all samples
    std::vector<float> samples(sf_info.frames * sf_info.channels);
    sf_count_t read = sf_readf_float(file, samples.data(), sf_info.frames);
    sf_close(file);
    
    if (read != sf_info.frames) {
        return false;
    }
    
    // Separate channels
    if (sf_info.channels == 1) {
        // Mono: duplicate to stereo
        out_l = samples;
        out_r = samples;
    } else if (sf_info.channels >= 2) {
        // Stereo or multi-channel: use first two channels
        out_l.reserve(sf_info.frames);
        out_r.reserve(sf_info.frames);
        for (sf_count_t i = 0; i < sf_info.frames; ++i) {
            out_l.push_back(samples[i * sf_info.channels + 0]);
            out_r.push_back(samples[i * sf_info.channels + 1]);
        }
    } else {
        return false;
    }
    
    return true;
}

void VoiceInstrument::clear_cache() {
    m_audio_cache.clear();
    m_current_text.clear();
    m_current_audio = {std::vector<float>(), std::vector<float>()};
    m_playback_pos = 0;
    m_playing = false;
}

} // namespace disgrace_ns
