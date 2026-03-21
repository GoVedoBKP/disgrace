#pragma once

#include "audio_backend.h"
#include <atomic>

namespace disgrace_ns
{

class NullBackend : public AudioBackend
{
public:
    NullBackend() : m_active(false) {}
    virtual ~NullBackend() = default;

    bool start() override { m_active = true; return true; }
    void stop() override { m_active = false; }
    bool is_active() const override { return m_active; }

    uint32_t sample_rate() const override { return 44100; }
    uint32_t buffer_size() const override { return 512; }

private:
    std::atomic<bool> m_active;
};

} // namespace disgrace_ns
