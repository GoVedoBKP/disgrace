#include <wx/app.h>
#include <wx/dcclient.h>
#include <wx/msgdlg.h>
#include <wx/utils.h>
#include "wx_notation_panel.h"
#include "../core/engine.h"
#include "../io/lilypond_exporter.h"
#include "theme.h"
#include <fstream>

namespace disgrace_ns {

enum {
    ID_ZOOM_IN = 10101,
    ID_ZOOM_OUT,
    ID_VIEW_ALL,
    ID_VIEW_SEL,
    ID_PREVIEW_BASE = 10200
};

wxBEGIN_EVENT_TABLE(NotationPanel, wxPanel)
    EVT_BUTTON(ID_ZOOM_IN, NotationPanel::on_zoom_in)
    EVT_BUTTON(ID_ZOOM_OUT, NotationPanel::on_zoom_out)
    EVT_BUTTON(ID_VIEW_ALL, NotationPanel::on_view_all)
    EVT_BUTTON(ID_VIEW_SEL, NotationPanel::on_view_sel)
wxEND_EVENT_TABLE()

NotationPanel::NotationPanel(wxWindow* parent, Engine& engine)
    : wxPanel(parent, wxID_ANY), m_engine(engine)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    int btn_w = 80;
    int btn_h = 25;

    m_zoom_in_btn = new wxButton(this, ID_ZOOM_IN, "Zoom In", wxDefaultPosition, wxSize(btn_w, btn_h));
    m_zoom_out_btn = new wxButton(this, ID_ZOOM_OUT, "Zoom Out", wxDefaultPosition, wxSize(btn_w, btn_h));
    m_view_all_btn = new wxButton(this, ID_VIEW_ALL, "View All", wxDefaultPosition, wxSize(btn_w, btn_h));
    m_view_sel_btn = new wxButton(this, ID_VIEW_SEL, "View Sel", wxDefaultPosition, wxSize(btn_w, btn_h));

    btn_sizer->Add(m_zoom_in_btn, 0, wxALL, 2);
    btn_sizer->Add(m_zoom_out_btn, 0, wxALL, 2);
    btn_sizer->Add(m_view_all_btn, 0, wxALL, 2);
    btn_sizer->Add(m_view_sel_btn, 0, wxALL, 2);

    main_sizer->Add(btn_sizer, 0, wxEXPAND | wxALL, 2);

    m_notation_view = new NotationView(this, wxID_ANY, m_engine);
    main_sizer->Add(m_notation_view, 1, wxEXPAND | wxALL, 0);

    SetSizer(main_sizer);

    m_notation_view->view_all();
}

void NotationPanel::update() {
    m_notation_view->Refresh();
}

void NotationPanel::on_zoom_in(wxCommandEvent& event) { m_notation_view->zoom_in(); }
void NotationPanel::on_zoom_out(wxCommandEvent& event) { m_notation_view->zoom_out(); }
void NotationPanel::on_view_all(wxCommandEvent& event) { m_notation_view->view_all(); }
void NotationPanel::on_view_sel(wxCommandEvent& event) { m_notation_view->view_selection(); }

NotationView::NotationView(wxWindow* parent, wxWindowID id, Engine& engine)
    : wxScrolledWindow(parent, id), m_engine(engine), m_zoom(25.0) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetScrollRate(1, 1);
}

void NotationView::zoom_in() { m_zoom *= 1.5; update_view(); }
void NotationView::zoom_out() { m_zoom /= 1.5; if (m_zoom < 1.0) m_zoom = 1.0; update_view(); }
void NotationView::view_all() {
    int total = get_total_ticks();
    if (total > 0) {
        wxSize size = GetClientSize();
        if (size.GetWidth() <= 0) size = GetParent()->GetClientSize();
        m_zoom = (double)(size.GetWidth() - 140) / total;
        if (m_zoom < 1.0) m_zoom = 1.0;
    }
    update_view();
}
void NotationView::view_selection() {
    if (m_sel_start_tick != -1 && m_sel_end_tick != -1) {
        int diff = std::abs(m_sel_end_tick - m_sel_start_tick);
        if (diff > 0) {
            wxSize size = GetClientSize();
            if (size.GetWidth() <= 0) size = GetParent()->GetClientSize();
            m_zoom = (double)(size.GetWidth() - 140) / diff;
            if (m_zoom < 1.0) m_zoom = 1.0;
        }
    }
    update_view();
}

void NotationView::update_view() {
    int total_w = 120 + tick_to_x(get_total_ticks()) + 50;
    int total_h = 30 + (int)m_engine.track_count() * 120 + 50;
    SetVirtualSize(total_w, total_h);

    // Update preview buttons
    for (auto* btn : m_preview_buttons) {
        btn->Destroy();
    }
    m_preview_buttons.clear();

    int track_h = 120;
    for (int t = 0; t < (int)m_engine.track_count(); ++t) {
        auto& track_obj = m_engine.track(t);
        if (track_obj.instrument()) {
            int ty = 30 + t * track_h;
            wxButton* b = new wxButton(this, ID_PREVIEW_BASE + t, "Preview (LY)", wxPoint(5, ty + 70), wxSize(110, 25));
            wxFont btn_font(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
            b->SetFont(btn_font);
            b->Bind(wxEVT_BUTTON, &NotationView::on_preview_track, this);
            m_preview_buttons.push_back(b);
        }
    }

    Refresh();
}

void NotationView::on_preview_track(wxCommandEvent& event) {
    int t_idx = event.GetId() - ID_PREVIEW_BASE;
    if (t_idx < 0 || t_idx >= (int)m_engine.track_count()) return;

    std::string ly_src = LilyPondExporter::generate_ly_source(m_engine, t_idx);
    
    std::string tmp_ly = "/tmp/disgrace_preview.ly";
    std::string tmp_pdf = "/tmp/disgrace_preview.pdf";
    std::ofstream f(tmp_ly);
    if (f.is_open()) {
        f << ly_src;
        f.close();
        
        std::string cmd = "lilypond -o /tmp/disgrace_preview " + tmp_ly;
        if (wxExecute(cmd, wxEXEC_SYNC) == 0) {
            wxLaunchDefaultApplication(tmp_pdf);
        } else {
            wxMessageBox("LilyPond execution failed. Make sure 'lilypond' is installed and in your PATH.", "Error", wxOK | wxICON_ERROR);
        }
    }
}

int NotationView::get_total_ticks() {
    auto order = m_engine.order_list();
    int total_rows = 0;
    for (auto pat_idx : order) {
        total_rows += (int)m_engine.pattern(pat_idx).row_count();
    }
    return total_rows;
}

int NotationView::tick_to_x(int tick) { return (int)(tick * m_zoom); }
int NotationView::x_to_tick(int x) { return (m_zoom > 0) ? (int)(x / m_zoom) : 0; }

void NotationView::OnPaint(wxPaintEvent& event) {
    wxPaintDC dc(this);
    PrepareDC(dc);
    draw(dc);
}

void NotationView::draw_staff(wxDC& dc, int tx, int ty, int tw, int type) {
    dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_text)));
    int line_spacing = 8;
    
    auto draw_5_lines = [&](int start_y) {
        for (int i = 0; i < 5; ++i) {
            dc.DrawLine(tx, start_y + i * line_spacing, tx + tw, start_y + i * line_spacing);
        }
    };

    if (type == 0 || type == 3) { // Violin or Drums
        draw_5_lines(ty + 20);
        if (type == 0) { // Treble Clef
            wxFont clef_font(24, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
            dc.SetFont(clef_font);
            dc.DrawText("&", tx + 10, ty + 10); 
        } else { // Drum Clef
            dc.DrawLine(tx + 10, ty + 28, tx + 10, ty + 44);
            dc.DrawLine(tx + 15, ty + 28, tx + 15, ty + 44);
        }
    } else if (type == 1) { // Bass Clef
        draw_5_lines(ty + 20);
        wxFont clef_font(20, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        dc.SetFont(clef_font);
        dc.DrawText("?", tx + 10, ty + 12);
        dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_text)));
        dc.DrawCircle(tx + 26, ty + 26, 1);
        dc.DrawCircle(tx + 26, ty + 34, 1);
    } else if (type == 2) { // Grand Staff
        draw_5_lines(ty + 10);
        draw_5_lines(ty + 10 + 6 * line_spacing);
        wxFont clef_font(20, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        dc.SetFont(clef_font);
        dc.DrawText("&", tx + 10, ty - 5);
        dc.DrawText("?", tx + 10, ty - 5 + 6 * line_spacing);
        dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_text)));
        dc.DrawCircle(tx + 26, ty + 19 + 6 * line_spacing, 1);
        dc.DrawCircle(tx + 26, ty + 27 + 6 * line_spacing, 1);
        dc.DrawLine(tx, ty + 10, tx, ty + 10 + 10 * line_spacing);
    }
}

void NotationView::draw_note(wxDC& dc, int nx, int ny, int note, int staff_type) {
    int line_spacing = 8;
    int base_y = ny + 20; 
    
    static const int pitch_to_pos[] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
    static const bool is_sharp[] = {false, true, false, true, false, false, true, false, true, false, true, false};

    int octave = note / 12;
    int pitch = note % 12;
    int pos = pitch_to_pos[pitch] + (octave - 4) * 7;
    
    int dy = 0;
    if (staff_type == 0 || staff_type == 2 || staff_type == 3) {
        dy = (10 - pos) * (line_spacing / 2);
    } else if (staff_type == 1) {
        dy = (pos + 2) * (line_spacing / 2);
    }

    int final_y = base_y + dy;
    dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_note)));
    dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_note)));
    dc.DrawEllipse(nx - 4, final_y - 3, 8, 6);
    dc.DrawLine(nx + 3, final_y, nx + 3, final_y - 20);

    if (is_sharp[pitch]) {
        wxFont sharp_font(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        dc.SetFont(sharp_font);
        dc.DrawText("#", nx - 12, final_y - 8);
    }
}

void NotationView::draw(wxDC& dc) {
    dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_bg)));
    dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_bg)));
    wxSize virtual_size = GetVirtualSize();
    dc.DrawRectangle(0, 0, virtual_size.GetWidth(), virtual_size.GetHeight());

    int track_h = 120;
    int header_w = 120;
    int num_tracks = (int)m_engine.track_count();
    auto order = m_engine.order_list();
    int total_rows = get_total_ticks();
    int full_tw = tick_to_x(total_rows);

    for (int t = 0; t < num_tracks; ++t) {
        auto& track_obj = m_engine.track(t);
        Instrument* inst = track_obj.instrument();
        if (!inst || (inst->type() != InstrumentType::SoundFont && 
                      inst->type() != InstrumentType::Plugin && 
                      inst->type() != InstrumentType::Midi)) continue;

        int ty = 30 + t * track_h;
        if (ty > virtual_size.GetHeight()) break;

        dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_bg_color)));
        dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_fg_color)));
        dc.DrawRectangle(0, ty, header_w, track_h - 1);

        wxFont bold_font(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        dc.SetFont(bold_font);
        wxString name = track_obj.name().substr(0, 15);
        dc.DrawText(name, 5, ty + track_h / 2 - 10);

        int staff_type = (int)track_obj.notation();
        int staff_x = header_w;
        dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_text)));
        dc.DrawLine(staff_x, ty + 10, staff_x, ty + track_h - 10); 
        
        draw_staff(dc, staff_x, ty, full_tw, staff_type);

        int rows_done = 0;
        for (auto pat_idx : order) {
            auto& pat = m_engine.pattern(pat_idx);
            int pat_rows = (int)pat.row_count();
            int px = staff_x + tick_to_x(rows_done);
            size_t num_cols = pat.column_count(t);
            for (int r = 0; r < pat_rows; ++r) {
                for (size_t c = 0; c < num_cols; ++c) {
                    const auto& ev = pat.event(t, r, c);
                    if (ev.note < 128) {
                        draw_note(dc, px + tick_to_x(r), ty, ev.note, staff_type);
                    }
                }
            }
            rows_done += pat_rows;
        }
    }

    if (m_engine.transport_state() != TransportState::Stopped) {
        int play_x = header_w + tick_to_x((int)m_engine.m_current_row);
        dc.SetPen(wxPen(wxColour(255, 255, 255)));
        dc.DrawLine(play_x, 0, play_x, virtual_size.GetHeight());
    }

    if (m_sel_start_tick != -1 && m_sel_end_tick != -1) {
        int s1 = std::min(m_sel_start_tick, m_sel_end_tick);
        int s2 = std::max(m_sel_start_tick, m_sel_end_tick);
        int sx1 = header_w + tick_to_x(s1);
        int sx2 = header_w + tick_to_x(s2);
        
        dc.SetBrush(wxBrush(wxColour(0, 120, 215, 64))); 
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(sx1, 20, sx2 - sx1, virtual_size.GetHeight() - 20);
    }
}

void NotationView::OnMouseDown(wxMouseEvent& event) {
    int x, y;
    CalcUnscrolledPosition(event.GetX(), event.GetY(), &x, &y);
    int header_w = 120;
    if (x > header_w) {
        m_is_selecting = true;
        m_sel_start_tick = x_to_tick(x - header_w);
        m_sel_end_tick = m_sel_start_tick;
        Refresh();
    }
}

void NotationView::OnMouseDrag(wxMouseEvent& event) {
    if (m_is_selecting) {
        int x, y;
        CalcUnscrolledPosition(event.GetX(), event.GetY(), &x, &y);
        int header_w = 120;
        if (x > header_w) {
            m_sel_end_tick = x_to_tick(x - header_w);
            Refresh();
        }
    }
}

void NotationView::OnMouseUp(wxMouseEvent& event) {
    m_is_selecting = false;
}

void NotationView::OnMouseWheel(wxMouseEvent& event) {
    if (event.ControlDown()) {
        if (event.GetWheelRotation() < 0) zoom_out();
        else zoom_in();
    } else {
        event.Skip();
    }
}

wxBEGIN_EVENT_TABLE(NotationView, wxScrolledWindow)
    EVT_PAINT(NotationView::OnPaint)
    EVT_LEFT_DOWN(NotationView::OnMouseDown)
    EVT_MOTION(NotationView::OnMouseDrag)
    EVT_LEFT_UP(NotationView::OnMouseUp)
    EVT_MOUSEWHEEL(NotationView::OnMouseWheel)
wxEND_EVENT_TABLE()

} // namespace disgrace_ns
