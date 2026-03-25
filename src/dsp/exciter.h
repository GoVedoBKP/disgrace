#pragma once
#include "dsp.h"
#include <nlohmann/json.hpp>
#include <cmath>

namespace disgrace_ns
{

class ExciterDSP : public disgrace_ns::DSP
{
public:
    ExciterDSP() { m_current_preset = "Master Polish"; }
    float threshold = 0.5f;
    float amount = 0.3f;
    float freq = 0.5f; // High-pass cutoff proxy

    std::string name() const override { return "Exciter"; }
    std::string type_name() const override { return "Exciter"; }

    void process(float* l, float* r, size_t nframes) override
    {
        if (m_bypassed) return;
        float alpha = freq * 0.9f; // Simple HPF coefficient

        for (size_t i = 0; i < nframes; ++i)
        {
            float in_l = l[i];
            float in_r = r[i];

            // HPF
            m_hpf_l = alpha * (m_hpf_l + in_l - m_prev_l);
            m_hpf_r = alpha * (m_hpf_r + in_r - m_prev_r);
            m_prev_l = in_l;
            m_prev_r = in_r;

            // Saturate high frequencies
            float sat_l = std::tanh(m_hpf_l * amount * 5.0f);
            float sat_r = std::tanh(m_hpf_r * amount * 5.0f);

            l[i] = in_l + sat_l * amount;
            r[i] = in_r + sat_r * amount;
        }
    }

    std::string get_state() const override {
        nlohmann::json j;
        j["amount"] = amount;
        j["freq"] = freq;
        j["bypassed"] = m_bypassed;
        return j.dump();
    }

    void set_state(const std::string& state) override {
        try {
            auto j = nlohmann::json::parse(state);
            if (j.contains("amount")) amount = j["amount"];
            if (j.contains("freq")) freq = j["freq"];
            if (j.contains("bypassed")) m_bypassed = j["bypassed"];
        } catch(...) {}
    }

    std::vector<std::string> get_presets() override {
        return {"Vocal Air", "Drum Crisp", "Master Polish"};
    }

    void load_preset(const std::string& name) override {
        m_current_preset = name;
        if (name == "Vocal Air") { amount = 0.4f; freq = 0.8f; }
        else if (name == "Drum Crisp") { amount = 0.6f; freq = 0.6f; }
        else if (name == "Master Polish") { amount = 0.2f; freq = 0.7f; }
    }

private:
    float m_hpf_l = 0, m_hpf_r = 0;
    float m_prev_l = 0, m_prev_r = 0;
};

} // namespace disgrace_ns
