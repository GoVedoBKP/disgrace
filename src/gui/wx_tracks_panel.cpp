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

#include <wx/app.h>
#include <wx/dcclient.h>
#include "wx_tracks_panel.h"
#include "wx_detached_frame.h"
#include "../core/engine.h"
#include "../instrument/sample_instrument.h"
#include "../instrument/soundfont_instrument.h"
#include "theme.h"

namespace disgrace_ns {

static void draw_waveform_helper(wxDC& dc, int x, int y, int w, int h, const SampleData& data, const wxColour& col) {
    if (data.left.empty() || w <= 0) return;
    dc.SetPen(wxPen(col));
    
    bool is_stereo = !data.right.empty();
    int ch_h = is_stereo ? h / 2 : h;

    auto draw_channel = [&](const std::vector<float>& ch_data, int ch_y) {
        int mid_y = ch_y + ch_h / 2;
        double samples_per_pixel = (double)ch_data.size() / w;
        for (int i = 0; i < w; ++i) {
            size_t start = (size_t)(i * samples_per_pixel);
            size_t end = (size_t)((i + 1) * samples_per_pixel);
            if (end > ch_data.size()) end = ch_data.size();
            if (start >= end) {
                if (start < ch_data.size()) {
                    int amp = (int)(ch_data[start] * (ch_h / 2 - 2));
                    dc.DrawLine(x + i, mid_y - amp, x + i, mid_y + amp);
                }
                continue;
            }
            float min_v = 1.0f, max_v = -1.0f;
            for (size_t s = start; s < end; ++s) {
                if (ch_data[s] < min_v) min_v = ch_data[s];
                if (ch_data[s] > max_v) max_v = ch_data[s];
            }
            int y1 = mid_y + (int)(min_v * (ch_h / 2 - 2));
            int y2 = mid_y + (int)(max_v * (ch_h / 2 - 2));
            dc.DrawLine(x + i, y1, x + i, y2);
        }
    };

    draw_channel(data.left, y);
    if (is_stereo) {
        draw_channel(data.right, y + ch_h);
        // Draw a small divider line
        dc.SetPen(wxPen(wxColour(100, 100, 100, 128)));
        dc.DrawLine(x, y + ch_h, x + w, y + ch_h);
        dc.SetPen(wxPen(col));
    }
}

enum {
    ID_ZOOM_IN = 10001,
    ID_ZOOM_OUT,
    ID_VIEW_ALL,
    ID_VIEW_SEL,
    ID_DETACH
};

wxBEGIN_EVENT_TABLE(TracksPanel, wxPanel)
    EVT_BUTTON(ID_ZOOM_IN, TracksPanel::on_zoom_in)
    EVT_BUTTON(ID_ZOOM_OUT, TracksPanel::on_zoom_out)
    EVT_BUTTON(ID_VIEW_ALL, TracksPanel::on_view_all)
    EVT_BUTTON(ID_VIEW_SEL, TracksPanel::on_view_sel)
    EVT_BUTTON(ID_DETACH, TracksPanel::on_detach)
wxEND_EVENT_TABLE()

TracksPanel::TracksPanel(wxWindow* parent, Engine& engine)
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
    m_detach_btn = new wxButton(this, ID_DETACH, "[]", wxDefaultPosition, wxSize(30, btn_h));

    btn_sizer->Add(m_zoom_in_btn, 0, wxALL, 2);
    btn_sizer->Add(m_zoom_out_btn, 0, wxALL, 2);
    btn_sizer->Add(m_view_all_btn, 0, wxALL, 2);
    btn_sizer->Add(m_view_sel_btn, 0, wxALL, 2);
    btn_sizer->Add(m_detach_btn, 0, wxALL, 2);

    main_sizer->Add(btn_sizer, 0, wxEXPAND | wxALL, 2);

    m_tracks_view = new TracksView(this, wxID_ANY, m_engine);
    main_sizer->Add(m_tracks_view, 1, wxEXPAND | wxALL, 0);

    SetSizer(main_sizer);
}

void TracksPanel::update() {
    static int last_total_ticks = -1;
    static int last_track_count = -1;
    int current_total = m_tracks_view->get_total_ticks();
    int current_tracks = (int)m_engine.track_count();
    if (current_total != last_total_ticks || current_tracks != last_track_count) {
        m_tracks_view->update_view();
        last_total_ticks = current_total;
        last_track_count = current_tracks;
    }
    m_tracks_view->Refresh();
}

void TracksPanel::on_zoom_in(wxCommandEvent& event) { m_tracks_view->zoom_in(); }
void TracksPanel::on_zoom_out(wxCommandEvent& event) { m_tracks_view->zoom_out(); }
void TracksPanel::on_view_all(wxCommandEvent& event) { m_tracks_view->view_all(); }
void TracksPanel::on_view_sel(wxCommandEvent& event) { m_tracks_view->view_selection(); }
void TracksPanel::on_detach(wxCommandEvent& event) {
    if (m_detached_frame) {
        return;
    }
    Hide();
    m_detached_frame = new DetachedFrame(this, "Tracks", GetParent(), m_tab_index);
    m_detached_frame->set_on_detach_callback([this]() { m_detached_frame = nullptr; });
}

wxBEGIN_EVENT_TABLE(TracksView, wxScrolledWindow)
    EVT_PAINT(TracksView::OnPaint)
    EVT_SIZE(TracksView::OnSize)
    EVT_LEFT_DOWN(TracksView::OnMouseDown)
    EVT_MOTION(TracksView::OnMouseDrag)
    EVT_LEFT_UP(TracksView::OnMouseUp)
    EVT_MOUSEWHEEL(TracksView::OnMouseWheel)
wxEND_EVENT_TABLE()

TracksView::TracksView(wxWindow* parent, wxWindowID id, Engine& engine)
    : wxScrolledWindow(parent, id), m_engine(engine)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_zoom = 10.0;
    SetScrollRate(1, 1);
    m_needs_initial_view_all = true;
}

void TracksView::OnSize(wxSizeEvent& event) {
    if (m_needs_initial_view_all && GetClientSize().x > 150) {
        view_all();
    }
    event.Skip();
}

int TracksView::get_total_ticks() {
    auto order = m_engine.order_list();
    int total_rows = 0;
    for (auto pat_idx : order) {
        total_rows += (int)m_engine.pattern(pat_idx).row_count();
    }
    return total_rows;
}

int TracksView::tick_to_x(int tick) { return (int)(tick * m_zoom); }
int TracksView::x_to_tick(int x) { return (m_zoom > 0) ? (int)(x / m_zoom) : 0; }

int TracksView::get_track_height(int track_idx) const {
    if (track_idx < 0 || (size_t)track_idx >= m_engine.track_count()) {
        return 80;
    }
    auto& track = m_engine.track(track_idx);
    return track.is_minimized() ? 20 : 80;
}

void TracksView::toggle_track_minimize(int track_idx) {
    if (track_idx < 0 || (size_t)track_idx >= m_engine.track_count()) {
        return;
    }
    auto& track = m_engine.track(track_idx);
    track.set_minimized(!track.is_minimized());
    update_view();
    Refresh();
}

void TracksView::OnPaint(wxPaintEvent& event) {
    wxPaintDC dc(this);
    PrepareDC(dc);
    draw(dc);
}

void TracksView::draw(wxDC& dc) {
    dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_bg)));
    dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_bg)));
    wxSize virtual_size = GetVirtualSize();
    dc.DrawRectangle(0, 0, virtual_size.GetWidth(), virtual_size.GetHeight());

    int track_h = 80;
    int header_w = 120;
    int num_tracks = (int)m_engine.track_count();
    auto order = m_engine.order_list();
    uint32_t lpb = m_engine.lpb();

    // Draw Time Scale (Header)
    dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_lpb_highlight)));
    dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_lpb_highlight)));
    dc.DrawRectangle(header_w, 0, virtual_size.GetWidth() - header_w, 20);

    dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_text));
    wxFont header_font(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
    dc.SetFont(header_font);

    int total_rows = 0;
    size_t current_pos = m_engine.current_order_pos();
    int play_tick = 0;

    for (size_t i = 0; i < order.size(); ++i) {
        auto& pat = m_engine.pattern(order[i]);
        int pat_rows = (int)pat.row_count();
        int px = header_w + tick_to_x(total_rows);

        // Calculate playhead position
        if (i < current_pos) {
            play_tick += pat_rows;
        } else if (i == current_pos) {
            play_tick += (int)m_engine.current_row();
        }

        // Pattern boundary
        dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_lpb_highlight)));
        dc.DrawLine(px, 0, px, virtual_size.GetHeight());

        // Pattern label
        wxString buf;
        buf.Printf("POS %zu (PAT %zu)", i, order[i]);
        dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_text));
        dc.DrawText(buf, px + 5, 2);

        // Beat markers
        if (lpb > 0) {
            for (int r = 0; r < pat_rows; r += lpb) {
                int bx = px + tick_to_x(r);
                dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_lpb_highlight)));
                dc.DrawLine(bx, 15, bx, 25);
                if (r % (lpb * 4) == 0) {
                    wxString bbuf;
                    bbuf.Printf("%d", r / lpb);
                    dc.DrawText(bbuf, bx + 2, 28);
                }
            }
        }

        total_rows += pat_rows;
    }

    // Draw Tracks
    int cur_y = 30;
    for (int t = 0; t < num_tracks; ++t) {
        auto& track_obj = m_engine.track(t);
        int track_h_actual = track_obj.is_minimized() ? 20 : 80;
        int ty = cur_y;
        cur_y += track_h_actual;

        // Track Header
        dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_bg_color)));
        dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_fg_color)));
        dc.DrawRectangle(0, ty, header_w, track_h_actual - 1);

        // Minimize button (- for expanded, + for minimized)
        int btn_w = 20;
        int btn_h = 18;
        int btn_x = 5;
        int btn_y = ty + (track_h_actual - btn_h) / 2;
        
        dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_lpb_highlight)));
        dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_fg_color)));
        dc.DrawRectangle(btn_x, btn_y, btn_w, btn_h);
        
        dc.SetTextForeground(*wxWHITE);
        wxFont btn_font(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        dc.SetFont(btn_font);
        wxString btn_text = track_obj.is_minimized() ? "+" : "-";
        dc.DrawText(btn_text, btn_x + 6, btn_y + 2);

        wxFont bold_font(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        dc.SetFont(bold_font);
        dc.SetTextForeground(*wxWHITE);
        wxString name = track_obj.name().substr(0, 15);
        dc.DrawText(name, 30, ty + 5);

        Instrument* inst = track_obj.instrument();

        // Instrument Info (only shown when not minimized)
        if (!track_obj.is_minimized()) {
            wxFont normal_font(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
            dc.SetFont(normal_font);
            dc.SetTextForeground(wxColour(200, 200, 200));
            if (inst) {
                wxString inst_name = inst->name().substr(0, 20);
                dc.DrawText(inst_name, 30, ty + 20);
                const char* type_str = "";
                switch(inst->type()) {
                    case InstrumentType::Sampler: type_str = "[Sampler]"; break;
                    case InstrumentType::SoundFont: type_str = "[SoundFont]"; break;
                    case InstrumentType::Plugin: type_str = "[Plugin]"; break;
                    case InstrumentType::Midi: type_str = "[MIDI]"; break;
                    default: type_str = "[None]"; break;
                }
                dc.DrawText(type_str, 30, ty + 35);
            }
        }

        // Track Content Area (only shown when not minimized)
        if (!track_obj.is_minimized()) {
            int rows_done = 0;
            for (auto pat_idx : order) {
                auto& pat = m_engine.pattern(pat_idx);
                int pat_rows = (int)pat.row_count();
                int px = header_w + tick_to_x(rows_done);

                size_t num_cols = pat.column_count(t);
                for (int r = 0; r < pat_rows; ++r) {
                    for (size_t c = 0; c < num_cols; ++c) {
                        const auto& ev = pat.event(t, r, c);
                        if (ev.note != 255) {
                            int nx = px + tick_to_x(r);

                            if (ev.note == 254) { // Note Off
                                dc.SetPen(wxPen(wxColour(255, 100, 100)));
                                dc.DrawLine(nx, ty + 5, nx, ty + track_h_actual - 5);
                            } else {
                                // Find note length
                                int note_len = 1;
                                bool found_end = false;
                                for (int r2 = r + 1; r2 < pat_rows; ++r2) {
                                    if (pat.event(t, r2, c).note != 255) {
                                        note_len = r2 - r;
                                        found_end = true;
                                        break;
                                    }
                                }
                                if (!found_end) note_len = pat_rows - r;

                                int nw = tick_to_x(note_len);
                                if (nw < 2) nw = 2;

                                if (inst && inst->type() == InstrumentType::Sampler) {
                                    SampleInstrument* sampler = static_cast<SampleInstrument*>(inst);
                                    size_t s_idx = (ev.sample_idx > 0) ? (ev.sample_idx - 1) : sampler->selected_sample();
                                    if (s_idx < sampler->sample_count()) {
                                        auto& sample = sampler->get_sample(s_idx);
                                        if (sample.data) {
                                            // Draw sample background
                                            dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_lpb_highlight)));
                                            dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_lpb_highlight)));
                                            dc.DrawRectangle(nx, ty + 5, nw, track_h_actual - 10);
                                            // Draw waveform
                                            draw_waveform_helper(dc, nx, ty + 5, nw, track_h_actual - 10, *sample.data, ThemeManager::toWxColour(m_engine.m_tracker_note));
                                        }
                                    }
                                } else {
                                    // Just a block
                                    dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_note)));
                                    dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_note)));
                                    dc.DrawRectangle(nx, ty + 5, nw, track_h_actual - 10);
                                    
                                    dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
                                    wxFont note_font(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
                                    dc.SetFont(note_font);
                                    const char* notes[] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
                                    wxString nbuf;
                                    nbuf.Printf("%s%d", notes[ev.note % 12], ev.note / 12);
                                    if (nw > 20) dc.DrawText(nbuf, nx + 2, ty + 15);
                                }
                            }
                        }
                    }
                }
                rows_done += pat_rows;
            }
        }

        // Horizontal line between tracks
        dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_lpb_highlight)));
        dc.DrawLine(header_w, ty + track_h_actual - 1, header_w + tick_to_x(total_rows), ty + track_h_actual - 1);
    }

    // Current Playback Marker
    if (m_engine.transport_state() != TransportState::Stopped) {
        int play_x = header_w + tick_to_x(play_tick);
        dc.SetPen(wxPen(wxColour(255, 255, 255)));
        dc.DrawLine(play_x, 0, play_x, virtual_size.GetHeight());
    }

    // Selection
    if (m_sel_start_tick != -1 && m_sel_end_tick != -1) {
        int s1 = std::min(m_sel_start_tick, m_sel_end_tick);
        int s2 = std::max(m_sel_start_tick, m_sel_end_tick);
        int sx1 = header_w + tick_to_x(s1);
        int sx2 = header_w + tick_to_x(s2);
        
        // Use a semi-transparent selection if possible, or just lines for now to match FLTK
        dc.SetPen(wxPen(wxColour(0, 120, 215), 2));
        dc.DrawLine(sx1, 20, sx1, virtual_size.GetHeight());
        dc.DrawLine(sx2, 20, sx2, virtual_size.GetHeight());
        
        // Better selection look:
        dc.SetBrush(wxBrush(wxColour(0, 120, 215, 64))); // Semi-transparent blue
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(sx1, 20, sx2 - sx1, virtual_size.GetHeight() - 20);
    }
}

void TracksView::OnMouseDown(wxMouseEvent& event) {
    int x, y;
    CalcUnscrolledPosition(event.GetX(), event.GetY(), &x, &y);
    int header_w = 120;
    
    // Check for minimize button clicks (in header area)
    if (x < header_w) {
        // Calculate which track was clicked
        int num_tracks = (int)m_engine.track_count();
        int cur_y = 30;
        for (int t = 0; t < num_tracks; ++t) {
            auto& track_obj = m_engine.track(t);
            int track_h = track_obj.is_minimized() ? 20 : 80;
            int ty = cur_y;
            
            if (y >= ty && y < ty + track_h) {
                // Check if minimize button was clicked (5-25 pixels from left)
                if (x >= 5 && x <= 25) {
                    toggle_track_minimize(t);
                    return;
                }
                break;
            }
            cur_y += track_h;
        }
        return;
    }
    
    // Track content selection
    if (x > header_w) {
        m_is_selecting = true;
        m_sel_start_tick = x_to_tick(x - header_w);
        m_sel_end_tick = m_sel_start_tick;
        Refresh();
    }
}

void TracksView::OnMouseDrag(wxMouseEvent& event) {
    if (m_is_selecting) {
        int x, y;
        CalcUnscrolledPosition(event.GetX(), event.GetY(), &x, &y);
        int header_w = 120;
        m_sel_end_tick = x_to_tick(x - header_w);
        Refresh();
    }
}

void TracksView::OnMouseUp(wxMouseEvent& event) {
    m_is_selecting = false;
}

void TracksView::OnMouseWheel(wxMouseEvent& event) {
    if (event.ControlDown()) {
        if (event.GetWheelRotation() < 0) zoom_out();
        else zoom_in();
    } else {
        event.Skip();
    }
}

void TracksView::zoom_in() { m_zoom *= 1.5; update_view(); }
void TracksView::zoom_out() { m_zoom /= 1.5; if (m_zoom < 0.1) m_zoom = 0.1; update_view(); }
void TracksView::view_all() {
    int total = get_total_ticks();
    if (total > 0) {
        wxSize size = GetClientSize();
        if (size.GetWidth() <= 150) size = GetParent()->GetClientSize();
        if (size.GetWidth() > 150) {
            m_zoom = (double)(size.GetWidth() - 140) / total;
            if (m_zoom < 0.1) m_zoom = 0.1;
            m_needs_initial_view_all = false;
        }
    }
    update_view();
}
void TracksView::view_selection() {
    if (m_sel_start_tick != -1 && m_sel_end_tick != -1) {
        int diff = std::abs(m_sel_end_tick - m_sel_start_tick);
        if (diff > 0) {
            wxSize size = GetParent()->GetClientSize();
            m_zoom = (double)(size.GetWidth() - 140) / diff;
        }
    }
    update_view();
}

void TracksView::update_view() {
    int num_tracks = (int)m_engine.track_count();
    int total_h = 30;
    for (int t = 0; t < num_tracks; ++t) {
        total_h += get_track_height(t);
    }
    int total_w = 120 + tick_to_x(get_total_ticks()) + 50;
    total_h += 50;
    SetVirtualSize(total_w, total_h);
    Refresh();
}

} // namespace disgrace_ns
