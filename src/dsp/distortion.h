#pragma once
#include "dsp.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>

namespace disgrace_ns
{

class DistortionDSP : public disgrace_ns::DSP
{
public:
    DistortionDSP() { m_current_preset = "Mild Overdrive"; }
    float drive = 0.5f; // 0 to 1
    float mix = 1.0f;   // 0 to 1

    std::string name() const override { return "Distortion"; }
    std::string type_name() const override { return "Distortion"; }

    void process(float* l, float* r, size_t nframes) override
    {
        if (m_bypassed) return;

        float gain = 1.0f + drive * 20.0f;

        for (size_t i = 0; i < nframes; ++i)
        {
            float in_l = l[i];
            float in_r = r[i];

            // Soft-clipping using atan
            float out_l = std::atan(in_l * gain) / (M_PI / 2.0f);
            float out_r = std::atan(in_r * gain) / (M_PI / 2.0f);

            l[i] = in_l * (1.0f - mix) + out_l * mix;
            r[i] = in_r * (1.0f - mix) + out_r * mix;
        }
    }

    std::string get_state() const override {
        nlohmann::json j;
        j["drive"] = drive;
        j["mix"] = mix;
        j["bypassed"] = m_bypassed;
        return j.dump();
    }

    void set_state(const std::string& state) override {
        try {
            auto j = nlohmann::json::parse(state);
            if (j.contains("drive")) drive = j["drive"];
            if (j.contains("mix")) mix = j["mix"];
            if (j.contains("bypassed")) m_bypassed = j["bypassed"];
        } catch(...) {}
    }

    std::vector<std::string> get_presets() override {
        return {"Mild Overdrive", "Crunch", "Heavy Fuzz", "Digital Shred"};
    }

    void load_preset(const std::string& name) override {
        m_current_preset = name;
        if (name == "Mild Overdrive") { drive = 0.2f; mix = 0.8f; }
        else if (name == "Crunch") { drive = 0.5f; mix = 1.0f; }
        else if (name == "Heavy Fuzz") { drive = 0.8f; mix = 1.0f; }
        else if (name == "Digital Shred") { drive = 1.0f; mix = 1.0f; }
    }
};

} // namespace disgrace_ns
