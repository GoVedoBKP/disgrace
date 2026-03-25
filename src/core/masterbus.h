#pragma once
#include <atomic>
#include <cstddef>
#include <string>
#include <memory>
#include "../dsp/mastering_filter.h"
#include "../dsp/mastering_styles.h"
#include "../dsp/reference_matcher.h"

namespace disgrace_ns
{

class MasterBus
{
public:
    void set_gain(float g);
    float gain() const;

    void set_pan(float p);
    float pan() const;

    void set_mute(bool m);
    bool muted() const;

    void process(float* l,
                 float* r,
                 size_t nframes);

    float meter_l() const;
    float meter_r() const;

    // Mastering Controls
    disgrace_ns::MasteringFilterDSP& mastering_filter() { return m_filter; }
    const disgrace_ns::MasteringFilterDSP& mastering_filter() const { return m_filter; }
    disgrace_ns::MasteringStylesDSP& mastering_styles() { return m_styles; }
    const disgrace_ns::MasteringStylesDSP& mastering_styles() const { return m_styles; }
    disgrace_ns::ReferenceMatcherDSP& reference_matcher() { return m_reference_matcher; }
    const disgrace_ns::ReferenceMatcherDSP& reference_matcher() const { return m_reference_matcher; }

    ::std::atomic<bool> m_is_recording{false};
    ::std::atomic<bool> m_export_mute{false};
    ::std::vector<float> m_recorded_l;
    ::std::vector<float> m_recorded_r;

private:
    float soft_clip(float x);

    ::std::atomic<float> m_gain{1.f};
    ::std::atomic<float> m_pan{0.f};
    ::std::atomic<float> m_meter_l{0.f};
    ::std::atomic<float> m_meter_r{0.f};
    ::std::atomic<bool> m_muted{false};

    disgrace_ns::MasteringFilterDSP m_filter;
    disgrace_ns::MasteringStylesDSP m_styles;
    disgrace_ns::ReferenceMatcherDSP m_reference_matcher;
};

} // namespace disgrace_ns
