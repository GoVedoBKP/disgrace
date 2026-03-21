#pragma once

#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/button.h>
#include <wx/sizer.h>

namespace disgrace_ns {

class Engine;

class NotationView : public wxScrolledWindow {
public:
    NotationView(wxWindow* parent, wxWindowID id, Engine& engine);
    void zoom_in();
    void zoom_out();
    void view_all();
    void view_selection();
    void update_view();
    void OnPaint(wxPaintEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseDrag(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void draw(wxDC& dc);

private:
    void draw_staff(wxDC& dc, int tx, int ty, int tw, int type);
    void draw_note(wxDC& dc, int nx, int ny, int note, int staff_type);
    void on_preview_track(wxCommandEvent& event);

    int get_total_ticks();
    int tick_to_x(int tick);
    int x_to_tick(int x);

    Engine& m_engine;
    double m_zoom;
    int m_sel_start_tick = -1;
    int m_sel_end_tick = -1;
    bool m_is_selecting = false;

    std::vector<wxButton*> m_preview_buttons;

    wxDECLARE_EVENT_TABLE();
};

class NotationPanel : public wxPanel {
public:
    NotationPanel(wxWindow* parent, Engine& engine);
    void update();

private:
    void on_zoom_in(wxCommandEvent& event);
    void on_zoom_out(wxCommandEvent& event);
    void on_view_all(wxCommandEvent& event);
    void on_view_sel(wxCommandEvent& event);

    Engine& m_engine;
    wxButton* m_zoom_in_btn;
    wxButton* m_zoom_out_btn;
    wxButton* m_view_all_btn;
    wxButton* m_view_sel_btn;
    class NotationView* m_notation_view;

    wxDECLARE_EVENT_TABLE();
};

} // namespace disgrace_ns
