#include "wx_mixer_panel.h"
#include "wx_vu_meter.h"
#include "wx_spectral_view.h"
#include "../core/engine.h"
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

#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/scrolwin.h>
#include <algorithm>

namespace disgrace_ns {

wxBEGIN_EVENT_TABLE(MixerPanel, wxPanel)
    EVT_SLIDER(wxID_ANY, MixerPanel::on_master_gain)
    EVT_CHECKBOX(wxID_ANY, MixerPanel::on_master_mute)
    EVT_BUTTON(wxID_ANY, MixerPanel::on_add_fx)
    EVT_BUTTON(wxID_ANY, MixerPanel::on_save_chain)
    EVT_BUTTON(wxID_ANY, MixerPanel::on_load_chain)
wxEND_EVENT_TABLE()

MixerPanel::MixerPanel(wxWindow* parent, Engine& engine)
    : wxPanel(parent, wxID_ANY), m_engine(engine)
{
    m_selected_track = kSelectedMaster;
    m_selected_fx_slot = -1;

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    m_splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_VERTICAL);
    m_splitter->SetMinimumPaneSize(100);

    // --- Upper Pane: Tracks ---
    m_upper_pane = new wxPanel(m_splitter, wxID_ANY);
    wxBoxSizer* upper_sizer = new wxBoxSizer(wxHORIZONTAL);

    m_track_group = new wxScrolledWindow(m_upper_pane, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL);
    m_track_group->SetScrollRate(20, 0);
    upper_sizer->Add(m_track_group, 1, wxEXPAND | wxALL, 2);

    // Master Channel
    m_master_group = new wxPanel(m_upper_pane, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_THEME);
    wxBoxSizer* master_sizer = new wxBoxSizer(wxVERTICAL);
    
    wxStaticText* master_label = new wxStaticText(m_master_group, wxID_ANY, "Master");
    master_sizer->Add(master_label, 0, wxALIGN_CENTER | wxALL, 2);

    wxBoxSizer* master_controls = new wxBoxSizer(wxHORIZONTAL);

    m_master_gain = new wxSlider(m_master_group, wxID_ANY, 100, 0, 200, wxDefaultPosition, wxSize(-1, 100), wxSL_VERTICAL | wxSL_INVERSE);
    m_master_gain->SetValue((int)(m_engine.master_gain() * 100));
    master_controls->Add(m_master_gain, 0, wxEXPAND | wxALL, 2);

    m_master_meter_l = new VUMeter(m_master_group, wxID_ANY, m_engine, false);
    m_master_meter_r = new VUMeter(m_master_group, wxID_ANY, m_engine, false);
    m_master_meter_l->SetMinSize(wxSize(10, 100));
    m_master_meter_r->SetMinSize(wxSize(10, 100));
    master_controls->Add(m_master_meter_l, 1, wxEXPAND | wxALL, 1);
    master_controls->Add(m_master_meter_r, 1, wxEXPAND | wxALL, 1);
    
    master_sizer->Add(master_controls, 1, wxEXPAND | wxALL, 2);

    m_master_mute = new wxCheckBox(m_master_group, wxID_ANY, "M");
    m_master_mute->SetValue(m_engine.m_master.muted());
    master_sizer->Add(m_master_mute, 0, wxALIGN_CENTER | wxALL, 2);

    m_master_sel_btn = new wxButton(m_master_group, wxID_ANY, "SEL", wxDefaultPosition, wxSize(60, 25));
    if (m_selected_track == kSelectedMaster) {
        m_master_sel_btn->SetBackgroundColour(wxColour(255, 255, 0));
        m_master_sel_btn->SetForegroundColour(*wxBLACK);
    }
    m_master_sel_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent& ev) {
        this->CallAfter([this]() {
            m_selected_track = kSelectedMaster;
            m_selected_fx_slot = -1;
            update_mixer_ui();
            update_effect_editor();
        });
    });
    master_sizer->Add(m_master_sel_btn, 0, wxALIGN_CENTER | wxALL, 2);

    m_master_group->SetSizer(master_sizer);
    upper_sizer->Add(m_master_group, 0, wxEXPAND | wxALL, 2);

    m_upper_pane->SetSizer(upper_sizer);

    // --- Lower Pane: Effects ---
    m_lower_pane = new wxPanel(m_splitter, wxID_ANY);
    wxBoxSizer* lower_sizer = new wxBoxSizer(wxHORIZONTAL);

    // Available Effects List
    wxBoxSizer* avail_sizer = new wxBoxSizer(wxVERTICAL);
    avail_sizer->Add(new wxStaticText(m_lower_pane, wxID_ANY, "Available Effects"), 0, wxALL, 2);
    m_avail_fx_browser = new wxListBox(m_lower_pane, wxID_ANY, wxDefaultPosition, wxSize(150, -1));
    std::vector<std::string> fx_list = {
        "Gain", "Delay", "Reverb", "Limiter", "Exciter",
        "Phaser", "Flanger", "Echo", "Compressor",
        "Graphical EQ", "Cabinet", "Distortion",
        "Chorus", "Stereo Expander", "Ring Modulator", "Gate"
    };
    std::sort(fx_list.begin(), fx_list.end());
    for (const auto& fx : fx_list) m_avail_fx_browser->Append(fx);
    avail_sizer->Add(m_avail_fx_browser, 1, wxEXPAND | wxALL, 2);
    
    wxButton* add_btn = new wxButton(m_lower_pane, wxID_ANY, "Add Effect");
    add_btn->Bind(wxEVT_BUTTON, &MixerPanel::on_add_fx, this);
    avail_sizer->Add(add_btn, 0, wxEXPAND | wxALL, 2);
    lower_sizer->Add(avail_sizer, 0, wxEXPAND | wxALL, 2);

    // Effect Chain
    wxBoxSizer* chain_sizer = new wxBoxSizer(wxVERTICAL);
    chain_sizer->Add(new wxStaticText(m_lower_pane, wxID_ANY, "Effect Chain"), 0, wxALL, 2);
    m_fx_chain_group = new wxScrolledWindow(m_lower_pane, wxID_ANY, wxDefaultPosition, wxSize(250, -1), wxVSCROLL);
    m_fx_chain_group->SetScrollRate(0, 20);
    m_fx_chain_group->SetBackgroundColour(wxColour(30, 30, 30)); // Darker background for chain
    chain_sizer->Add(m_fx_chain_group, 1, wxEXPAND | wxALL, 2);

    wxBoxSizer* chain_btns = new wxBoxSizer(wxHORIZONTAL);
    m_load_chain_btn = new wxButton(m_lower_pane, wxID_ANY, "Load Chain");
    m_save_chain_btn = new wxButton(m_lower_pane, wxID_ANY, "Save Chain");
    chain_btns->Add(m_load_chain_btn, 1, wxALL, 2);
    chain_btns->Add(m_save_chain_btn, 1, wxALL, 2);
    chain_sizer->Add(chain_btns, 0, wxEXPAND | wxALL, 2);
    lower_sizer->Add(chain_sizer, 0, wxEXPAND | wxALL, 2);

    // Effect Parameters
    wxBoxSizer* params_sizer = new wxBoxSizer(wxVERTICAL);
    params_sizer->Add(new wxStaticText(m_lower_pane, wxID_ANY, "Effect Controls"), 0, wxALL, 2);
    m_fx_params_group = new wxScrolledWindow(m_lower_pane, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    m_fx_params_group->SetScrollRate(0, 20);
    params_sizer->Add(m_fx_params_group, 1, wxEXPAND | wxALL, 2);
    lower_sizer->Add(params_sizer, 1, wxEXPAND | wxALL, 2);

    m_lower_pane->SetSizer(lower_sizer);

    m_splitter->SplitHorizontally(m_upper_pane, m_lower_pane);
    m_splitter->SetSashGravity(0.5);
    main_sizer->Add(m_splitter, 1, wxEXPAND | wxALL, 0);

    SetSizer(main_sizer);

    update_mixer_ui();
    update_effect_editor();
}

void MixerPanel::update_mixer_ui() {
    m_track_group->DestroyChildren();
    m_track_meters.clear();
    m_bus_meters.clear();

    size_t num_tracks = m_engine.track_count();
    size_t num_buses = m_engine.bus_count();
    
    wxBoxSizer* tracks_sizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Master Selection Button update
    if (m_selected_track == kSelectedMaster) {
        m_master_sel_btn->SetBackgroundColour(wxColour(255, 255, 0));
        m_master_sel_btn->SetForegroundColour(*wxBLACK);
    } else {
        m_master_sel_btn->SetBackgroundColour(wxNullColour);
        m_master_sel_btn->SetForegroundColour(wxNullColour);
    }

    // --- Tracks ---
    for (size_t i = 0; i < num_tracks; ++i) {
        wxPanel* track_panel = new wxPanel(m_track_group, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_THEME);
        wxBoxSizer* track_sizer = new wxBoxSizer(wxVERTICAL);

        wxString track_name;
        track_name.Printf("Track %zu", i + 1);
        wxStaticText* label = new wxStaticText(track_panel, wxID_ANY, track_name);
        track_sizer->Add(label, 0, wxALIGN_CENTER | wxALL, 2);

        wxBoxSizer* track_controls = new wxBoxSizer(wxHORIZONTAL);

        // Volume Slider
        wxSlider* vol = new wxSlider(track_panel, wxID_ANY, 100, 0, 100, wxDefaultPosition, wxSize(-1, 100), wxSL_VERTICAL | wxSL_INVERSE);
        vol->SetValue((int)(m_engine.track(i).volume() * 100));
        vol->Bind(wxEVT_SLIDER, [this, i](wxCommandEvent& ev) {
            float val = (float)ev.GetInt() / 100.0f;
            m_engine.track(i).set_volume(val);
        });
        track_controls->Add(vol, 0, wxEXPAND | wxALL, 2);

        // Meters
        VUMeter* meter_l = new VUMeter(track_panel, wxID_ANY, m_engine, false);
        VUMeter* meter_r = new VUMeter(track_panel, wxID_ANY, m_engine, false);
        meter_l->SetMinSize(wxSize(10, 80));
        meter_r->SetMinSize(wxSize(10, 80));
        track_controls->Add(meter_l, 1, wxEXPAND | wxALL, 1);
        track_controls->Add(meter_r, 1, wxEXPAND | wxALL, 1);
        m_track_meters.push_back({meter_l, meter_r});

        track_sizer->Add(track_controls, 1, wxEXPAND | wxALL, 2);

        // Pan Slider
        wxSlider* pan = new wxSlider(track_panel, wxID_ANY, 0, -100, 100, wxDefaultPosition, wxSize(60, -1), wxSL_HORIZONTAL);
        pan->SetValue((int)(m_engine.track(i).get_pan() * 100));
        pan->Bind(wxEVT_SLIDER, [this, i](wxCommandEvent& ev) {
            float val = (float)ev.GetInt() / 100.0f;
            m_engine.track(i).set_pan(val);
        });
        track_sizer->Add(pan, 0, wxALIGN_CENTER | wxALL, 2);

        // Mute/Solo
        wxBoxSizer* ms_sizer = new wxBoxSizer(wxHORIZONTAL);
        wxCheckBox* mute = new wxCheckBox(track_panel, wxID_ANY, "M");
        mute->SetValue(m_engine.track(i).muted());
        mute->Bind(wxEVT_CHECKBOX, [this, i](wxCommandEvent& ev) {
            m_engine.track(i).set_mute(ev.IsChecked());
        });
        ms_sizer->Add(mute, 0, wxALL, 2);

        wxCheckBox* solo = new wxCheckBox(track_panel, wxID_ANY, "S");
        solo->SetValue(m_engine.track(i).solo());
        solo->Bind(wxEVT_CHECKBOX, [this, i](wxCommandEvent& ev) {
            m_engine.track(i).set_solo(ev.IsChecked());
        });
        ms_sizer->Add(solo, 0, wxALL, 2);
        track_sizer->Add(ms_sizer, 0, wxALIGN_CENTER | wxALL, 2);

        // Select Button
        wxButton* sel_btn = new wxButton(track_panel, wxID_ANY, "SEL", wxDefaultPosition, wxSize(60, 25));
        if (m_selected_track == kSelectedTrackBase + (int)i) {
            sel_btn->SetBackgroundColour(wxColour(255, 255, 0));
            sel_btn->SetForegroundColour(*wxBLACK);
        } else {
            sel_btn->SetBackgroundColour(wxNullColour);
            sel_btn->SetForegroundColour(wxNullColour);
        }
        sel_btn->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent& ev) {
            this->CallAfter([this, i]() {
                m_selected_track = kSelectedTrackBase + (int)i;
                m_selected_fx_slot = -1;
                update_mixer_ui();
                update_effect_editor();
            });
        });
        track_sizer->Add(sel_btn, 0, wxALIGN_CENTER | wxALL, 2);

        track_panel->SetSizer(track_sizer);
        tracks_sizer->Add(track_panel, 0, wxEXPAND | wxALL, 2);
    }

    // --- Buses ---
    for (size_t i = 0; i < num_buses; ++i) {
        wxPanel* bus_panel = new wxPanel(m_track_group, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_THEME);
        wxBoxSizer* bus_sizer = new wxBoxSizer(wxVERTICAL);

        wxString bus_name;
        bus_name.Printf("Bus %zu", i + 1);
        wxStaticText* label = new wxStaticText(bus_panel, wxID_ANY, bus_name);
        bus_sizer->Add(label, 0, wxALIGN_CENTER | wxALL, 2);

        wxBoxSizer* bus_controls = new wxBoxSizer(wxHORIZONTAL);

        // Volume Slider
        wxSlider* vol = new wxSlider(bus_panel, wxID_ANY, 100, 0, 100, wxDefaultPosition, wxSize(-1, 100), wxSL_VERTICAL | wxSL_INVERSE);
        vol->SetValue((int)(m_engine.bus(i).volume() * 100));
        vol->Bind(wxEVT_SLIDER, [this, i](wxCommandEvent& ev) {
            float val = (float)ev.GetInt() / 100.0f;
            m_engine.bus(i).set_volume(val);
        });
        bus_controls->Add(vol, 0, wxEXPAND | wxALL, 2);

        // Meters
        VUMeter* meter_l = new VUMeter(bus_panel, wxID_ANY, m_engine, false);
        VUMeter* meter_r = new VUMeter(bus_panel, wxID_ANY, m_engine, false);
        meter_l->SetMinSize(wxSize(10, 80));
        meter_r->SetMinSize(wxSize(10, 80));
        bus_controls->Add(meter_l, 1, wxEXPAND | wxALL, 1);
        bus_controls->Add(meter_r, 1, wxEXPAND | wxALL, 1);
        m_bus_meters.push_back({meter_l, meter_r});

        bus_sizer->Add(bus_controls, 1, wxEXPAND | wxALL, 2);

        // Pan Slider
        wxSlider* pan = new wxSlider(bus_panel, wxID_ANY, 0, -100, 100, wxDefaultPosition, wxSize(60, -1), wxSL_HORIZONTAL);
        pan->SetValue((int)(m_engine.bus(i).pan() * 100));
        pan->Bind(wxEVT_SLIDER, [this, i](wxCommandEvent& ev) {
            float val = (float)ev.GetInt() / 100.0f;
            m_engine.bus(i).set_pan(val);
        });
        bus_sizer->Add(pan, 0, wxALIGN_CENTER | wxALL, 2);

        // Mute
        wxCheckBox* mute = new wxCheckBox(bus_panel, wxID_ANY, "M");
        mute->SetValue(m_engine.bus(i).muted());
        mute->Bind(wxEVT_CHECKBOX, [this, i](wxCommandEvent& ev) {
            m_engine.bus(i).set_mute(ev.IsChecked());
        });
        bus_sizer->Add(mute, 0, wxALIGN_CENTER | wxALL, 2);

        // Select Button
        wxButton* sel_btn = new wxButton(bus_panel, wxID_ANY, "SEL", wxDefaultPosition, wxSize(60, 25));
        if (m_selected_track == kSelectedBusBase + (int)i) {
            sel_btn->SetBackgroundColour(wxColour(255, 255, 0));
            sel_btn->SetForegroundColour(*wxBLACK);
        } else {
            sel_btn->SetBackgroundColour(wxNullColour);
            sel_btn->SetForegroundColour(wxNullColour);
        }
        sel_btn->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent& ev) {
            this->CallAfter([this, i]() {
                m_selected_track = kSelectedBusBase + (int)i;
                m_selected_fx_slot = -1;
                update_mixer_ui();
                update_effect_editor();
            });
        });
        bus_sizer->Add(sel_btn, 0, wxALIGN_CENTER | wxALL, 2);

        bus_panel->SetSizer(bus_sizer);
        tracks_sizer->Add(bus_panel, 0, wxEXPAND | wxALL, 2);
    }

    m_track_group->SetSizer(tracks_sizer);
    m_track_group->FitInside(); // Update scrollbars
}

void MixerPanel::update_meters() {
    if (m_master_meter_l) m_master_meter_l->level(m_engine.master_meter_l());
    if (m_master_meter_r) m_master_meter_r->level(m_engine.master_meter_r());

    for (size_t i = 0; i < m_engine.track_count() && i < m_track_meters.size(); ++i) {
        auto& meters = m_track_meters[i];
        if (meters.first) meters.first->level(m_engine.track(i).meter_level_l());
        if (meters.second) meters.second->level(m_engine.track(i).meter_level_r());
    }

    for (size_t i = 0; i < m_engine.bus_count() && i < m_bus_meters.size(); ++i) {
        auto& meters = m_bus_meters[i];
        if (meters.first) meters.first->level(m_engine.bus(i).meter_l());
        if (meters.second) meters.second->level(m_engine.bus(i).meter_r());
    }
}

void MixerPanel::update_effect_editor() {
    m_fx_chain_group->DestroyChildren();
    m_fx_params_group->DestroyChildren();

    bool is_master = (m_selected_track == kSelectedMaster);
    bool is_bus = (m_selected_track >= kSelectedBusBase);
    bool is_track = (m_selected_track >= kSelectedTrackBase && m_selected_track < kSelectedBusBase);

    int bus_idx = is_bus ? (m_selected_track - kSelectedBusBase) : -1;
    int track_idx = is_track ? (m_selected_track - kSelectedTrackBase) : -1;

    auto get_fx = [&](size_t idx) -> DSP* {
        if (is_master) return m_engine.m_master.get_effect(idx);
        if (is_bus && bus_idx >= 0 && bus_idx < (int)m_engine.bus_count())
            return m_engine.bus(bus_idx).get_effect(idx);
        if (is_track && track_idx >= 0 && track_idx < (int)m_engine.track_count())
            return m_engine.track(track_idx).get_effect(idx);
        return nullptr;
    };

    wxBoxSizer* chain_sizer = new wxBoxSizer(wxVERTICAL);

    for (size_t i = 0; i < MAX_INSERTS; ++i) {
        DSP* dsp = get_fx(i);
        if (dsp) {
            wxPanel* row = new wxPanel(m_fx_chain_group, wxID_ANY);
            row->SetBackgroundColour(wxColour(50, 50, 50));
            wxBoxSizer* row_sizer = new wxBoxSizer(wxHORIZONTAL);

            wxCheckBox* bypass = new wxCheckBox(row, wxID_ANY, "");
            bypass->SetValue(!dsp->is_bypassed());
            bypass->Bind(wxEVT_CHECKBOX, [this, i, is_master, is_bus, is_track, track_idx, bus_idx](wxCommandEvent& ev) {
                 DSP* d = nullptr;
                 if (is_master) d = m_engine.m_master.get_effect(i);
                 else if (is_bus && bus_idx >= 0 && bus_idx < (int)m_engine.bus_count()) d = m_engine.bus(bus_idx).get_effect(i);
                 else if (is_track && track_idx >= 0 && track_idx < (int)m_engine.track_count()) d = m_engine.track(track_idx).get_effect(i);
                 
                 if(d) d->set_bypass(!ev.IsChecked());
            });
            row_sizer->Add(bypass, 0, wxALL, 2);

            wxButton* sel_btn = new wxButton(row, wxID_ANY, dsp->name(), wxDefaultPosition, wxSize(100, 25));
            if ((int)i == m_selected_fx_slot) {
                sel_btn->SetBackgroundColour(wxColour(255, 255, 0));
                sel_btn->SetForegroundColour(*wxBLACK);
            }
            sel_btn->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent& ev) {
                m_selected_fx_slot = (int)i;
                update_effect_editor();
            });
            row_sizer->Add(sel_btn, 1, wxALL, 2);

            wxButton* up = new wxButton(row, wxID_ANY, "^", wxDefaultPosition, wxSize(25, 25));
            up->Bind(wxEVT_BUTTON, [this, i, is_master, is_bus, is_track, track_idx, bus_idx](wxCommandEvent& ev) {
                if (is_master) m_engine.m_master.move_effect_up(i);
                else if (is_bus && bus_idx >= 0 && bus_idx < (int)m_engine.bus_count()) m_engine.bus(bus_idx).move_effect_up(i);
                else if (is_track && track_idx >= 0 && track_idx < (int)m_engine.track_count()) m_engine.track(track_idx).move_effect_up(i);
                update_effect_editor();
            });
            row_sizer->Add(up, 0, wxALL, 1);

            wxButton* down = new wxButton(row, wxID_ANY, "v", wxDefaultPosition, wxSize(25, 25));
            down->Bind(wxEVT_BUTTON, [this, i, is_master, is_bus, is_track, track_idx, bus_idx](wxCommandEvent& ev) {
                if (is_master) m_engine.m_master.move_effect_down(i);
                else if (is_bus && bus_idx >= 0 && bus_idx < (int)m_engine.bus_count()) m_engine.bus(bus_idx).move_effect_down(i);
                else if (is_track && track_idx >= 0 && track_idx < (int)m_engine.track_count()) m_engine.track(track_idx).move_effect_down(i);
                update_effect_editor();
            });
            row_sizer->Add(down, 0, wxALL, 1);

            wxButton* rem = new wxButton(row, wxID_ANY, "X", wxDefaultPosition, wxSize(25, 25));
            rem->SetForegroundColour(*wxRED);
            rem->Bind(wxEVT_BUTTON, [this, i, is_master, is_bus, is_track, track_idx, bus_idx](wxCommandEvent& ev) {
                if (is_master) m_engine.m_master.remove_effect(i);
                else if (is_bus && bus_idx >= 0 && bus_idx < (int)m_engine.bus_count()) m_engine.bus(bus_idx).remove_effect(i);
                else if (is_track && track_idx >= 0 && track_idx < (int)m_engine.track_count()) m_engine.track(track_idx).remove_effect(i);
                m_selected_fx_slot = -1;
                update_effect_editor();
            });
            row_sizer->Add(rem, 0, wxALL, 1);

            row->SetSizer(row_sizer);
            chain_sizer->Add(row, 0, wxEXPAND | wxALL, 1);
        }
    }
    m_fx_chain_group->SetSizer(chain_sizer);
    m_fx_chain_group->FitInside();

    // --- Build Parameter UI ---
    wxBoxSizer* params_sizer = new wxBoxSizer(wxVERTICAL);
    DSP* dsp = get_fx(m_selected_fx_slot);
    
    if (dsp) {
        wxStaticText* header = new wxStaticText(m_fx_params_group, wxID_ANY, "Editing: " + dsp->name());
        wxFont font = header->GetFont();
        font.SetWeight(wxFONTWEIGHT_BOLD);
        header->SetFont(font);
        params_sizer->Add(header, 0, wxALL, 5);

        // Helper to add sliders
        auto add_slider = [&](const wxString& label, float min, float max, float val, std::function<void(float)> setter) {
            wxBoxSizer* s_row = new wxBoxSizer(wxHORIZONTAL);
            s_row->Add(new wxStaticText(m_fx_params_group, wxID_ANY, label, wxDefaultPosition, wxSize(80, -1)), 0, wxALL, 5);
            
            // Use integer slider for range, map float to it
            int steps = 1000;
            wxSlider* sl = new wxSlider(m_fx_params_group, wxID_ANY, 0, 0, steps, wxDefaultPosition, wxSize(200, -1));
            float normalized = (val - min) / (max - min);
            sl->SetValue((int)(normalized * steps));
            
            sl->Bind(wxEVT_SLIDER, [min, max, steps, setter](wxCommandEvent& ev) {
                float v = (float)ev.GetInt() / (float)steps; // 0..1
                float final_val = min + v * (max - min);
                setter(final_val);
            });
            s_row->Add(sl, 1, wxEXPAND | wxALL, 5);
            params_sizer->Add(s_row, 0, wxEXPAND | wxALL, 2);
        };

        if (auto* g = dynamic_cast<GainDSP*>(dsp)) {
            add_slider("Gain", 0.0f, 2.0f, g->gain, [g](float v){ g->gain = v; });
        } else if (auto* d = dynamic_cast<DelayDSP*>(dsp)) {
            add_slider("Feedback", 0.0f, 0.99f, d->feedback, [d](float v){ d->feedback = v; });
            add_slider("Mix", 0.0f, 1.0f, d->mix, [d](float v){ d->mix = v; });
        } else if (auto* rev = dynamic_cast<ReverbDSP*>(dsp)) {
            add_slider("Room Size", 0.0f, 1.0f, rev->room_size, [rev](float v){ rev->room_size = v; });
            add_slider("Damp", 0.0f, 1.0f, rev->damp, [rev](float v){ rev->damp = v; });
            add_slider("Mix", 0.0f, 1.0f, rev->mix, [rev](float v){ rev->mix = v; });
        } else if (auto* lim = dynamic_cast<LimiterDSP*>(dsp)) {
            add_slider("Ceiling", 0.0f, 1.0f, lim->ceiling, [lim](float v){ lim->ceiling = v; });
            add_slider("Threshold", 0.0f, 1.0f, lim->threshold, [lim](float v){ lim->threshold = v; });
        } else if (auto* exc = dynamic_cast<ExciterDSP*>(dsp)) {
            add_slider("Amount", 0.0f, 1.0f, exc->amount, [exc](float v){ exc->amount = v; });
            add_slider("Freq", 0.0f, 1.0f, exc->freq, [exc](float v){ exc->freq = v; });
        } else if (auto* pha = dynamic_cast<PhaserDSP*>(dsp)) {
            add_slider("Rate", 0.0f, 1.0f, pha->rate, [pha](float v){ pha->rate = v; });
            add_slider("Depth", 0.0f, 1.0f, pha->depth, [pha](float v){ pha->depth = v; });
            add_slider("Feedback", 0.0f, 1.0f, pha->feedback, [pha](float v){ pha->feedback = v; });
            add_slider("Mix", 0.0f, 1.0f, pha->mix, [pha](float v){ pha->mix = v; });
        } else if (auto* fla = dynamic_cast<FlangerDSP*>(dsp)) {
            add_slider("Rate", 0.0f, 1.0f, fla->rate, [fla](float v){ fla->rate = v; });
            add_slider("Depth", 0.0f, 1.0f, fla->depth, [fla](float v){ fla->depth = v; });
            add_slider("Feedback", 0.0f, 1.0f, fla->feedback, [fla](float v){ fla->feedback = v; });
            add_slider("Mix", 0.0f, 1.0f, fla->mix, [fla](float v){ fla->mix = v; });
        } else if (auto* ech = dynamic_cast<EchoDSP*>(dsp)) {
            add_slider("Time", 0.0f, 1.0f, ech->time, [ech](float v){ ech->time = v; });
            add_slider("Feedback", 0.0f, 1.0f, ech->feedback, [ech](float v){ ech->feedback = v; });
            add_slider("Damp", 0.0f, 1.0f, ech->damp, [ech](float v){ ech->damp = v; });
            add_slider("Mix", 0.0f, 1.0f, ech->mix, [ech](float v){ ech->mix = v; });
        } else if (auto* cmp = dynamic_cast<CompressorDSP*>(dsp)) {
            add_slider("Threshold", 0.0f, 1.0f, cmp->threshold, [cmp](float v){ cmp->threshold = v; });
            add_slider("Ratio", 1.0f, 20.0f, cmp->ratio, [cmp](float v){ cmp->ratio = v; });
            add_slider("Attack", 0.001f, 0.5f, cmp->attack, [cmp](float v){ cmp->attack = v; });
            add_slider("Release", 0.01f, 2.0f, cmp->release, [cmp](float v){ cmp->release = v; });
            add_slider("Makeup", 0.0f, 2.0f, cmp->makeup, [cmp](float v){ cmp->makeup = v; });
        } else if (auto* geq = dynamic_cast<GraphicalEQDSP*>(dsp)) {
            // Graphical EQ needs vertical sliders
            wxBoxSizer* eq_sizer = new wxBoxSizer(wxHORIZONTAL);
            for (int b = 0; b < 12; ++b) {
                wxBoxSizer* band_sizer = new wxBoxSizer(wxVERTICAL);
                wxSlider* s = new wxSlider(m_fx_params_group, wxID_ANY, 0, -120, 120, wxDefaultPosition, wxSize(-1, 100), wxSL_VERTICAL | wxSL_INVERSE);
                s->SetValue((int)(geq->get_band_gain(b) * 10.0f));
                s->Bind(wxEVT_SLIDER, [geq, b](wxCommandEvent& ev){
                    geq->set_band_gain(b, (float)ev.GetInt() / 10.0f);
                });
                band_sizer->Add(s, 1, wxEXPAND | wxALL, 2);
                
                wxString lbl;
                float f = geq->get_band_freq(b);
                if (f >= 1000) lbl.Printf("%dk", (int)(f/1000));
                else lbl.Printf("%d", (int)f);
                wxStaticText* t = new wxStaticText(m_fx_params_group, wxID_ANY, lbl);
                wxFont font = t->GetFont(); font.SetPointSize(8); t->SetFont(font);
                band_sizer->Add(t, 0, wxALIGN_CENTER | wxALL, 1);
                
                eq_sizer->Add(band_sizer, 1, wxEXPAND | wxALL, 2);
            }
            params_sizer->Add(eq_sizer, 0, wxEXPAND | wxALL, 2);
        } else if (auto* cab = dynamic_cast<CabinetDSP*>(dsp)) {
            add_slider("Low Cut", 20.0f, 500.0f, cab->low_cut, [cab](float v){ cab->low_cut = v; });
            add_slider("High Cut", 1000.0f, 15000.0f, cab->high_cut, [cab](float v){ cab->high_cut = v; });
            add_slider("Peak Freq", 500.0f, 8000.0f, cab->peak_freq, [cab](float v){ cab->peak_freq = v; });
            add_slider("Peak Gain", -12.0f, 12.0f, cab->peak_gain, [cab](float v){ cab->peak_gain = v; });
        } else if (auto* dis = dynamic_cast<DistortionDSP*>(dsp)) {
            add_slider("Drive", 0.0f, 1.0f, dis->drive, [dis](float v){ dis->drive = v; });
            add_slider("Mix", 0.0f, 1.0f, dis->mix, [dis](float v){ dis->mix = v; });
        } else if (auto* cho = dynamic_cast<ChorusDSP*>(dsp)) {
            add_slider("Rate", 0.0f, 1.0f, cho->rate, [cho](float v){ cho->rate = v; });
            add_slider("Depth", 0.0f, 1.0f, cho->depth, [cho](float v){ cho->depth = v; });
            add_slider("Feedback", 0.0f, 1.0f, cho->feedback, [cho](float v){ cho->feedback = v; });
            add_slider("Mix", 0.0f, 1.0f, cho->mix, [cho](float v){ cho->mix = v; });
        } else if (auto* exp = dynamic_cast<StereoExpanderDSP*>(dsp)) {
            add_slider("Width", 0.0f, 2.0f, exp->width, [exp](float v){ exp->width = v; });
        } else if (auto* rmo = dynamic_cast<RingModulatorDSP*>(dsp)) {
            add_slider("Freq", 0.0f, 1.0f, rmo->freq, [rmo](float v){ rmo->freq = v; });
            add_slider("Mix", 0.0f, 1.0f, rmo->mix, [rmo](float v){ rmo->mix = v; });
        } else if (auto* gate = dynamic_cast<GateDSP*>(dsp)) {
            add_slider("Threshold", 0.0f, 1.0f, gate->threshold, [gate](float v){ gate->threshold = v; });
            add_slider("Range", 0.0f, 1.0f, gate->range, [gate](float v){ gate->range = v; });
            add_slider("Attack", 0.001f, 0.5f, gate->attack, [gate](float v){ gate->attack = v; });
            add_slider("Release", 0.01f, 2.0f, gate->release, [gate](float v){ gate->release = v; });
        }
    } else {
        params_sizer->Add(new wxStaticText(m_fx_params_group, wxID_ANY, "No effect selected"), 0, wxALL, 5);
    }

    m_fx_params_group->SetSizer(params_sizer);
    m_fx_params_group->FitInside();
}

void MixerPanel::on_master_gain(wxCommandEvent& event) {
    float gain = event.GetInt() / 100.0f;
    m_engine.set_master_gain(gain);
}

void MixerPanel::on_master_mute(wxCommandEvent& event) {
    m_engine.m_master.set_mute(event.IsChecked());
}

void MixerPanel::on_track_volume(wxCommandEvent& event) {}
void MixerPanel::on_track_pan(wxCommandEvent& event) {}
void MixerPanel::on_track_mute(wxCommandEvent& event) {}
void MixerPanel::on_track_solo(wxCommandEvent& event) {}
void MixerPanel::on_track_select(wxCommandEvent& event) {
    // This is a generic handler, but we use specific Bind() calls for tracks/buses.
    // If we get here, it might be an unhandled event.
    event.Skip();
}

void MixerPanel::on_bus_volume(wxCommandEvent& event) {}
void MixerPanel::on_bus_pan(wxCommandEvent& event) {}
void MixerPanel::on_bus_mute(wxCommandEvent& event) {}
void MixerPanel::on_bus_select(wxCommandEvent& event) {}

void MixerPanel::on_add_fx(wxCommandEvent& event) {
    int sel = m_avail_fx_browser->GetSelection();
    if (sel == wxNOT_FOUND) return;
    wxString fx_name = m_avail_fx_browser->GetString(sel);

    bool is_master = (m_selected_track == kSelectedMaster);
    bool is_bus = (m_selected_track >= kSelectedBusBase);
    bool is_track = (m_selected_track >= kSelectedTrackBase && m_selected_track < kSelectedBusBase);
    
    int bus_idx = is_bus ? (m_selected_track - kSelectedBusBase) : -1;
    int track_idx = is_track ? (m_selected_track - kSelectedTrackBase) : -1;

    auto get_fx_at = [&](size_t idx) -> DSP* {
        if (is_master) return m_engine.m_master.get_effect(idx);
        if (is_bus && bus_idx >= 0 && bus_idx < (int)m_engine.bus_count()) return m_engine.bus(bus_idx).get_effect(idx);
        if (is_track && track_idx >= 0 && track_idx < (int)m_engine.track_count()) return m_engine.track(track_idx).get_effect(idx);
        return nullptr;
    };
    auto set_fx_at = [&](size_t idx, std::unique_ptr<DSP> dsp) {
        if (is_master) { 
            m_engine.m_master.set_effect(idx, std::move(dsp)); 
            m_engine.m_master.enable_effect(idx, true); 
        } else if (is_bus && bus_idx >= 0 && bus_idx < (int)m_engine.bus_count()) {
             m_engine.bus(bus_idx).set_effect(idx, std::move(dsp)); 
             m_engine.bus(bus_idx).enable_effect(idx, true);
        } else if (is_track && track_idx >= 0 && track_idx < (int)m_engine.track_count()) {
             m_engine.track(track_idx).set_effect(idx, std::move(dsp)); 
             m_engine.track(track_idx).enable_effect(idx, true);
        }
    };

    int slot = -1;
    for (size_t i = 0; i < MAX_INSERTS; ++i) {
        if (!get_fx_at(i)) {
            slot = (int)i;
            break;
        }
    }
    if (slot == -1) return;

    std::unique_ptr<DSP> dsp;
    if (fx_name == "Gain") dsp = std::make_unique<GainDSP>();
    else if (fx_name == "Delay") dsp = std::make_unique<DelayDSP>();
    else if (fx_name == "Reverb") dsp = std::make_unique<ReverbDSP>();
    else if (fx_name == "Limiter") dsp = std::make_unique<LimiterDSP>();
    else if (fx_name == "Exciter") dsp = std::make_unique<ExciterDSP>();
    else if (fx_name == "Phaser") dsp = std::make_unique<PhaserDSP>();
    else if (fx_name == "Flanger") dsp = std::make_unique<FlangerDSP>();
    else if (fx_name == "Echo") dsp = std::make_unique<EchoDSP>();
    else if (fx_name == "Compressor") dsp = std::make_unique<CompressorDSP>();
    else if (fx_name == "Graphical EQ") dsp = std::make_unique<GraphicalEQDSP>();
    else if (fx_name == "Cabinet") dsp = std::make_unique<CabinetDSP>();
    else if (fx_name == "Distortion") dsp = std::make_unique<DistortionDSP>();
    else if (fx_name == "Chorus") dsp = std::make_unique<ChorusDSP>();
    else if (fx_name == "Stereo Expander") dsp = std::make_unique<StereoExpanderDSP>();
    else if (fx_name == "Ring Modulator") dsp = std::make_unique<RingModulatorDSP>();
    else if (fx_name == "Gate") dsp = std::make_unique<GateDSP>();

    if (dsp) {
        set_fx_at(slot, std::move(dsp));
        m_selected_fx_slot = slot;
        update_effect_editor();
    }
}

void MixerPanel::on_fx_up(wxCommandEvent& event) {}
void MixerPanel::on_fx_down(wxCommandEvent& event) {}
void MixerPanel::on_fx_remove(wxCommandEvent& event) {}
void MixerPanel::on_fx_bypass(wxCommandEvent& event) {}

void MixerPanel::on_save_chain(wxCommandEvent& event) {
    wxFileDialog dlg(this, "Save Chain", "", "", "Chain files (*.chain)|*.chain", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        bool is_master = (m_selected_track == kSelectedMaster);
        bool is_bus = (m_selected_track >= kSelectedBusBase);
        bool is_track = (m_selected_track >= kSelectedTrackBase && m_selected_track < kSelectedBusBase);

        if (is_master) m_engine.m_master.save_effect_chain(dlg.GetPath().ToStdString());
        else if (is_bus) {
            int idx = m_selected_track - kSelectedBusBase;
            if (idx >= 0 && idx < (int)m_engine.bus_count())
                m_engine.bus(idx).save_effect_chain(dlg.GetPath().ToStdString());
        }
        else if (is_track) {
            int idx = m_selected_track - kSelectedTrackBase;
            if (idx >= 0 && idx < (int)m_engine.track_count())
                m_engine.track(idx).save_effect_chain(dlg.GetPath().ToStdString());
        }
    }
}

void MixerPanel::on_load_chain(wxCommandEvent& event) {
    wxFileDialog dlg(this, "Load Chain", "", "", "Chain files (*.chain)|*.chain", wxFD_OPEN);
    if (dlg.ShowModal() == wxID_OK) {
        bool is_master = (m_selected_track == kSelectedMaster);
        bool is_bus = (m_selected_track >= kSelectedBusBase);
        bool is_track = (m_selected_track >= kSelectedTrackBase && m_selected_track < kSelectedBusBase);

        if (is_master) m_engine.m_master.load_effect_chain(dlg.GetPath().ToStdString());
        else if (is_bus) {
            int idx = m_selected_track - kSelectedBusBase;
            if (idx >= 0 && idx < (int)m_engine.bus_count())
                m_engine.bus(idx).load_effect_chain(dlg.GetPath().ToStdString());
        }
        else if (is_track) {
            int idx = m_selected_track - kSelectedTrackBase;
            if (idx >= 0 && idx < (int)m_engine.track_count())
                m_engine.track(idx).load_effect_chain(dlg.GetPath().ToStdString());
        }
        m_selected_fx_slot = -1;
        update_effect_editor();
    }
}

} // namespace disgrace_ns
