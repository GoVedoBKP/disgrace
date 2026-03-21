#include "wx_tracker_view.h"
#include "theme.h"
#include "../core/engine.h"

#include <wx/dcclient.h>
#include <wx/msgdlg.h>
#include <wx/settings.h>

namespace disgrace_ns {

wxBEGIN_EVENT_TABLE(TrackerView, wxScrolledWindow)
    EVT_PAINT(TrackerView::OnPaint)
    EVT_KEY_DOWN(TrackerView::OnKeyDown)
    EVT_LEFT_DOWN(TrackerView::OnMouseDown)
    EVT_MOTION(TrackerView::OnMouseDrag)
    EVT_LEFT_UP(TrackerView::OnMouseUp)
wxEND_EVENT_TABLE()

static const char* note_names[] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};

TrackerView::TrackerView(wxWindow* parent, wxWindowID id, Pattern& pattern, Engine& engine)
    : wxScrolledWindow(parent, id), m_engine(engine), m_pattern(&pattern)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    // Use a fixed-width font for the grid
    SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    recalculate_size();
}

void TrackerView::recalculate_size() {
    if (!m_pattern) return;

    int track_count = (int)m_engine.track_count();
    int row_count = (int)m_pattern->row_count();
    int char_w = 8;
    int row_h = 18;
    int header_w = 40;

    m_track_ui.clear();
    int cur_x = header_w;
    for (int t = 0; t < track_count; ++t) {
        TrackUI tui;
        tui.x = cur_x;
        int num_cols = (int)m_pattern->column_count(t);
        // note(4) + sample(3) + vol(3) per col + 2x(fx(3)+param(3))
        tui.w = (int)(num_cols * 10 * char_w + 12 * char_w + 10);
        tui.btn_plus_x = cur_x + tui.w - 20;
        tui.btn_minus_x = cur_x + tui.w - 40;
        m_track_ui.push_back(tui);
        cur_x += tui.w + 10;
    }

    int total_w = cur_x + 50;
    int total_h = 20 + row_count * row_h + 20;
    SetVirtualSize(total_w, total_h);
    SetScrollRate(8, 18);
}

int TrackerView::get_center_row_y() {
    int row_h = 18;
    int h = GetClientSize().GetHeight();
    int center_y = 20 + (h - 20) / 2;
    // Align center_y to row grid
    return ((center_y - 20) / row_h) * row_h + 20;
}

void TrackerView::OnPaint(wxPaintEvent& event) {
    wxPaintDC dc(this);
    PrepareDC(dc);
    draw(dc);
}

void TrackerView::draw(wxDC& dc) {
    if (!m_pattern) return;

    int track_count = (int)m_engine.track_count();
    if (track_count != (int)m_track_ui.size()) {
        recalculate_size();
    }

    wxColour bg_col = ThemeManager::toWxColour(m_engine.m_tracker_bg);
    dc.SetBrush(wxBrush(bg_col));
    dc.SetPen(wxPen(bg_col));
    wxSize client_size = GetClientSize();
    dc.DrawRectangle(0, 0, std::max(20000, client_size.x), std::max(20000, client_size.y));

    if (m_engine.track_count() == 0) {
        dc.SetTextForeground(*wxWHITE);
        dc.DrawText("No tracks available. Add tracks in Project tab.", 50, 50);
        return;
    }

    int char_w = 8;
    int row_h = 18;
    int row_count = (int)m_pattern->row_count();
    int playing_row = (int)m_engine.current_row();
    bool is_playing = m_engine.transport_state() != TransportState::Stopped;
    int center_row = is_playing ? playing_row : m_cursor_row;
    int center_y = get_center_row_y();

    // 1. Row Highlights
    for (int r = 0; r < row_count; ++r) {
        int ry = center_y + (r - center_row) * row_h;
        if (ry < 20) continue;
        if (ry > client_size.y + 100) break;

        // LPB highlight
        uint32_t lpb = m_engine.lpb();
        if (lpb > 0 && r % lpb == 0) {
            dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_lpb_highlight)));
            dc.DrawLine(0, ry, 20000, ry);
        }

        // Playing row highlight
        if (is_playing && r == playing_row) {
            dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_row_highlight)));
            dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_row_highlight)));
            dc.DrawRectangle(0, ry, 20000, row_h);
        }

        // Cursor row highlight
        if (r == m_cursor_row) {
            wxColour cur_row_col = ThemeManager::toWxColour(m_engine.m_tracker_row_highlight);
            if (!HasFocus()) {
                cur_row_col = wxColour(cur_row_col.Red() / 2, cur_row_col.Green() / 2, cur_row_col.Blue() / 2);
            }
            dc.SetBrush(wxBrush(cur_row_col));
            dc.SetPen(wxPen(cur_row_col));
            dc.DrawRectangle(40, ry, 20000, row_h);
        }

        // Row number
        dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_text));
        dc.DrawText(wxString::Format("%03d", r), 2, ry + 2);
    }

    // 2. Tracks
    for (int t = 0; t < track_count; ++t) {
        if (t >= (int)m_track_ui.size()) break;
        const auto& tui = m_track_ui[t];
        auto& track_obj = m_engine.track(t);
        bool is_sampler = (track_obj.instrument() && track_obj.instrument()->type() == InstrumentType::Sampler);

        // Header
        dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_bg_color)));
        dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_fg_color)));
        dc.DrawRectangle(tui.x, 0, tui.w, 20);
        dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_fg_color));
        dc.DrawText(track_obj.name().substr(0, 10), tui.x + 5, 2);

        // +/- Buttons
        dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_button_color)));
        dc.DrawRectangle(tui.btn_minus_x, 2, 18, 16);
        dc.DrawRectangle(tui.btn_plus_x, 2, 18, 16);
        dc.DrawText("-", tui.btn_minus_x + 5, 2);
        dc.DrawText("+", tui.btn_plus_x + 5, 2);

        int num_cols = (int)m_pattern->column_count(t);
        for (int r = 0; r < row_count; ++r) {
            int ry = center_y + (r - center_row) * row_h;
            if (ry < 20) continue;
            if (ry > client_size.y + 100) break;

            // Selection
            if (m_sel_active) {
                int s_t = std::min(m_sel_start_track, m_sel_end_track);
                int e_t = std::max(m_sel_start_track, m_sel_end_track);
                int s_r = std::min(m_sel_start_row, m_sel_end_row);
                int e_r = std::max(m_sel_start_row, m_sel_end_row);
                if (t >= s_t && t <= e_t && r >= s_r && r <= e_r) {
                    dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                    dc.DrawRectangle(tui.x, ry, tui.w, row_h);
                }
            }

            int col_x = tui.x + 2;
            for (int c = 0; c < num_cols; ++c) {
                const auto& ev = m_pattern->event(t, r, c);

                // Note
                if (r == m_cursor_row && t == m_cursor_track && c == m_cursor_col && m_cursor_field == 0) {
                    dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                    dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                    dc.DrawRectangle(col_x - 1, ry, 3 * char_w + 4, row_h);
                    dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
                } else {
                    dc.SetTextForeground(ev.note == 255 ? ThemeManager::toWxColour(m_engine.m_tracker_lpb_highlight) : ThemeManager::toWxColour(m_engine.m_tracker_note));
                }
                if (ev.note == 255) dc.DrawText("---", col_x, ry + 2);
                else if (ev.note == 254) dc.DrawText("OFF", col_x, ry + 2);
                else dc.DrawText(wxString::Format("%s%d", note_names[ev.note % 12], ev.note / 12), col_x, ry + 2);
                col_x += 4 * char_w;

                // Sample
                if (r == m_cursor_row && t == m_cursor_track && c == m_cursor_col && m_cursor_field == 1) {
                    dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                    dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                    dc.DrawRectangle(col_x - 1, ry, 2 * char_w + 4, row_h);
                    dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
                } else {
                    wxColour s_col = ThemeManager::toWxColour(m_engine.m_tracker_sample);
                    if (!is_sampler) s_col = wxColour(s_col.Red() / 3, s_col.Green() / 3, s_col.Blue() / 3);
                    dc.SetTextForeground(s_col);
                }
                if (ev.sample_idx == 0) dc.DrawText("..", col_x, ry + 2);
                else dc.DrawText(wxString::Format("%02X", ev.sample_idx), col_x, ry + 2);
                col_x += 3 * char_w;

                // Volume
                if (r == m_cursor_row && t == m_cursor_track && c == m_cursor_col && m_cursor_field == 2) {
                    dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                    dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                    dc.DrawRectangle(col_x - 1, ry, 2 * char_w + 4, row_h);
                    dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
                } else {
                    dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_volume));
                }
                if (ev.volume == 255) dc.DrawText("..", col_x, ry + 2);
                else dc.DrawText(wxString::Format("%02X", ev.volume), col_x, ry + 2);
                col_x += 3 * char_w;
            }

            const auto& row_ev = m_pattern->event(t, r, 0);
            // FX1
            if (r == m_cursor_row && t == m_cursor_track && m_cursor_field == 3) {
                dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.DrawRectangle(col_x - 1, ry, 2 * char_w + 4, row_h);
                dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
            } else dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_effect));
            dc.DrawText(wxString::Format("%02X", row_ev.effect1), col_x, ry + 2);
            col_x += 3 * char_w;

            // P1
            if (r == m_cursor_row && t == m_cursor_track && m_cursor_field == 4) {
                dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.DrawRectangle(col_x - 1, ry, 2 * char_w + 4, row_h);
                dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
            } else dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_effect));
            dc.DrawText(wxString::Format("%02X", row_ev.param1), col_x, ry + 2);
            col_x += 3 * char_w;

            // FX2
            if (r == m_cursor_row && t == m_cursor_track && m_cursor_field == 5) {
                dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.DrawRectangle(col_x - 1, ry, 2 * char_w + 4, row_h);
                dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
            } else dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_effect));
            dc.DrawText(wxString::Format("%02X", row_ev.effect2), col_x, ry + 2);
            col_x += 3 * char_w;

            // P2
            if (r == m_cursor_row && t == m_cursor_track && m_cursor_field == 6) {
                dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.DrawRectangle(col_x - 1, ry, 2 * char_w + 4, row_h);
                dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
            } else dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_effect));
            dc.DrawText(wxString::Format("%02X", row_ev.param2), col_x, ry + 2);
        }
        dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_fg_color)));
        dc.DrawLine(tui.x + tui.w + 5, 0, tui.x + tui.w + 5, 20000);
    }
}

void TrackerView::OnKeyDown(wxKeyEvent& event) {
    int key = event.GetKeyCode();
    int mods = event.GetModifiers();
    Action action = m_engine.m_key_bindings.get_action(key, mods);

    if (m_cursor_track >= (int)m_engine.track_count()) return;
    int num_cols = (int)m_pattern->column_count(m_cursor_track);
    int total_fields = num_cols * 3 + 4;
    int abs_field = (m_cursor_field < 3) ? (m_cursor_col * 3 + m_cursor_field) : (num_cols * 3 + m_cursor_field - 3);

    switch (key) {
        case WXK_UP:
            m_cursor_row -= m_engine.step_size();
            if (m_cursor_row < 0) m_cursor_row = (int)m_pattern->row_count() - 1;
            break;
        case WXK_DOWN:
            m_cursor_row += m_engine.step_size();
            if (m_cursor_row >= (int)m_pattern->row_count()) m_cursor_row = 0;
            break;
        case WXK_LEFT:
            abs_field--;
            if (abs_field < 0) {
                if (m_cursor_track > 0) {
                    m_cursor_track--;
                    abs_field = (int)m_pattern->column_count(m_cursor_track) * 3 + 4 - 1;
                } else abs_field = 0;
            }
            if (abs_field < (int)m_pattern->column_count(m_cursor_track) * 3) {
                m_cursor_col = abs_field / 3;
                m_cursor_field = abs_field % 3;
            } else {
                m_cursor_col = 0;
                m_cursor_field = 3 + (abs_field - (int)m_pattern->column_count(m_cursor_track) * 3);
            }
            break;
        case WXK_RIGHT:
            abs_field++;
            if (abs_field >= total_fields) {
                if (m_cursor_track < (int)m_engine.track_count() - 1) {
                    m_cursor_track++;
                    abs_field = 0;
                } else abs_field = total_fields - 1;
            }
            if (abs_field < (int)m_pattern->column_count(m_cursor_track) * 3) {
                m_cursor_col = abs_field / 3;
                m_cursor_field = abs_field % 3;
            } else {
                m_cursor_col = 0;
                m_cursor_field = 3 + (abs_field - (int)m_pattern->column_count(m_cursor_track) * 3);
            }
            break;
        case WXK_PAGEUP: m_cursor_row = std::max(0, m_cursor_row - 16); break;
        case WXK_PAGEDOWN: m_cursor_row = std::min((int)m_pattern->row_count() - 1, m_cursor_row + 16); break;
        case WXK_HOME: m_cursor_row = 0; break;
        case WXK_END: m_cursor_row = (int)m_pattern->row_count() - 1; break;
        case WXK_DELETE:
        case WXK_BACK:
            if (m_engine.m_record_enabled.load()) {
                delete_current_field();
                if (key == WXK_BACK) {
                    m_cursor_row -= m_engine.step_size();
                    if (m_cursor_row < 0) m_cursor_row = (int)m_pattern->row_count() - 1;
                }
            }
            break;
        default:
            if (!handle_action(action)) {
                event.Skip();
            }
            return;
    }

    ensure_cursor_visible();
    Refresh();
}

void TrackerView::OnMouseDown(wxMouseEvent& event) {
    SetFocus();
    int mx, my;
    CalcUnscrolledPosition(event.GetX(), event.GetY(), &mx, &my);

    if (my >= 0 && my < 20) { // Header
        for (size_t t = 0; t < m_track_ui.size(); ++t) {
            const auto& ui = m_track_ui[t];
            if (mx >= ui.btn_plus_x && mx < ui.btn_plus_x + 18) {
                m_engine.pattern().set_column_count(t, m_engine.pattern().column_count(t) + 1);
                recalculate_size(); Refresh(); return;
            }
            if (mx >= ui.btn_minus_x && mx < ui.btn_minus_x + 18) {
                size_t current = m_engine.pattern().column_count(t);
                if (current > 1) { m_engine.pattern().set_column_count(t, current - 1); recalculate_size(); Refresh(); }
                return;
            }
        }
    } else {
        int row_h = 18;
        int center_y = get_center_row_y();
        bool is_playing = m_engine.transport_state() != TransportState::Stopped;
        int center_row = is_playing ? (int)m_engine.current_row() : m_cursor_row;

        m_cursor_row = center_row + (my - center_y) / row_h;
        m_cursor_row = std::max(0, std::min((int)m_pattern->row_count() - 1, m_cursor_row));

        for (size_t t = 0; t < m_track_ui.size(); ++t) {
            if (mx >= m_track_ui[t].x && mx < m_track_ui[t].x + m_track_ui[t].w) { m_cursor_track = (int)t; break; }
        }
        
        if (event.ShiftDown()) {
            if (!m_sel_active) { m_sel_active = true; m_sel_start_row = m_cursor_row; m_sel_start_track = m_cursor_track; }
            m_sel_end_row = m_cursor_row; m_sel_end_track = m_cursor_track;
        } else { m_sel_active = false; }
        
        m_selecting = true;
        ensure_cursor_visible();
        Refresh();
    }
}

void TrackerView::OnMouseDrag(wxMouseEvent& event) {
    if (m_selecting) {
        int mx, my;
        CalcUnscrolledPosition(event.GetX(), event.GetY(), &mx, &my);
        int row_h = 18;
        int center_y = get_center_row_y();
        bool is_playing = m_engine.transport_state() != TransportState::Stopped;
        int center_row = is_playing ? (int)m_engine.current_row() : m_cursor_row;

        int row = center_row + (my - center_y) / row_h;
        row = std::max(0, std::min((int)m_pattern->row_count() - 1, row));
        
        int track = 0;
        for (size_t t = 0; t < m_track_ui.size(); ++t) {
            if (mx >= m_track_ui[t].x && mx < m_track_ui[t].x + m_track_ui[t].w) { track = (int)t; break; }
        }

        if (!m_sel_active) { m_sel_active = true; m_sel_start_row = m_cursor_row; m_sel_start_track = m_cursor_track; }
        m_sel_end_row = row; m_sel_end_track = track;
        Refresh();
    }
}

void TrackerView::OnMouseUp(wxMouseEvent& event) {
    m_selecting = false;
}

void TrackerView::set_current_row(int row) {
    m_cursor_row = row;
    clamp_cursor();
    Refresh();
}

void TrackerView::set_pattern(Pattern& pattern) {
    m_pattern = &pattern;
    recalculate_size();
    Refresh();
}

void TrackerView::ensure_cursor_visible() {
    // Horizontal scrolling:
    if (m_cursor_track < (int)m_track_ui.size()) {
        int tx = m_track_ui[m_cursor_track].x;
        int tw = m_track_ui[m_cursor_track].w;
        int x, y;
        GetViewStart(&x, &y);
        int xu, yu;
        GetScrollPixelsPerUnit(&xu, &yu);
        int vx = x * xu;
        int vw = GetClientSize().x;

        if (tx < vx) Scroll(tx / xu, y);
        else if (tx + tw > vx + vw) Scroll((tx + tw - vw + 20) / xu, y);
    }
}

void TrackerView::delete_current_field() {
    if (!m_pattern) return;
    auto& ev = m_pattern->event(m_cursor_track, m_cursor_row, m_cursor_col);
    switch (m_cursor_field) {
        case 0: ev.note = 255; break;
        case 1: ev.sample_idx = 0; break;
        case 2: ev.volume = 255; break;
        case 3: ev.effect1 = 0; break;
        case 4: ev.param1 = 0; break;
        case 5: ev.effect2 = 0; break;
        case 6: ev.param2 = 0; break;
    }
    Refresh();
}

void TrackerView::clamp_cursor() {
    if (m_pattern) {
        if (m_cursor_row < 0) m_cursor_row = 0;
        if (m_cursor_row >= (int)m_pattern->row_count()) m_cursor_row = (int)m_pattern->row_count() - 1;
        if (m_cursor_track < 0) m_cursor_track = 0;
        if (m_cursor_track >= (int)m_engine.track_count()) m_cursor_track = (int)m_engine.track_count() - 1;
    }
}

void TrackerView::insert_note(uint8_t note) {
    if (!m_pattern) return;
    int target_row = m_cursor_row;
    if (m_engine.is_playing() && m_engine.m_record_enabled.load()) {
        target_row = (int)m_engine.current_row();
    }

    uint8_t final_note = note;
    if (note != 254) {
        int octave_note = note + m_engine.base_octave() * 12;
        if (octave_note > 119) octave_note = 119;
        final_note = (uint8_t)octave_note;
    }

    if (m_engine.m_record_enabled.load()) {
        m_pattern->event(m_cursor_track, (size_t)target_row, m_cursor_col).note = final_note;
        if (!m_engine.is_playing()) {
            m_cursor_row = std::min((int)m_pattern->row_count() - 1, m_cursor_row + (int)m_engine.step_size());
        }
    }
    m_engine.preview_note(m_cursor_track, final_note, m_cursor_col);
    Refresh();
}

bool TrackerView::handle_action(Action action) {
    int note = -1;
    switch (action) {
        case Action::NoteOff: note = 254; break;
        case Action::NoteC: note = 0; break;
        case Action::NoteCs: note = 1; break;
        case Action::NoteD: note = 2; break;
        case Action::NoteDs: note = 3; break;
        case Action::NoteE: note = 4; break;
        case Action::NoteF: note = 5; break;
        case Action::NoteFs: note = 6; break;
        case Action::NoteG: note = 7; break;
        case Action::NoteGs: note = 8; break;
        case Action::NoteA: note = 9; break;
        case Action::NoteAs: note = 10; break;
        case Action::NoteB: note = 11; break;
        case Action::NoteC2: note = 12; break;
        case Action::NoteCs2: note = 13; break;
        case Action::NoteD2: note = 14; break;
        case Action::NoteDs2: note = 15; break;
        case Action::NoteE2: note = 16; break;
        case Action::NoteF2: note = 17; break;
        case Action::NoteFs2: note = 18; break;
        case Action::NoteG2: note = 19; break;
        case Action::NoteGs2: note = 20; break;
        case Action::NoteA2: note = 21; break;
        case Action::NoteAs2: note = 22; break;
        case Action::NoteB2: note = 23; break;
        case Action::NoteC3: note = 24; break;
        default: return false;
    }

    if (note != -1) {
        insert_note((uint8_t)note);
        ensure_cursor_visible();
        Refresh();
        return true;
    }
    return false;
}

} // namespace disgrace_ns
