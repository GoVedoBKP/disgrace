#pragma once

#include <wx/wxprec.h>
#include <wx/panel.h>
#include <memory>

#include "../audio/sample_data.h"

namespace disgrace_ns {

class Engine;

enum class ChannelMode { Both, Left, Right };

class WaveformView : public wxPanel {
public:
    WaveformView(wxWindow* parent, wxWindowID id, Engine& engine);

    void set_sample(std::shared_ptr<SampleData> s);
    void set_color(unsigned int c) { m_color = c; Refresh(); }

    void zoom_in();
    void zoom_out();
    void view_all();
    void view_selection();

    void OnPaint(wxPaintEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseDrag(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);

    size_t selection_start() const { return m_sel_start; }
    size_t selection_end() const { return m_sel_end; }
    void set_channel_mode(ChannelMode mode) { m_mode = mode; Refresh(); }

private:
    void get_view_range(size_t& start, size_t& end);

    Engine& m_engine;
    std::shared_ptr<SampleData> m_sample;
    unsigned int m_color = 0x40FF4000;

    size_t m_sel_start = 0;
    size_t m_sel_end = 0;

    double m_zoom = 1.0;
    size_t m_offset = 0;

    ChannelMode m_mode = ChannelMode::Both;

    wxDECLARE_EVENT_TABLE();
};

} // namespace disgrace_ns
