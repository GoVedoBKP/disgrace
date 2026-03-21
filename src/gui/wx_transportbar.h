#pragma once

#include <wx/wxprec.h>
#include <wx/panel.h>
#include <wx/slider.h>
#include <wx/button.h>
#include <wx/tglbtn.h>
#include <wx/stattext.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/sizer.h>

#include "../core/transport.h"

namespace disgrace_ns {

class Engine;

class TransportBar : public wxPanel {
public:
    TransportBar(wxWindow* parent, wxWindowID id, Engine& engine);

    void update();
    void resize(int w, int h);

private:
    void on_play(wxCommandEvent& event);
    void on_stop(wxCommandEvent& event);
    void on_record(wxCommandEvent& event);
    void on_metronome(wxCommandEvent& event);
    void on_loop(wxCommandEvent& event);
    void on_tempo(wxSpinEvent& event);
    void on_lpb(wxSpinEvent& event);
    void on_octave(wxSpinEvent& event);
    void on_step(wxSpinEvent& event);

    Engine& m_engine;

    wxButton* m_play;
    wxButton* m_stop;
    wxToggleButton* m_record;
    wxToggleButton* m_metronome;
    wxToggleButton* m_loop;
    wxSpinCtrl* m_tempo_spin;
    wxSpinCtrl* m_lpb_spin;
    wxSpinCtrl* m_octave_spin;
    wxSpinCtrl* m_step_spin;
    wxStaticText* m_status;
    wxStaticText* m_clock;
    class VUMeter* m_meter_l;
    class VUMeter* m_meter_r;

    wxString m_clock_str;

    wxDECLARE_EVENT_TABLE();
};

} // namespace disgrace_ns
