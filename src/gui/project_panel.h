#pragma once

#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_File_Browser.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Choice.H>

namespace disgrace_ns {

class Engine;

class ProjectPanel : public Fl_Group {
public:
    ProjectPanel(int x, int y, int w, int h, Engine& engine);

    void update_track_list();

private:
    Engine& m_engine;
    
    Fl_Group* m_left_panel;
    Fl_Group* m_right_panel;

    Fl_Button* m_new_btn;
    Fl_Button* m_load_btn;
    Fl_Button* m_save_btn;
    Fl_Button* m_export_btn;
    Fl_File_Browser* m_file_browser;

    Fl_Scroll* m_track_scroll;
    Fl_Group*  m_track_container;
    Fl_Button* m_add_track_btn;
    Fl_Button* m_add_bus_btn;

    static void cb_new(Fl_Widget*, void*);
    static void cb_load(Fl_Widget*, void*);
    static void cb_save(Fl_Widget*, void*);
    static void cb_export(Fl_Widget*, void*);
    static void cb_file_select(Fl_Widget*, void*);
    static void cb_add_track(Fl_Widget*, void*);
    static void cb_remove_track(Fl_Widget*, void*);
    static void cb_track_name(Fl_Widget*, void*);
    static void cb_track_inst(Fl_Widget*, void*);
    static void cb_track_output(Fl_Widget*, void*);
    static void cb_move_track_up(Fl_Widget*, void*);
    static void cb_move_track_down(Fl_Widget*, void*);

    static void cb_add_bus(Fl_Widget*, void*);
    static void cb_remove_bus(Fl_Widget*, void*);
    static void cb_bus_name(Fl_Widget*, void*);
    static void cb_bus_output(Fl_Widget*, void*);
};

} // namespace disgrace_ns
