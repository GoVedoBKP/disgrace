#include "wx_waveform_view.h"
#include "theme.h"
#include "../core/engine.h"

#include <wx/dcclient.h>

namespace disgrace_ns {

wxBEGIN_EVENT_TABLE(WaveformView, wxPanel)
    EVT_PAINT(WaveformView::OnPaint)
    EVT_LEFT_DOWN(WaveformView::OnMouseDown)
    EVT_MOTION(WaveformView::OnMouseDrag)
    EVT_LEFT_UP(WaveformView::OnMouseUp)
    EVT_MOUSEWHEEL(WaveformView::OnMouseWheel)
wxEND_EVENT_TABLE()

WaveformView::WaveformView(wxWindow* parent, wxWindowID id, Engine& engine)
    : wxPanel(parent, id), m_engine(engine)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

void WaveformView::set_sample(std::shared_ptr<SampleData> s) {
    m_sample = s;
    Refresh();
}

void WaveformView::get_view_range(size_t& start, size_t& end) {
    if (!m_sample || m_sample->left.empty()) {
        start = 0;
        end = 0;
        return;
    }
    size_t total = m_sample->left.size();
    start = (size_t)m_offset;
    end = (size_t)(m_offset + total / m_zoom);
    if (end > total) end = total;
}

void WaveformView::zoom_in() {
    m_zoom *= 1.5;
    Refresh();
}

void WaveformView::zoom_out() {
    m_zoom /= 1.5;
    if (m_zoom < 1.0) m_zoom = 1.0;
    Refresh();
}

void WaveformView::view_all() {
    m_zoom = 1.0;
    m_offset = 0;
    Refresh();
}

void WaveformView::view_selection() {
    if (m_sel_start != m_sel_end) {
        size_t start = std::min(m_sel_start, m_sel_end);
        size_t end = std::max(m_sel_start, m_sel_end);
        m_offset = start;
        m_zoom = (double)GetClientSize().GetWidth() / (end - start);
        Refresh();
    }
}

void WaveformView::OnPaint(wxPaintEvent& event) {
    wxPaintDC dc(this);
    wxSize size = GetClientSize();

    dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_bg)));
    dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_bg)));
    dc.DrawRectangle(0, 0, size.GetWidth(), size.GetHeight());

    if (!m_sample || m_sample->left.empty()) return;

    dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_lpb_highlight)));
    dc.DrawLine(0, size.GetHeight() / 2, size.GetWidth(), size.GetHeight() / 2);

    dc.SetPen(wxPen(ThemeManager::toWxColour(m_color)));

    size_t start, end;
    get_view_range(start, end);

    if (end <= start) return;

    int mid_y = size.GetHeight() / 2;
    double samples_per_pixel = (double)(end - start) / size.GetWidth();

    for (int i = 0; i < size.GetWidth(); ++i) {
        size_t s = start + (size_t)(i * samples_per_pixel);
        size_t e = start + (size_t)((i + 1) * samples_per_pixel);
        if (e > m_sample->left.size()) e = m_sample->left.size();

        float min_v = 1.0f, max_v = -1.0f;
        for (size_t j = s; j < e && j < m_sample->left.size(); ++j) {
            float v = m_sample->left[j];
            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
        }

        int y1 = mid_y + (int)(min_v * (size.GetHeight() / 2 - 2));
        int y2 = mid_y + (int)(max_v * (size.GetHeight() / 2 - 2));
        dc.DrawLine(i, y1, i, y2);
    }
}

void WaveformView::OnMouseDown(wxMouseEvent& event) {
    SetFocus();
    int x = event.GetX();
    size_t start, end;
    get_view_range(start, end);
    if (end > start) {
        m_sel_start = start + (size_t)((double)x / GetClientSize().GetWidth() * (end - start));
        m_sel_end = m_sel_start;
    }
    Refresh();
}

void WaveformView::OnMouseDrag(wxMouseEvent& event) {
    int x = event.GetX();
    size_t start, end;
    get_view_range(start, end);
    if (end > start) {
        m_sel_end = start + (size_t)((double)x / GetClientSize().GetWidth() * (end - start));
    }
    Refresh();
}

void WaveformView::OnMouseUp(wxMouseEvent& event) {}

void WaveformView::OnMouseWheel(wxMouseEvent& event) {
    if (event.ControlDown()) {
        if (event.GetWheelRotation() < 0) zoom_out();
        else zoom_in();
    }
}

} // namespace disgrace_ns
