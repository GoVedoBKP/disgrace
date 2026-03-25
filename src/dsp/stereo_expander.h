#pragma once
#include "dsp.h"
#include <nlohmann/json.hpp>
#include <cmath>

namespace disgrace_ns
{

class StereoExpanderDSP : public disgrace_ns::DSP
{
public:
    float width = 1.0f; // 0.0 (Mono) to 2.0 (Extra wide), 1.0 is original

    std::string name() const override { return "Stereo Expander"; }
    std::string type_name() const override { return "StereoExpander"; }

    void process(float* l, float* r, size_t nframes) override
    {
        if (m_bypassed) return;
        for (size_t i = 0; i < nframes; ++i)
        {
            float mid = (l[i] + r[i]) * 0.5f;
            float side = (l[i] - r[i]) * 0.5f;

            l[i] = mid + side * width;
            r[i] = mid - side * width;
        }
    }

    std::string get_state() const override {
        nlohmann::json j;
        j["width"] = width;
        j["bypassed"] = m_bypassed;
        return j.dump();
    }

    void set_state(const std::string& state) override {
        try {
            auto j = nlohmann::json::parse(state);
            if (j.contains("width")) width = j["width"];
            if (j.contains("bypassed")) m_bypassed = j["bypassed"];
        } catch(...) {}
    }

    std::vector<std::string> get_presets() override {
        return {"Mono", "Stereo (Normal)", "Wide", "Extra Wide"};
    }

    void load_preset(const std::string& name) override {
        if (name == "Mono") width = 0.0f;
        else if (name == "Stereo (Normal)") width = 1.0f;
        else if (name == "Wide") width = 1.5f;
        else if (name == "Extra Wide") width = 2.0f;
    }
};

} // namespace disgrace_ns
