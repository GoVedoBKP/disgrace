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

#include "sample_instrument.h"
#include "../audio/sample_voice.h"
#include <cmath>
#include <algorithm>

namespace disgrace_ns
{

    disgrace_ns::SampleInstrument::SampleInstrument(double engine_rate)
    : m_engine_rate(engine_rate)
    {
    }

    void disgrace_ns::SampleInstrument::note_on(uint8_t note, uint8_t velocity, size_t column_index, size_t offset_samples, uint8_t sample_index)
    {
        if (m_samples.empty()) return;
        uint8_t s_idx = (sample_index > 0 && sample_index <= m_samples.size()) ? (sample_index - 1) : m_selected_sample_index;
        if (s_idx >= m_samples.size()) return;
        auto& sample = m_samples[s_idx];
        if (!sample.data) return;

        // Cut previous note on SAME column - trackers usually hard-cut here.
        for (auto& v : m_voices) {
            if (v && v->active() && v->column() == column_index) {
                v->panic();
            }
        }
        
        float freq = 440.0f * powf(2.0f, (int(note) - 69) / 12.0f);
        disgrace_ns::Voice* v = allocate_voice(column_index);
        if (v) {
            static_cast<disgrace_ns::SampleVoice*>(v)->set_sample(sample.data);
            v->start(note, velocity, freq, offset_samples);
        }
    }

    void disgrace_ns::SampleInstrument::note_off(size_t column_index)
    {
        for (auto& v : m_voices) {
            if (v && v->active() && v->column() == column_index) {
                v->stop();
            }
        }
    }

    void disgrace_ns::SampleInstrument::panic()
    {
        for (auto& v : m_voices) if (v && v->active()) v->panic();
    }

    void disgrace_ns::SampleInstrument::set_sample_rate(double sr)
    {
        m_engine_rate = sr;
        for (auto& v : m_voices) {
            if (v) static_cast<disgrace_ns::SampleVoice*>(v.get())->set_engine_rate(sr);
        }
    }

    void disgrace_ns::SampleInstrument::set_volume(float vol)
    {
    }

    void disgrace_ns::SampleInstrument::set_pitch(float freq)
    {
        for (auto& v : m_voices) if (v && v->active()) v->set_pitch(freq);
    }

    void disgrace_ns::SampleInstrument::process(float* out_l, float* out_r, size_t frames)
    {
        for (size_t i = 0; i < frames; ++i) { out_l[i] = 0.f; out_r[i] = 0.f; }
        for (auto& v : m_voices) if (v && v->active()) v->process(out_l, out_r, frames);
    }

    void disgrace_ns::SampleInstrument::add_sample(const std::string& name, std::shared_ptr<disgrace_ns::SampleData> data)
    {
        m_samples.push_back({name, data});
        
        // Ensure voices are allocated in the GUI thread during setup, not in the audio thread.
        for (size_t i = 0; i < MAX_VOICES; ++i) {
            if (!m_voices[i]) {
                m_voices[i] = create_voice();
            }
        }
    }

    void disgrace_ns::SampleInstrument::remove_sample(size_t index)
    {
        if (index < m_samples.size()) {
            m_samples.erase(m_samples.begin() + index);
            // Re-index undo states
            std::map<size_t, UndoState> next_states;
            for (auto& pair : m_undo_states) {
                if (pair.first < index) next_states[pair.first] = std::move(pair.second);
                else if (pair.first > index) next_states[pair.first - 1] = std::move(pair.second);
            }
            m_undo_states = std::move(next_states);
        }
    }

    void disgrace_ns::SampleInstrument::move_sample(size_t from, size_t to)
    {
        if (from < m_samples.size() && to < m_samples.size() && from != to) {
            SampleEntry entry = std::move(m_samples[from]);
            m_samples.erase(m_samples.begin() + from);
            m_samples.insert(m_samples.begin() + to, std::move(entry));
            
            // Re-index undo states
            std::map<size_t, UndoState> next_states;
            UndoState moving = std::move(m_undo_states[from]);
            m_undo_states.erase(from);
            
            for (auto& pair : m_undo_states) {
                size_t idx = pair.first;
                if (from < to) {
                    if (idx > from && idx <= to) next_states[idx - 1] = std::move(pair.second);
                    else next_states[idx] = std::move(pair.second);
                } else {
                    if (idx >= to && idx < from) next_states[idx + 1] = std::move(pair.second);
                    else next_states[idx] = std::move(pair.second);
                }
            }
            next_states[to] = std::move(moving);
            m_undo_states = std::move(next_states);
        }
    }

    void disgrace_ns::SampleInstrument::update_sample_data(size_t index, std::shared_ptr<disgrace_ns::SampleData> data)
    {
        if (index < m_samples.size()) {
            m_samples[index].data = data;
            // Note: Voices currently keep their own reference to SampleData.
            // If they are active, they will continue to use the old data.
            // This is generally safe and prevents audio glitches.
        }
    }

    void SampleInstrument::push_undo(size_t index) {
        if (index >= m_samples.size() || !m_samples[index].data) return;
        auto& state = m_undo_states[index];
        auto copy = std::make_shared<SampleData>(*m_samples[index].data);
        state.undo_stack.push_back(copy);
        if (state.undo_stack.size() > 10) state.undo_stack.erase(state.undo_stack.begin());
        state.redo_stack.clear();
    }

    void SampleInstrument::undo(size_t index) {
        if (index >= m_samples.size() || !m_samples[index].data) return;
        auto& state = m_undo_states[index];
        if (state.undo_stack.empty()) return;
        
        auto current = std::make_shared<SampleData>(*m_samples[index].data);
        state.redo_stack.push_back(current);
        
        m_samples[index].data = state.undo_stack.back();
        state.undo_stack.pop_back();
    }

    void SampleInstrument::redo(size_t index) {
        if (index >= m_samples.size() || !m_samples[index].data) return;
        auto& state = m_undo_states[index];
        if (state.redo_stack.empty()) return;
        
        auto current = std::make_shared<SampleData>(*m_samples[index].data);
        state.undo_stack.push_back(current);
        
        m_samples[index].data = state.redo_stack.back();
        state.redo_stack.pop_back();
    }

    bool SampleInstrument::can_undo(size_t index) const {
        auto it = m_undo_states.find(index);
        return it != m_undo_states.end() && !it->second.undo_stack.empty();
    }

    bool SampleInstrument::can_redo(size_t index) const {
        auto it = m_undo_states.find(index);
        return it != m_undo_states.end() && !it->second.redo_stack.empty();
    }

    void disgrace_ns::SampleInstrument::set_sample_name(size_t index, const std::string& name)
    {
        if (index < m_samples.size()) {
            m_samples[index].name = name;
        }
    }

    void disgrace_ns::SampleInstrument::convert_sample_format(size_t index, SampleFormatAction action)
    {
        if (index >= m_samples.size() || !m_samples[index].data) return;
        auto data_copy = std::make_shared<SampleData>(*m_samples[index].data);
        switch (action) {
            case SampleFormatAction::Stereo:          /* No change */ break;
            case SampleFormatAction::StereoToMonoL:   data_copy->to_mono_l(); break;
            case SampleFormatAction::StereoToMonoR:   data_copy->to_mono_r(); break;
            case SampleFormatAction::StereoToMonoMix: data_copy->to_mono_mix(); break;
            case SampleFormatAction::MonoToStereo:    data_copy->to_stereo(); break;
        }
        m_samples[index].data = data_copy;
    }

    ::std::unique_ptr<disgrace_ns::Voice>
    disgrace_ns::SampleInstrument::create_voice()
    {
        if (m_samples.empty() || m_selected_sample_index >= m_samples.size() || !m_samples[m_selected_sample_index].data) return nullptr;
        return ::std::make_unique<disgrace_ns::SampleVoice>(
            m_samples[m_selected_sample_index].data,
            m_engine_rate);
    }

    double disgrace_ns::SampleInstrument::voice_position() const {
        for (auto& v : m_voices) {
            if (v && v->active())
                return static_cast<const disgrace_ns::SampleVoice*>(v.get())->position();
        }
        return -1.0;
    }

} // namespace disgrace_ns
