#pragma once

#include <FL/Fl_Widget.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Button.H>
#include <vector>

namespace disgrace_ns {

class Engine;

class NotationView : public Fl_Group {
public:
    NotationView(int x, int y, int w, int h, Engine& engine);
    void draw() override;
    int handle(int event) override;

    void zoom_in();
    void zoom_out();
    void view_all();
    void view_selection();

    void update_view();

private:
    static void cb_preview_track(Fl_Widget*, void*);
    Engine& m_engine;
    double m_zoom = 20.0; // pixels per row
    
    int m_sel_start_tick = -1;
    int m_sel_end_tick = -1;
    bool m_is_selecting = false;

    void draw_staff(int tx, int ty, int tw, int type); // 0: Violin, 1: Bass, 2: Both, 3: Drums
    void draw_note(int x, int y, int note, int staff_type);
    
    int get_total_ticks();
    int tick_to_x(int tick);
    int x_to_tick(int x);
};

class NotationPanel : public Fl_Group {
public:
    NotationPanel(int x, int y, int w, int h, Engine& engine);
    void update();

private:
    Engine& m_engine;
    Fl_Scroll* m_scroll;
    NotationView* m_notation_view;

    Fl_Button* m_zoom_in_btn;
    Fl_Button* m_zoom_out_btn;
    Fl_Button* m_view_all_btn;
    Fl_Button* m_view_sel_btn;

    static void cb_zoom_in(Fl_Widget*, void*);
    static void cb_zoom_out(Fl_Widget*, void*);
    static void cb_view_all(Fl_Widget*, void*);
    static void cb_view_sel(Fl_Widget*, void*);
};

} // namespace disgrace_ns
