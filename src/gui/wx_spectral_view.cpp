#include "wx_spectral_view.h"
#include "../core/engine.h"

#include <wx/dcclient.h>

namespace disgrace_ns {

wxBEGIN_EVENT_TABLE(SpectralView, wxPanel)
    EVT_PAINT(SpectralView::OnPaint)
wxEND_EVENT_TABLE()

SpectralView::SpectralView(wxWindow* parent, wxWindowID id, Engine& engine)
    : wxPanel(parent, id), m_engine(engine)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_history.resize(64 * 512);
}

SpectralView::~SpectralView() {}

void SpectralView::update() {
    Refresh();
}

void SpectralView::OnPaint(wxPaintEvent& event) {
    wxPaintDC dc(this);
    wxSize size = GetClientSize();

    dc.SetBrush(wxBrush(wxColour(m_engine.m_tracker_bg)));
    dc.SetPen(wxPen(wxColour(m_engine.m_tracker_bg)));
    dc.DrawRectangle(0, 0, size.GetWidth(), size.GetHeight());

    dc.SetPen(wxPen(wxColour(0, 255, 0)));
    int bar_w = size.GetWidth() / 64;
    for (int i = 0; i < 64; ++i) {
        float level = m_engine.master_meter_l() * ((64 - i) / 64.0f);
        int bar_h = (int)(level * size.GetHeight());
        dc.DrawRectangle(i * bar_w, size.GetHeight() - bar_h, bar_w - 1, bar_h);
    }
}

} // namespace disgrace_ns
