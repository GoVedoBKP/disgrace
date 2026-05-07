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
#include "../util/linear_smoother.h"
#include <nlohmann/json.hpp>

namespace disgrace_ns
{

class GainDSP : public disgrace_ns::DSP
{
public:
    GainDSP() : m_smoother(1.0f) { m_current_preset = "Unity"; }
    float gain = 1.0f;

    std::string name() const override { return "Gain"; }
    std::string type_name() const override { return "Gain"; }

    void process(float* l,
                 float* r,
                 size_t nframes) override
    {
        if (m_bypassed) return;
        m_smoother.set_target(gain, nframes);
        for (size_t i = 0; i < nframes; ++i)
        {
            float cur = m_smoother.next();
            l[i] *= cur;
            r[i] *= cur;
        }
    }

    std::string get_state() const override {
        nlohmann::json j;
        j["gain"] = gain;
        j["bypassed"] = m_bypassed;
        return j.dump();
    }

    void set_state(const std::string& state) override {
        auto j = nlohmann::json::parse(state);
        if (j.contains("gain")) {
            gain = j["gain"];
            m_smoother = LinearSmoother(gain);
        }
        if (j.contains("bypassed")) m_bypassed = j["bypassed"];
    }

    std::vector<std::string> get_presets() override {
        return {"Unity", "Silent", "Boost (+6dB)", "Boost (+12dB)", "Attenuate (-6dB)",
                "Boost (+3dB)", "Attenuate (-3dB)", "Attenuate (-12dB)", "Attenuate (-18dB)"};
    }

    void load_preset(const std::string& name) override {
        m_current_preset = name;
        if      (name == "Unity")             gain = 1.0f;
        else if (name == "Silent")            gain = 0.0f;
        else if (name == "Boost (+6dB)")      gain = 2.0f;
        else if (name == "Boost (+12dB)")     gain = 4.0f;
        else if (name == "Attenuate (-6dB)")  gain = 0.5f;
        else if (name == "Boost (+3dB)")      gain = 1.414f;
        else if (name == "Attenuate (-3dB)")  gain = 0.707f;
        else if (name == "Attenuate (-12dB)") gain = 0.25f;
        else if (name == "Attenuate (-18dB)") gain = 0.125f;
    }

private:
    LinearSmoother m_smoother;
};

} // namespace disgrace_ns
