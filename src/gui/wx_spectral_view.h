#pragma once

#include <wx/wxprec.h>
#include <wx/panel.h>
#include <memory>
#include <vector>

namespace disgrace_ns {

class Engine;

class SpectralView : public wxPanel {
public:
    SpectralView(wxWindow* parent, wxWindowID id, Engine& engine);
    ~SpectralView();

    void update();
    void OnPaint(wxPaintEvent& event);

private:
    Engine& m_engine;
    std::vector<float> m_history;
    size_t m_fft_size = 1024;

    wxDECLARE_EVENT_TABLE();
};

} // namespace disgrace_ns
