#include "wx_tracker_view.h"
#include "theme.h"
#include "../core/engine.h"
#include "../edit/cmd_edit_block.h"

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

int TrackerView::get_field_at(int track, int x) {
    if (track < 0 || track >= (int)m_track_ui.size() || !m_pattern) return 0;
    int rx = x - m_track_ui[track].x - 2;
    int char_w = 8;
    int char_x = rx / char_w;
    int num_cols = (int)m_pattern->column_count(track);
    
    if (char_x < num_cols * 10) {
        int col = char_x / 10;
        int field_x = char_x % 10;
        if (field_x < 4) return col * 3 + 0; // Note
        if (field_x < 7) return col * 3 + 1; // Sample
        return col * 3 + 2; // Volume
    } else {
        int fx_char_x = char_x - num_cols * 10;
        int fx_field = fx_char_x / 3;
        return num_cols * 3 + std::min(3, fx_field); // 3, 4, 5, 6
    }
}

int TrackerView::get_field_x(int track, int abs_field, int& width) {
    if (track < 0 || track >= (int)m_track_ui.size() || !m_pattern) return 0;
    int num_cols = (int)m_pattern->column_count(track);
    int char_w = 8;
    int x = m_track_ui[track].x + 2;
    if (abs_field < num_cols * 3) {
        int col = abs_field / 3;
        int field = abs_field % 3;
        x += col * 10 * char_w;
        if (field == 0) { width = 3 * char_w; }
        else if (field == 1) { x += 4 * char_w; width = 2 * char_w; }
        else { x += 7 * char_w; width = 2 * char_w; }
    } else {
        x += num_cols * 10 * char_w;
        int fx_field = abs_field - num_cols * 3;
        x += fx_field * 3 * char_w;
        width = 2 * char_w;
    }
    return x;
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

            auto is_selected = [&](int field) {
                if (!m_sel_active) return false;
                auto start = std::make_pair(m_sel_start.track, m_sel_start.field);
                auto end = std::make_pair(m_sel_end.track, m_sel_end.field);
                if (start > end) std::swap(start, end);
                auto pos = std::make_pair(t, field);
                int min_r = std::min(m_sel_start.row, m_sel_end.row);
                int max_r = std::max(m_sel_start.row, m_sel_end.row);
                return r >= min_r && r <= max_r && pos >= start && pos <= end;
            };

            int col_x = tui.x + 2;
            for (int c = 0; c < num_cols; ++c) {
                const auto& ev = m_pattern->event(t, r, c);

                // Note
                int f_idx = c * 3 + 0;
                int f_w = 0;
                int f_x = get_field_x(t, f_idx, f_w);
                if (is_selected(f_idx)) {
                    dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                    dc.DrawRectangle(f_x - 1, ry, f_w + 2, row_h);
                    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
                } else if (r == m_cursor_row && t == m_cursor_track && c == m_cursor_col && m_cursor_field == 0) {
                    dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                    dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                    dc.DrawRectangle(f_x - 1, ry, f_w + 2, row_h);
                    dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
                } else {
                    dc.SetTextForeground(ev.note == 255 ? ThemeManager::toWxColour(m_engine.m_tracker_lpb_highlight) : ThemeManager::toWxColour(m_engine.m_tracker_note));
                }
                if (ev.note == 255) dc.DrawText("---", f_x, ry + 2);
                else if (ev.note == 254) dc.DrawText("OFF", f_x, ry + 2);
                else dc.DrawText(wxString::Format("%s%d", note_names[ev.note % 12], ev.note / 12), f_x, ry + 2);

                // Sample
                f_idx = c * 3 + 1;
                f_x = get_field_x(t, f_idx, f_w);
                if (is_selected(f_idx)) {
                    dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                    dc.DrawRectangle(f_x - 1, ry, f_w + 2, row_h);
                    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
                } else if (r == m_cursor_row && t == m_cursor_track && c == m_cursor_col && m_cursor_field == 1) {
                    dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                    dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                    dc.DrawRectangle(f_x - 1, ry, f_w + 2, row_h);
                    dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
                } else {
                    wxColour s_col = ThemeManager::toWxColour(m_engine.m_tracker_sample);
                    if (!is_sampler) s_col = wxColour(s_col.Red() / 3, s_col.Green() / 3, s_col.Blue() / 3);
                    dc.SetTextForeground(s_col);
                }
                if (ev.sample_idx == 0) dc.DrawText("..", f_x, ry + 2);
                else dc.DrawText(wxString::Format("%02X", ev.sample_idx), f_x, ry + 2);

                // Volume
                f_idx = c * 3 + 2;
                f_x = get_field_x(t, f_idx, f_w);
                if (is_selected(f_idx)) {
                    dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                    dc.DrawRectangle(f_x - 1, ry, f_w + 2, row_h);
                    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
                } else if (r == m_cursor_row && t == m_cursor_track && c == m_cursor_col && m_cursor_field == 2) {
                    dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                    dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                    dc.DrawRectangle(f_x - 1, ry, f_w + 2, row_h);
                    dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
                } else {
                    dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_volume));
                }
                if (ev.volume == 255) dc.DrawText("..", f_x, ry + 2);
                else dc.DrawText(wxString::Format("%02X", ev.volume), f_x, ry + 2);
            }

            const auto& row_ev = m_pattern->event(t, r, 0);
            // FX1
            int f_idx = num_cols * 3 + 0;
            int f_w = 0;
            int f_x = get_field_x(t, f_idx, f_w);
            if (is_selected(f_idx)) {
                dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                dc.DrawRectangle(f_x - 1, ry, f_w + 2, row_h);
                dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
            } else if (r == m_cursor_row && t == m_cursor_track && m_cursor_field == 3) {
                dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.DrawRectangle(f_x - 1, ry, f_w + 2, row_h);
                dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
            } else dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_effect));
            dc.DrawText(wxString::Format("%02X", row_ev.effect1), f_x, ry + 2);

            // P1
            f_idx = num_cols * 3 + 1;
            f_x = get_field_x(t, f_idx, f_w);
            if (is_selected(f_idx)) {
                dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                dc.DrawRectangle(f_x - 1, ry, f_w + 2, row_h);
                dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
            } else if (r == m_cursor_row && t == m_cursor_track && m_cursor_field == 4) {
                dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.DrawRectangle(f_x - 1, ry, f_w + 2, row_h);
                dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
            } else dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_effect));
            dc.DrawText(wxString::Format("%02X", row_ev.param1), f_x, ry + 2);

            // FX2
            f_idx = num_cols * 3 + 2;
            f_x = get_field_x(t, f_idx, f_w);
            if (is_selected(f_idx)) {
                dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                dc.DrawRectangle(f_x - 1, ry, f_w + 2, row_h);
                dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
            } else if (r == m_cursor_row && t == m_cursor_track && m_cursor_field == 5) {
                dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.DrawRectangle(f_x - 1, ry, f_w + 2, row_h);
                dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
            } else dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_effect));
            dc.DrawText(wxString::Format("%02X", row_ev.effect2), f_x, ry + 2);

            // P2
            f_idx = num_cols * 3 + 3;
            f_x = get_field_x(t, f_idx, f_w);
            if (is_selected(f_idx)) {
                dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));
                dc.DrawRectangle(f_x - 1, ry, f_w + 2, row_h);
                dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));
            } else if (r == m_cursor_row && t == m_cursor_track && m_cursor_field == 6) {
                dc.SetBrush(wxBrush(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_tracker_cursor)));
                dc.DrawRectangle(f_x - 1, ry, f_w + 2, row_h);
                dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_bg));
            } else dc.SetTextForeground(ThemeManager::toWxColour(m_engine.m_tracker_effect));
            dc.DrawText(wxString::Format("%02X", row_ev.param2), f_x, ry + 2);
        }
        dc.SetPen(wxPen(ThemeManager::toWxColour(m_engine.m_fg_color)));
        dc.DrawLine(tui.x + tui.w + 5, 0, tui.x + tui.w + 5, 20000);
    }
}

void TrackerView::OnKeyDown(wxKeyEvent& event) {
    int key = event.GetKeyCode();
    int wx_mods = event.GetModifiers();
    int modifiers = 0;
    if (wx_mods & wxMOD_CONTROL) modifiers |= 0x1000;
    if (wx_mods & wxMOD_ALT)     modifiers |= 0x2000;
    if (wx_mods & wxMOD_SHIFT)   modifiers |= 0x4000;

    Action action = m_engine.m_key_bindings.get_action(key, modifiers);
    bool shift = (wx_mods & wxMOD_SHIFT);

    if (m_cursor_track >= (int)m_engine.track_count()) return;
    int num_cols = (int)m_pattern->column_count(m_cursor_track);
    int total_fields = num_cols * 3 + 4;
    int abs_field = (m_cursor_field < 3) ? (m_cursor_col * 3 + m_cursor_field) : (num_cols * 3 + m_cursor_field - 3);

    int old_row = m_cursor_row;
    int old_track = m_cursor_track;
    int old_field = abs_field;

    bool navigated = false;

    switch (key) {
        case WXK_UP:
            m_cursor_row -= m_engine.step_size();
            if (m_cursor_row < 0) m_cursor_row = (int)m_pattern->row_count() - 1;
            navigated = true;
            break;
        case WXK_DOWN:
            m_cursor_row += m_engine.step_size();
            if (m_cursor_row >= (int)m_pattern->row_count()) m_cursor_row = 0;
            navigated = true;
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
            navigated = true;
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
            navigated = true;
            break;
        case WXK_PAGEUP: m_cursor_row = std::max(0, m_cursor_row - 16); navigated = true; break;
        case WXK_PAGEDOWN: m_cursor_row = std::min((int)m_pattern->row_count() - 1, m_cursor_row + 16); navigated = true; break;
        case WXK_HOME: m_cursor_row = 0; navigated = true; break;
        case WXK_END: m_cursor_row = (int)m_pattern->row_count() - 1; navigated = true; break;
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

    if (navigated) {
        if (shift) {
            if (!m_sel_active) { m_sel_active = true; m_sel_start = {old_track, old_row, old_field}; }
            int cur_f = (m_cursor_field < 3) ? (m_cursor_col * 3 + m_cursor_field) : ((int)m_pattern->column_count(m_cursor_track) * 3 + m_cursor_field - 3);
            m_sel_end = {m_cursor_track, m_cursor_row, cur_f};
        } else {
            m_sel_active = false;
        }
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

        int row = center_row + (my - center_y) / row_h;
        row = std::max(0, std::min((int)m_pattern->row_count() - 1, row));
        m_cursor_row = row;

        int track = -1;
        for (size_t t = 0; t < m_track_ui.size(); ++t) {
            if (mx >= m_track_ui[t].x && mx < m_track_ui[t].x + m_track_ui[t].w) { track = (int)t; break; }
        }
        if (track != -1) m_cursor_track = track;
        
        int field = get_field_at(m_cursor_track, mx);
        int num_cols = (int)m_pattern->column_count(m_cursor_track);
        if (field < num_cols * 3) {
            m_cursor_col = field / 3;
            m_cursor_field = field % 3;
        } else {
            m_cursor_col = 0;
            m_cursor_field = 3 + (field - num_cols * 3);
        }

        if (event.ShiftDown()) {
            if (!m_sel_active) { 
                m_sel_active = true; 
                m_sel_start = {m_cursor_track, m_cursor_row, field};
            }
            m_sel_end = {m_cursor_track, m_cursor_row, field};
        } else { m_sel_active = false; }
        
        m_selecting = true;
        if (!event.ShiftDown()) {
            m_sel_start = {m_cursor_track, m_cursor_row, field};
            m_sel_end = {m_cursor_track, m_cursor_row, field};
        }
        
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

        int field = get_field_at(track, mx);

        if (!m_sel_active) { m_sel_active = true; }
        m_sel_end = {track, row, field};
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
    int abs_f = (m_cursor_field < 3) ? (m_cursor_col * 3 + m_cursor_field) : ((int)m_pattern->column_count(m_cursor_track) * 3 + m_cursor_field - 3);
    uint8_t old_v = m_pattern->get_field(m_cursor_track, m_cursor_row, abs_f);
    uint8_t new_v = 0;
    if (m_cursor_field == 0) new_v = 255;
    else if (m_cursor_field == 2) new_v = 255;

    if (old_v != new_v) {
        std::vector<CmdEditBlock::CellEdit> edits;
        edits.push_back({(size_t)m_cursor_track, (size_t)m_cursor_row, (size_t)abs_f, old_v, new_v});
        m_engine.undo_stack().execute(std::make_unique<CmdEditBlock>(*m_pattern, edits));
        Refresh();
    }
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
        int abs_f = m_cursor_col * 3 + 0;
        uint8_t old_note = m_pattern->get_field(m_cursor_track, target_row, abs_f);
        if (old_note != final_note) {
            std::vector<CmdEditBlock::CellEdit> edits;
            edits.push_back({(size_t)m_cursor_track, (size_t)target_row, (size_t)abs_f, old_note, final_note});
            m_engine.undo_stack().execute(std::make_unique<CmdEditBlock>(*m_pattern, edits));
        }

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
        case Action::Undo: m_engine.undo_stack().undo(); Refresh(); return true;
        case Action::Redo: m_engine.undo_stack().redo(); Refresh(); return true;

        case Action::Copy: {
            if (!m_sel_active) return true;
            auto& clip = m_engine.clipboard();
            clip.cells.clear();
            auto start = std::make_pair(m_sel_start.track, m_sel_start.field);
            auto end = std::make_pair(m_sel_end.track, m_sel_end.field);
            if (start > end) std::swap(start, end);
            int min_r = std::min(m_sel_start.row, m_sel_end.row);
            int max_r = std::max(m_sel_start.row, m_sel_end.row);
            clip.width_tracks = end.first - start.first + 1;
            clip.height_rows = max_r - min_r + 1;
            for (int t = start.first; t <= end.first; ++t) {
                int total_f = (int)m_pattern->column_count(t) * 3 + 4;
                for (int r = min_r; r <= max_r; ++r) {
                    for (int f = 0; f < total_f; ++f) {
                        auto cur_pos = std::make_pair(t, f);
                        if (cur_pos >= start && cur_pos <= end) {
                            uint8_t val = m_pattern->get_field(t, r, f);
                            clip.cells.push_back({t - start.first, r - min_r, f, val});
                        }
                    }
                }
            }
            return true;
        }
        case Action::Cut: {
            if (!m_sel_active) return true;
            handle_action(Action::Copy);
            handle_action(Action::Clear);
            return true;
        }
        case Action::Paste: {
            const auto& clip = m_engine.clipboard();
            if (clip.cells.empty()) return true;
            std::vector<CmdEditBlock::CellEdit> edits;
            for (const auto& cell : clip.cells) {
                int target_t = m_cursor_track + cell.rel_track;
                int target_r = m_cursor_row + cell.rel_row;
                int target_f = cell.abs_field;
                if (target_t < (int)m_engine.track_count() && target_r < (int)m_pattern->row_count()) {
                    int total_f = (int)m_pattern->column_count(target_t) * 3 + 4;
                    if (target_f < total_f) {
                        uint8_t old_v = m_pattern->get_field(target_t, target_r, target_f);
                        if (old_v != cell.value)
                            edits.push_back({(size_t)target_t, (size_t)target_r, (size_t)target_f, old_v, cell.value});
                    }
                }
            }
            if (!edits.empty()) {
                m_engine.undo_stack().execute(std::make_unique<CmdEditBlock>(*m_pattern, edits));
                Refresh();
            }
            return true;
        }

        case Action::NoteOff: note = 254; break;
        // ... (Note actions remain same)
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
        
        case Action::JumpToRow0:  m_cursor_row = 0; Refresh(); return true;
        case Action::JumpToRow16: m_cursor_row = 16; clamp_cursor(); Refresh(); return true;
        case Action::JumpToRow32: m_cursor_row = 32; clamp_cursor(); Refresh(); return true;
        case Action::JumpToRow48: m_cursor_row = 48; clamp_cursor(); Refresh(); return true;

        case Action::JumpToNextColumn: {
            int num_cols = (int)m_pattern->column_count(m_cursor_track);
            m_cursor_col++;
            if (m_cursor_col >= num_cols) {
                m_cursor_col = 0;
                m_cursor_track++;
                if (m_cursor_track >= (int)m_engine.track_count()) m_cursor_track = 0;
            }
            m_cursor_field = 0;
            ensure_cursor_visible(); Refresh(); return true;
        }
        case Action::JumpToPrevColumn: {
            m_cursor_col--;
            if (m_cursor_col < 0) {
                m_cursor_track--;
                if (m_cursor_track < 0) m_cursor_track = (int)m_engine.track_count() - 1;
                m_cursor_col = (int)m_pattern->column_count(m_cursor_track) - 1;
            }
            m_cursor_field = 0;
            ensure_cursor_visible(); Refresh(); return true;
        }

        case Action::IncPatternIndex: {
            size_t pos = m_engine.m_edit_order_pos.load();
            auto order = m_engine.order_list();
            if (pos < order.size()) {
                if (order[pos] < m_engine.pattern_count() - 1) {
                    order[pos]++;
                    m_engine.set_order(order);
                    m_engine.set_active_pattern(order[pos]);
                    set_pattern(m_engine.pattern());
                    Refresh();
                }
            }
            return true;
        }
        case Action::DecPatternIndex: {
            size_t pos = m_engine.m_edit_order_pos.load();
            auto order = m_engine.order_list();
            if (pos < order.size()) {
                if (order[pos] > 0) {
                    order[pos]--;
                    m_engine.set_order(order);
                    m_engine.set_active_pattern(order[pos]);
                    set_pattern(m_engine.pattern());
                    Refresh();
                }
            }
            return true;
        }

        case Action::SelectAll:
            m_sel_active = true;
            m_sel_start = {0, 0, 0};
            m_sel_end = {(int)m_engine.track_count() - 1, (int)m_pattern->row_count() - 1, (int)m_pattern->column_count(m_engine.track_count() - 1) * 3 + 3};
            Refresh();
            return true;

        case Action::Clear:
            if (m_sel_active) {
                std::vector<CmdEditBlock::CellEdit> edits;
                auto start = std::make_pair(m_sel_start.track, m_sel_start.field);
                auto end = std::make_pair(m_sel_end.track, m_sel_end.field);
                if (start > end) std::swap(start, end);
                int min_r = std::min(m_sel_start.row, m_sel_end.row);
                int max_r = std::max(m_sel_start.row, m_sel_end.row);

                for (int t = start.first; t <= end.first; ++t) {
                    int num_cols = (int)m_pattern->column_count(t);
                    int total_f = num_cols * 3 + 4;
                    for (int r = min_r; r <= max_r; ++r) {
                        for (int f = 0; f < total_f; ++f) {
                            auto cur_pos = std::make_pair(t, f);
                            if (cur_pos >= start && cur_pos <= end) {
                                uint8_t old_v = m_pattern->get_field(t, r, f);
                                uint8_t new_v = 0;
                                if (f < num_cols * 3) {
                                    if (f % 3 == 0) new_v = 255; // Note
                                    else if (f % 3 == 2) new_v = 255; // Vol
                                }
                                if (old_v != new_v)
                                    edits.push_back({(size_t)t, (size_t)r, (size_t)f, old_v, new_v});
                            }
                        }
                    }
                }
                if (!edits.empty()) {
                    m_engine.undo_stack().execute(std::make_unique<CmdEditBlock>(*m_pattern, edits));
                    Refresh();
                }
                return true;
            } else if (m_engine.m_record_enabled.load()) {
                delete_current_field();
                return true;
            }
            break;

        case Action::InsertRow:
            if (m_engine.m_record_enabled.load()) {
                m_pattern->insert_row(m_cursor_row);
                Refresh(); return true;
            }
            break;
        case Action::DeleteRow:
            if (m_engine.m_record_enabled.load()) {
                m_pattern->delete_row(m_cursor_row);
                Refresh(); return true;
            }
            break;

        case Action::InsertPattern:
            m_engine.add_pattern_to_order();
            Refresh(); return true;
        case Action::DeletePattern: {
            size_t pos = m_engine.m_edit_order_pos.load();
            m_engine.remove_pattern_from_order(pos);
            if (pos >= m_engine.m_order.size() && pos > 0) m_engine.m_edit_order_pos.store(pos - 1);
            if (!m_engine.m_order.empty()) m_engine.set_active_pattern(m_engine.m_order[m_engine.m_edit_order_pos.load()]);
            set_pattern(m_engine.pattern());
            Refresh(); return true;
        }
        case Action::DuplicatePattern: {
            size_t pos = m_engine.m_edit_order_pos.load();
            m_engine.copy_pattern_in_order(pos);
            Refresh(); return true;
        }

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
