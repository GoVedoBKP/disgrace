#include "sample_instrument.h"
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

        // Cut previous note on SAME column
        for (auto& v : m_voices) {
            if (v && v->active() && v->column() == column_index) {
                v->stop();
            }
        }
        
        float freq = 440.0f * powf(2.0f, (int(note) - 69) / 12.0f);
        disgrace_ns::Voice* v = allocate_voice(column_index);
        if (v) {
            static_cast<disgrace_ns::SampleVoice*>(v)->set_sample(sample.data.get());
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

    void disgrace_ns::SampleInstrument::set_volume(float vol)
    {
        for (auto& v : m_voices) if (v && v->active()) v->set_volume(vol);
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
    }

    void disgrace_ns::SampleInstrument::remove_sample(size_t index)
    {
        if (index < m_samples.size()) {
            m_samples.erase(m_samples.begin() + index);
        }
    }

    void disgrace_ns::SampleInstrument::move_sample(size_t from, size_t to)
    {
        if (from < m_samples.size() && to < m_samples.size() && from != to) {
            auto it_from = m_samples.begin() + from;
            SampleEntry entry = std::move(*it_from);
            m_samples.erase(it_from);
            m_samples.insert(m_samples.begin() + to, std::move(entry));
        }
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
        auto& data = *m_samples[index].data;
        switch (action) {
            case SampleFormatAction::Stereo:          /* No change */ break;
            case SampleFormatAction::StereoToMonoL:   data.to_mono_l(); break;
            case SampleFormatAction::StereoToMonoR:   data.to_mono_r(); break;
            case SampleFormatAction::StereoToMonoMix: data.to_mono_mix(); break;
            case SampleFormatAction::MonoToStereo:    data.to_stereo(); break;
        }
    }

    ::std::unique_ptr<disgrace_ns::Voice>
    disgrace_ns::SampleInstrument::create_voice()
    {
        if (m_samples.empty() || m_selected_sample_index >= m_samples.size() || !m_samples[m_selected_sample_index].data) return nullptr;
        return ::std::make_unique<disgrace_ns::SampleVoice>(
            m_samples[m_selected_sample_index].data.get(),
            m_engine_rate);
    }

} // namespace disgrace_ns
