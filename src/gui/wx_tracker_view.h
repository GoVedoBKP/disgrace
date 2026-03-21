#pragma once

#include <wx/wxprec.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <vector>

#include "../sequencer/pattern.h"
#include "../core/key_bindings.h"

namespace disgrace_ns {

class Engine;

class TrackerView : public wxScrolledWindow {
public:
    TrackerView(wxWindow* parent, wxWindowID id, Pattern& pattern, Engine& engine);

    void draw(wxDC& dc);
    void OnPaint(wxPaintEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseDrag(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);

    void set_current_row(int row);
    int get_cursor_row() const { return m_cursor_row; }
    void set_pattern(Pattern& pattern);
    void recalculate_size();
    void ensure_cursor_visible();
    bool handle_action(Action action);

private:
    void delete_current_field();
    void clamp_cursor();
    void insert_note(uint8_t note);
    int get_center_row_y();

    Engine& m_engine;
    Pattern* m_pattern;

    struct TrackUI {
        int x, w;
        int btn_plus_x, btn_minus_x;
    };
    std::vector<TrackUI> m_track_ui;

    int m_cursor_row = 0;
    int m_cursor_track = 0;
    int m_cursor_col = 0;
    int m_cursor_field = 0; // 0: Note, 1: Sample, 2: Volume, 3: FX1, 4: P1, 5: FX2, 6: P2

    bool m_sel_active = false;
    int m_sel_start_track = -1;
    int m_sel_start_row = -1;
    int m_sel_end_track = -1;
    int m_sel_end_row = -1;

    bool m_selecting = false;
    int m_sel_row_start = -1;
    int m_sel_track_start = -1;
    int m_sel_row_end = -1;
    int m_sel_track_end = -1;

    wxDECLARE_EVENT_TABLE();
};

} // namespace disgrace_ns
