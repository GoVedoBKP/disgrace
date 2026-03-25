#pragma once
#include "dsp.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>

namespace disgrace_ns
{

class GateDSP : public disgrace_ns::DSP
{
public:
    GateDSP() { m_current_preset = "Noise Floor"; }
    float threshold = 0.1f; // 0 to 1
    float range = 0.0f;     // 0 to 1 (0 is total silence when closed)
    float attack = 0.01f;   // seconds
    float release = 0.1f;   // seconds

    std::string name() const override { return "Gate"; }
    std::string type_name() const override { return "Gate"; }

    void process(float* l, float* r, size_t nframes) override
    {
        if (m_bypassed) return;

        float attack_coeff = std::exp(-1.0f / (attack * 44100.0f));
        float release_coeff = std::exp(-1.0f / (release * 44100.0f));

        for (size_t i = 0; i < nframes; ++i)
        {
            float env = std::max(std::abs(l[i]), std::abs(r[i]));
            
            if (env > m_envelope)
                m_envelope = env + attack_coeff * (m_envelope - env);
            else
                m_envelope = env + release_coeff * (m_envelope - env);

            float target_gain = (m_envelope > threshold) ? 1.0f : range;
            
            // Smooth gain change
            m_gain = target_gain + 0.95f * (m_gain - target_gain);

            l[i] *= m_gain;
            r[i] *= m_gain;
        }
    }

    std::string get_state() const override {
        nlohmann::json j;
        j["threshold"] = threshold;
        j["range"] = range;
        j["attack"] = attack;
        j["release"] = release;
        j["bypassed"] = m_bypassed;
        return j.dump();
    }

    void set_state(const std::string& state) override {
        try {
            auto j = nlohmann::json::parse(state);
            if (j.contains("threshold")) threshold = j["threshold"];
            if (j.contains("range")) range = j["range"];
            if (j.contains("attack")) attack = j["attack"];
            if (j.contains("release")) release = j["release"];
            if (j.contains("bypassed")) m_bypassed = j["bypassed"];
        } catch(...) {}
    }

    std::vector<std::string> get_presets() override {
        return {"Noise Floor", "Tight Drums", "Hard Cut"};
    }

    void load_preset(const std::string& name) override {
        m_current_preset = name;
        if (name == "Noise Floor") { threshold = 0.05f; range = 0.0f; attack = 0.01f; release = 0.1f; }
        else if (name == "Tight Drums") { threshold = 0.2f; range = 0.0f; attack = 0.001f; release = 0.05f; }
        else if (name == "Hard Cut") { threshold = 0.5f; range = 0.0f; attack = 0.001f; release = 0.01f; }
    }

private:
    float m_envelope = 0.0f;
    float m_gain = 1.0f;
};

} // namespace disgrace_ns
