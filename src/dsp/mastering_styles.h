#pragma once
#include "dsp.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <string>

namespace disgrace_ns
{

enum class MasteringStyle {
    TRANSPARENT,
    WARM,
    PUNCHY,
    AGGRESSIVE
};

class MasteringStylesDSP : public disgrace_ns::DSP
{
public:
    MasteringStylesDSP() {
        m_current_preset = "Transparent";
        m_style = MasteringStyle::TRANSPARENT;
    }

    std::string name() const override { return "Mastering Styles"; }
    std::string type_name() const override { return "MasteringStyles"; }

    void process(float* l, float* r, size_t nframes) override {
        if (m_bypassed) return;
        for (size_t i = 0; i < nframes; ++i) {
            float sl = l[i];
            float sr = r[i];

            switch (m_style) {
                case MasteringStyle::TRANSPARENT:
                    // Just a very gentle soft clip to prevent digital clipping
                    sl = std::tanh(sl * 1.05f);
                    sr = std::tanh(sr * 1.05f);
                    break;
                case MasteringStyle::WARM:
                    // Saturation + slight low-mid boost (simulated by non-linear gain)
                    sl = std::tanh(sl * 1.2f);
                    sr = std::tanh(sr * 1.2f);
                    sl *= 0.95f; // makeup gain
                    sr *= 0.95f;
                    break;
                case MasteringStyle::PUNCHY:
                    // Emphasize transients by slight compression (simulated with envelope-ish behavior)
                    // (Simplified implementation for now)
                    sl = (sl > 0 ? std::pow(sl, 0.9f) : -std::pow(-sl, 0.9f));
                    sr = (sr > 0 ? std::pow(sr, 0.9f) : -std::pow(-sr, 0.9f));
                    break;
                case MasteringStyle::AGGRESSIVE:
                    // More drive and harder clipping
                    sl = std::tanh(sl * 1.5f);
                    sr = std::tanh(sr * 1.5f);
                    break;
            }

            l[i] = sl;
            r[i] = sr;
        }
    }

    std::vector<std::string> get_presets() override {
        return {"Transparent", "Warm", "Punchy", "Aggressive"};
    }

    void load_preset(const std::string& name) override {
        m_current_preset = name;
        if (name == "Transparent") m_style = MasteringStyle::TRANSPARENT;
        else if (name == "Warm") m_style = MasteringStyle::WARM;
        else if (name == "Punchy") m_style = MasteringStyle::PUNCHY;
        else if (name == "Aggressive") m_style = MasteringStyle::AGGRESSIVE;
    }

    std::string get_state() const override {
        nlohmann::json j;
        j["style"] = (int)m_style;
        j["bypassed"] = m_bypassed;
        return j.dump();
    }

    void set_state(const std::string& state) override {
        try {
            auto j = nlohmann::json::parse(state);
            if (j.contains("style")) m_style = (MasteringStyle)j["style"];
            if (j.contains("bypassed")) m_bypassed = j["bypassed"];
        } catch(...) {}
    }

private:
    MasteringStyle m_style = MasteringStyle::TRANSPARENT;
};

} // namespace disgrace_ns
