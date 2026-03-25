#pragma once
#include <string>
#include <vector>

namespace disgrace_ns
{

class DSP
{
public:
    virtual ~DSP() = default;

    virtual void process(float* l,
                         float* r,
                         size_t nframes) = 0;

    virtual size_t latency() const { return 0; }
    
    virtual std::string name() const = 0;
    virtual std::string type_name() const = 0;

    bool is_bypassed() const { return m_bypassed; }
    void set_bypass(bool b) { m_bypassed = b; }

    virtual std::vector<std::string> get_presets() { return {"Default"}; }
    virtual void load_preset(const std::string& name) { m_current_preset = name; }
    virtual void save_preset(const std::string& name) {}

    const std::string& current_preset() const { return m_current_preset; }
    void set_current_preset(const std::string& name) { m_current_preset = name; }

    virtual std::string get_state() const { return "{}"; }
    virtual void set_state(const std::string& state) {}

protected:
    bool m_bypassed = false;
    std::string m_current_preset = "Default";
};


} // namespace disgrace_ns
