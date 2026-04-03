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
#include "voice_synthesis_worker.h"
#include <cstdlib>
#include <cstring>
#include <sndfile.h>
#include <algorithm>
#include <cmath>
#include <samplerate.h>
#include <sys/stat.h>
#include <sys/types.h>

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
    
    // Playback at normal speed (pitch shifting already applied in cache)
    for (size_t i = 0; i < nframes; ++i) {
        if (m_playback_pos >= audio_len) {
            m_playing = false;
            break;
        }
        
        // Direct sample output (no pitch shifting needed)
        l[i] = m_current_audio.left[m_playback_pos] * m_volume;
        r[i] = m_current_audio.right[m_playback_pos] * m_volume;
        
        m_playback_pos += 1.0f;
    }
}

bool VoiceInstrument::synthesize_text(const std::string& text, float base_freq) {
    if (text.empty()) {
        return false;
    }
    
    // Calculate pitch factor (relative to 440 Hz)
    float pitch_factor = base_freq / 440.0f;
    pitch_factor = std::max(0.5f, std::min(2.0f, pitch_factor));
    
    // Create cache key with pitch
    std::string cache_key = make_cache_key(text, pitch_factor);
    
    // Check memory cache first
    auto cache_it = m_audio_cache.find(cache_key);
    if (cache_it != m_audio_cache.end()) {
        m_current_audio = cache_it->second;
        return true;
    }
    
    // Check disk cache
    CachedAudio disk_audio;
    if (load_from_disk_cache(cache_key, disk_audio)) {
        m_audio_cache[cache_key] = disk_audio;
        m_current_audio = disk_audio;
        return true;
    }
    
    // Synthesize base text (at 440 Hz)
    std::vector<float> out_l, out_r;
    
    switch (m_tts_mode) {
        case TTSMode::RealTimeEspeak:
            if (!synthesize_with_espeak(text, out_l, out_r)) {
                return false;
            }
            break;
        case TTSMode::OfflineFestival:
            if (!synthesize_with_festival(text, out_l, out_r)) {
                return false;
            }
            break;
    }
    
    if (out_l.empty()) {
        return false;
    }
    
    // Apply pitch shifting with libsamplerate if pitch != 1.0
    if (std::abs(pitch_factor - 1.0f) > 0.001f) {
        std::vector<float> pitched_l, pitched_r;
        if (!apply_libsamplerate_pitch(out_l, out_r, pitch_factor, pitched_l, pitched_r)) {
            return false;
        }
        out_l = pitched_l;
        out_r = pitched_r;
    }
    
    // Cache the pitch-shifted result in memory
    CachedAudio final_audio = {out_l, out_r};
    m_audio_cache[cache_key] = final_audio;
    m_current_audio = final_audio;
    m_current_text = text;
    
    // Save to disk cache for future sessions
    save_to_disk_cache(cache_key, final_audio);
    
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
    
    // Build espeak command with parameters
    std::string cmd = "espeak-ng";
    
    // Add voice selection
    if (m_voice_index > 0) {
        cmd += " -v +m" + std::to_string(m_voice_index);
    }
    
    // Add speed parameter (espeak uses -s for speed)
    int speed = (int)(m_speed * 175.0f);  // 175 is default, scale with our speed factor
    cmd += " -s " + std::to_string(speed);
    
    // Add output file and text
    cmd += " -w \"" + std::string(tmp_wav) + "\" \"" + escaped_text + "\" 2>/dev/null";
    
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

bool VoiceInstrument::synthesize_with_festival(const std::string& text, std::vector<float>& out_l, std::vector<float>& out_r) {
    // Create temp file paths
    const char* tmp_scm = "/tmp/disgrace_voice.scm";
    const char* tmp_wav = "/tmp/disgrace_voice.wav";
    
    // Create Festival Scheme script to synthesize
    FILE* scm_file = fopen(tmp_scm, "w");
    if (!scm_file) {
        return false;
    }
    
    // Select voice based on voice_index
    if (m_voice_index == 0) {
        fprintf(scm_file, "(voice_default)\n");
    } else {
        // Voices: 1=kal_diphone, 2=rms_diphone, 3=awb_diphone, etc
        fprintf(scm_file, "(voice_kal_diphone)\n");  // Default fallback
    }
    
    // Set pitch accent/intonation (Festival par.pd_targets controls pitch)
    if (m_pitch_accent != 0.5f) {
        fprintf(scm_file, "(set! (Parameter.evaluate par.pd_targets) %.2f)\n", 
                0.5f + m_pitch_accent);
    }
    
    // Set speech rate (duration stretch)
    if (m_speed != 1.0f) {
        fprintf(scm_file, "(set! (Parameter.evaluate Duration_Stretch) %.2f)\n", 
                1.0f / m_speed);  // Inverse because stretch >1 is slower
    }
    
    // Synthesize
    fprintf(scm_file, "(utt.save.wave (utt.synth (eval (list 'Utterance 'Text \"%s\"))) \"%s\" 'riff)\n",
            text.c_str(), tmp_wav);
    fclose(scm_file);
    
    // Run Festival with the script
    std::string cmd = "festival --script \"" + std::string(tmp_scm) + "\" 2>/dev/null";
    int ret = system(cmd.c_str());
    
    // Clean up script file
    remove(tmp_scm);
    
    if (ret != 0) {
        return false;
    }
    
    // Load WAV file
    if (!load_wav_from_file(tmp_wav, out_l, out_r)) {
        return false;
    }
    
    // Clean up WAV file
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

std::string VoiceInstrument::make_cache_key(const std::string& text, float pitch_factor) const {
    // Round pitch_factor to 2 decimals to avoid cache misses from float precision
    int pitch_int = (int)(pitch_factor * 100.0f);
    return text + "@" + std::to_string(pitch_int);
}

std::string VoiceInstrument::get_cache_file_path(const std::string& cache_key) const {
    if (m_cache_dir.empty()) {
        return "";
    }
    // Create safe filename from cache key (replace @ with -)
    std::string safe_key = cache_key;
    for (auto& c : safe_key) {
        if (c == '@' || c == '/' || c == '\\') {
            c = '-';
        }
    }
    return m_cache_dir + "/" + safe_key + ".wav";
}

bool VoiceInstrument::load_from_disk_cache(const std::string& cache_key, CachedAudio& out_audio) {
    if (m_cache_dir.empty()) {
        return false;
    }
    
    std::string path = get_cache_file_path(cache_key);
    if (path.empty()) {
        return false;
    }
    
    // Check if file exists
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) {
        return false;  // File doesn't exist
    }
    
    // Load WAV file
    return load_wav_from_file(path, out_audio.left, out_audio.right);
}

bool VoiceInstrument::save_to_disk_cache(const std::string& cache_key, const CachedAudio& audio) {
    if (m_cache_dir.empty()) {
        return false;
    }
    
    // Create cache directory if needed
    mkdir(m_cache_dir.c_str(), 0755);
    
    std::string path = get_cache_file_path(cache_key);
    if (path.empty()) {
        return false;
    }
    
    // Save as WAV file
    SF_INFO sf_info = {};
    sf_info.samplerate = 44100;  // Standard rate
    sf_info.channels = 2;
    sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    
    SNDFILE* file = sf_open(path.c_str(), SFM_WRITE, &sf_info);
    if (!file) {
        return false;
    }
    
    // Interleave samples: L, R, L, R, ...
    std::vector<float> interleaved;
    interleaved.reserve(audio.left.size() * 2);
    for (size_t i = 0; i < audio.left.size(); ++i) {
        interleaved.push_back(audio.left[i]);
        interleaved.push_back(audio.right[i]);
    }
    
    sf_count_t written = sf_writef_float(file, interleaved.data(), audio.left.size());
    sf_close(file);
    
    return written == (sf_count_t)audio.left.size();
}

bool VoiceInstrument::apply_libsamplerate_pitch(const std::vector<float>& in_left, 
                                                const std::vector<float>& in_right,
                                                float pitch_factor,
                                                std::vector<float>& out_left,
                                                std::vector<float>& out_right) {
    out_left.clear();
    out_right.clear();
    
    if (in_left.empty()) {
        return false;
    }
    
    // Clamp pitch factor to reasonable range
    pitch_factor = std::max(0.5f, std::min(2.0f, pitch_factor));
    
    // If pitch is 1.0, just copy
    if (std::abs(pitch_factor - 1.0f) < 0.001f) {
        out_left = in_left;
        out_right = in_right;
        return true;
    }
    
    // Output length = input_length / pitch_factor
    // (higher pitch = shorter duration, lower pitch = longer duration)
    size_t out_frames = (size_t)(in_left.size() / pitch_factor);
    
    // Create SRC state for each channel
    int error = 0;
    SRC_STATE* src_l = src_new(SRC_SINC_BEST_QUALITY, 1, &error);  // Highest quality
    if (!src_l) {
        return false;
    }
    
    SRC_STATE* src_r = src_new(SRC_SINC_BEST_QUALITY, 1, &error);
    if (!src_r) {
        src_delete(src_l);
        return false;
    }
    
    // Prepare input/output data
    SRC_DATA src_data = {};
    src_data.data_in = const_cast<float*>(in_left.data());
    src_data.input_frames = in_left.size();
    src_data.src_ratio = 1.0 / pitch_factor;  // Resample ratio
    
    // Allocate output buffer
    std::vector<float> out_buf_l(out_frames);
    std::vector<float> out_buf_r(out_frames);
    src_data.data_out = out_buf_l.data();
    src_data.output_frames = out_frames;
    
    // Process left channel
    error = src_process(src_l, &src_data);
    if (error) {
        src_delete(src_l);
        src_delete(src_r);
        return false;
    }
    
    // Process right channel
    src_data.data_in = const_cast<float*>(in_right.data());
    src_data.data_out = out_buf_r.data();
    src_data.output_frames = out_frames;
    error = src_process(src_r, &src_data);
    
    src_delete(src_l);
    src_delete(src_r);
    
    if (error) {
        return false;
    }
    
    // Copy to output
    out_left.assign(out_buf_l.begin(), out_buf_l.begin() + src_data.output_frames_gen);
    out_right.assign(out_buf_r.begin(), out_buf_r.begin() + src_data.output_frames_gen);
    
    return true;
}

void VoiceInstrument::clear_cache() {
    m_audio_cache.clear();
    m_current_text.clear();
    m_current_audio = {std::vector<float>(), std::vector<float>()};
    m_playback_pos = 0;
    m_playing = false;
}

void VoiceInstrument::start_synthesis_worker() {
    if (!m_worker) {
        m_worker = new VoiceSynthesisWorker(this);
    }
    m_worker->start();
}

void VoiceInstrument::stop_synthesis_worker() {
    if (m_worker) {
        m_worker->stop();
        delete m_worker;
        m_worker = nullptr;
    }
}

} // namespace disgrace_ns
