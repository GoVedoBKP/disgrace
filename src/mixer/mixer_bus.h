#pragma once

#include "../dsp/dsp_chain.h"
#include <string>
#include <atomic>
#include <vector>

namespace disgrace_ns {

class MixerBus {
public:
    MixerBus();
    MixerBus(MixerBus&& other) noexcept;
    MixerBus& operator=(MixerBus&& other) noexcept;

    MixerBus(const MixerBus&) = delete;
    MixerBus& operator=(const MixerBus&) = delete;

    void process(float* out_l, float* out_r, size_t nframes);
    
    void set_name(const std::string& name);
    const std::string& name() const;

    void set_volume(float v);
    float volume() const;

    void set_pan(float p);
    float pan() const;

    void set_mute(bool m);
    bool muted() const;

    float meter_l() const;
    float meter_r() const;

    // Routing: -1 = Master, 0+ = Index of another MixerBus
    void set_output_bus(int bus_idx);
    int output_bus() const;

    const disgrace_ns::DSPChain& chain() const { return m_chain; }
    disgrace_ns::DSPChain& chain() { return m_chain; }

    // DSP Chain management
    void set_effect(size_t index, ::std::unique_ptr<disgrace_ns::DSP> dsp);
    void enable_effect(size_t index, bool en);
    void move_effect_up(size_t index);
    void move_effect_down(size_t index);
    void remove_effect(size_t index);
    disgrace_ns::DSP* get_effect(size_t index) const;
    bool is_effect_enabled(size_t index) const;

    void save_effect_chain(const std::string& path);
    void load_effect_chain(const std::string& path);

private:
    std::string m_name;
    float m_volume = 1.0f;
    float m_pan = 0.0f;
    bool m_mute = false;
    int m_output_bus = -1; // Default to Master

    disgrace_ns::DSPChain m_chain;
    std::atomic<float> m_meter_l{0.0f};
    std::atomic<float> m_meter_r{0.0f};
};

} // namespace disgrace_ns
