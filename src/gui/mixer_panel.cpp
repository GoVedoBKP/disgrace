#include "mixer_panel.h"
#include "detached_window.h"
#include "../core/engine.h"
#include <FL/Fl_Scroll.H>

namespace disgrace_ns {

MixerPanel::MixerPanel(int x, int y, int w, int h, Engine& engine)
    : Fl_Group(x, y, w, h), m_engine(engine) {
    
    begin();

    m_detach_btn = new Fl_Button(w - 30, 0, 30, 20, "[]");
    m_detach_btn->callback(cb_detach, this);

    // Use a scroll area for tracks
    Fl_Scroll* scroll = new Fl_Scroll(0, 30, w, h - 70);
    scroll->type(Fl_Scroll::HORIZONTAL);

    // This group will hold the tracks
    m_track_group = new Fl_Group(0, 30, w, h - 70);
    m_track_group->end();
    scroll->add(m_track_group);
    scroll->end();

    m_master_gain = new Fl_Value_Slider(w - 150 - 50, h - 30, 150, 25, "Master");
    m_master_gain->type(FL_HOR_NICE_SLIDER);
    m_master_gain->range(0.0, 2.0);
    m_master_gain->value(1.0);
    m_master_gain->callback(cb_master_gain, this);

    m_master_meter_l = new VUMeter(w - 45, h - 30, 15, 25);
    m_master_meter_r = new VUMeter(w - 25, h - 30, 15, 25);
    m_spectral_view = new SpectralView(w - 200, h - 140, 190, 100, m_engine);

    Fl_Box* mixer_spacer = new Fl_Box(0, 0, w, h);
    mixer_spacer->hide();
    resizable(mixer_spacer);

    end();

    update_mixer_ui();
}

void MixerPanel::update_mixer_ui() {
    for (int i = 0; i < m_track_group->children(); ++i) {
        void* d = m_track_group->child(i)->user_data();
        if (d) delete static_cast<std::pair<MixerPanel*, int>*>(d);
    }
    m_track_group->clear();
    m_track_group->begin();
    m_track_meters.clear();
    m_bus_meters.clear();

    size_t num_tracks = m_engine.track_count();
    size_t num_buses = m_engine.bus_count();
    
    int x_offset = 0;

    for (size_t i = 0; i < num_tracks; ++i)
    {
      new Fl_Box(20 + x_offset, 0, 80, 20, strdup(("Track " + ::std::to_string(i+1)).c_str()));
      
      Fl_Slider* vol = new Fl_Slider(20 + x_offset, 25, 20, 150);
      vol->type(FL_VERTICAL);
      vol->bounds(1, 0); 
      vol->value(m_engine.track(i).volume());
      vol->callback(cb_track_volume, new ::std::pair<MixerPanel*,int>(this, (int)i));

      VUMeter* meter_l = new VUMeter(45 + x_offset, 25, 8, 150);
      VUMeter* meter_r = new VUMeter(55 + x_offset, 25, 8, 150);
      m_track_meters.push_back({meter_l, meter_r});

      Fl_Slider* pan_slider = new Fl_Slider(20 + x_offset, 180, 75, 15);
      pan_slider->type(FL_HOR_SLIDER);
      pan_slider->range(-1.0, 1.0);
      pan_slider->value(m_engine.track(i).get_pan());
      pan_slider->callback(cb_track_pan, new ::std::pair<MixerPanel*,int>(this, (int)i));

      Fl_Check_Button* mute = new Fl_Check_Button(20 + x_offset, 200, 35, 20, "M");
      mute->value(m_engine.track(i).muted());
      mute->callback(cb_track_mute, new ::std::pair<MixerPanel*,int>(this, (int)i));

      Fl_Check_Button* solo = new Fl_Check_Button(60 + x_offset, 200, 35, 20, "S");
      solo->value(m_engine.track(i).solo());
      solo->callback(cb_track_solo, new ::std::pair<MixerPanel*,int>(this, (int)i));

      x_offset += 100;
    }

    if (num_buses > 0) {
      Fl_Box* separator = new Fl_Box(20 + x_offset - 5, 0, 5, 220);
      separator->box(FL_THIN_DOWN_BOX);
      x_offset += 10;

      for (size_t i = 0; i < num_buses; ++i) {
        new Fl_Box(20 + x_offset, 0, 80, 20, strdup(("Bus " + ::std::to_string(i+1)).c_str()));
        
        Fl_Slider* vol = new Fl_Slider(20 + x_offset, 25, 20, 150);
        vol->type(FL_VERTICAL);
        vol->bounds(1, 0); 
        vol->value(m_engine.bus(i).volume());
        vol->callback(cb_bus_volume, new ::std::pair<MixerPanel*,int>(this, (int)i));

        VUMeter* meter_l = new VUMeter(45 + x_offset, 25, 8, 150);
        VUMeter* meter_r = new VUMeter(55 + x_offset, 25, 8, 150);
        m_bus_meters.push_back({meter_l, meter_r});

        Fl_Slider* pan_slider = new Fl_Slider(20 + x_offset, 180, 75, 15);
        pan_slider->type(FL_HOR_SLIDER);
        pan_slider->range(-1.0, 1.0);
        pan_slider->value(m_engine.bus(i).pan());
        pan_slider->callback(cb_bus_pan, new ::std::pair<MixerPanel*,int>(this, (int)i));

        Fl_Check_Button* mute = new Fl_Check_Button(20 + x_offset, 200, 35, 20, "M");
        mute->value(m_engine.bus(i).muted());
        mute->callback(cb_bus_mute, new ::std::pair<MixerPanel*,int>(this, (int)i));
        
        x_offset += 100;
      }
    }

    m_track_group->end();
    m_track_group->size((int)(x_offset + 40), m_track_group->h());
    m_track_group->redraw();
}

void MixerPanel::update_meters() {
    if (m_master_meter_l) m_master_meter_l->level(m_engine.master_meter_l());
    if (m_master_meter_r) m_master_meter_r->level(m_engine.master_meter_r());

    if (m_spectral_view) {
        m_spectral_view->update();
    }

    for (size_t i = 0; i < m_track_meters.size() && i < m_engine.track_count(); ++i) {
        m_track_meters[i].first->level(m_engine.track(i).meter_level_l());
        m_track_meters[i].second->level(m_engine.track(i).meter_level_r());
    }

    for (size_t i = 0; i < m_bus_meters.size() && i < m_engine.bus_count(); ++i) {
        m_bus_meters[i].first->level(m_engine.bus(i).meter_l());
        m_bus_meters[i].second->level(m_engine.bus(i).meter_r());
    }
}

void MixerPanel::cb_master_gain(Fl_Widget* w, void* data) {
    MixerPanel* self = static_cast<MixerPanel*>(data);
    float g = static_cast<Fl_Value_Slider*>(w)->value();
    self->m_engine.set_master_gain(g);
}

void MixerPanel::cb_track_volume(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    MixerPanel* self = pair->first;
    float v = static_cast<Fl_Slider*>(w)->value();
    self->m_engine.track(pair->second).set_volume(v);
}

void MixerPanel::cb_track_pan(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    MixerPanel* self = pair->first;
    float p = static_cast<Fl_Slider*>(w)->value();
    self->m_engine.track(pair->second).set_pan(p);
}

void MixerPanel::cb_track_mute(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    MixerPanel* self = pair->first;
    bool m = static_cast<Fl_Check_Button*>(w)->value();
    self->m_engine.track(pair->second).set_mute(m);
}

void MixerPanel::cb_track_solo(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    MixerPanel* self = pair->first;
    bool s = static_cast<Fl_Check_Button*>(w)->value();
    self->m_engine.track(pair->second).set_solo(s);
}

void MixerPanel::cb_bus_volume(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    MixerPanel* self = pair->first;
    float v = static_cast<Fl_Slider*>(w)->value();
    self->m_engine.bus(pair->second).set_volume(v);
}

void MixerPanel::cb_bus_pan(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    MixerPanel* self = pair->first;
    float p = static_cast<Fl_Slider*>(w)->value();
    self->m_engine.bus(pair->second).set_pan(p);
}

void MixerPanel::cb_bus_mute(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    MixerPanel* self = pair->first;
    bool m = static_cast<Fl_Check_Button*>(w)->value();
    self->m_engine.bus(pair->second).set_mute(m);
}

void MixerPanel::cb_detach(Fl_Widget*, void* data) {
    MixerPanel* self = static_cast<MixerPanel*>(data);
    Fl_Group* parent_grp = self->parent();
    if (self->m_detached_window) {
        self->m_detached_window->show();
    } else {
        self->m_detached_window = new DetachedWindow(850, 300, "Mixer", self, parent_grp);
        self->m_detached_window->show();
    }
    self->hide();
}

} // namespace disgrace_ns
