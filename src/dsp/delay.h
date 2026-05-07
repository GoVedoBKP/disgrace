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
#include "dsp.h"
#include <array>
#include <nlohmann/json.hpp>
#include <algorithm>

namespace disgrace_ns
{

class DelayDSP : public disgrace_ns::DSP
{
public:
    DelayDSP() { m_current_preset = "Default"; }
    static constexpr size_t MAX_DELAY = 48000;

    float time = 0.25f;     // 0.0 to 1.0 seconds
    float feedback = 0.4f;
    float mix = 0.3f;

    std::string name() const override { return "Delay"; }
    std::string type_name() const override { return "Delay"; }

    void set_sample_rate(float sr) override { m_sample_rate = sr; }

    void process(float* l,
                 float* r,
                 size_t nframes) override
    {
        if (m_bypassed) return;
        size_t delay_samples = std::min((size_t)(time * m_sample_rate), MAX_DELAY - 1);
        for (size_t i = 0; i < nframes; ++i)
        {
            size_t read_pos = (m_write_pos + MAX_DELAY - delay_samples) % MAX_DELAY;
            float dl = m_buffer_l[read_pos];
            float dr = m_buffer_r[read_pos];

            m_buffer_l[m_write_pos] = l[i] + dl * feedback;
            m_buffer_r[m_write_pos] = r[i] + dr * feedback;

            l[i] = l[i] * (1.0f - mix) + dl * mix;
            r[i] = r[i] * (1.0f - mix) + dr * mix;

            m_write_pos = (m_write_pos + 1) % MAX_DELAY;
        }
    }

    std::string get_state() const override {
        nlohmann::json j;
        j["time"] = time;
        j["feedback"] = feedback;
        j["mix"] = mix;
        j["bypassed"] = m_bypassed;
        return j.dump();
    }

    void set_state(const std::string& state) override {
        auto j = nlohmann::json::parse(state);
        if (j.contains("time")) time = j["time"];
        if (j.contains("feedback")) feedback = j["feedback"];
        if (j.contains("mix")) mix = j["mix"];
        if (j.contains("bypassed")) m_bypassed = j["bypassed"];
    }

    std::vector<std::string> get_presets() override {
        return {"Default", "Slapback", "Long Echo", "Feedback Beast",
                "Eighth @ 120BPM", "Quarter @ 120BPM", "Tape Echo", "Doubler"};
    }

    void load_preset(const std::string& name) override {
        m_current_preset = name;
        if (name == "Default")            { time = 0.25f; feedback = 0.4f;  mix = 0.3f;  }
        else if (name == "Slapback")      { time = 0.05f; feedback = 0.1f;  mix = 0.5f;  }
        else if (name == "Long Echo")     { time = 0.6f;  feedback = 0.6f;  mix = 0.4f;  }
        else if (name == "Feedback Beast"){ time = 0.4f;  feedback = 0.95f; mix = 0.5f;  }
        else if (name == "Eighth @ 120BPM") { time = 0.25f; feedback = 0.5f; mix = 0.35f; }
        else if (name == "Quarter @ 120BPM"){ time = 0.5f;  feedback = 0.45f;mix = 0.35f; }
        else if (name == "Tape Echo")     { time = 0.35f; feedback = 0.55f; mix = 0.45f; }
        else if (name == "Doubler")       { time = 0.02f; feedback = 0.0f;  mix = 0.5f;  }
    }

private:
    ::std::array<float, MAX_DELAY> m_buffer_l{};
    ::std::array<float, MAX_DELAY> m_buffer_r{};
    size_t m_write_pos = 0;
    float m_sample_rate = 44100.0f;
};

} // namespace disgrace_ns
