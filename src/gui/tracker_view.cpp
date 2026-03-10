#include "tracker_view.h"
#include "main_window.h"
#include "../core/engine.h"
#include <FL/fl_draw.H>
#include <FL/Fl_Scroll.H>
#include <algorithm>
#include <cstdio>

namespace disgrace_ns {

TrackerView::TrackerView(int x, int y, int w, int h, Pattern& pattern, Engine& engine)
    : Fl_Widget(x, y, w, h), m_engine(engine) {
}

void TrackerView::set_pattern(Pattern& pattern) {
    recalculate_size();
    redraw();
}

void TrackerView::clamp_cursor() {
    auto& pat = m_engine.pattern();
    int num_tracks = (int)m_engine.track_count();
    int num_rows = (int)pat.row_count();

    if (num_tracks > 0) {
        if (m_cursor_track >= num_tracks) m_cursor_track = num_tracks - 1;
        if (m_cursor_track < 0) m_cursor_track = 0;
    } else {
        m_cursor_track = 0;
    }

    if (num_rows > 0) {
        if (m_cursor_row >= num_rows) m_cursor_row = num_rows - 1;
        if (m_cursor_row < 0) m_cursor_row = 0;
    } else {
        m_cursor_row = 0;
    }
}

void TrackerView::draw() {
    auto& pat = m_engine.pattern();
    clamp_cursor();
    
    fl_push_clip(x(), y(), w(), h());
    fl_color(30, 30, 30);
    fl_rectf(x(), y(), w(), h());

    if (Fl::focus() == this) {
        fl_color(255, 0, 0);
        fl_rect(x(), y(), w(), h());
    }

    int row_h = 18;
    int char_w = 8;
    
    size_t num_tracks = m_engine.track_count();
    size_t num_rows = pat.row_count();

    m_track_ui.clear();

    if (num_tracks == 0) {
        fl_color(200, 200, 200);
        fl_font(FL_HELVETICA, 16);
        fl_draw("No tracks. Click '+ Track' in the transport bar to add one.", x() + 50, y() + 50);
        fl_pop_clip();
        return;
    }

    int playing_row = (int)m_engine.current_row();
    bool is_playing = m_engine.transport_state() != TransportState::Stopped;

    int center_row = is_playing ? playing_row : m_cursor_row;
    int center_y = y() + 20 + (h() - 20) / 2;
    // Align center_y to row grid to avoid half-row offsets
    center_y = ((center_y - (y() + 20)) / row_h) * row_h + (y() + 20);

    // Draw row numbers
    fl_color(100, 100, 100);
    fl_font(FL_COURIER, 12);
    for (size_t r = 0; r < num_rows; ++r) {
        int ry = center_y + ((int)r - center_row) * row_h;
        if (ry < y() + 20) continue;
        if (ry > y() + h()) break;
        char buf[8]; snprintf(buf, 8, "%03zu", r);
        fl_draw(buf, x() + 2, ry + 14);
    }

    // Draw row highlights (Playing and Cursor)
    for (size_t r = 0; r < num_rows; ++r) {
        int ry = center_y + ((int)r - center_row) * row_h;
        if (ry < y() + 20) continue;
        if (ry > y() + h()) break;

        // LPB highlight
        uint32_t lpb = m_engine.lpb();
        if (lpb > 0 && r % lpb == 0) {
            fl_color(40, 40, 40); 
            fl_rectf(x(), ry, w(), row_h);
        }

        // Playing row highlight (Full width)
        if (is_playing && (int)r == playing_row) {
            fl_color(40, 80, 40); 
            fl_rectf(x(), ry, w(), row_h);
        }
        
        // Cursor row highlight (Full width, but lighter if not focused)
        if ((int)r == m_cursor_row) {
            if (Fl::focus() == this) fl_color(45, 45, 60);
            else fl_color(35, 35, 45);
            fl_rectf(x() + 40, ry, w() - 40, row_h);
        }
    }

    int cur_x = x() + 40;
    for (size_t t = 0; t < num_tracks; ++t) {
        auto& track_obj = m_engine.track(t);
        bool is_sampler = (track_obj.instrument() && track_obj.instrument()->type() == InstrumentType::Sampler);
        size_t num_cols = pat.column_count(t);
        int track_w = (int)(num_cols * 10 * char_w + 12 * char_w + 10);
        
        TrackUI ui; ui.x = cur_x; ui.w = track_w; ui.btn_plus_x = cur_x + track_w - 20; ui.btn_minus_x = cur_x + track_w - 40;
        m_track_ui.push_back(ui);

        fl_color(60, 60, 60); fl_rectf(cur_x, y(), track_w, 20);
        fl_color(200, 200, 200); fl_draw(track_obj.name().substr(0, 10).c_str(), cur_x + 5, y() + 15);
        fl_color(80, 80, 80); fl_rectf(ui.btn_minus_x, y() + 2, 18, 16); fl_rectf(ui.btn_plus_x, y() + 2, 18, 16);
        fl_color(255, 255, 255); fl_draw("-", ui.btn_minus_x + 5, y() + 14); fl_draw("+", ui.btn_plus_x + 5, y() + 14);
        
        for (size_t r = 0; r < num_rows; ++r) {
            int ry = center_y + ((int)r - center_row) * row_h;
            if (ry < y() + 20) continue;
            if (ry > y() + h()) break;

            // Selection highlight
            if (m_sel_active) {
                int s_t = std::min(m_sel_start_track, m_sel_end_track);
                int e_t = std::max(m_sel_start_track, m_sel_end_track);
                int s_r = std::min(m_sel_start_row, m_sel_end_row);
                int e_r = std::max(m_sel_start_row, m_sel_end_row);
                if ((int)t >= s_t && (int)t <= e_t && (int)r >= s_r && (int)r <= e_r) {
                    fl_color(60, 60, 100);
                    fl_rectf(cur_x, ry, track_w, row_h);
                }
            }

            if ((int)r == m_cursor_row && (int)t == m_cursor_track) {
                fl_color(60, 60, 80); fl_rectf(cur_x, ry, track_w, row_h);
            }

            int col_x = cur_x + 2;
            for (size_t c = 0; c < num_cols; ++c) {
                const auto& ev = pat.event(t, r, c);
                
                // Note
                if ((int)r == m_cursor_row && (int)t == m_cursor_track && (int)c == m_cursor_col && m_cursor_field == 0) {
                    fl_color(100, 100, 150); fl_rectf(col_x - 1, ry, 3 * char_w + 4, row_h);
                }
                fl_color(ev.note == 255 ? 80 : 255, ev.note == 255 ? 80 : 255, ev.note == 255 ? 80 : 255);
                if (ev.note == 255) fl_draw("---", col_x, ry + 14);
                else if (ev.note == 254) fl_draw("===", col_x, ry + 14);
                else {
                    const char* notes[] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
                    char buf[8]; snprintf(buf, 8, "%s%d", notes[ev.note % 12], ev.note / 12);
                    fl_draw(buf, col_x, ry + 14);
                }
                col_x += 4 * char_w;

                // Sample
                if ((int)r == m_cursor_row && (int)t == m_cursor_track && (int)c == m_cursor_col && m_cursor_field == 1) {
                    fl_color(100, 100, 150); fl_rectf(col_x - 1, ry, 2 * char_w + 4, row_h);
                }
                fl_color(0, is_sampler ? 200 : 40, is_sampler ? 200 : 40);
                if (ev.sample_idx == 0) fl_draw("..", col_x, ry + 14);
                else { char buf[4]; snprintf(buf, 4, "%02X", ev.sample_idx); fl_draw(buf, col_x, ry + 14); }
                col_x += 3 * char_w;

                // Volume
                if ((int)r == m_cursor_row && (int)t == m_cursor_track && (int)c == m_cursor_col && m_cursor_field == 2) {
                    fl_color(100, 100, 150); fl_rectf(col_x - 1, ry, 2 * char_w + 4, row_h);
                }
                fl_color(0, 255, 0);
                if (ev.volume == 255) fl_draw("..", col_x, ry + 14);
                else { char buf[4]; snprintf(buf, 4, "%02X", ev.volume); fl_draw(buf, col_x, ry + 14); }
                col_x += 3 * char_w;
            }

            const auto& row_ev = pat.event(t, r, 0); 
            
            // FX1
            if ((int)r == m_cursor_row && (int)t == m_cursor_track && m_cursor_field == 3) {
                fl_color(100, 100, 150); fl_rectf(col_x - 1, ry, 2 * char_w + 4, row_h);
            }
            fl_color(255, 255, 0);
            char fx1_type[4]; snprintf(fx1_type, 4, "%02X", row_ev.effect1); fl_draw(fx1_type, col_x, ry + 14);
            col_x += 3 * char_w;

            // Param1
            if ((int)r == m_cursor_row && (int)t == m_cursor_track && m_cursor_field == 4) {
                fl_color(100, 100, 150); fl_rectf(col_x - 1, ry, 2 * char_w + 4, row_h);
            }
            char fx1_param[4]; snprintf(fx1_param, 4, "%02X", row_ev.param1); fl_draw(fx1_param, col_x, ry + 14);
            col_x += 3 * char_w;
            
            // FX2
            if ((int)r == m_cursor_row && (int)t == m_cursor_track && m_cursor_field == 5) {
                fl_color(100, 100, 150); fl_rectf(col_x - 1, ry, 2 * char_w + 4, row_h);
            }
            char fx2_type[4]; snprintf(fx2_type, 4, "%02X", row_ev.effect2); fl_draw(fx2_type, col_x, ry + 14);
            col_x += 3 * char_w;

            // Param2
            if ((int)r == m_cursor_row && (int)t == m_cursor_track && m_cursor_field == 6) {
                fl_color(100, 100, 150); fl_rectf(col_x - 1, ry, 2 * char_w + 4, row_h);
            }
            char fx2_param[4]; snprintf(fx2_param, 4, "%02X", row_ev.param2); fl_draw(fx2_param, col_x, ry + 14);
        }
        fl_color(50, 50, 50); fl_line(cur_x + track_w, y(), cur_x + track_w, y() + h());
        cur_x += track_w + 10;
    }
    fl_pop_clip();
}

void TrackerView::delete_current_field() {
    clamp_cursor();
    if (m_cursor_track < (int)m_engine.track_count()) {
        auto& ev = m_engine.pattern().event(m_cursor_track, m_cursor_row, m_cursor_col);
        switch (m_cursor_field) {
            case 0: ev.note = 255; break;
            case 1: ev.sample_idx = 0; break;
            case 2: ev.volume = 255; break;
            case 3: ev.effect1 = 0; break;
            case 4: ev.param1 = 0; break;
            case 5: ev.effect2 = 0; break;
            case 6: ev.param2 = 0; break;
        }
        redraw();
    }
}

void TrackerView::ensure_cursor_visible() {
    clamp_cursor();
    Fl_Scroll* scroll = dynamic_cast<Fl_Scroll*>(parent());
    if (!scroll) return;

    // Horizontal scroll
    if (m_cursor_track < (int)m_track_ui.size()) {
        int tx = m_track_ui[m_cursor_track].x - x(); 
        int tw = m_track_ui[m_cursor_track].w;
        int scroll_x = scroll->xposition();
        int scroll_w = scroll->w();

        if (tx < scroll_x) {
            scroll->scroll_to(std::max(0, tx - 20), scroll->yposition());
        } else if (tx + tw > scroll_x + scroll_w) {
            scroll->scroll_to(tx - scroll_w + tw + 20, scroll->yposition());
        }
    }
}

int TrackerView::handle(int event) {
    clamp_cursor();
    switch (event) {
        case FL_FOCUS:
        case FL_UNFOCUS:
            redraw();
            return 1;
        case FL_PUSH: {
            take_focus();
            int mx = Fl::event_x(), my = Fl::event_y();
            if (my >= y() && my < y() + 20) { // Header
                for (size_t t = 0; t < m_track_ui.size(); ++t) {
                    const auto& ui = m_track_ui[t];
                    if (mx >= ui.btn_plus_x && mx < ui.btn_plus_x + 18) { m_engine.pattern().set_column_count(t, m_engine.pattern().column_count(t) + 1); redraw(); return 1; }
                    if (mx >= ui.btn_minus_x && mx < ui.btn_minus_x + 18) { size_t current = m_engine.pattern().column_count(t); if (current > 1) { m_engine.pattern().set_column_count(t, current - 1); redraw(); } return 1; }
                }
            } else { // Grid
                int row_h = 18;
                int center_row = (m_engine.transport_state() != TransportState::Stopped) ? (int)m_engine.current_row() : m_cursor_row;
                int center_y = y() + 20 + (h() - 20) / 2;
                center_y = ((center_y - (y() + 20)) / row_h) * row_h + (y() + 20);
                
                m_cursor_row = center_row + (my - center_y) / row_h;
                m_cursor_row = std::max(0, std::min((int)m_engine.pattern().row_count() - 1, m_cursor_row));

                int tx = x() + 40;
                for (size_t t = 0; t < m_track_ui.size(); ++t) {
                    if (mx >= m_track_ui[t].x && mx < m_track_ui[t].x + m_track_ui[t].w) { m_cursor_track = (int)t; break; }
                }
                if (Fl::event_state(FL_SHIFT)) {
                    if (!m_sel_active) { m_sel_active = true; m_sel_start_row = m_cursor_row; m_sel_start_track = m_cursor_track; }
                    m_sel_end_row = m_cursor_row; m_sel_end_track = m_cursor_track;
                } else { m_sel_active = false; }
                ensure_cursor_visible();
                redraw();
            }
            return 1;
        }
        case FL_KEYUP: {
             Action action = m_engine.m_key_bindings.get_action(Fl::event_key(), Fl::event_state() & (FL_CTRL | FL_SHIFT | FL_ALT | FL_META));
             auto is_note_action = [](Action a) -> bool {
                return ((int)a >= (int)Action::NoteC && (int)a <= (int)Action::NoteB) ||
                       ((int)a >= (int)Action::NoteC2 && (int)a <= (int)Action::NoteB2) ||
                       (a == Action::NoteC3) || (a == Action::NoteOff);
             };
             if (is_note_action(action)) {
                 m_engine.stop_preview(m_cursor_track, m_cursor_col);
                 return 1;
             }
             return 0;
        }
        case FL_DRAG: {
            int mx = Fl::event_x(), my = Fl::event_y();
            int row_h = 18;
            int center_row = (m_engine.transport_state() != TransportState::Stopped) ? (int)m_engine.current_row() : m_cursor_row;
            int center_y = y() + 20 + (h() - 20) / 2;
            center_y = ((center_y - (y() + 20)) / row_h) * row_h + (y() + 20);

            m_cursor_row = center_row + (my - center_y) / row_h;
            m_cursor_row = std::max(0, std::min((int)m_engine.pattern().row_count() - 1, m_cursor_row));

            for (size_t t = 0; t < m_track_ui.size(); ++t) {
                if (mx >= m_track_ui[t].x && mx < m_track_ui[t].x + m_track_ui[t].w) { m_cursor_track = (int)t; break; }
            }
            if (!m_sel_active) { m_sel_active = true; m_sel_start_row = m_cursor_row; m_sel_start_track = m_cursor_track; }
            m_sel_end_row = m_cursor_row; m_sel_end_track = m_cursor_track;
            ensure_cursor_visible();
            redraw();
            return 1;
        }
        case FL_KEYDOWN: {
            int key = Fl::event_key();
            bool shift = Fl::event_state(FL_SHIFT);
            
            Action action = m_engine.m_key_bindings.get_action(key, Fl::event_state() & (FL_CTRL | FL_SHIFT | FL_ALT | FL_META));
            
            auto action_to_note = [](Action a) -> int {
                switch(a) {
                    case Action::NoteC: return 0;  case Action::NoteCs: return 1;
                    case Action::NoteD: return 2;  case Action::NoteDs: return 3;
                    case Action::NoteE: return 4;  case Action::NoteF: return 5;
                    case Action::NoteFs: return 6; case Action::NoteG: return 7;
                    case Action::NoteGs: return 8; case Action::NoteA: return 9;
                    case Action::NoteAs: return 10; case Action::NoteB: return 11;
                    case Action::NoteC2: return 12; case Action::NoteCs2: return 13;
                    case Action::NoteD2: return 14; case Action::NoteDs2: return 15;
                    case Action::NoteE2: return 16; case Action::NoteF2: return 17;
                    case Action::NoteFs2: return 18; case Action::NoteG2: return 19;
                    case Action::NoteGs2: return 20; case Action::NoteA2: return 21;
                    case Action::NoteAs2: return 22; case Action::NoteB2: return 23;
                    case Action::NoteC3: return 24;
                    case Action::NoteOff: return 254;
                    default: return -1;
                }
            };
            
            auto is_note_action = [](Action a) -> bool {
                return ((int)a >= (int)Action::NoteC && (int)a <= (int)Action::NoteB) ||
                       ((int)a >= (int)Action::NoteC2 && (int)a <= (int)Action::NoteB2) ||
                       (a == Action::NoteC3) || (a == Action::NoteOff);
            };

            if (is_note_action(action)) {
                int note = action_to_note(action);
                if (note != -1) {
                    insert_note(note);
                }
                return 1;
            }

            if (action == Action::OctaveUp) {
                m_engine.set_base_octave(m_engine.base_octave() + 1);
                redraw();
                return 1;
            }
            if (action == Action::OctaveDown) {
                m_engine.set_base_octave(m_engine.base_octave() - 1);
                redraw();
                return 1;
            }
            if (action == Action::NextPattern || action == Action::PrevPattern) {
                auto order = m_engine.order_list();
                size_t pos = m_engine.m_edit_order_pos.load();
                if (pos < order.size()) {
                    if (action == Action::NextPattern) {
                        if (order[pos] < m_engine.pattern_count() - 1) order[pos]++;
                    } else {
                        if (order[pos] > 0) order[pos]--;
                    }
                    m_engine.set_order(order);
                    m_engine.set_active_pattern(order[pos]);
                    redraw();
                    
                    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
                        MainWindow* mw = dynamic_cast<MainWindow*>(win);
                        if (mw) mw->request_update();
                    }
                }
                return 1;
            }

            if (action == Action::NextOrderPos || action == Action::PrevOrderPos) {
                auto order = m_engine.order_list();
                size_t pos = m_engine.m_edit_order_pos.load();
                if (action == Action::NextOrderPos) {
                    if (pos < order.size() - 1) pos++;
                } else {
                    if (pos > 0) pos--;
                }
                m_engine.m_edit_order_pos.store(pos);
                if (pos < order.size()) {
                    m_engine.set_active_pattern(order[pos]);
                }
                redraw();
                for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
                    MainWindow* mw = dynamic_cast<MainWindow*>(win);
                    if (mw) mw->request_update();
                }
                return 1;
            }

            if (shift && !m_sel_active) { m_sel_active = true; m_sel_start_row = m_cursor_row; m_sel_start_track = m_cursor_track; }
            
            int num_note_cols = (int)m_engine.pattern().column_count(m_cursor_track);
            int total_fields_in_track = num_note_cols * 3 + 4;
            int abs_field = m_cursor_col * 3 + m_cursor_field;

            switch (key) {
                case FL_Up: 
                    m_cursor_row -= m_engine.step_size();
                    if (m_cursor_row < 0) m_cursor_row = (int)m_engine.pattern().row_count() - 1;
                    break;
                case FL_Down: 
                    m_cursor_row += m_engine.step_size();
                    if (m_cursor_row >= (int)m_engine.pattern().row_count()) m_cursor_row = 0;
                    break;
                case FL_Left: 
                    abs_field--;
                    if (abs_field < 0) {
                        if (m_cursor_track > 0) {
                            m_cursor_track--;
                            abs_field = (int)m_engine.pattern().column_count(m_cursor_track) * 3 + 4 - 1;
                        } else abs_field = 0;
                    }
                    if (abs_field < (int)m_engine.pattern().column_count(m_cursor_track) * 3) {
                        m_cursor_col = abs_field / 3;
                        m_cursor_field = abs_field % 3;
                    } else {
                        m_cursor_col = 0;
                        m_cursor_field = 3 + (abs_field - (int)m_engine.pattern().column_count(m_cursor_track) * 3);
                    }
                    break;
                case FL_Right: 
                    abs_field++;
                    if (abs_field >= total_fields_in_track) {
                        if (m_cursor_track < (int)m_engine.track_count() - 1) {
                            m_cursor_track++;
                            abs_field = 0;
                        } else abs_field = total_fields_in_track - 1;
                    }
                    if (abs_field < (int)m_engine.pattern().column_count(m_cursor_track) * 3) {
                        m_cursor_col = abs_field / 3;
                        m_cursor_field = abs_field % 3;
                    } else {
                        m_cursor_col = 0;
                        m_cursor_field = 3 + (abs_field - (int)m_engine.pattern().column_count(m_cursor_track) * 3);
                    }
                    break;
                case FL_Page_Up: m_cursor_row = std::max(0, m_cursor_row - 16); break;
                case FL_Page_Down: m_cursor_row = std::min((int)m_engine.pattern().row_count() - 1, m_cursor_row + 16); break;
                case FL_Home: m_cursor_row = 0; break;
                case FL_End: m_cursor_row = (int)m_engine.pattern().row_count() - 1; break;
                case FL_Escape: m_sel_active = false; break;
                case FL_BackSpace:
                    delete_current_field();
                    m_cursor_row -= m_engine.step_size();
                    if (m_cursor_row < 0) m_cursor_row = (int)m_engine.pattern().row_count() - 1;
                    break;
                case FL_Delete:
                    delete_current_field();
                    break;
            }
            if (shift) { m_sel_end_row = m_cursor_row; m_sel_end_track = m_cursor_track; }
            else if (key != FL_Shift_L && key != FL_Shift_R) { m_sel_active = false; }
            ensure_cursor_visible();
            redraw();
            return 1;
        }
    }
    return Fl_Widget::handle(event);
}

void TrackerView::set_current_row(int row) { m_cursor_row = row; clamp_cursor(); redraw(); }
void TrackerView::insert_note(uint8_t note) {
    clamp_cursor();
    if (m_cursor_track < (int)m_engine.track_count()) {
        if (note != 254) {
            int final_note = note + m_engine.base_octave() * 12;
            if (final_note > 119) final_note = 119;
            m_engine.pattern().event(m_cursor_track, m_cursor_row, m_cursor_col).note = (uint8_t)final_note;
        } else {
            m_engine.pattern().event(m_cursor_track, m_cursor_row, m_cursor_col).note = 254;
        }
        m_cursor_row = std::min((int)m_engine.pattern().row_count()-1, m_cursor_row + (int)m_engine.step_size());
        redraw();
    }
}

void TrackerView::recalculate_size() {
    int char_w = 8;
    int row_h = 18;
    size_t num_tracks = m_engine.track_count();
    size_t num_rows = m_engine.pattern().row_count();
    
    int total_w = 40; 
    for (size_t t = 0; t < num_tracks; ++t) {
        size_t num_cols = m_engine.pattern().column_count(t);
        total_w += (int)(num_cols * 10 * char_w + 12 * char_w + 20);
    }
    
    int total_h = 20 + (int)num_rows * row_h + 20;
    
    if (parent()) {
        if (parent()->w() > total_w) total_w = parent()->w();
        if (parent()->h() > total_h) total_h = parent()->h();
    }
    
    if (total_w != w() || total_h != h()) {
        this->size(total_w, total_h);
    }
}

} // namespace disgrace_ns
