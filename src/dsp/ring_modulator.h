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
#include <nlohmann/json.hpp>
#include <cmath>

namespace disgrace_ns
{

class RingModulatorDSP : public disgrace_ns::DSP
{
public:
    RingModulatorDSP() { m_current_preset = "Metallic Ring"; }
    float freq = 300.0f; // Modulation frequency in Hz (1–1000 Hz)
    float mix = 1.0f;

    std::string name() const override { return "Ring Modulator"; }
    std::string type_name() const override { return "RingModulator"; }

    void set_sample_rate(float sr) override { m_sample_rate = sr; }

    void process(float* l, float* r, size_t nframes) override
    {
        if (m_bypassed) return;
        for (size_t i = 0; i < nframes; ++i)
        {
            float carrier = std::sin(m_phase);
            m_phase += 2.0f * M_PI * freq / m_sample_rate;
            if (m_phase > 2.0f * M_PI) m_phase -= 2.0f * M_PI;

            float out_l = l[i] * carrier;
            float out_r = r[i] * carrier;

            l[i] = l[i] * (1.0f - mix) + out_l * mix;
            r[i] = r[i] * (1.0f - mix) + out_r * mix;
        }
    }

    std::string get_state() const override {
        nlohmann::json j;
        j["freq"] = freq;
        j["mix"] = mix;
        j["bypassed"] = m_bypassed;
        return j.dump();
    }

    void set_state(const std::string& state) override {
        try {
            auto j = nlohmann::json::parse(state);
            if (j.contains("freq")) freq = j["freq"];
            if (j.contains("mix")) mix = j["mix"];
            if (j.contains("bypassed")) m_bypassed = j["bypassed"];
        } catch(...) {}
    }

    std::vector<std::string> get_presets() override {
        return {"Tremolo 5Hz", "Tremolo 10Hz", "Sideband 100Hz",
                "Metallic Ring", "Bell 220Hz", "Alien Bell", "Robot Voice"};
    }

    void load_preset(const std::string& name) override {
        m_current_preset = name;
        if      (name == "Tremolo 5Hz")    { freq =   5.0f; mix = 0.5f; }
        else if (name == "Tremolo 10Hz")   { freq =  10.0f; mix = 0.4f; }
        else if (name == "Sideband 100Hz") { freq = 100.0f; mix = 0.8f; }
        else if (name == "Metallic Ring")  { freq = 300.0f; mix = 0.5f; }
        else if (name == "Bell 220Hz")     { freq = 220.0f; mix = 0.7f; }
        else if (name == "Alien Bell")     { freq = 440.0f; mix = 0.7f; }
        else if (name == "Robot Voice")    { freq =  80.0f; mix = 1.0f; }
    }

private:
    float m_phase = 0;
    float m_sample_rate = 44100.0f;
};

} // namespace disgrace_ns
