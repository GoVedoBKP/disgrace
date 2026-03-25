#include "masterbus.h"
#include <cmath>

namespace disgrace_ns
{

void MasterBus::set_gain(float g)
{
    m_gain.store(g);
}

float MasterBus::gain() const
{
    return m_gain.load();
}

void MasterBus::set_pan(float p)
{
    m_pan.store(p);
}

float MasterBus::pan() const
{
    return m_pan.load();
}

void MasterBus::set_mute(bool m)
{
    m_muted.store(m);
}

bool MasterBus::muted() const
{
    return m_muted.load();
}

float MasterBus::soft_clip(float x)
{
    // simple musical soft limiter
    return ::std::tanh(x);
}

void MasterBus::process(float* l,
                        float* r,
                        size_t nframes)
{
    m_filter.process(l, r, nframes);
    m_styles.process(l, r, nframes);
    m_reference_matcher.process(l, r, nframes);

    float vol = m_muted.load() ? 0.f : m_gain.load();
    float pan = m_pan.load();
    float left_gain  = vol * (pan <= 0 ? 1.0f : 1.0f - pan);
    float right_gain = vol * (pan >= 0 ? 1.0f : 1.0f + pan);

    float peak_l = 0.f;
    float peak_r = 0.f;

    for (size_t i = 0; i < nframes; ++i)
    {
        float sl = l[i] * left_gain;
        float sr = r[i] * right_gain;

        sl = soft_clip(sl);
        sr = soft_clip(sr);

        l[i] = sl;
        r[i] = sr;

        if (m_is_recording.load()) {
            m_recorded_l.push_back(sl);
            m_recorded_r.push_back(sr);
        }

        float pl = ::std::fabs(sl);
        if (pl > peak_l) peak_l = pl;

        float pr = ::std::fabs(sr);
        if (pr > peak_r) peak_r = pr;
    }

    if (m_export_mute.load()) {
        for (size_t i = 0; i < nframes; ++i) {
            l[i] = 0.f; r[i] = 0.f;
        }
    }

    float prev_l = m_meter_l.load();
    float smoothed_l = 0.9f * prev_l + 0.1f * peak_l;
    m_meter_l.store(smoothed_l);

    float prev_r = m_meter_r.load();
    float smoothed_r = 0.9f * prev_r + 0.1f * peak_r;
    m_meter_r.store(smoothed_r);
}

float MasterBus::meter_l() const
{
    return m_meter_l.load();
}

float MasterBus::meter_r() const
{
    return m_meter_r.load();
}

} // namespace disgrace_ns