#include "wx_transportbar.h"
#include "wx_vu_meter.h"
#include "wx_main_window.h"
#include "../core/engine.h"

#include <wx/sizer.h>
#include <cstdio>

namespace disgrace_ns {

enum {
    ID_PLAY = wxID_HIGHEST + 1,
    ID_STOP,
    ID_RECORD,
    ID_METRONOME,
    ID_LOOP,
    ID_TEMPO,
    ID_LPB,
    ID_OCTAVE,
    ID_STEP
};

wxBEGIN_EVENT_TABLE(TransportBar, wxPanel)
    EVT_BUTTON(ID_PLAY, TransportBar::on_play)
    EVT_BUTTON(ID_STOP, TransportBar::on_stop)
    EVT_TOGGLEBUTTON(ID_RECORD, TransportBar::on_record)
    EVT_TOGGLEBUTTON(ID_METRONOME, TransportBar::on_metronome)
    EVT_TOGGLEBUTTON(ID_LOOP, TransportBar::on_loop)
    EVT_SPINCTRL(ID_TEMPO, TransportBar::on_tempo)
    EVT_SPINCTRL(ID_LPB, TransportBar::on_lpb)
    EVT_SPINCTRL(ID_OCTAVE, TransportBar::on_octave)
    EVT_SPINCTRL(ID_STEP, TransportBar::on_step)
wxEND_EVENT_TABLE()

TransportBar::TransportBar(wxWindow* parent, wxWindowID id, Engine& engine)
    : wxPanel(parent, id), m_engine(engine)
{
    SetSizeHints(wxDefaultCoord, wxDefaultCoord, -1, 40);

    wxBoxSizer* main_sizer = new wxBoxSizer(wxHORIZONTAL);
    main_sizer->SetMinSize(wxSize(-1, 40));

    int btn_w = 60;
    int btn_h = 25;
    int btn_spacing = 4;

    m_play = new wxButton(this, ID_PLAY, "Play", wxDefaultPosition, wxSize(btn_w, btn_h));
    main_sizer->Add(m_play, 0, wxALL, btn_spacing);

    m_stop = new wxButton(this, ID_STOP, "Stop", wxDefaultPosition, wxSize(btn_w, btn_h));
    main_sizer->Add(m_stop, 0, wxALL, btn_spacing);

    m_record = new wxToggleButton(this, ID_RECORD, "Edit", wxDefaultPosition, wxSize(btn_w, btn_h));
    main_sizer->Add(m_record, 0, wxALL, btn_spacing);

    m_loop = new wxToggleButton(this, ID_LOOP, "Loop", wxDefaultPosition, wxSize(btn_w, btn_h));
    m_loop->SetValue(true);
    main_sizer->Add(m_loop, 0, wxALL, btn_spacing);

    m_metronome = new wxToggleButton(this, ID_METRONOME, "Metro", wxDefaultPosition, wxSize(btn_w + 10, btn_h));
    m_metronome->SetValue(true);
    main_sizer->Add(m_metronome, 0, wxALL, btn_spacing);

    int counter_w = 35;
    int label_w = 35;

    wxStaticText* tempo_label = new wxStaticText(this, wxID_ANY, "BPM", wxDefaultPosition, wxSize(label_w, btn_h));
    main_sizer->Add(tempo_label, 0, wxALIGN_CENTER_VERTICAL | wxALL, btn_spacing);

    m_tempo_spin = new wxSpinCtrl(this, ID_TEMPO, wxEmptyString, wxDefaultPosition, wxSize(counter_w + 30, btn_h));
    m_tempo_spin->SetRange(30, 300);
    m_tempo_spin->SetValue((int)m_engine.tempo());
    main_sizer->Add(m_tempo_spin, 0, wxALIGN_CENTER_VERTICAL | wxALL, btn_spacing);

    wxStaticText* lpb_label = new wxStaticText(this, wxID_ANY, "LPB", wxDefaultPosition, wxSize(label_w, btn_h));
    main_sizer->Add(lpb_label, 0, wxALIGN_CENTER_VERTICAL | wxALL, btn_spacing);

    m_lpb_spin = new wxSpinCtrl(this, ID_LPB, wxEmptyString, wxDefaultPosition, wxSize(counter_w + 30, btn_h));
    m_lpb_spin->SetRange(1, 128);
    m_lpb_spin->SetValue((int)m_engine.lpb());
    main_sizer->Add(m_lpb_spin, 0, wxALIGN_CENTER_VERTICAL | wxALL, btn_spacing);

    wxStaticText* oct_label = new wxStaticText(this, wxID_ANY, "Oct", wxDefaultPosition, wxSize(label_w, btn_h));
    main_sizer->Add(oct_label, 0, wxALIGN_CENTER_VERTICAL | wxALL, btn_spacing);

    m_octave_spin = new wxSpinCtrl(this, ID_OCTAVE, wxEmptyString, wxDefaultPosition, wxSize(counter_w + 30, btn_h));
    m_octave_spin->SetRange(0, 9);
    m_octave_spin->SetValue((int)m_engine.base_octave());
    main_sizer->Add(m_octave_spin, 0, wxALIGN_CENTER_VERTICAL | wxALL, btn_spacing);

    wxStaticText* step_label = new wxStaticText(this, wxID_ANY, "Step", wxDefaultPosition, wxSize(label_w, btn_h));
    main_sizer->Add(step_label, 0, wxALIGN_CENTER_VERTICAL | wxALL, btn_spacing);

    m_step_spin = new wxSpinCtrl(this, ID_STEP, wxEmptyString, wxDefaultPosition, wxSize(counter_w + 30, btn_h));
    m_step_spin->SetRange(1, 64);
    m_step_spin->SetValue(1);
    main_sizer->Add(m_step_spin, 0, wxALIGN_CENTER_VERTICAL | wxALL, btn_spacing);

    m_status = new wxStaticText(this, wxID_ANY, "Stopped", wxDefaultPosition, wxSize(70, btn_h));
    wxFont status_font = m_status->GetFont();
    status_font.SetWeight(wxFONTWEIGHT_BOLD);
    m_status->SetFont(status_font);
    main_sizer->Add(m_status, 0, wxALIGN_CENTER_VERTICAL | wxALL, btn_spacing);

    m_clock = new wxStaticText(this, wxID_ANY, "00:00.000", wxDefaultPosition, wxSize(100, btn_h));
    wxFont clock_font = m_clock->GetFont();
    clock_font.SetFamily(wxFONTFAMILY_TELETYPE);
    clock_font.SetWeight(wxFONTWEIGHT_BOLD);
    clock_font.SetPointSize(12);
    m_clock->SetFont(clock_font);
    m_clock->SetForegroundColour(wxColour(0, 0, 0));
    main_sizer->Add(m_clock, 0, wxALIGN_CENTER_VERTICAL | wxALL, btn_spacing);

    wxBoxSizer* meter_stack = new wxBoxSizer(wxVERTICAL);
    
    wxBoxSizer* l_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* l_lbl = new wxStaticText(this, wxID_ANY, "L");
    wxFont mini_font = l_lbl->GetFont(); mini_font.SetPointSize(8); l_lbl->SetFont(mini_font);
    l_row->Add(l_lbl, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
    m_meter_l = new VUMeter(this, wxID_ANY, m_engine, true);
    m_meter_l->SetMinSize(wxSize(120, 8));
    l_row->Add(m_meter_l, 1, wxEXPAND);
    meter_stack->Add(l_row, 1, wxEXPAND | wxBOTTOM, 1);

    wxBoxSizer* r_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* r_lbl = new wxStaticText(this, wxID_ANY, "R");
    r_lbl->SetFont(mini_font);
    r_row->Add(r_lbl, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
    m_meter_r = new VUMeter(this, wxID_ANY, m_engine, true);
    m_meter_r->SetMinSize(wxSize(120, 8));
    r_row->Add(m_meter_r, 1, wxEXPAND);
    meter_stack->Add(r_row, 1, wxEXPAND);

    main_sizer->Add(meter_stack, 0, wxALIGN_CENTER_VERTICAL | wxALL, btn_spacing);

    m_clock_str = "00:00.000";

    SetSizer(main_sizer);
}

void TransportBar::on_play(wxCommandEvent& event) {
    m_engine.play();
}

void TransportBar::on_stop(wxCommandEvent& event) {
    m_engine.stop();
}

void TransportBar::on_record(wxCommandEvent& event) {
    m_engine.enable_record(m_record->GetValue());
}

void TransportBar::on_metronome(wxCommandEvent& event) {
    m_engine.set_metronome_enabled(m_metronome->GetValue());
}

void TransportBar::on_loop(wxCommandEvent& event) {
    m_engine.set_loop(m_loop->GetValue());
}

void TransportBar::on_tempo(wxSpinEvent& event) {
    m_engine.set_tempo(m_tempo_spin->GetValue());
}

void TransportBar::on_lpb(wxSpinEvent& event) {
    m_engine.set_lpb(m_lpb_spin->GetValue());
}

void TransportBar::on_octave(wxSpinEvent& event) {
    m_engine.set_base_octave(m_octave_spin->GetValue());
}

void TransportBar::on_step(wxSpinEvent& event) {
    m_engine.set_step_size(m_step_spin->GetValue());
}

void TransportBar::update() {
    m_tempo_spin->SetValue((int)m_engine.tempo());
    m_lpb_spin->SetValue((int)m_engine.lpb());
    m_octave_spin->SetValue(m_engine.base_octave());
    m_step_spin->SetValue((int)m_engine.step_size());
    m_record->SetValue(m_engine.m_record_enabled);

    auto state = m_engine.transport_state();
    switch (state) {
        case TransportState::Stopped:
            m_status->SetLabel("Stopped");
            break;
        case TransportState::Playing:
            m_status->SetLabel("Playing");
            break;
    }

    double total_seconds = 0;
    if (state == TransportState::Stopped) {
        WxMainWindow* main_win = dynamic_cast<WxMainWindow*>(GetParent());
        if (main_win) {
            total_seconds = m_engine.get_time_at_row(main_win->get_cursor_row());
        } else {
            total_seconds = m_engine.get_current_time_seconds();
        }
    } else {
        total_seconds = m_engine.get_current_time_seconds();
    }

    int minutes = (int)(total_seconds / 60);
    double seconds = total_seconds - (minutes * 60);

    m_clock_str.Printf("%02d:%06.3f", minutes, seconds);
    m_clock->SetLabel(m_clock_str);

    if (m_meter_l) m_meter_l->level(m_engine.master_meter_l());
    if (m_meter_r) m_meter_r->level(m_engine.master_meter_r());

    Refresh();
}

void TransportBar::resize(int w, int h) {
    SetSize(w, h);
}

} // namespace disgrace_ns
