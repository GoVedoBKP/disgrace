#pragma once

#include <FL/Fl_Group.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include "vu_meter.h"
#include "spectral_view.h"
#include <vector>

namespace disgrace_ns {

class Engine;
class DetachedWindow;

class MixerPanel : public Fl_Group {
public:
    MixerPanel(int x, int y, int w, int h, Engine& engine);

    void update_mixer_ui();
    void update_meters();

private:
    Engine& m_engine;
    Fl_Group* m_track_group;
    Fl_Value_Slider* m_master_gain;
    VUMeter* m_master_meter_l;
    VUMeter* m_master_meter_r;
    SpectralView* m_spectral_view;
    std::vector<std::pair<VUMeter*, VUMeter*>> m_track_meters;
    std::vector<std::pair<VUMeter*, VUMeter*>> m_bus_meters;
    Fl_Button* m_detach_btn;
    DetachedWindow* m_detached_window = nullptr;

    static void cb_master_gain(Fl_Widget*, void*);
    static void cb_track_volume(Fl_Widget*, void*);
    static void cb_track_pan(Fl_Widget*, void*);
    static void cb_track_mute(Fl_Widget*, void*);
    static void cb_track_solo(Fl_Widget*, void*);
    
    static void cb_bus_volume(Fl_Widget*, void*);
    static void cb_bus_pan(Fl_Widget*, void*);
    static void cb_bus_mute(Fl_Widget*, void*);

    static void cb_detach(Fl_Widget*, void*);
};

} // namespace disgrace_ns
