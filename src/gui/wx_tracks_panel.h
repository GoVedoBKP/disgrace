/*
 * Disgrace - Digital Audio Workstation
 * Copyright (C) 2025  Miroslav Shaltev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/dc.h>

namespace disgrace_ns {

class Engine;

class TracksPanel : public wxPanel {
public:
    TracksPanel(wxWindow* parent, Engine& engine);
    void update();

public:
    void set_tab_index(int idx) { m_tab_index = idx; }

private:
    void on_zoom_in(wxCommandEvent& event);
    void on_zoom_out(wxCommandEvent& event);
    void on_view_all(wxCommandEvent& event);
    void on_view_sel(wxCommandEvent& event);
    void on_detach(wxCommandEvent& event);

    Engine& m_engine;
    int m_tab_index = -1;
    wxButton* m_zoom_in_btn;
    wxButton* m_zoom_out_btn;
    wxButton* m_view_all_btn;
    wxButton* m_view_sel_btn;
    wxButton* m_detach_btn;
    class TracksView* m_tracks_view;
    class DetachedFrame* m_detached_frame = nullptr;

    wxDECLARE_EVENT_TABLE();
};

class TracksView : public wxScrolledWindow {
public:
    TracksView(wxWindow* parent, wxWindowID id, Engine& engine);

    int get_total_ticks();
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
    void OnSize(wxSizeEvent& event);
    void draw(wxDC& dc);

private:
    int tick_to_x(int tick);
    int x_to_tick(int x);
    int get_track_height(int track_idx) const;
    void toggle_track_minimize(int track_idx);

    Engine& m_engine;
    double m_zoom = 10.0;
    int m_scroll_x = 0;
    bool m_needs_initial_view_all = true;

    int m_sel_start_tick = -1;
    int m_sel_end_tick = -1;
    bool m_is_selecting = false;

    wxDECLARE_EVENT_TABLE();
};

} // namespace disgrace_ns
