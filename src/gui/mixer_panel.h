#pragma once

#include <FL/Fl_Group.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Tile.H>
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
    void update_effect_editor();

private:
    void clear_callback_data();
    template<typename T>
    T* reg_cb(T* data) {
        m_callback_data.push_back(static_cast<void*>(data));
        return data;
    }

    Engine& m_engine;
    int m_selected_track = -1;
    int m_selected_fx_slot = -1;
    std::string m_current_preset_name = "";

    std::vector<void*> m_callback_data;

    Fl_Tile* m_tile;
    Fl_Group* m_upper_pane;
    Fl_Group* m_lower_pane;

    Fl_Group* m_track_group;
    Fl_Value_Slider* m_master_gain;
    Fl_Button* m_master_sel_btn;
    VUMeter* m_master_meter_l;
    VUMeter* m_master_meter_r;
    SpectralView* m_spectral_view;
    std::vector<std::pair<VUMeter*, VUMeter*>> m_track_meters;
    std::vector<std::pair<VUMeter*, VUMeter*>> m_bus_meters;
    Fl_Button* m_detach_btn;
    DetachedWindow* m_detached_window = nullptr;

    // Effect Editor components
    Fl_Browser* m_avail_fx_browser;
    Fl_Group*   m_fx_chain_group;
    Fl_Group*   m_fx_params_group;
    Fl_Button*  m_load_chain_btn;
    Fl_Button*  m_save_chain_btn;

    static void cb_master_gain(Fl_Widget*, void*);
    static void cb_track_volume(Fl_Widget*, void*);
    static void cb_track_pan(Fl_Widget*, void*);
    static void cb_track_mute(Fl_Widget*, void*);
    static void cb_track_solo(Fl_Widget*, void*);
    static void cb_track_select(Fl_Widget*, void*);
    static void cb_track_input(Fl_Widget*, void*);
    static void cb_track_delay(Fl_Widget*, void*);
    
    static void cb_bus_volume(Fl_Widget*, void*);
    static void cb_bus_pan(Fl_Widget*, void*);
    static void cb_bus_mute(Fl_Widget*, void*);

    static void cb_detach(Fl_Widget*, void*);

    // Effect Callbacks
    static void cb_add_fx(Fl_Widget*, void*);
    static void cb_fx_up(Fl_Widget*, void*);
    static void cb_fx_down(Fl_Widget*, void*);
    static void cb_fx_remove(Fl_Widget*, void*);
    static void cb_fx_bypass(Fl_Widget*, void*);
    static void cb_fx_preset_select(Fl_Widget*, void*);
    static void cb_fx_preset_save(Fl_Widget*, void*);
    static void cb_fx_preset_load(Fl_Widget*, void*);
    static void cb_save_chain(Fl_Widget*, void*);
    static void cb_load_chain(Fl_Widget*, void*);
};

} // namespace disgrace_ns
