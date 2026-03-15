#pragma once

#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Scroll.H>
#include <unordered_map>
#include <utility>

namespace disgrace_ns {

class Engine;
class TrackerView;
class DetachedWindow;

class TrackerPanel : public Fl_Group {
public:
    TrackerPanel(int x, int y, int w, int h, Engine& engine);

    void update_pattern_list_browser();
    void update();
    void grab_focus();
    TrackerView* tracker_view() const { return m_tracker; }
    void resize(int x, int y, int w, int h) override;

private:
    Engine& m_engine;
    TrackerView* m_tracker;
    Fl_Scroll*   m_main_scroll;

    Fl_Button* m_add_pattern_btn;
    Fl_Button* m_remove_pattern_btn;
    Fl_Button* m_copy_pattern_btn;
    Fl_Button* m_dec_pattern_btn;
    Fl_Button* m_inc_pattern_btn;
    Fl_Scroll* m_pattern_scroll;
    Fl_Group* m_pattern_list_container;
    Fl_Button* m_detach_btn;
    DetachedWindow* m_detached_window = nullptr;
    int m_selected_order_idx = 0;
    size_t m_last_order_size = 0;
    size_t m_last_pattern_count = 0;
    bool m_follow_playback = false;
    std::unordered_map<long, Fl_Widget*> m_pattern_length_inputs;
    std::unordered_map<long, Fl_Widget*> m_order_buttons;

    static void cb_add_pattern(Fl_Widget*, void*);
    static void cb_remove_pattern(Fl_Widget*, void*);
    static void cb_copy_pattern(Fl_Widget*, void*);
    static void cb_inc_pattern(Fl_Widget*, void*);
    static void cb_dec_pattern(Fl_Widget*, void*);
    static void cb_pattern_length(Fl_Widget*, void*);
    static void cb_detach(Fl_Widget*, void*);
    static void cb_follow_playback(Fl_Widget*, void*);
};

} // namespace disgrace_ns
