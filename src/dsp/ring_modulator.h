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
    float freq = 0.2f; // Proxy for modulation frequency
    float mix = 1.0f;

    std::string name() const override { return "Ring Modulator"; }
    std::string type_name() const override { return "RingModulator"; }

    void process(float* l, float* r, size_t nframes) override
    {
        if (m_bypassed) return;
        for (size_t i = 0; i < nframes; ++i)
        {
            float carrier = std::sin(m_phase);
            m_phase += freq * 0.1f; // High frequency modulation
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
        return {"Robot Voice", "Alien Bell", "Metallic Ring"};
    }

    void load_preset(const std::string& name) override {
        m_current_preset = name;
        if (name == "Robot Voice") { freq = 0.4f; mix = 1.0f; }
        else if (name == "Alien Bell") { freq = 0.8f; mix = 0.7f; }
        else if (name == "Metallic Ring") { freq = 0.2f; mix = 0.5f; }
    }

private:
    float m_phase = 0;
};

} // namespace disgrace_ns
