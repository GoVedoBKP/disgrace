#include "wx_instrument_panel.h"
#include "wx_waveform_view.h"
#include "wx_main_window.h"
#include "../core/engine.h"
#include "../instrument/sample_instrument.h"
#include "../instrument/soundfont_instrument.h"
#include "../instrument/dssi_instrument.h"
#include "../instrument/midi_instrument.h"
#include "../io/audio_file.h"

#include <wx/sizer.h>
#include <wx/msgdlg.h>
#include <wx/filedlg.h>
#include <wx/textdlg.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <dlfcn.h>
#include <algorithm>

namespace disgrace_ns {

InstrumentPanel::InstrumentPanel(wxWindow* parent, Engine& engine)
    : wxPanel(parent, wxID_ANY), m_engine(engine)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxHORIZONTAL);

    // --- Left Panel: Instrument List ---
    m_left_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(320, -1));
    wxBoxSizer* left_sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* top_btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_detach_btn = new wxButton(m_left_panel, wxID_ANY, "[]", wxDefaultPosition, wxSize(30, 20));
    m_detach_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_detach, this);
    top_btn_sizer->Add(new wxStaticText(m_left_panel, wxID_ANY, "Instruments"), 1, wxALL, 2);
    top_btn_sizer->Add(m_detach_btn, 0, wxALL, 2);
    left_sizer->Add(top_btn_sizer, 0, wxEXPAND | wxALL, 2);

    wxBoxSizer* btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_new_btn = new wxButton(m_left_panel, wxID_ANY, "New", wxDefaultPosition, wxSize(60, 25));
    m_new_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_new, this);
    m_load_btn = new wxButton(m_left_panel, wxID_ANY, "Load", wxDefaultPosition, wxSize(60, 25));
    m_load_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_load, this);
    m_save_btn = new wxButton(m_left_panel, wxID_ANY, "Save", wxDefaultPosition, wxSize(60, 25));
    m_save_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_save, this);
    m_delete_btn = new wxButton(m_left_panel, wxID_ANY, "Del", wxDefaultPosition, wxSize(60, 25));
    m_delete_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_delete, this);
    btn_sizer->Add(m_new_btn, 0, wxALL, 2);
    btn_sizer->Add(m_load_btn, 0, wxALL, 2);
    btn_sizer->Add(m_save_btn, 0, wxALL, 2);
    btn_sizer->Add(m_delete_btn, 0, wxALL, 2);
    left_sizer->Add(btn_sizer, 0, wxEXPAND | wxALL, 2);

    m_inst_scroll = new wxScrolledWindow(m_left_panel, wxID_ANY, wxDefaultPosition, wxSize(-1, 200), wxVSCROLL);
    m_inst_scroll->SetScrollRate(0, 20);
    m_inst_scroll->SetSizer(new wxBoxSizer(wxVERTICAL));
    left_sizer->Add(m_inst_scroll, 0, wxEXPAND | wxALL, 2);

    m_file_browser = new wxFileCtrl(m_left_panel, wxID_ANY, wxEmptyString, wxEmptyString, wxEmptyString, wxFC_DEFAULT_STYLE, wxDefaultPosition, wxSize(-1, -1));
    left_sizer->Add(m_file_browser, 1, wxEXPAND | wxALL, 2);

    m_left_panel->SetSizer(left_sizer);
    main_sizer->Add(m_left_panel, 0, wxEXPAND | wxALL, 2);

    // --- Right Panel: Editors ---
    m_right_panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* right_sizer = new wxBoxSizer(wxVERTICAL);

    // 1. Sampler Editor
    m_sampler_editor = new wxPanel(m_right_panel, wxID_ANY);
    wxBoxSizer* sampler_sizer = new wxBoxSizer(wxVERTICAL);

    // Sample List / Recording Controls
    wxBoxSizer* top_sampler_sizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Sample List
    m_sample_list_grp = new wxPanel(m_sampler_editor, wxID_ANY, wxDefaultPosition, wxSize(220, -1), wxBORDER_THEME);
    wxBoxSizer* sample_list_sizer = new wxBoxSizer(wxVERTICAL);
    m_add_sample_btn = new wxButton(m_sample_list_grp, wxID_ANY, "Add Sample");
    m_add_sample_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_add_sample, this);
    sample_list_sizer->Add(m_add_sample_btn, 0, wxEXPAND | wxALL, 2);
    
    m_sample_scroll = new wxScrolledWindow(m_sample_list_grp, wxID_ANY, wxDefaultPosition, wxSize(-1, 200), wxVSCROLL);
    m_sample_scroll->SetScrollRate(0, 20);
    m_sample_scroll->SetSizer(new wxBoxSizer(wxVERTICAL));
    sample_list_sizer->Add(m_sample_scroll, 1, wxEXPAND | wxALL, 0);
    m_sample_list_grp->SetSizer(sample_list_sizer);
    top_sampler_sizer->Add(m_sample_list_grp, 0, wxEXPAND | wxALL, 2);

    // Recording / Playback
    wxPanel* rec_panel = new wxPanel(m_sampler_editor, wxID_ANY);
    wxBoxSizer* rec_sizer = new wxBoxSizer(wxVERTICAL);
    
    wxBoxSizer* playback_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_sample_play_btn = new wxButton(rec_panel, wxID_ANY, "Play", wxDefaultPosition, wxSize(60, 25));
    m_sample_play_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_sample_play, this);
    m_sample_stop_btn = new wxButton(rec_panel, wxID_ANY, "Stop", wxDefaultPosition, wxSize(60, 25));
    m_sample_stop_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_sample_stop, this);
    m_rec_btn = new wxToggleButton(rec_panel, wxID_ANY, "Record", wxDefaultPosition, wxSize(80, 25));
    m_rec_btn->Bind(wxEVT_TOGGLEBUTTON, &InstrumentPanel::on_record_sample, this);
    playback_sizer->Add(m_sample_play_btn, 0, wxALL, 2);
    playback_sizer->Add(m_sample_stop_btn, 0, wxALL, 2);
    playback_sizer->Add(m_rec_btn, 0, wxALL, 2);
    rec_sizer->Add(playback_sizer, 0, wxALL, 2);

    wxBoxSizer* rec_cfg_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_rec_mode_ch = new wxChoice(rec_panel, wxID_ANY);
    m_rec_mode_ch->Append("Free"); m_rec_mode_ch->Append("Synced"); m_rec_mode_ch->SetSelection(0);
    
    m_rec_input_ch = new wxChoice(rec_panel, wxID_ANY);
    m_mono_btn = new wxCheckBox(rec_panel, wxID_ANY, "Mono");
    m_mono_btn->Bind(wxEVT_CHECKBOX, &InstrumentPanel::on_mono_toggle, this);
    
    rec_cfg_sizer->Add(m_rec_mode_ch, 0, wxALL, 2);
    rec_cfg_sizer->Add(m_rec_input_ch, 0, wxALL, 2);
    rec_cfg_sizer->Add(m_mono_btn, 0, wxALL, 2);
    rec_sizer->Add(rec_cfg_sizer, 0, wxALL, 2);

    m_sample_fmt_ch = new wxChoice(rec_panel, wxID_ANY);
    m_sample_fmt_ch->Append("Stereo");
    m_sample_fmt_ch->Append("Stereo -> Mono (L)");
    m_sample_fmt_ch->Append("Stereo -> Mono (R)");
    m_sample_fmt_ch->Append("Stereo -> Mono (Mix)");
    m_sample_fmt_ch->Append("Mono -> Stereo");
    m_sample_fmt_ch->SetSelection(0);
    m_sample_fmt_ch->Bind(wxEVT_CHOICE, &InstrumentPanel::on_sample_fmt, this);
    rec_sizer->Add(m_sample_fmt_ch, 0, wxALL, 2);

    top_sampler_sizer->Add(rec_panel, 1, wxEXPAND | wxALL, 2);
    sampler_sizer->Add(top_sampler_sizer, 0, wxEXPAND | wxALL, 2);

    // Waveform View
    m_waveform_view = new WaveformView(m_sampler_editor, wxID_ANY, m_engine);
    m_waveform_view->SetMinSize(wxSize(-1, 250));
    sampler_sizer->Add(m_waveform_view, 1, wxEXPAND | wxALL, 2);

    // Waveform Controls
    wxBoxSizer* wf_ctrl_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_zoom_in_btn = new wxButton(m_sampler_editor, wxID_ANY, "Zoom In", wxDefaultPosition, wxSize(70, 25));
    m_zoom_in_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_zoom_in, this);
    m_zoom_out_btn = new wxButton(m_sampler_editor, wxID_ANY, "Zoom Out", wxDefaultPosition, wxSize(70, 25));
    m_zoom_out_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_zoom_out, this);
    m_view_all_btn = new wxButton(m_sampler_editor, wxID_ANY, "View All", wxDefaultPosition, wxSize(70, 25));
    m_view_all_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_view_all, this);
    m_view_sel_btn = new wxButton(m_sampler_editor, wxID_ANY, "View Sel", wxDefaultPosition, wxSize(70, 25));
    m_view_sel_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_view_sel, this);
    
    m_view_mode_ch = new wxChoice(m_sampler_editor, wxID_ANY);
    m_view_mode_ch->Append("Both"); m_view_mode_ch->Append("Left"); m_view_mode_ch->Append("Right");
    m_view_mode_ch->SetSelection(0);
    m_view_mode_ch->Bind(wxEVT_CHOICE, &InstrumentPanel::on_view_mode, this);

    wf_ctrl_sizer->Add(m_zoom_in_btn, 0, wxALL, 1);
    wf_ctrl_sizer->Add(m_zoom_out_btn, 0, wxALL, 1);
    wf_ctrl_sizer->Add(m_view_all_btn, 0, wxALL, 1);
    wf_ctrl_sizer->Add(m_view_sel_btn, 0, wxALL, 1);
    wf_ctrl_sizer->Add(m_view_mode_ch, 0, wxALL, 1);
    sampler_sizer->Add(wf_ctrl_sizer, 0, wxEXPAND | wxALL, 2);

    // Processing Controls
    wxBoxSizer* proc_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_silence_btn = new wxButton(m_sampler_editor, wxID_ANY, "Silence");
    m_silence_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_silence, this);
    m_norm_btn = new wxButton(m_sampler_editor, wxID_ANY, "Normalize");
    m_norm_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_normalize, this);
    m_vol_btn = new wxButton(m_sampler_editor, wxID_ANY, "Gain");
    m_vol_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_adjust_vol, this);
    
    proc_sizer->Add(m_silence_btn, 0, wxALL, 1);
    proc_sizer->Add(m_norm_btn, 0, wxALL, 1);
    proc_sizer->Add(m_vol_btn, 0, wxALL, 1);
    
    m_fade_in_lin_btn = new wxButton(m_sampler_editor, wxID_ANY, "In Lin");
    m_fade_in_lin_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_fade_in_lin, this);
    m_fade_in_log_btn = new wxButton(m_sampler_editor, wxID_ANY, "In Log");
    m_fade_in_log_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_fade_in_log, this);
    m_fade_out_lin_btn = new wxButton(m_sampler_editor, wxID_ANY, "Out Lin");
    m_fade_out_lin_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_fade_out_lin, this);
    m_fade_out_log_btn = new wxButton(m_sampler_editor, wxID_ANY, "Out Log");
    m_fade_out_log_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_fade_out_log, this);
    
    proc_sizer->Add(m_fade_in_lin_btn, 0, wxALL, 1);
    proc_sizer->Add(m_fade_in_log_btn, 0, wxALL, 1);
    proc_sizer->Add(m_fade_out_lin_btn, 0, wxALL, 1);
    proc_sizer->Add(m_fade_out_log_btn, 0, wxALL, 1);

    m_vol_input = new wxSpinCtrlDouble(m_sampler_editor, wxID_ANY);
    m_vol_input->SetRange(0.0, 10.0); m_vol_input->SetIncrement(0.1); m_vol_input->SetValue(1.0);
    proc_sizer->Add(m_vol_input, 0, wxALL, 1);

    wxBoxSizer* clip_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_cut_btn = new wxButton(m_sampler_editor, wxID_ANY, "Cut");
    m_cut_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&){ cut(); });
    m_copy_btn = new wxButton(m_sampler_editor, wxID_ANY, "Copy");
    m_copy_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&){ copy(); });
    m_paste_btn = new wxButton(m_sampler_editor, wxID_ANY, "Paste");
    m_paste_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&){ paste(); });

    clip_sizer->Add(m_cut_btn, 0, wxALL, 1);
    clip_sizer->Add(m_copy_btn, 0, wxALL, 1);
    clip_sizer->Add(m_paste_btn, 0, wxALL, 1);

    sampler_sizer->Add(clip_sizer, 0, wxEXPAND | wxALL, 2);
    m_sampler_editor->SetSizer(sampler_sizer);
    right_sizer->Add(m_sampler_editor, 1, wxEXPAND | wxALL, 2);

    // 2. SoundFont Editor
    m_sfont_editor = new wxPanel(m_right_panel, wxID_ANY);
    wxBoxSizer* sfont_sizer = new wxBoxSizer(wxVERTICAL);
    m_sfont_load_btn = new wxButton(m_sfont_editor, wxID_ANY, "Load SoundFont");
    m_sfont_load_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&){
        wxFileDialog dlg(this, "Load SoundFont", "", "", "SoundFont files (*.sf2;*.sf3)|*.sf2;*.sf3", wxFD_OPEN);
        if (dlg.ShowModal() == wxID_OK) {
            auto& inst = m_engine.instrument(m_selected_instrument);
            if (inst.type() == InstrumentType::SoundFont) {
                static_cast<SoundFontInstrument*>(&inst)->load_soundfont(dlg.GetPath().ToStdString());
                update_editor();
            }
        }
    });
    sfont_sizer->Add(m_sfont_load_btn, 0, wxALL, 5);

    m_sfont_browser = new wxListBox(m_sfont_editor, wxID_ANY);
    m_sfont_browser->Bind(wxEVT_LISTBOX, [this](wxCommandEvent& ev){
        int idx = m_sfont_browser->GetSelection();
        if (idx != wxNOT_FOUND) {
            auto& inst = m_engine.instrument(m_selected_instrument);
            if (inst.type() == InstrumentType::SoundFont) {
                static_cast<SoundFontInstrument*>(&inst)->set_preset(idx);
            }
        }
    });
    sfont_sizer->Add(m_sfont_browser, 1, wxEXPAND | wxALL, 5);

    m_sfont_vol_slider = new wxSlider(m_sfont_editor, wxID_ANY, 100, 0, 128);
    m_sfont_vol_slider->Bind(wxEVT_SLIDER, [this](wxCommandEvent& ev){
        auto& inst = m_engine.instrument(m_selected_instrument);
        if (inst.type() == InstrumentType::SoundFont) {
            static_cast<SoundFontInstrument*>(&inst)->set_volume((float)m_sfont_vol_slider->GetValue() / 128.0f);
        }
    });
    sfont_sizer->Add(new wxStaticText(m_sfont_editor, wxID_ANY, "Volume"), 0, wxALIGN_LEFT|wxLEFT, 5);
    sfont_sizer->Add(m_sfont_vol_slider, 0, wxEXPAND | wxALL, 5);
    
    m_sfont_editor->SetSizer(sfont_sizer);
    m_sfont_editor->Hide();
    right_sizer->Add(m_sfont_editor, 1, wxEXPAND | wxALL, 2);

    // 3. Plugin Editor
    m_plugin_editor = new wxPanel(m_right_panel, wxID_ANY);
    wxBoxSizer* plugin_sizer = new wxBoxSizer(wxVERTICAL);
    m_plugin_scan_btn = new wxButton(m_plugin_editor, wxID_ANY, "Scan Plugins");
    m_plugin_scan_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_plugin_scan, this);
    plugin_sizer->Add(m_plugin_scan_btn, 0, wxALL, 5);

    wxBoxSizer* pl_split = new wxBoxSizer(wxHORIZONTAL);
    m_plugin_browser = new wxListBox(m_plugin_editor, wxID_ANY, wxDefaultPosition, wxSize(250, -1));
    m_plugin_browser->Bind(wxEVT_LISTBOX, &InstrumentPanel::on_plugin_select, this);
    pl_split->Add(m_plugin_browser, 0, wxEXPAND | wxALL, 2);

    wxBoxSizer* pl_right_sizer = new wxBoxSizer(wxVERTICAL);
    
    // ZynAddSubFX Editor
    m_zyn_editor = new wxPanel(m_plugin_editor, wxID_ANY);
    wxBoxSizer* zyn_sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* zyn_top = new wxBoxSizer(wxHORIZONTAL);
    m_zyn_bank_ch = new wxChoice(m_zyn_editor, wxID_ANY);
    m_zyn_bank_ch->Bind(wxEVT_CHOICE, &InstrumentPanel::on_zyn_bank, this);
    zyn_top->Add(new wxStaticText(m_zyn_editor, wxID_ANY, "Bank:"), 0, wxALIGN_CENTER_VERTICAL|wxALL, 2);
    zyn_top->Add(m_zyn_bank_ch, 1, wxALL, 2);
    zyn_sizer->Add(zyn_top, 0, wxEXPAND | wxALL, 2);
    
    wxBoxSizer* zyn_btns = new wxBoxSizer(wxHORIZONTAL);
    m_zyn_prev_btn = new wxButton(m_zyn_editor, wxID_ANY, "<", wxDefaultPosition, wxSize(30, 25));
    m_zyn_prev_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_zyn_prev, this);
    m_zyn_next_btn = new wxButton(m_zyn_editor, wxID_ANY, ">", wxDefaultPosition, wxSize(30, 25));
    m_zyn_next_btn->Bind(wxEVT_BUTTON, &InstrumentPanel::on_zyn_next, this);
    zyn_btns->Add(m_zyn_prev_btn, 0, wxALL, 2);
    zyn_btns->Add(m_zyn_next_btn, 0, wxALL, 2);
    zyn_sizer->Add(zyn_btns, 0, wxALL, 2);
    
    m_zyn_preset_browser = new wxListBox(m_zyn_editor, wxID_ANY);
    m_zyn_preset_browser->Bind(wxEVT_LISTBOX, &InstrumentPanel::on_zyn_preset, this);
    zyn_sizer->Add(m_zyn_preset_browser, 1, wxEXPAND | wxALL, 2);
    m_zyn_editor->SetSizer(zyn_sizer);
    m_zyn_editor->Hide();
    pl_right_sizer->Add(m_zyn_editor, 0, wxEXPAND | wxALL, 0);

    m_plugin_scroll = new wxScrolledWindow(m_plugin_editor, wxID_ANY);
    m_plugin_scroll->SetScrollRate(0, 20);
    m_plugin_scroll->SetSizer(new wxBoxSizer(wxVERTICAL));
    pl_right_sizer->Add(m_plugin_scroll, 1, wxEXPAND | wxALL, 2);

    pl_split->Add(pl_right_sizer, 1, wxEXPAND | wxALL, 2);

    plugin_sizer->Add(pl_split, 1, wxEXPAND | wxALL, 2);
    m_plugin_editor->SetSizer(plugin_sizer);
    m_plugin_editor->Hide();
    right_sizer->Add(m_plugin_editor, 1, wxEXPAND | wxALL, 2);

    // 4. MIDI Editor
    m_midi_editor = new wxPanel(m_right_panel, wxID_ANY);
    wxBoxSizer* midi_sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* m_ch_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_ch_sizer->Add(new wxStaticText(m_midi_editor, wxID_ANY, "Channel:"), 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    m_midi_channel = new wxSpinCtrl(m_midi_editor, wxID_ANY);
    m_midi_channel->SetRange(1, 16);
    m_midi_channel->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent& ev){
        if (m_selected_instrument >= 0) {
            auto& inst = m_engine.instrument(m_selected_instrument);
            if (inst.type() == InstrumentType::Midi)
                static_cast<MidiInstrument*>(&inst)->set_channel(ev.GetPosition() - 1);
        }
    });
    m_ch_sizer->Add(m_midi_channel, 0, wxALL, 5);
    midi_sizer->Add(m_ch_sizer, 0, wxALL, 5);
    
    wxBoxSizer* m_pg_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_pg_sizer->Add(new wxStaticText(m_midi_editor, wxID_ANY, "Program:"), 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    m_midi_program = new wxSpinCtrl(m_midi_editor, wxID_ANY);
    m_midi_program->SetRange(0, 127);
    m_midi_program->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent& ev){
        if (m_selected_instrument >= 0) {
            auto& inst = m_engine.instrument(m_selected_instrument);
            if (inst.type() == InstrumentType::Midi)
                static_cast<MidiInstrument*>(&inst)->set_program(ev.GetPosition());
        }
    });
    m_pg_sizer->Add(m_midi_program, 0, wxALL, 5);
    midi_sizer->Add(m_pg_sizer, 0, wxALL, 5);

    m_midi_editor->SetSizer(midi_sizer);
    m_midi_editor->Hide();
    right_sizer->Add(m_midi_editor, 1, wxEXPAND | wxALL, 2);

    m_right_panel->SetSizer(right_sizer);
    main_sizer->Add(m_right_panel, 1, wxEXPAND | wxALL, 2);

    SetSizer(main_sizer);

    update_rec_inputs();
    update_instrument_list();
    update_editor();
}

void InstrumentPanel::update_rec_inputs() {
    m_rec_input_ch->Clear();
    bool mono = m_mono_btn->GetValue();
    uint32_t num_ins = m_engine.m_num_ins;
    if (mono) {
        for (uint32_t i = 0; i < num_ins; ++i) {
            m_rec_input_ch->Append(wxString::Format("Channel %u", i + 1));
        }
    } else {
        for (uint32_t i = 0; i < num_ins; i += 2) {
            if (i + 1 < num_ins) m_rec_input_ch->Append(wxString::Format("Channels %u/%u", i + 1, i + 2));
            else m_rec_input_ch->Append(wxString::Format("Channel %u (L)", i + 1));
        }
    }
    if (m_rec_input_ch->GetCount() > 0) m_rec_input_ch->SetSelection(0);
}

void InstrumentPanel::update_instrument_list() {
    if (m_inst_scroll->GetSizer()) {
        m_inst_scroll->GetSizer()->Clear(true);
    }
    
    for (size_t i = 0; i < m_engine.instrument_count(); ++i) {
        auto& inst = m_engine.instrument(i);
        wxPanel* row = new wxPanel(m_inst_scroll, wxID_ANY);
        wxBoxSizer* row_sizer = new wxBoxSizer(wxHORIZONTAL);
        
        wxButton* sel = new wxButton(row, wxID_ANY, wxString::Format("%zu:", i + 1), wxDefaultPosition, wxSize(40, 25), wxBORDER_NONE);
        if ((int)i == m_selected_instrument) {
            sel->SetBackgroundColour(wxColour(255, 255, 0));
            sel->SetForegroundColour(*wxBLACK);
        }
        sel->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&){ 
            this->CallAfter([this, i](){
                on_inst_select_idx(i); 
            });
        });
        row_sizer->Add(sel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
        
        wxTextCtrl* name = new wxTextCtrl(row, wxID_ANY, inst.name(), wxDefaultPosition, wxSize(120, -1), wxTE_PROCESS_ENTER);
        name->Bind(wxEVT_TEXT_ENTER, [this, i](wxCommandEvent& ev){
            m_engine.instrument(i).set_name(ev.GetString().ToStdString());
        });
        row_sizer->Add(name, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 2);
        
        wxChoice* type = new wxChoice(row, wxID_ANY);
        type->Append("None"); type->Append("Sampler"); type->Append("SoundFont"); type->Append("Plugin"); type->Append("Midi");
        type->SetSelection((int)inst.type());
        type->Bind(wxEVT_CHOICE, [this, i](wxCommandEvent& ev){
            int sel_idx = ev.GetSelection();
            this->CallAfter([this, i, sel_idx](){
                m_engine.set_instrument_type(i, (InstrumentType)sel_idx);
                m_selected_instrument = (int)i;
                m_selected_sample = -1;
                update_instrument_list();
                update_editor();
            });
        });
        row_sizer->Add(type, 0, wxALIGN_CENTER_VERTICAL, 0);

        row->SetSizer(row_sizer);
        m_inst_scroll->GetSizer()->Add(row, 0, wxEXPAND | wxBOTTOM, 2);
    }
    m_inst_scroll->Layout();
    m_inst_scroll->FitInside();
}

void InstrumentPanel::on_inst_select_idx(int idx) {
    m_selected_instrument = idx;
    m_selected_sample = -1;
    update_instrument_list();
    update_editor();
}

void InstrumentPanel::update_editor() {
    m_sampler_editor->Hide();
    m_sfont_editor->Hide();
    m_plugin_editor->Hide();
    m_midi_editor->Hide();

    if (m_selected_instrument >= 0 && m_selected_instrument < (int)m_engine.instrument_count()) {
        auto& inst = m_engine.instrument(m_selected_instrument);
        
        if (inst.type() == InstrumentType::Sampler) {
            m_sampler_editor->Show();
            SampleInstrument* sampler = static_cast<SampleInstrument*>(&inst);
            
            if (m_sample_scroll->GetSizer()) {
                m_sample_scroll->GetSizer()->Clear(true);
            }

            for (size_t i = 0; i < sampler->sample_count(); ++i) {
                const auto& entry = sampler->get_sample(i);
                wxPanel* row = new wxPanel(m_sample_scroll, wxID_ANY);
                wxBoxSizer* rs = new wxBoxSizer(wxHORIZONTAL);
                
                wxButton* sel = new wxButton(row, wxID_ANY, wxString::Format("%zu", i + 1), wxDefaultPosition, wxSize(25, 25), wxBORDER_NONE);
                if ((int)i == m_selected_sample) {
                    sel->SetBackgroundColour(wxColour(255, 255, 0));
                    sel->SetForegroundColour(*wxBLACK);
                }
                sel->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&){
                    m_selected_sample = (int)i;
                    static_cast<SampleInstrument*>(&m_engine.instrument(m_selected_instrument))->set_selected_sample(i);
                    update_editor();
                });
                rs->Add(sel, 0, wxALL, 1);
                
                wxTextCtrl* name = new wxTextCtrl(row, wxID_ANY, entry.name, wxDefaultPosition, wxSize(80, -1), wxTE_PROCESS_ENTER);
                name->Bind(wxEVT_TEXT_ENTER, [this, i](wxCommandEvent& ev){
                    static_cast<SampleInstrument*>(&m_engine.instrument(m_selected_instrument))->set_sample_name(i, ev.GetString().ToStdString());
                });
                rs->Add(name, 1, wxALL, 1);
                
                wxButton* load = new wxButton(row, wxID_ANY, "L", wxDefaultPosition, wxSize(25, 25));
                load->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent& ev){ 
                    m_selected_sample = (int)i;
                    wxCommandEvent dummy; on_load_sample(dummy); 
                });
                rs->Add(load, 0, wxALL, 0);

                wxButton* save = new wxButton(row, wxID_ANY, "S", wxDefaultPosition, wxSize(25, 25));
                save->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent& ev){
                    m_selected_sample = (int)i;
                    wxCommandEvent dummy; on_save_sample(dummy);
                });
                rs->Add(save, 0, wxALL, 0);

                wxButton* up = new wxButton(row, wxID_ANY, "U", wxDefaultPosition, wxSize(25, 25));
                up->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&){
                    if (i > 0) {
                        static_cast<SampleInstrument*>(&m_engine.instrument(m_selected_instrument))->move_sample(i, i - 1);
                        m_selected_sample = (int)(i - 1);
                        update_editor();
                    }
                });
                rs->Add(up, 0, wxALL, 0);

                wxButton* down = new wxButton(row, wxID_ANY, "D", wxDefaultPosition, wxSize(25, 25));
                down->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&){
                    auto* s = static_cast<SampleInstrument*>(&m_engine.instrument(m_selected_instrument));
                    if (i < s->sample_count() - 1) {
                        s->move_sample(i, i + 1);
                        m_selected_sample = (int)(i + 1);
                        update_editor();
                    }
                });
                rs->Add(down, 0, wxALL, 0);

                wxButton* rem = new wxButton(row, wxID_ANY, "X", wxDefaultPosition, wxSize(25, 25));
                rem->SetForegroundColour(*wxRED);
                rem->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&){
                    static_cast<SampleInstrument*>(&m_engine.instrument(m_selected_instrument))->remove_sample(i);
                    m_selected_sample = -1;
                    update_editor();
                });
                rs->Add(rem, 0, wxALL, 0);
                
                row->SetSizer(rs);
                m_sample_scroll->GetSizer()->Add(row, 0, wxEXPAND | wxBOTTOM, 1);
            }
            m_sample_scroll->Layout();
            m_sample_scroll->FitInside();

            m_waveform_view->set_color(m_engine.m_waveform_color);
            if (m_selected_sample >= 0 && m_selected_sample < (int)sampler->sample_count()) {
                m_waveform_view->set_sample(sampler->get_sample(m_selected_sample).data);
            } else {
                m_waveform_view->set_sample(nullptr);
            }
        } 
        else if (inst.type() == InstrumentType::SoundFont) {
            m_sfont_editor->Show();
            SoundFontInstrument* sf = static_cast<SoundFontInstrument*>(&inst);
            m_sfont_browser->Clear();
            const auto& presets = sf->presets();
            for (const auto& p : presets) {
                m_sfont_browser->Append(wxString::Format("[%03d:%03d] %s", p.bank, p.num, wxString::FromUTF8(p.name)));
            }
            if (sf->current_preset() >= 0 && sf->current_preset() < (int)m_sfont_browser->GetCount()) {
                m_sfont_browser->SetSelection(sf->current_preset());
            }
            m_sfont_vol_slider->SetValue((int)(sf->get_volume() * 128));
            m_sfont_editor->Layout();
        }
        else if (inst.type() == InstrumentType::Midi) {
            m_midi_editor->Show();
            MidiInstrument* midi = static_cast<MidiInstrument*>(&inst);
            m_midi_channel->SetValue(midi->channel() + 1);
            m_midi_program->SetValue(midi->program());
            m_midi_editor->Layout();
        }
        else if (inst.type() == InstrumentType::Plugin) {
            m_plugin_editor->Show();
            if (m_plugin_browser->GetCount() == 0) {
                wxCommandEvent dummy; on_plugin_scan(dummy);
            }
            
            std::string current_path;
            if (auto* dssi = dynamic_cast<DSSIInstrument*>(&inst)) {
                current_path = dssi->path();
                for (int i = 0; i < (int)m_plugin_browser->GetCount(); ++i) {
                    if (m_plugin_map.count(i)) {
                        auto const& info = m_plugin_map[i];
                        if (info.path == current_path && info.index == dssi->index()) {
                            m_plugin_browser->SetSelection(i);
                            break;
                        }
                    }
                }
            }

            std::string p_name = inst.plugin_name();
            bool is_zyn = (!p_name.empty() && p_name.find("ZynAddSubFX") != std::string::npos);
            if (is_zyn) {
                m_zyn_editor->Show();
                if (m_zyn_bank_ch->GetCount() == 0) {
                    std::vector<std::string> bank_paths = {"/usr/share/zynaddsubfx/banks", "/usr/local/share/zynaddsubfx/banks"};
                    for (const auto& bp : bank_paths) {
                        wxDir dir(bp);
                        if (!dir.IsOpened()) continue;
                        wxString dirname;
                        bool cont = dir.GetFirst(&dirname, "", wxDIR_DIRS);
                        while (cont) {
                            m_zyn_bank_ch->Append(dirname);
                            cont = dir.GetNext(&dirname);
                        }
                    }
                    if (m_zyn_bank_ch->GetCount() > 0) {
                        m_zyn_bank_ch->SetSelection(0);
                        wxCommandEvent dummy; on_zyn_bank(dummy);
                    }
                }
            } else {
                m_zyn_editor->Hide();
            }

            if (m_plugin_scroll->GetSizer()) {
                m_plugin_scroll->GetSizer()->Clear(true);
            }
            size_t num_params = inst.parameter_count();
            for (size_t i = 0; i < num_params; ++i) {
                auto param = inst.get_parameter(i);
                wxPanel* p_row = new wxPanel(m_plugin_scroll, wxID_ANY);
                wxBoxSizer* p_sizer = new wxBoxSizer(wxVERTICAL);
                
                wxStaticText* label = new wxStaticText(p_row, wxID_ANY, param.name);
                wxFont lbl_font = label->GetFont(); lbl_font.SetPointSize(8); label->SetFont(lbl_font);
                p_sizer->Add(label, 0, wxALL, 1);
                
                wxSlider* slider = new wxSlider(p_row, wxID_ANY, (int)(param.value * 1000), (int)(param.min * 1000), (int)(param.max * 1000));
                slider->Bind(wxEVT_SLIDER, [this, i](wxCommandEvent& ev){
                    m_engine.instrument(m_selected_instrument).set_parameter(i, (float)ev.GetInt() / 1000.0f);
                });
                p_sizer->Add(slider, 0, wxEXPAND | wxALL, 1);
                
                p_row->SetSizer(p_sizer);
                m_plugin_scroll->GetSizer()->Add(p_row, 0, wxEXPAND | wxBOTTOM, 2);
            }
            m_plugin_scroll->Layout();
            m_plugin_scroll->FitInside();
            m_plugin_editor->Layout();
        }
    }
    m_right_panel->Layout();
    m_right_panel->Refresh();
}

void InstrumentPanel::on_new(wxCommandEvent& event) {
    m_engine.add_instrument();
    m_selected_instrument = (int)m_engine.instrument_count() - 1;
    update_instrument_list();
    update_editor();
}

void InstrumentPanel::on_load(wxCommandEvent& event) {}
void InstrumentPanel::on_save(wxCommandEvent& event) {}

void InstrumentPanel::on_delete(wxCommandEvent& event) {
    if (m_selected_instrument >= 0) {
        if (wxMessageBox("Delete selected instrument?", "Confirm", wxYES_NO | wxICON_QUESTION) == wxYES) {
            m_engine.remove_instrument(m_selected_instrument);
            m_selected_instrument = -1;
            m_selected_sample = -1;
            update_instrument_list();
            update_editor();
        }
    }
}

void InstrumentPanel::on_inst_select(wxCommandEvent& event) {}

void InstrumentPanel::on_add_sample(wxCommandEvent& event) {
    if (m_selected_instrument < 0) return;
    auto& inst = m_engine.instrument(m_selected_instrument);
    if (inst.type() == InstrumentType::Sampler) {
        static_cast<SampleInstrument*>(&inst)->add_sample("New Sample", std::make_shared<SampleData>());
        update_editor();
    }
}

void InstrumentPanel::on_sample_play(wxCommandEvent& event) {
    if (m_selected_instrument >= 0) {
        auto& inst = m_engine.instrument(m_selected_instrument);
        inst.note_off();
        inst.note_on(60, 100);
    }
}

void InstrumentPanel::on_sample_stop(wxCommandEvent& event) {
    if (m_selected_instrument >= 0) {
        m_engine.instrument(m_selected_instrument).note_off();
    }
}

void InstrumentPanel::on_record_sample(wxCommandEvent& event) {
    if (m_rec_btn->GetValue()) {
        uint32_t channel = 0;
        int sel = m_rec_input_ch->GetSelection();
        if (sel != wxNOT_FOUND) channel = (uint32_t)sel;
        bool mono = m_mono_btn->GetValue();
        if (!mono) channel *= 2; 
        Engine::SampleRecordMode mode = (Engine::SampleRecordMode)m_rec_mode_ch->GetSelection();
        m_engine.start_recording_sample(mode, channel, mono);
    } else {
        m_engine.stop_recording_sample();
        if (m_selected_instrument >= 0) {
            auto& inst = m_engine.instrument(m_selected_instrument);
            if (inst.type() == InstrumentType::Sampler) {
                auto* sampler = static_cast<SampleInstrument*>(&inst);
                if (m_engine.m_recording_sample_data && !m_engine.m_recording_sample_data->left.empty()) {
                    sampler->add_sample("Recorded Sample", m_engine.m_recording_sample_data);
                    m_selected_sample = (int)sampler->sample_count() - 1;
                    update_editor();
                }
            }
        }
    }
}

void InstrumentPanel::on_mono_toggle(wxCommandEvent& event) {
    update_rec_inputs();
}

void InstrumentPanel::on_zoom_in(wxCommandEvent& event) { if (m_waveform_view) m_waveform_view->zoom_in(); }
void InstrumentPanel::on_zoom_out(wxCommandEvent& event) { if (m_waveform_view) m_waveform_view->zoom_out(); }
void InstrumentPanel::on_view_all(wxCommandEvent& event) { if (m_waveform_view) m_waveform_view->view_all(); }
void InstrumentPanel::on_view_sel(wxCommandEvent& event) { if (m_waveform_view) m_waveform_view->view_selection(); }
void InstrumentPanel::on_view_mode(wxCommandEvent& event) { if (m_waveform_view) m_waveform_view->set_channel_mode((ChannelMode)event.GetSelection()); }
void InstrumentPanel::on_sample_fmt(wxCommandEvent& event) {
    if (m_selected_sample >= 0 && m_selected_instrument >= 0) {
        auto& inst = m_engine.instrument(m_selected_instrument);
        if (inst.type() == InstrumentType::Sampler) {
             static_cast<SampleInstrument*>(&inst)->convert_sample_format(m_selected_sample, (SampleFormatAction)event.GetSelection());
             update_editor();
        }
    }
}

void InstrumentPanel::on_silence(wxCommandEvent& event) {
    if (m_selected_sample < 0 || !m_waveform_view) return;
    auto& inst = m_engine.instrument(m_selected_instrument);
    if (inst.type() == InstrumentType::Sampler) {
        auto& sample = static_cast<SampleInstrument*>(&inst)->get_sample(m_selected_sample);
        if (sample.data) {
            size_t s1 = m_waveform_view->selection_start(), s2 = m_waveform_view->selection_end();
            if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); }
            else if (s1 > s2) std::swap(s1, s2);
            sample.data->silence(s1, s2);
            update_editor();
        }
    }
}

void InstrumentPanel::on_fade_in_lin(wxCommandEvent& event) {
    if (m_selected_sample < 0 || !m_waveform_view) return;
    auto& inst = m_engine.instrument(m_selected_instrument);
    if (inst.type() == InstrumentType::Sampler) {
        auto& sample = static_cast<SampleInstrument*>(&inst)->get_sample(m_selected_sample);
        if (sample.data) {
            size_t s1 = m_waveform_view->selection_start(), s2 = m_waveform_view->selection_end();
            if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); }
            else if (s1 > s2) std::swap(s1, s2);
            sample.data->fade_in(s1, s2, false);
            update_editor();
        }
    }
}

void InstrumentPanel::on_fade_in_log(wxCommandEvent& event) {
    if (m_selected_sample < 0 || !m_waveform_view) return;
    auto& inst = m_engine.instrument(m_selected_instrument);
    if (inst.type() == InstrumentType::Sampler) {
        auto& sample = static_cast<SampleInstrument*>(&inst)->get_sample(m_selected_sample);
        if (sample.data) {
            size_t s1 = m_waveform_view->selection_start(), s2 = m_waveform_view->selection_end();
            if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); }
            else if (s1 > s2) std::swap(s1, s2);
            sample.data->fade_in(s1, s2, true);
            update_editor();
        }
    }
}

void InstrumentPanel::on_fade_out_lin(wxCommandEvent& event) {
    if (m_selected_sample < 0 || !m_waveform_view) return;
    auto& inst = m_engine.instrument(m_selected_instrument);
    if (inst.type() == InstrumentType::Sampler) {
        auto& sample = static_cast<SampleInstrument*>(&inst)->get_sample(m_selected_sample);
        if (sample.data) {
            size_t s1 = m_waveform_view->selection_start(), s2 = m_waveform_view->selection_end();
            if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); }
            else if (s1 > s2) std::swap(s1, s2);
            sample.data->fade_out(s1, s2, false);
            update_editor();
        }
    }
}

void InstrumentPanel::on_fade_out_log(wxCommandEvent& event) {
    if (m_selected_sample < 0 || !m_waveform_view) return;
    auto& inst = m_engine.instrument(m_selected_instrument);
    if (inst.type() == InstrumentType::Sampler) {
        auto& sample = static_cast<SampleInstrument*>(&inst)->get_sample(m_selected_sample);
        if (sample.data) {
            size_t s1 = m_waveform_view->selection_start(), s2 = m_waveform_view->selection_end();
            if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); }
            else if (s1 > s2) std::swap(s1, s2);
            sample.data->fade_out(s1, s2, true);
            update_editor();
        }
    }
}

void InstrumentPanel::on_normalize(wxCommandEvent& event) {
    if (m_selected_sample >= 0 && m_waveform_view) {
        auto& inst = m_engine.instrument(m_selected_instrument);
        if (inst.type() == InstrumentType::Sampler) {
            auto& sample = static_cast<SampleInstrument*>(&inst)->get_sample(m_selected_sample);
            if (sample.data) {
                size_t s1 = m_waveform_view->selection_start(), s2 = m_waveform_view->selection_end();
                if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); }
                else if (s1 > s2) std::swap(s1, s2);
                sample.data->normalize(s1, s2);
                update_editor();
            }
        }
    }
}

void InstrumentPanel::on_adjust_vol(wxCommandEvent& event) {
    double val = m_vol_input->GetValue();
    if (m_selected_sample >= 0 && m_waveform_view) {
        auto& inst = m_engine.instrument(m_selected_instrument);
        if (inst.type() == InstrumentType::Sampler) {
            auto& sample = static_cast<SampleInstrument*>(&inst)->get_sample(m_selected_sample);
            if (sample.data) {
                size_t s1 = m_waveform_view->selection_start(), s2 = m_waveform_view->selection_end();
                if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); }
                else if (s1 > s2) std::swap(s1, s2);
                sample.data->adjust_volume(s1, s2, (float)val);
                update_editor();
            }
        }
    }
}

void InstrumentPanel::cut() {
    if (m_selected_sample >= 0 && m_waveform_view) {
        auto& inst = m_engine.instrument(m_selected_instrument);
        if (inst.type() == InstrumentType::Sampler) {
            auto& sample = static_cast<SampleInstrument*>(&inst)->get_sample(m_selected_sample);
            if (sample.data) {
                size_t s1 = m_waveform_view->selection_start(), s2 = m_waveform_view->selection_end();
                if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); }
                else if (s1 > s2) std::swap(s1, s2);
                m_engine.sample_clipboard().data = std::make_shared<SampleData>(sample.data->cut(s1, s2));
                update_editor();
            }
        }
    }
}

void InstrumentPanel::copy() {
    if (m_selected_sample >= 0 && m_waveform_view) {
        auto& inst = m_engine.instrument(m_selected_instrument);
        if (inst.type() == InstrumentType::Sampler) {
            auto& sample = static_cast<SampleInstrument*>(&inst)->get_sample(m_selected_sample);
            if (sample.data) {
                size_t s1 = m_waveform_view->selection_start(), s2 = m_waveform_view->selection_end();
                if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); }
                else if (s1 > s2) std::swap(s1, s2);
                auto cb = std::make_shared<SampleData>();
                cb->sample_rate = sample.data->sample_rate;
                cb->left.assign(sample.data->left.begin() + s1, sample.data->left.begin() + s2);
                if (!sample.data->right.empty())
                    cb->right.assign(sample.data->right.begin() + s1, sample.data->right.begin() + s2);
                m_engine.sample_clipboard().data = cb;
            }
        }
    }
}

void InstrumentPanel::paste() {
    if (m_selected_sample >= 0 && m_waveform_view && m_engine.sample_clipboard().data) {
        auto& inst = m_engine.instrument(m_selected_instrument);
        if (inst.type() == InstrumentType::Sampler) {
            auto& sample = static_cast<SampleInstrument*>(&inst)->get_sample(m_selected_sample);
            if (sample.data) {
                sample.data->paste_at(m_waveform_view->selection_start(), *m_engine.sample_clipboard().data);
                update_editor();
            }
        }
    }
}

void InstrumentPanel::on_inst_name(wxCommandEvent& event) {
    wxTextCtrl* ctrl = dynamic_cast<wxTextCtrl*>(event.GetEventObject());
    if (ctrl && m_selected_instrument >= 0) {
        m_engine.instrument(m_selected_instrument).set_name(ctrl->GetValue().ToStdString());
        update_instrument_list();
    }
}

void InstrumentPanel::on_inst_type(wxCommandEvent& event) {
    wxChoice* ctrl = dynamic_cast<wxChoice*>(event.GetEventObject());
    if (ctrl && m_selected_instrument >= 0) {
        m_engine.set_instrument_type(m_selected_instrument, (InstrumentType)ctrl->GetSelection());
        update_editor();
    }
}

void InstrumentPanel::on_load_sample(wxCommandEvent& event) {
    if (m_selected_instrument < 0) return;
    auto& inst = m_engine.instrument(m_selected_instrument);
    if (inst.type() != InstrumentType::Sampler) return;

    wxFileDialog dlg(this, "Load Sample", "", "", "Audio Files|*.wav;*.flac;*.mp3;*.ogg", wxFD_OPEN);
    if (dlg.ShowModal() == wxID_OK) {
        auto data = std::make_shared<SampleData>();
        uint32_t sr = 0;
        if (AudioFile::load_audio(dlg.GetPath().ToStdString(), data->left, data->right, sr)) {
            data->sample_rate = (int)sr;
            auto* s = static_cast<SampleInstrument*>(&inst);
            if (m_selected_sample < 0) {
                s->add_sample(dlg.GetFilename().ToStdString(), data);
                m_selected_sample = (int)s->sample_count() - 1;
            } else {
                s->set_sample_name(m_selected_sample, dlg.GetFilename().ToStdString());
                s->get_sample(m_selected_sample).data = data;
            }
            update_editor();
        }
    }
}

void InstrumentPanel::on_save_sample(wxCommandEvent& event) {
    if (m_selected_instrument < 0 || m_selected_sample < 0) return;
    auto& inst = m_engine.instrument(m_selected_instrument);
    if (inst.type() != InstrumentType::Sampler) return;
    auto* s = static_cast<SampleInstrument*>(&inst);
    auto& entry = s->get_sample(m_selected_sample);
    if (!entry.data) return;

    wxFileDialog dlg(this, "Save Sample", "", entry.name, "WAV Files|*.wav", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        if (!AudioFile::save_wav(dlg.GetPath().ToStdString(), entry.data->left, entry.data->right, (uint32_t)entry.data->sample_rate)) {
            wxMessageBox("Failed to save audio file.", "Error", wxOK | wxICON_ERROR);
        }
    }
}

void InstrumentPanel::on_remove_sample(wxCommandEvent& event) {
    if (m_selected_instrument >= 0 && m_selected_sample >= 0) {
        auto& inst = m_engine.instrument(m_selected_instrument);
        if (inst.type() == InstrumentType::Sampler) {
            static_cast<SampleInstrument*>(&inst)->remove_sample(m_selected_sample);
            m_selected_sample = -1;
            update_editor();
        }
    }
}

void InstrumentPanel::on_move_sample_up(wxCommandEvent& event) {
    if (m_selected_instrument >= 0 && m_selected_sample > 0) {
        auto& inst = m_engine.instrument(m_selected_instrument);
        if (inst.type() == InstrumentType::Sampler) {
            static_cast<SampleInstrument*>(&inst)->move_sample(m_selected_sample, m_selected_sample - 1);
            m_selected_sample--;
            update_editor();
        }
    }
}

void InstrumentPanel::on_move_sample_down(wxCommandEvent& event) {
    if (m_selected_instrument >= 0 && m_selected_sample >= 0) {
        auto& inst = m_engine.instrument(m_selected_instrument);
        if (inst.type() == InstrumentType::Sampler) {
            auto* s = static_cast<SampleInstrument*>(&inst);
            if (m_selected_sample < (int)s->sample_count() - 1) {
                s->move_sample(m_selected_sample, m_selected_sample + 1);
                m_selected_sample++;
                update_editor();
            }
        }
    }
}

void InstrumentPanel::on_plugin_scan(wxCommandEvent& event) {
    m_plugin_browser->Clear();
    m_plugin_map.clear();
    std::vector<std::string> paths = {"/usr/lib/dssi", "/usr/local/lib/dssi", "/usr/lib/x86_64-linux-gnu/dssi"};
    for (const auto& path : paths) {
        wxDir dir(path); if (!dir.IsOpened()) continue;
        wxString filename; bool cont = dir.GetFirst(&filename, "*.so", wxDIR_FILES);
        while (cont) {
            wxString fullpath = path + "/" + filename;
            void* handle = dlopen(fullpath.ToStdString().c_str(), RTLD_LAZY);
            if (handle) {
                DSSI_Descriptor_Function df = (DSSI_Descriptor_Function)dlsym(handle, "dssi_descriptor");
                if (df) {
                    for (int i=0; ; ++i) {
                        const DSSI_Descriptor* desc = df(i); if (!desc) break;
                        wxString label = wxString::FromUTF8(desc->LADSPA_Plugin->Name) + " (DSSI)";
                        int item = m_plugin_browser->Append(label);
                        m_plugin_map[item] = {desc->LADSPA_Plugin->Name, fullpath.ToStdString(), i, false};
                    }
                }
                dlclose(handle);
            }
            cont = dir.GetNext(&filename);
        }
    }
}

void InstrumentPanel::on_plugin_select(wxCommandEvent& event) {
    int sel = m_plugin_browser->GetSelection();
    if (sel != wxNOT_FOUND && m_plugin_map.count(sel)) {
        auto& info = m_plugin_map[sel];
        auto& inst = m_engine.instrument(m_selected_instrument);
        if (inst.type() == InstrumentType::Plugin) {
            if (static_cast<DSSIInstrument*>(&inst)->load_plugin(info.path, info.index)) {
                update_editor();
            }
        }
    }
}

void InstrumentPanel::on_plugin_param(wxScrollEvent& event) {}

void InstrumentPanel::on_zyn_bank(wxCommandEvent& event) {
    m_zyn_preset_browser->Clear();
    int bidx = m_zyn_bank_ch->GetSelection(); if (bidx == wxNOT_FOUND) return;
    wxString bank_name = m_zyn_bank_ch->GetString(bidx);
    std::vector<std::string> bank_paths = {"/usr/share/zynaddsubfx/banks", "/usr/local/share/zynaddsubfx/banks"};
    for (const auto& bp : bank_paths) {
        wxString full_path = wxString(bp) + "/" + bank_name;
        wxDir dir(full_path); if (!dir.IsOpened()) continue;
        wxString fname; bool cont = dir.GetFirst(&fname, "*.xiz", wxDIR_FILES);
        while (cont) {
            m_zyn_preset_browser->Append(fname);
            cont = dir.GetNext(&fname);
        }
    }
}

void InstrumentPanel::on_zyn_preset(wxCommandEvent& event) {
    int bidx = m_zyn_bank_ch->GetSelection();
    int pidx = m_zyn_preset_browser->GetSelection();
    if (bidx != wxNOT_FOUND && pidx != wxNOT_FOUND && m_selected_instrument >= 0) {
        auto& inst = m_engine.instrument(m_selected_instrument);
        if (inst.type() == InstrumentType::Plugin) {
            static_cast<DSSIInstrument*>(&inst)->load_program((unsigned long)bidx, (unsigned long)pidx);
        }
    }
}

void InstrumentPanel::on_zyn_prev(wxCommandEvent& event) {
    int sel = m_zyn_preset_browser->GetSelection();
    if (sel > 0) {
        m_zyn_preset_browser->SetSelection(sel - 1);
        wxCommandEvent dummy; on_zyn_preset(dummy);
    }
}

void InstrumentPanel::on_zyn_next(wxCommandEvent& event) {
    int sel = m_zyn_preset_browser->GetSelection();
    if (sel != wxNOT_FOUND && sel < (int)m_zyn_preset_browser->GetCount() - 1) {
        m_zyn_preset_browser->SetSelection(sel + 1);
        wxCommandEvent dummy; on_zyn_preset(dummy);
    }
}

void InstrumentPanel::on_detach(wxCommandEvent& event) {
    if (!m_detached_window) {
        m_detached_window = new wxFrame(NULL, wxID_ANY, "Instruments", wxDefaultPosition, wxSize(800, 600));
        m_detached_window->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& ev){
            m_detached_window->Hide();
        });
    }
    m_detached_window->Show();
}

} // namespace disgrace_ns
