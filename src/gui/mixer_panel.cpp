#include "mixer_panel.h"
#include "detached_window.h"
#include "../core/engine.h"
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Value_Input.H>
#include <FL/filename.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Native_File_Chooser.H>
#include "../dsp/gain.h"
#include "../dsp/delay.h"
#include "../dsp/reverb.h"
#include "../dsp/limiter.h"
#include "../dsp/exciter.h"
#include "../dsp/phaser.h"
#include "../dsp/flanger.h"
#include "../dsp/echo.h"
#include "../dsp/compressor.h"
#include "../dsp/graphical_eq.h"
#include "../dsp/cabinet.h"
#include "../dsp/distortion.h"
#include "../dsp/chorus.h"
#include "../dsp/stereo_expander.h"
#include "../dsp/ring_modulator.h"
#include "../dsp/gate.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace disgrace_ns {

MixerPanel::MixerPanel(int x, int y, int w, int h, Engine& engine)
    : Fl_Group(x, y, w, h), m_engine(engine) {
    
    m_selected_track = -1; // Default to Master
    m_selected_fx_slot = -1;
    
    begin();

    m_tile = new Fl_Tile(x, y, w, h);
    m_tile->begin();

    // Upper Pane: Tracks
    m_upper_pane = new Fl_Group(x, y, w, h / 2);
    m_upper_pane->box(FL_ENGRAVED_FRAME);
    m_upper_pane->begin();

    m_detach_btn = new Fl_Button(x + w - 30, y, 30, 20, "[]");
    m_detach_btn->callback(cb_detach, this);

    // Use a scroll area for tracks
    Fl_Scroll* scroll = new Fl_Scroll(x, y + 25, w, h / 2 - 25);
    scroll->type(Fl_Scroll::HORIZONTAL);

    // This group will hold the tracks
    m_track_group = new Fl_Group(x, y + 25, w, h / 2 - 25);
    m_track_group->end();
    scroll->add(m_track_group);
    scroll->end();
    m_upper_pane->end();

    // Lower Pane: Effects
    m_lower_pane = new Fl_Group(x, y + h / 2, w, h / 2);
    m_lower_pane->box(FL_ENGRAVED_FRAME);
    m_lower_pane->begin();
    
    int margin = 5;
    int cur_x = x + margin;
    int cur_y = y + h / 2 + margin;
    int std_h = m_engine.m_gui_button_height;

    // Available Effects Browser
    new Fl_Box(cur_x, cur_y, 120, 20, "Available Effects");
    m_avail_fx_browser = new Fl_Browser(cur_x, cur_y + 20, 120, h / 2 - 60);
    m_avail_fx_browser->type(FL_HOLD_BROWSER);
    
    std::vector<std::string> fx_list = {
        "Gain", "Delay", "Reverb", "Limiter", "Exciter", 
        "Phaser", "Flanger", "Echo", "Compressor", 
        "Graphical EQ", "Cabinet", "Distortion",
        "Chorus", "Stereo Expander", "Ring Modulator", "Gate"
    };
    std::sort(fx_list.begin(), fx_list.end());
    for (const auto& fx : fx_list) m_avail_fx_browser->add(fx.c_str());
    
    Fl_Button* add_fx_btn = new Fl_Button(cur_x, y + h - 35, 120, std_h, "Add Effect");
    add_fx_btn->callback(cb_add_fx, this);

    cur_x += 120 + margin;

    // Selected Track Effect Chain
    new Fl_Box(cur_x, cur_y, 240, 20, "Effect Chain");
    m_fx_chain_group = new Fl_Group(cur_x, cur_y + 20, 240, h / 2 - 60);
    m_fx_chain_group->box(FL_DOWN_FRAME);
    m_fx_chain_group->end();

    m_load_chain_btn = new Fl_Button(cur_x, y + h - 35, 115, std_h, "Load Chain");
    m_load_chain_btn->callback(cb_load_chain, this);
    m_save_chain_btn = new Fl_Button(cur_x + 125, y + h - 35, 115, std_h, "Save Chain");
    m_save_chain_btn->callback(cb_save_chain, this);

    cur_x += 240 + margin;

    // Effect Parameters
    new Fl_Box(cur_x, cur_y, w - (cur_x - x) - margin, 20, "Effect Controls");
    m_fx_params_group = new Fl_Group(cur_x, cur_y + 20, w - (cur_x - x) - margin, h / 2 - 30);
    m_fx_params_group->box(FL_ENGRAVED_FRAME);
    m_fx_params_group->end();

    m_lower_pane->end();

    m_tile->end();

    // Master section
    m_upper_pane->begin();
    m_master_gain = new Fl_Value_Slider(x + w - 250, y + h/2 - 35, 150, 25, "Master");
    m_master_gain->type(FL_HOR_NICE_SLIDER);
    m_master_gain->range(0.0, 2.0);
    m_master_gain->value(1.0);
    m_master_gain->callback(cb_master_gain, this);

    m_master_meter_l = new VUMeter(x + w - 45, y + h/2 - 35, 15, 25);
    m_master_meter_r = new VUMeter(x + w - 25, y + h/2 - 35, 15, 25);

    m_master_sel_btn = new Fl_Button(x + w - 330, y + h/2 - 35, 75, 25, "SEL");
    // We will set callback in update_mixer_ui
    
    m_upper_pane->end();

    m_lower_pane->begin();
    m_spectral_view = new SpectralView(x + w - 200, y + h - 140, 190, 100, m_engine);
    m_lower_pane->end();

    end();

    update_mixer_ui();
    update_effect_editor();
}

void MixerPanel::clear_callback_data() {
    for (void* p : m_callback_data) {
        delete static_cast<std::pair<MixerPanel*, int>*>(p);
    }
    m_callback_data.clear();
}

void MixerPanel::update_mixer_ui() {
    clear_callback_data();

    // Reset Master button state
    m_master_sel_btn->callback(cb_track_select, reg_cb(new ::std::pair<MixerPanel*,int>(this, -1)));
    m_master_sel_btn->color(m_selected_track == -1 ? FL_YELLOW : FL_BACKGROUND_COLOR);

    m_track_group->clear();
    m_track_group->begin();
    m_track_meters.clear();
    m_bus_meters.clear();

    size_t num_tracks = m_engine.track_count();
    size_t num_buses = m_engine.bus_count();
    
    int x_offset = 0;
    int std_h = m_engine.m_gui_button_height;
    int font_sz = m_engine.m_gui_font_size;

    for (size_t i = 0; i < num_tracks; ++i)
    {
      int cur_y = 5;
      Fl_Box* b = new Fl_Box(20 + x_offset, cur_y, 80, std_h, strdup(("Track " + ::std::to_string(i+1)).c_str()));
      b->labelsize(font_sz);
      cur_y += std_h + 5;
      
      Fl_Slider* vol = new Fl_Slider(20 + x_offset, cur_y, 20, 150);
      vol->type(FL_VERTICAL);
      vol->bounds(1, 0); 
      vol->value(m_engine.track(i).volume());
      vol->callback(cb_track_volume, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));

      VUMeter* meter_l = new VUMeter(45 + x_offset, cur_y, 8, 150);
      VUMeter* meter_r = new VUMeter(55 + x_offset, cur_y, 8, 150);
      m_track_meters.push_back({meter_l, meter_r});
      cur_y += 150 + 5;

      Fl_Slider* pan_slider = new Fl_Slider(20 + x_offset, cur_y, 75, 15);
      pan_slider->type(FL_HOR_SLIDER);
      pan_slider->range(-1.0, 1.0);
      pan_slider->value(m_engine.track(i).get_pan());
      pan_slider->callback(cb_track_pan, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));
      cur_y += 15 + 5;

      Fl_Check_Button* mute = new Fl_Check_Button(20 + x_offset, cur_y, 35, std_h, "M");
      mute->labelsize(font_sz);
      mute->value(m_engine.track(i).muted());
      mute->callback(cb_track_mute, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));

      Fl_Check_Button* solo = new Fl_Check_Button(60 + x_offset, cur_y, 35, std_h, "S");
      solo->labelsize(font_sz);
      solo->value(m_engine.track(i).solo());
      solo->callback(cb_track_solo, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));
      cur_y += std_h + 5;

      Fl_Button* sel = new Fl_Button(20 + x_offset, cur_y, 75, std_h, "SEL");
      sel->labelsize(font_sz);
      sel->color((int)i == m_selected_track ? FL_YELLOW : FL_BACKGROUND_COLOR);
      sel->callback(cb_track_select, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));
      cur_y += std_h + 5;

      // Audio Input for MIDI instruments
      if (m_engine.track(i).instrument() && m_engine.track(i).instrument()->type() == InstrumentType::Midi) {
        Fl_Choice* input_choice = new Fl_Choice(20 + x_offset, cur_y, 75, std_h);
        input_choice->labelsize(font_sz - 2);
        input_choice->add("None");
        uint32_t num_ins = m_engine.m_num_ins;
        for (uint32_t j = 0; j < num_ins; ++j) {
            char name[32];
            snprintf(name, sizeof(name), "In %u", j + 1);
            input_choice->add(strdup(name));
        }
        for (uint32_t j = 0; j + 1 < num_ins; j += 2) {
            char name[32];
            snprintf(name, sizeof(name), "In %u+%u", j + 1, j + 2);
            input_choice->add(strdup(name));
        }

        int cl, cr;
        m_engine.track(i).get_audio_input(cl, cr);
        if (cl == -1) input_choice->value(0);
        else if (cr == -1) input_choice->value(cl + 1);
        else if (cr == cl + 1) input_choice->value((int)(num_ins + 1 + cl / 2));
        
        input_choice->callback(cb_track_input, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));
        cur_y += std_h + 5;

        Fl_Value_Input* delay_input = new Fl_Value_Input(20 + x_offset + 30, cur_y, 45, std_h, "Dly");
        delay_input->labelsize(font_sz - 2);
        delay_input->value(m_engine.track(i).input_delay());
        delay_input->callback(cb_track_delay, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));
      }

      x_offset += 100;
    }

    if (num_buses > 0) {
      Fl_Box* separator = new Fl_Box(20 + x_offset - 5, 0, 5, 350);
      separator->box(FL_THIN_DOWN_BOX);
      x_offset += 10;

      for (size_t i = 0; i < num_buses; ++i) {
        int cur_y = 5;
        Fl_Box* b = new Fl_Box(20 + x_offset, cur_y, 80, std_h, strdup(("Bus " + ::std::to_string(i+1)).c_str()));
        b->labelsize(font_sz);
        cur_y += std_h + 5;
        
        Fl_Slider* vol = new Fl_Slider(20 + x_offset, cur_y, 20, 150);
        vol->type(FL_VERTICAL);
        vol->bounds(1, 0); 
        vol->value(m_engine.bus(i).volume());
        vol->callback(cb_bus_volume, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));

        VUMeter* meter_l = new VUMeter(45 + x_offset, cur_y, 8, 150);
        VUMeter* meter_r = new VUMeter(55 + x_offset, cur_y, 8, 150);
        m_bus_meters.push_back({meter_l, meter_r});
        cur_y += 150 + 5;

        Fl_Slider* pan_slider = new Fl_Slider(20 + x_offset, cur_y, 75, 15);
        pan_slider->type(FL_HOR_SLIDER);
        pan_slider->range(-1.0, 1.0);
        pan_slider->value(m_engine.bus(i).pan());
        pan_slider->callback(cb_bus_pan, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));
        cur_y += 15 + 5;

        Fl_Check_Button* mute = new Fl_Check_Button(20 + x_offset, cur_y, 35, std_h, "M");
        mute->labelsize(font_sz);
        mute->value(m_engine.bus(i).muted());
        mute->callback(cb_bus_mute, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));
        
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

void MixerPanel::update_effect_editor() {
    // Note: We don't call clear_callback_data() here because it's called 
    // in update_mixer_ui, and they are often called sequentially.
    // However, if we only update the effect editor (e.g. adding an effect),
    // we might need to clear.
    // Actually, update_mixer_ui rebuilds the whole track list.
    // Let's just NOT clear here and let FLTK handle the widget deletion,
    // but we need to avoid double-deletion of the pairs.
    // A better way: ONLY clear in update_mixer_ui if we are rebuilding everything.
    // But effect editor rebuilds its own set of widgets.
    
    // Safety: just clear and rebuild. This is the most robust way.
    // To avoid deleting the track pairs, we might need two lists, 
    // but let's just rebuild everything for now.
    update_mixer_ui(); 

    m_fx_chain_group->clear();
    m_fx_params_group->clear();

    if (m_selected_track < -1 || m_selected_track >= (int)m_engine.track_count()) return;

    bool is_master = (m_selected_track == -1);
    auto get_fx = [&](size_t idx) -> DSP* {
        if (is_master) return m_engine.m_master.get_effect(idx);
        return m_engine.track(m_selected_track).get_effect(idx);
    };

    if (m_selected_fx_slot != -1 && !get_fx(m_selected_fx_slot)) m_selected_fx_slot = -1;

    int std_h = m_engine.m_gui_button_height;
    int font_sz = m_engine.m_gui_font_size;
    int spacing = 2;

    m_fx_chain_group->begin();
    Fl_Scroll* chain_scroll = new Fl_Scroll(m_fx_chain_group->x(), m_fx_chain_group->y(), m_fx_chain_group->w(), m_fx_chain_group->h());
    chain_scroll->type(Fl_Scroll::VERTICAL);
    Fl_Pack* chain_pack = new Fl_Pack(chain_scroll->x(), chain_scroll->y(), chain_scroll->w() - 20, 1);
    chain_pack->type(Fl_Pack::VERTICAL);
    chain_pack->spacing(spacing);
    chain_pack->begin();

    for (size_t i = 0; i < MAX_INSERTS; ++i) {
        DSP* dsp = get_fx(i);
        if (dsp) {
            Fl_Group* row = new Fl_Group(0, 0, chain_pack->w(), std_h);
            row->begin();
            Fl_Check_Button* bypass = new Fl_Check_Button(row->x(), row->y(), 25, std_h);
            bypass->value(!dsp->is_bypassed());
            bypass->callback(cb_fx_bypass, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));
            Fl_Button* sel_btn = new Fl_Button(row->x() + 28, row->y(), 100, std_h, strdup(dsp->name().c_str()));
            sel_btn->labelsize(font_sz);
            if ((int)i == m_selected_fx_slot) sel_btn->color(FL_YELLOW);
            sel_btn->callback([](Fl_Widget* w, void* d){
                auto* p = static_cast<::std::pair<MixerPanel*,int>*>(d);
                p->first->m_selected_fx_slot = p->second;
                p->first->update_effect_editor();
            }, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));
            int bx = row->x() + 132, bw = 30;
            Fl_Button* up = new Fl_Button(bx, row->y(), bw, std_h, "@8");
            up->callback(cb_fx_up, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));
            Fl_Button* dn = new Fl_Button(bx + bw + 2, row->y(), bw, std_h, "@2");
            dn->callback(cb_fx_down, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));
            Fl_Button* rm = new Fl_Button(bx + 2*(bw + 2), row->y(), bw, std_h, "X");
            rm->callback(cb_fx_remove, reg_cb(new ::std::pair<MixerPanel*,int>(this, (int)i)));
            row->end();
        }
    }
    chain_pack->end(); chain_scroll->end();
    m_fx_chain_group->end(); m_fx_chain_group->redraw();

    m_fx_params_group->begin();
    if (m_selected_fx_slot != -1) {
        DSP* dsp = get_fx(m_selected_fx_slot);
        if (dsp) {
            Fl_Scroll* param_scroll = new Fl_Scroll(m_fx_params_group->x(), m_fx_params_group->y(), m_fx_params_group->w(), m_fx_params_group->h());
            param_scroll->type(Fl_Scroll::VERTICAL);
            
            Fl_Pack* param_pack = new Fl_Pack(param_scroll->x(), param_scroll->y(), param_scroll->w() - 25, 1);
            param_pack->type(Fl_Pack::VERTICAL);
            param_pack->spacing(spacing * 2);
            param_pack->begin();

            int slider_w = param_pack->w() - 150;

            Fl_Box* header = new Fl_Box(0, 0, param_pack->w(), std_h, strdup(("Editing: " + dsp->name()).c_str()));
            header->labelfont(FL_BOLD); header->labelsize(font_sz + 2); header->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
            
            Fl_Group* pre_row = new Fl_Group(0, 0, param_pack->w(), std_h);
            pre_row->begin();
            Fl_Choice* presets = new Fl_Choice(pre_row->x(), pre_row->y(), 150, std_h, "Preset");
            presets->labelsize(font_sz); presets->align(FL_ALIGN_RIGHT);
            auto ps = dsp->get_presets();
            int sel_idx = 0;
            for (size_t i = 0; i < ps.size(); ++i) {
                presets->add(strdup(ps[i].c_str()));
                if (ps[i] == m_current_preset_name) sel_idx = (int)i;
            }
            presets->value(sel_idx); 
            presets->callback(cb_fx_preset_select, reg_cb(new ::std::pair<MixerPanel*,int>(this, m_selected_fx_slot)));
            Fl_Button* psave = new Fl_Button(pre_row->x() + 220, pre_row->y(), 60, std_h, "Save");
            psave->labelsize(font_sz); psave->callback(cb_fx_preset_save, reg_cb(new ::std::pair<MixerPanel*,int>(this, m_selected_fx_slot)));
            Fl_Button* pload = new Fl_Button(pre_row->x() + 285, pre_row->y(), 60, std_h, "Load");
            pload->labelsize(font_sz); pload->callback(cb_fx_preset_load, reg_cb(new ::std::pair<MixerPanel*,int>(this, m_selected_fx_slot)));
            pre_row->end();

            if (auto* g = dynamic_cast<GainDSP*>(dsp)) {
                Fl_Value_Slider* s = new Fl_Value_Slider(0, 0, slider_w, std_h, "Gain");
                s->labelsize(font_sz); s->type(FL_HOR_NICE_SLIDER); s->range(0, 2); s->value(g->gain); s->align(FL_ALIGN_RIGHT);
                s->callback([](Fl_Widget* w, void* d){ ((GainDSP*)d)->gain = (float)((Fl_Value_Slider*)w)->value(); }, g);
            } else if (auto* d = dynamic_cast<DelayDSP*>(dsp)) {
                Fl_Value_Slider* s1 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Feedback");
                s1->labelsize(font_sz); s1->type(FL_HOR_NICE_SLIDER); s1->range(0, 0.99); s1->value(d->feedback); s1->align(FL_ALIGN_RIGHT);
                s1->callback([](Fl_Widget* w, void* v){ ((DelayDSP*)v)->feedback = (float)((Fl_Value_Slider*)w)->value(); }, d);
                Fl_Value_Slider* s2 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Mix");
                s2->labelsize(font_sz); s2->type(FL_HOR_NICE_SLIDER); s2->range(0, 1); s2->value(d->mix); s2->align(FL_ALIGN_RIGHT);
                s2->callback([](Fl_Widget* w, void* v){ ((DelayDSP*)v)->mix = (float)((Fl_Value_Slider*)w)->value(); }, d);
            } else if (auto* rev = dynamic_cast<ReverbDSP*>(dsp)) {
                Fl_Value_Slider* s1 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Room Size");
                s1->labelsize(font_sz); s1->type(FL_HOR_NICE_SLIDER); s1->range(0, 1); s1->value(rev->room_size); s1->align(FL_ALIGN_RIGHT);
                s1->callback([](Fl_Widget* w, void* v){ ((ReverbDSP*)v)->room_size = (float)((Fl_Value_Slider*)w)->value(); }, rev);
                Fl_Value_Slider* s2 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Damp");
                s2->labelsize(font_sz); s2->type(FL_HOR_NICE_SLIDER); s2->range(0, 1); s2->value(rev->damp); s2->align(FL_ALIGN_RIGHT);
                s2->callback([](Fl_Widget* w, void* v){ ((ReverbDSP*)v)->damp = (float)((Fl_Value_Slider*)w)->value(); }, rev);
                Fl_Value_Slider* s3 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Mix");
                s3->labelsize(font_sz); s3->type(FL_HOR_NICE_SLIDER); s3->range(0, 1); s3->value(rev->mix); s3->align(FL_ALIGN_RIGHT);
                s3->callback([](Fl_Widget* w, void* v){ ((ReverbDSP*)v)->mix = (float)((Fl_Value_Slider*)w)->value(); }, rev);
            } else if (auto* lim = dynamic_cast<LimiterDSP*>(dsp)) {
                Fl_Value_Slider* s1 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Ceiling");
                s1->labelsize(font_sz); s1->type(FL_HOR_NICE_SLIDER); s1->range(0, 1); s1->value(lim->ceiling); s1->align(FL_ALIGN_RIGHT);
                s1->callback([](Fl_Widget* w, void* v){ ((LimiterDSP*)v)->ceiling = (float)((Fl_Value_Slider*)w)->value(); }, lim);
                Fl_Value_Slider* s2 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Threshold");
                s2->labelsize(font_sz); s2->type(FL_HOR_NICE_SLIDER); s2->range(0, 1); s2->value(lim->threshold); s2->align(FL_ALIGN_RIGHT);
                s2->callback([](Fl_Widget* w, void* v){ ((LimiterDSP*)v)->threshold = (float)((Fl_Value_Slider*)w)->value(); }, lim);
            } else if (auto* exc = dynamic_cast<ExciterDSP*>(dsp)) {
                Fl_Value_Slider* s1 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Amount");
                s1->labelsize(font_sz); s1->type(FL_HOR_NICE_SLIDER); s1->range(0, 1); s1->value(exc->amount); s1->align(FL_ALIGN_RIGHT);
                s1->callback([](Fl_Widget* w, void* v){ ((ExciterDSP*)v)->amount = (float)((Fl_Value_Slider*)w)->value(); }, exc);
                Fl_Value_Slider* s2 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Freq");
                s2->labelsize(font_sz); s2->type(FL_HOR_NICE_SLIDER); s2->range(0, 1); s2->value(exc->freq); s2->align(FL_ALIGN_RIGHT);
                s2->callback([](Fl_Widget* w, void* v){ ((ExciterDSP*)v)->freq = (float)((Fl_Value_Slider*)w)->value(); }, exc);
            } else if (auto* pha = dynamic_cast<PhaserDSP*>(dsp)) {
                Fl_Value_Slider* s1 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Rate");
                s1->labelsize(font_sz); s1->type(FL_HOR_NICE_SLIDER); s1->range(0, 1); s1->value(pha->rate); s1->align(FL_ALIGN_RIGHT);
                s1->callback([](Fl_Widget* w, void* v){ ((PhaserDSP*)v)->rate = (float)((Fl_Value_Slider*)w)->value(); }, pha);
                Fl_Value_Slider* s2 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Depth");
                s2->labelsize(font_sz); s2->type(FL_HOR_NICE_SLIDER); s2->range(0, 1); s2->value(pha->depth); s2->align(FL_ALIGN_RIGHT);
                s2->callback([](Fl_Widget* w, void* v){ ((PhaserDSP*)v)->depth = (float)((Fl_Value_Slider*)w)->value(); }, pha);
                Fl_Value_Slider* s3 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Feedback");
                s3->labelsize(font_sz); s3->type(FL_HOR_NICE_SLIDER); s3->range(0, 1); s3->value(pha->feedback); s3->align(FL_ALIGN_RIGHT);
                s3->callback([](Fl_Widget* w, void* v){ ((PhaserDSP*)v)->feedback = (float)((Fl_Value_Slider*)w)->value(); }, pha);
                Fl_Value_Slider* s4 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Mix");
                s4->labelsize(font_sz); s4->type(FL_HOR_NICE_SLIDER); s4->range(0, 1); s4->value(pha->mix); s4->align(FL_ALIGN_RIGHT);
                s4->callback([](Fl_Widget* w, void* v){ ((PhaserDSP*)v)->mix = (float)((Fl_Value_Slider*)w)->value(); }, pha);
            } else if (auto* fla = dynamic_cast<FlangerDSP*>(dsp)) {
                Fl_Value_Slider* s1 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Rate");
                s1->labelsize(font_sz); s1->type(FL_HOR_NICE_SLIDER); s1->range(0, 1); s1->value(fla->rate); s1->align(FL_ALIGN_RIGHT);
                s1->callback([](Fl_Widget* w, void* v){ ((FlangerDSP*)v)->rate = (float)((Fl_Value_Slider*)w)->value(); }, fla);
                Fl_Value_Slider* s2 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Depth");
                s2->labelsize(font_sz); s2->type(FL_HOR_NICE_SLIDER); s2->range(0, 1); s2->value(fla->depth); s2->align(FL_ALIGN_RIGHT);
                s2->callback([](Fl_Widget* w, void* v){ ((FlangerDSP*)v)->depth = (float)((Fl_Value_Slider*)w)->value(); }, fla);
                Fl_Value_Slider* s3 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Feedback");
                s3->labelsize(font_sz); s3->type(FL_HOR_NICE_SLIDER); s3->range(0, 1); s3->value(fla->feedback); s3->align(FL_ALIGN_RIGHT);
                s3->callback([](Fl_Widget* w, void* v){ ((FlangerDSP*)v)->feedback = (float)((Fl_Value_Slider*)w)->value(); }, fla);
                Fl_Value_Slider* s4 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Mix");
                s4->labelsize(font_sz); s4->type(FL_HOR_NICE_SLIDER); s4->range(0, 1); s4->value(fla->mix); s4->align(FL_ALIGN_RIGHT);
                s4->callback([](Fl_Widget* w, void* v){ ((FlangerDSP*)v)->mix = (float)((Fl_Value_Slider*)w)->value(); }, fla);
            } else if (auto* ech = dynamic_cast<EchoDSP*>(dsp)) {
                Fl_Value_Slider* s1 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Time");
                s1->labelsize(font_sz); s1->type(FL_HOR_NICE_SLIDER); s1->range(0, 1); s1->value(ech->time); s1->align(FL_ALIGN_RIGHT);
                s1->callback([](Fl_Widget* w, void* v){ ((EchoDSP*)v)->time = (float)((Fl_Value_Slider*)w)->value(); }, ech);
                Fl_Value_Slider* s2 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Feedback");
                s2->labelsize(font_sz); s2->type(FL_HOR_NICE_SLIDER); s2->range(0, 1); s2->value(ech->feedback); s2->align(FL_ALIGN_RIGHT);
                s2->callback([](Fl_Widget* w, void* v){ ((EchoDSP*)v)->feedback = (float)((Fl_Value_Slider*)w)->value(); }, ech);
                Fl_Value_Slider* s3 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Damp");
                s3->labelsize(font_sz); s3->type(FL_HOR_NICE_SLIDER); s3->range(0, 1); s3->value(ech->damp); s3->align(FL_ALIGN_RIGHT);
                s3->callback([](Fl_Widget* w, void* v){ ((EchoDSP*)v)->damp = (float)((Fl_Value_Slider*)w)->value(); }, ech);
                Fl_Value_Slider* s4 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Mix");
                s4->labelsize(font_sz); s4->type(FL_HOR_NICE_SLIDER); s4->range(0, 1); s4->value(ech->mix); s4->align(FL_ALIGN_RIGHT);
                s4->callback([](Fl_Widget* w, void* v){ ((EchoDSP*)v)->mix = (float)((Fl_Value_Slider*)w)->value(); }, ech);
            } else if (auto* cmp = dynamic_cast<CompressorDSP*>(dsp)) {
                Fl_Value_Slider* s1 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Threshold");
                s1->labelsize(font_sz); s1->type(FL_HOR_NICE_SLIDER); s1->range(0, 1); s1->value(cmp->threshold); s1->align(FL_ALIGN_RIGHT);
                s1->callback([](Fl_Widget* w, void* v){ ((CompressorDSP*)v)->threshold = (float)((Fl_Value_Slider*)w)->value(); }, cmp);
                Fl_Value_Slider* s2 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Ratio");
                s2->labelsize(font_sz); s2->type(FL_HOR_NICE_SLIDER); s2->range(1, 20); s2->value(cmp->ratio); s2->align(FL_ALIGN_RIGHT);
                s2->callback([](Fl_Widget* w, void* v){ ((CompressorDSP*)v)->ratio = (float)((Fl_Value_Slider*)w)->value(); }, cmp);
                Fl_Value_Slider* s3 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Attack (s)");
                s3->labelsize(font_sz); s3->type(FL_HOR_NICE_SLIDER); s3->range(0.001, 0.5); s3->value(cmp->attack); s3->align(FL_ALIGN_RIGHT);
                s3->callback([](Fl_Widget* w, void* v){ ((CompressorDSP*)v)->attack = (float)((Fl_Value_Slider*)w)->value(); }, cmp);
                Fl_Value_Slider* s4 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Release (s)");
                s4->labelsize(font_sz); s4->type(FL_HOR_NICE_SLIDER); s4->range(0.01, 2.0); s4->value(cmp->release); s4->align(FL_ALIGN_RIGHT);
                s4->callback([](Fl_Widget* w, void* v){ ((CompressorDSP*)v)->release = (float)((Fl_Value_Slider*)w)->value(); }, cmp);
                Fl_Value_Slider* s5 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Makeup");
                s5->labelsize(font_sz); s5->type(FL_HOR_NICE_SLIDER); s5->range(0, 2); s5->value(cmp->makeup); s5->align(FL_ALIGN_RIGHT);
                s5->callback([](Fl_Widget* w, void* v){ ((CompressorDSP*)v)->makeup = (float)((Fl_Value_Slider*)w)->value(); }, cmp);
            } else if (auto* geq = dynamic_cast<GraphicalEQDSP*>(dsp)) {
                Fl_Group* eq_grp = new Fl_Group(0, 0, param_pack->w(), 150);
                eq_grp->begin();
                int band_w = (param_pack->w() - 40) / 12;
                for (int b = 0; b < 12; ++b) {
                    Fl_Slider* s = new Fl_Slider(10 + b * band_w, eq_grp->y(), band_w - 5, 120);
                    s->type(FL_VERTICAL); s->range(12, -12); s->value(geq->get_band_gain(b));
                    struct EQData { GraphicalEQDSP* dsp; int band; };
                    s->callback([](Fl_Widget* w, void* d){ auto* ed = (EQData*)d; ed->dsp->set_band_gain(ed->band, (float)((Fl_Slider*)w)->value()); }, reg_cb(new EQData{geq, b}));
                    Fl_Box* lbl = new Fl_Box(10 + b * band_w, eq_grp->y() + 120, band_w - 5, 20);
                    float f = geq->get_band_freq(b);
                    if (f >= 1000) lbl->copy_label((std::to_string((int)(f/1000)) + "k").c_str()); else lbl->copy_label(std::to_string((int)f).c_str());
                    lbl->labelsize(9);
                }
                eq_grp->end();
            } else if (auto* cab = dynamic_cast<CabinetDSP*>(dsp)) {
                Fl_Value_Slider* s1 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Low Cut");
                s1->labelsize(font_sz); s1->type(FL_HOR_NICE_SLIDER); s1->range(20, 500); s1->value(cab->low_cut); s1->align(FL_ALIGN_RIGHT);
                s1->callback([](Fl_Widget* w, void* v){ ((CabinetDSP*)v)->low_cut = (float)((Fl_Value_Slider*)w)->value(); }, cab);
                Fl_Value_Slider* s2 = new Fl_Value_Slider(0, 0, slider_w, std_h, "High Cut");
                s2->labelsize(font_sz); s2->type(FL_HOR_NICE_SLIDER); s2->range(1000, 15000); s2->value(cab->high_cut); s2->align(FL_ALIGN_RIGHT);
                s2->callback([](Fl_Widget* w, void* v){ ((CabinetDSP*)v)->high_cut = (float)((Fl_Value_Slider*)w)->value(); }, cab);
                Fl_Value_Slider* s3 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Peak Freq");
                s3->labelsize(font_sz); s3->type(FL_HOR_NICE_SLIDER); s3->range(500, 8000); s3->value(cab->peak_freq); s3->align(FL_ALIGN_RIGHT);
                s3->callback([](Fl_Widget* w, void* v){ ((CabinetDSP*)v)->peak_freq = (float)((Fl_Value_Slider*)w)->value(); }, cab);
                Fl_Value_Slider* s4 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Peak Gain");
                s4->labelsize(font_sz); s4->type(FL_HOR_NICE_SLIDER); s4->range(-12, 12); s4->value(cab->peak_gain); s4->align(FL_ALIGN_RIGHT);
                s4->callback([](Fl_Widget* w, void* v){ ((CabinetDSP*)v)->peak_gain = (float)((Fl_Value_Slider*)w)->value(); }, cab);
            } else if (auto* gate = dynamic_cast<GateDSP*>(dsp)) {
                Fl_Value_Slider* s1 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Threshold");
                s1->labelsize(font_sz); s1->type(FL_HOR_NICE_SLIDER); s1->range(0, 1); s1->value(gate->threshold); s1->align(FL_ALIGN_RIGHT);
                s1->callback([](Fl_Widget* w, void* v){ ((GateDSP*)v)->threshold = (float)((Fl_Value_Slider*)w)->value(); }, gate);
                Fl_Value_Slider* s2 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Range");
                s2->labelsize(font_sz); s2->type(FL_HOR_NICE_SLIDER); s2->range(0, 1); s2->value(gate->range); s2->align(FL_ALIGN_RIGHT);
                s2->callback([](Fl_Widget* w, void* v){ ((GateDSP*)v)->range = (float)((Fl_Value_Slider*)w)->value(); }, gate);
                Fl_Value_Slider* s3 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Attack (s)");
                s3->labelsize(font_sz); s3->type(FL_HOR_NICE_SLIDER); s3->range(0.001, 0.5); s3->value(gate->attack); s3->align(FL_ALIGN_RIGHT);
                s3->callback([](Fl_Widget* w, void* v){ ((GateDSP*)v)->attack = (float)((Fl_Value_Slider*)w)->value(); }, gate);
                Fl_Value_Slider* s4 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Release (s)");
                s4->labelsize(font_sz); s4->type(FL_HOR_NICE_SLIDER); s4->range(0.01, 2.0); s4->value(gate->release); s4->align(FL_ALIGN_RIGHT);
                s4->callback([](Fl_Widget* w, void* v){ ((GateDSP*)v)->release = (float)((Fl_Value_Slider*)w)->value(); }, gate);
            } else if (auto* dis = dynamic_cast<DistortionDSP*>(dsp)) {
                Fl_Value_Slider* s1 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Drive");
                s1->labelsize(font_sz); s1->type(FL_HOR_NICE_SLIDER); s1->range(0, 1); s1->value(dis->drive); s1->align(FL_ALIGN_RIGHT);
                s1->callback([](Fl_Widget* w, void* v){ ((DistortionDSP*)v)->drive = (float)((Fl_Value_Slider*)w)->value(); }, dis);
                Fl_Value_Slider* s2 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Mix");
                s2->labelsize(font_sz); s2->type(FL_HOR_NICE_SLIDER); s2->range(0, 1); s2->value(dis->mix); s2->align(FL_ALIGN_RIGHT);
                s2->callback([](Fl_Widget* w, void* v){ ((DistortionDSP*)v)->mix = (float)((Fl_Value_Slider*)w)->value(); }, dis);
            } else if (auto* cho = dynamic_cast<ChorusDSP*>(dsp)) {
                Fl_Value_Slider* s1 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Rate");
                s1->labelsize(font_sz); s1->type(FL_HOR_NICE_SLIDER); s1->range(0, 1); s1->value(cho->rate); s1->align(FL_ALIGN_RIGHT);
                s1->callback([](Fl_Widget* w, void* v){ ((ChorusDSP*)v)->rate = (float)((Fl_Value_Slider*)w)->value(); }, cho);
                Fl_Value_Slider* s2 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Depth");
                s2->labelsize(font_sz); s2->type(FL_HOR_NICE_SLIDER); s2->range(0, 1); s2->value(cho->depth); s2->align(FL_ALIGN_RIGHT);
                s2->callback([](Fl_Widget* w, void* v){ ((ChorusDSP*)v)->depth = (float)((Fl_Value_Slider*)w)->value(); }, cho);
                Fl_Value_Slider* s3 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Feedback");
                s3->labelsize(font_sz); s3->type(FL_HOR_NICE_SLIDER); s3->range(0, 1); s3->value(cho->feedback); s3->align(FL_ALIGN_RIGHT);
                s3->callback([](Fl_Widget* w, void* v){ ((ChorusDSP*)v)->feedback = (float)((Fl_Value_Slider*)w)->value(); }, cho);
                Fl_Value_Slider* s4 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Mix");
                s4->labelsize(font_sz); s4->type(FL_HOR_NICE_SLIDER); s4->range(0, 1); s4->value(cho->mix); s4->align(FL_ALIGN_RIGHT);
                s4->callback([](Fl_Widget* w, void* v){ ((ChorusDSP*)v)->mix = (float)((Fl_Value_Slider*)w)->value(); }, cho);
            } else if (auto* exp = dynamic_cast<StereoExpanderDSP*>(dsp)) {
                Fl_Value_Slider* s1 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Width");
                s1->labelsize(font_sz); s1->type(FL_HOR_NICE_SLIDER); s1->range(0, 2); s1->value(exp->width); s1->align(FL_ALIGN_RIGHT);
                s1->callback([](Fl_Widget* w, void* v){ ((StereoExpanderDSP*)v)->width = (float)((Fl_Value_Slider*)w)->value(); }, exp);
            } else if (auto* rmo = dynamic_cast<RingModulatorDSP*>(dsp)) {
                Fl_Value_Slider* s1 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Freq");
                s1->labelsize(font_sz); s1->type(FL_HOR_NICE_SLIDER); s1->range(0, 1); s1->value(rmo->freq); s1->align(FL_ALIGN_RIGHT);
                s1->callback([](Fl_Widget* w, void* v){ ((RingModulatorDSP*)v)->freq = (float)((Fl_Value_Slider*)w)->value(); }, rmo);
                Fl_Value_Slider* s2 = new Fl_Value_Slider(0, 0, slider_w, std_h, "Mix");
                s2->labelsize(font_sz); s2->type(FL_HOR_NICE_SLIDER); s2->range(0, 1); s2->value(rmo->mix); s2->align(FL_ALIGN_RIGHT);
                s2->callback([](Fl_Widget* w, void* v){ ((RingModulatorDSP*)v)->mix = (float)((Fl_Value_Slider*)w)->value(); }, rmo);
            }
            param_pack->end(); param_scroll->end();
        }
    } else {
        Fl_Box* msg = new Fl_Box(m_fx_params_group->x(), m_fx_params_group->y(), m_fx_params_group->w(), m_fx_params_group->h(), "Select an effect to edit parameters");
        msg->labelsize(font_sz);
    }
    m_fx_params_group->end();
    m_fx_params_group->redraw();
}

void MixerPanel::cb_track_select(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    MixerPanel* self = pair->first;
    int track_idx = pair->second;
    
    self->m_selected_track = track_idx;
    self->m_selected_fx_slot = -1;
    
    self->update_mixer_ui();
    self->update_effect_editor();
}

void MixerPanel::cb_add_fx(Fl_Widget* w, void* data) {
    MixerPanel* self = static_cast<MixerPanel*>(data);
    int fx_idx = self->m_avail_fx_browser->value();
    if (fx_idx <= 0) return;
    std::string fx_name = self->m_avail_fx_browser->text(fx_idx);
    bool is_master = (self->m_selected_track == -1);
    auto get_fx_at = [&](size_t idx) -> DSP* {
        if (is_master) return self->m_engine.m_master.get_effect(idx);
        return self->m_engine.track(self->m_selected_track).get_effect(idx);
    };
    auto set_fx_at = [&](size_t idx, std::unique_ptr<DSP> dsp) {
        if (is_master) { self->m_engine.m_master.set_effect(idx, std::move(dsp)); self->m_engine.m_master.enable_effect(idx, true); }
        else { self->m_engine.track(self->m_selected_track).set_effect(idx, std::move(dsp)); self->m_engine.track(self->m_selected_track).enable_effect(idx, true); }
    };
    for (size_t i = 0; i < MAX_INSERTS; ++i) {
        DSP* dsp = get_fx_at(i);
        if (dsp && dsp->name() == fx_name) return;
    }
    for (size_t i = 0; i < MAX_INSERTS; ++i) {
        if (!get_fx_at(i)) {
            if (fx_name == "Gain") set_fx_at(i, std::make_unique<GainDSP>());
            else if (fx_name == "Delay") set_fx_at(i, std::make_unique<DelayDSP>());
            else if (fx_name == "Reverb") set_fx_at(i, std::make_unique<ReverbDSP>());
            else if (fx_name == "Limiter") set_fx_at(i, std::make_unique<LimiterDSP>());
            else if (fx_name == "Exciter") set_fx_at(i, std::make_unique<ExciterDSP>());
            else if (fx_name == "Phaser") set_fx_at(i, std::make_unique<PhaserDSP>());
            else if (fx_name == "Flanger") set_fx_at(i, std::make_unique<FlangerDSP>());
            else if (fx_name == "Echo") set_fx_at(i, std::make_unique<EchoDSP>());
            else if (fx_name == "Compressor") set_fx_at(i, std::make_unique<CompressorDSP>());
            else if (fx_name == "Graphical EQ") set_fx_at(i, std::make_unique<GraphicalEQDSP>());
            else if (fx_name == "Cabinet") set_fx_at(i, std::make_unique<CabinetDSP>());
            else if (fx_name == "Distortion") set_fx_at(i, std::make_unique<DistortionDSP>());
            else if (fx_name == "Chorus") set_fx_at(i, std::make_unique<ChorusDSP>());
            else if (fx_name == "Stereo Expander") set_fx_at(i, std::make_unique<StereoExpanderDSP>());
            else if (fx_name == "Ring Modulator") set_fx_at(i, std::make_unique<RingModulatorDSP>());
            else if (fx_name == "Gate") set_fx_at(i, std::make_unique<GateDSP>());
            self->m_selected_fx_slot = (int)i;
            break;
        }
    }
    self->update_effect_editor();
}

void MixerPanel::cb_fx_up(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    if (pair->first->m_selected_track == -1) pair->first->m_engine.m_master.move_effect_up(pair->second);
    else pair->first->m_engine.track(pair->first->m_selected_track).move_effect_up(pair->second);
    pair->first->update_effect_editor();
}

void MixerPanel::cb_fx_down(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    if (pair->first->m_selected_track == -1) pair->first->m_engine.m_master.move_effect_down(pair->second);
    else pair->first->m_engine.track(pair->first->m_selected_track).move_effect_down(pair->second);
    pair->first->update_effect_editor();
}

void MixerPanel::cb_fx_remove(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    if (pair->first->m_selected_track == -1) pair->first->m_engine.m_master.remove_effect(pair->second);
    else pair->first->m_engine.track(pair->first->m_selected_track).remove_effect(pair->second);
    pair->first->update_effect_editor();
}

void MixerPanel::cb_fx_bypass(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    DSP* dsp = (pair->first->m_selected_track == -1) ? pair->first->m_engine.m_master.get_effect(pair->second) : pair->first->m_engine.track(pair->first->m_selected_track).get_effect(pair->second);
    if (dsp) dsp->set_bypass(!static_cast<Fl_Check_Button*>(w)->value());
}

void MixerPanel::cb_fx_preset_select(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    Fl_Choice* choice = static_cast<Fl_Choice*>(w);
    int idx = choice->value();
    if (idx < 0) return;
    
    DSP* dsp = (pair->first->m_selected_track == -1) ? pair->first->m_engine.m_master.get_effect(pair->second) : pair->first->m_engine.track(pair->first->m_selected_track).get_effect(pair->second);
    if (dsp) {
        pair->first->m_current_preset_name = choice->text(idx);
        dsp->load_preset(pair->first->m_current_preset_name);
        // We don't call update_effect_editor() here fully because it rebuilds the Fl_Choice
        // and we'd lose the visual selection state unless we store it.
        // But we DO need to update the SLIDERS. 
        // Simplest: just store the index and rebuild.
        pair->first->update_effect_editor();
    }
}

void MixerPanel::cb_fx_preset_save(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    DSP* dsp = (pair->first->m_selected_track == -1) ? pair->first->m_engine.m_master.get_effect(pair->second) : pair->first->m_engine.track(pair->first->m_selected_track).get_effect(pair->second);
    if (!dsp) return;
    Fl_Native_File_Chooser fnfc; fnfc.title("Save Preset"); fnfc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE); fnfc.filter("JSON Files\t*.json\n");
    if (fnfc.show() == 0) { std::ofstream f(fnfc.filename()); if (f.is_open()) f << dsp->get_state(); }
}

void MixerPanel::cb_fx_preset_load(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    DSP* dsp = (pair->first->m_selected_track == -1) ? pair->first->m_engine.m_master.get_effect(pair->second) : pair->first->m_engine.track(pair->first->m_selected_track).get_effect(pair->second);
    if (!dsp) return;
    Fl_Native_File_Chooser fnfc; fnfc.title("Load Preset"); fnfc.type(Fl_Native_File_Chooser::BROWSE_FILE); fnfc.filter("JSON Files\t*.json\n");
    if (fnfc.show() == 0) { std::ifstream f(fnfc.filename()); if (f.is_open()) { std::stringstream buffer; buffer << f.rdbuf(); dsp->set_state(buffer.str()); pair->first->update_effect_editor(); } }
}

void MixerPanel::cb_save_chain(Fl_Widget* w, void* data) {
    MixerPanel* self = static_cast<MixerPanel*>(data);
    Fl_Native_File_Chooser fnfc; fnfc.title("Save Effect Chain"); fnfc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE); fnfc.filter("Chain Files\t*.chain\n");
    if (fnfc.show() == 0) { if (self->m_selected_track == -1) self->m_engine.m_master.save_effect_chain(fnfc.filename()); else self->m_engine.track(self->m_selected_track).save_effect_chain(fnfc.filename()); }
}

void MixerPanel::cb_load_chain(Fl_Widget* w, void* data) {
    MixerPanel* self = static_cast<MixerPanel*>(data);
    Fl_Native_File_Chooser fnfc; fnfc.title("Load Effect Chain"); fnfc.type(Fl_Native_File_Chooser::BROWSE_FILE); fnfc.filter("Chain Files\t*.chain\n");
    if (fnfc.show() == 0) { if (self->m_selected_track == -1) self->m_engine.m_master.load_effect_chain(fnfc.filename()); else self->m_engine.track(self->m_selected_track).load_effect_chain(fnfc.filename()); self->m_selected_fx_slot = -1; self->update_effect_editor(); }
}

void MixerPanel::cb_master_gain(Fl_Widget* w, void* data) {
    MixerPanel* self = static_cast<MixerPanel*>(data);
    float g = static_cast<Fl_Value_Slider*>(w)->value();
    self->m_engine.set_master_gain(g);
}

void MixerPanel::cb_track_volume(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    pair->first->m_engine.track(pair->second).set_volume(static_cast<Fl_Slider*>(w)->value());
}

void MixerPanel::cb_track_pan(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    pair->first->m_engine.track(pair->second).set_pan(static_cast<Fl_Slider*>(w)->value());
}

void MixerPanel::cb_track_mute(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    pair->first->m_engine.track(pair->second).set_mute(static_cast<Fl_Check_Button*>(w)->value());
}

void MixerPanel::cb_track_solo(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    pair->first->m_engine.track(pair->second).set_solo(static_cast<Fl_Check_Button*>(w)->value());
}

void MixerPanel::cb_track_input(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    int val = static_cast<Fl_Choice*>(w)->value(); uint32_t num_ins = pair->first->m_engine.m_num_ins;
    if (val == 0) pair->first->m_engine.track(pair->second).set_audio_input(-1, -1);
    else if (val <= (int)num_ins) pair->first->m_engine.track(pair->second).set_audio_input(val - 1, -1);
    else { int idx = val - (int)num_ins - 1; pair->first->m_engine.track(pair->second).set_audio_input(idx * 2, idx * 2 + 1); }
}

void MixerPanel::cb_track_delay(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    pair->first->m_engine.track(pair->second).set_input_delay((float)static_cast<Fl_Value_Input*>(w)->value(), pair->first->m_engine.sample_rate());
}

void MixerPanel::cb_bus_volume(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    pair->first->m_engine.bus(pair->second).set_volume(static_cast<Fl_Slider*>(w)->value());
}

void MixerPanel::cb_bus_pan(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    pair->first->m_engine.bus(pair->second).set_pan(static_cast<Fl_Slider*>(w)->value());
}

void MixerPanel::cb_bus_mute(Fl_Widget* w, void* data) {
    auto* pair = static_cast<::std::pair<MixerPanel*,int>*>(data);
    pair->first->m_engine.bus(pair->second).set_mute(static_cast<Fl_Check_Button*>(w)->value());
}

void MixerPanel::cb_detach(Fl_Widget*, void* data) {
    MixerPanel* self = static_cast<MixerPanel*>(data);
    if (self->m_detached_window) self->m_detached_window->show();
    else { self->m_detached_window = new DetachedWindow(850, 300, "Mixer", self, self->parent()); self->m_detached_window->show(); }
    self->hide();
}

} // namespace disgrace_ns
