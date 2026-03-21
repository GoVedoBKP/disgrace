#include "wx_settings_panel.h"
#include "theme.h"
#include "wx_main_window.h"
#include "../core/engine.h"

#include <wx/sizer.h>
#include <wx/msgdlg.h>
#include <wx/colordlg.h>

namespace disgrace_ns {

wxBEGIN_EVENT_TABLE(SettingsPanel, wxPanel)
    EVT_BUTTON(wxID_ANY, SettingsPanel::on_reinit_audio)
    EVT_BUTTON(wxID_ANY, SettingsPanel::on_reinit_midi)
    EVT_CHOICE(wxID_ANY, SettingsPanel::on_gui_theme)
    EVT_CHOICE(wxID_ANY, SettingsPanel::on_gui_btn_h)
    EVT_CHOICE(wxID_ANY, SettingsPanel::on_gui_font_size)
    EVT_CHOICE(wxID_ANY, SettingsPanel::on_kbd_layout)
    EVT_BUTTON(wxID_ANY, SettingsPanel::on_waveform_color)
    EVT_BUTTON(wxID_ANY, SettingsPanel::on_bg_color)
    EVT_BUTTON(wxID_ANY, SettingsPanel::on_fg_color)
    EVT_BUTTON(wxID_ANY, SettingsPanel::on_assign_key)
wxEND_EVENT_TABLE()

SettingsPanel::SettingsPanel(wxWindow* parent, Engine& engine)
    : wxPanel(parent, wxID_ANY), m_engine(engine)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    m_sub_tabs = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_TOP);

    init_audio_grp(0, 0, 400, 300);
    init_midi_grp(0, 0, 400, 300);
    init_mixer_grp(0, 0, 400, 300);
    init_gui_grp(0, 0, 400, 300);
    init_kbd_grp(0, 0, 400, 300);
    init_misc_grp(0, 0, 400, 300);

    main_sizer->Add(m_sub_tabs, 1, wxEXPAND | wxALL, 0);
    SetSizer(main_sizer);
}

void SettingsPanel::init_audio_grp(int x, int y, int w, int h) {
    m_audio_grp = new wxPanel(m_sub_tabs, wxID_ANY);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(m_audio_grp, wxID_ANY, "Input Channels:"), 0, wxALL, 2);
    m_audio_ins = new wxSpinCtrl(m_audio_grp, wxID_ANY, "2", wxDefaultPosition, wxSize(60, 25));
    m_audio_ins->SetRange(0, 32);
    m_audio_ins->SetValue(m_engine.m_num_ins);
    row->Add(m_audio_ins, 0, wxALL, 2);
    sizer->Add(row, 0, wxALL, 2);

    row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(m_audio_grp, wxID_ANY, "Output Channels:"), 0, wxALL, 2);
    m_audio_outs = new wxSpinCtrl(m_audio_grp, wxID_ANY, "2", wxDefaultPosition, wxSize(60, 25));
    m_audio_outs->SetRange(0, 32);
    m_audio_outs->SetValue(m_engine.m_num_outs);
    row->Add(m_audio_outs, 0, wxALL, 2);
    sizer->Add(row, 0, wxALL, 2);

    m_reinit_audio_btn = new wxButton(m_audio_grp, wxID_ANY, "Reinitialize Audio", wxDefaultPosition, wxSize(150, 30));
    m_reinit_audio_btn->Bind(wxEVT_BUTTON, &SettingsPanel::on_reinit_audio, this);
    sizer->Add(m_reinit_audio_btn, 0, wxALL, 2);

    m_audio_status = new wxStaticText(m_audio_grp, wxID_ANY, "Audio Status: Ready");
    sizer->Add(m_audio_status, 0, wxALL, 2);

    m_audio_grp->SetSizer(sizer);
    m_sub_tabs->AddPage(m_audio_grp, "Audio");
}

void SettingsPanel::init_midi_grp(int x, int y, int w, int h) {
    m_midi_grp = new wxPanel(m_sub_tabs, wxID_ANY);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(m_midi_grp, wxID_ANY, "Input Devices:"), 0, wxALL, 2);
    m_midi_ins = new wxSpinCtrl(m_midi_grp, wxID_ANY, "0", wxDefaultPosition, wxSize(60, 25));
    m_midi_ins->SetRange(0, 16);
    row->Add(m_midi_ins, 0, wxALL, 2);
    sizer->Add(row, 0, wxALL, 2);

    row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(m_midi_grp, wxID_ANY, "Output Devices:"), 0, wxALL, 2);
    m_midi_outs = new wxSpinCtrl(m_midi_grp, wxID_ANY, "0", wxDefaultPosition, wxSize(60, 25));
    m_midi_outs->SetRange(0, 16);
    row->Add(m_midi_outs, 0, wxALL, 2);
    sizer->Add(row, 0, wxALL, 2);

    m_reinit_midi_btn = new wxButton(m_midi_grp, wxID_ANY, "Reinitialize MIDI", wxDefaultPosition, wxSize(150, 30));
    m_reinit_midi_btn->Bind(wxEVT_BUTTON, &SettingsPanel::on_reinit_midi, this);
    sizer->Add(m_reinit_midi_btn, 0, wxALL, 2);

    m_midi_grp->SetSizer(sizer);
    m_sub_tabs->AddPage(m_midi_grp, "MIDI");
}

void SettingsPanel::init_mixer_grp(int x, int y, int w, int h) {
    m_mixer_grp = new wxPanel(m_sub_tabs, wxID_ANY);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(new wxStaticText(m_mixer_grp, wxID_ANY, "Mixer settings coming soon"), 0, wxALL, 2);
    m_mixer_grp->SetSizer(sizer);
    m_sub_tabs->AddPage(m_mixer_grp, "Mixer");
}

void SettingsPanel::init_gui_grp(int x, int y, int w, int h) {
    m_gui_grp = new wxPanel(m_sub_tabs, wxID_ANY);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(m_gui_grp, wxID_ANY, "Theme:"), 0, wxALL, 2);
    m_gui_theme = new wxChoice(m_gui_grp, wxID_ANY, wxDefaultPosition, wxSize(120, 25));
    
    const auto& themes = ThemeManager::get_available_themes();
    for (const auto& t : themes) {
        m_gui_theme->Append(t.name);
    }
    m_gui_theme->SetSelection((int)m_engine.m_gui_theme);
    m_gui_theme->Bind(wxEVT_CHOICE, &SettingsPanel::on_gui_theme, this);

    row->Add(m_gui_theme, 0, wxALL, 2);
    sizer->Add(row, 0, wxALL, 2);

    row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(m_gui_grp, wxID_ANY, "Button Height:"), 0, wxALL, 2);
    m_gui_btn_h = new wxChoice(m_gui_grp, wxID_ANY, wxDefaultPosition, wxSize(80, 25));
    m_gui_btn_h->Append("20");
    m_gui_btn_h->Append("25");
    m_gui_btn_h->Append("30");
    m_gui_btn_h->SetSelection(1);
    row->Add(m_gui_btn_h, 0, wxALL, 2);
    sizer->Add(row, 0, wxALL, 2);

    row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(m_gui_grp, wxID_ANY, "Font Size:"), 0, wxALL, 2);
    m_gui_font_size = new wxChoice(m_gui_grp, wxID_ANY, wxDefaultPosition, wxSize(80, 25));
    m_gui_font_size->Append("10");
    m_gui_font_size->Append("12");
    m_gui_font_size->Append("14");
    m_gui_font_size->SetSelection(1);
    row->Add(m_gui_font_size, 0, wxALL, 2);
    sizer->Add(row, 0, wxALL, 2);

    row = new wxBoxSizer(wxHORIZONTAL);
    m_bg_color_btn = new wxButton(m_gui_grp, wxID_ANY, "Background Color", wxDefaultPosition, wxSize(120, 25));
    m_bg_color_btn->Bind(wxEVT_BUTTON, &SettingsPanel::on_bg_color, this);
    m_fg_color_btn = new wxButton(m_gui_grp, wxID_ANY, "Foreground Color", wxDefaultPosition, wxSize(120, 25));
    m_fg_color_btn->Bind(wxEVT_BUTTON, &SettingsPanel::on_fg_color, this);
    m_waveform_color_btn = new wxButton(m_gui_grp, wxID_ANY, "Waveform Color", wxDefaultPosition, wxSize(120, 25));
    m_waveform_color_btn->Bind(wxEVT_BUTTON, &SettingsPanel::on_waveform_color, this);
    row->Add(m_bg_color_btn, 0, wxALL, 2);
    row->Add(m_fg_color_btn, 0, wxALL, 2);
    row->Add(m_waveform_color_btn, 0, wxALL, 2);
    sizer->Add(row, 0, wxALL, 2);

    m_gui_grp->SetSizer(sizer);
    m_sub_tabs->AddPage(m_gui_grp, "GUI");
}

void SettingsPanel::init_kbd_grp(int x, int y, int w, int h) {
    m_kbd_grp = new wxPanel(m_sub_tabs, wxID_ANY);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(m_kbd_grp, wxID_ANY, "Keyboard Layout:"), 0, wxALL, 2);
    m_kbd_layout = new wxChoice(m_kbd_grp, wxID_ANY, wxDefaultPosition, wxSize(120, 25));
    m_kbd_layout->Append("QWERTY");
    m_kbd_layout->Append("AZERTY");
    m_kbd_layout->Append("QWERTZ");
    
    KeyboardLayout layout = m_engine.m_key_bindings.get_layout();
    if (layout == KeyboardLayout::QWERTZ) m_kbd_layout->SetSelection(2);
    else if (layout == KeyboardLayout::QWERTY) m_kbd_layout->SetSelection(0);
    else m_kbd_layout->SetSelection(0); // Default

    m_kbd_layout->Bind(wxEVT_CHOICE, &SettingsPanel::on_kbd_layout, this);
    row->Add(m_kbd_layout, 0, wxALL, 2);
    sizer->Add(row, 0, wxALL, 2);

    row = new wxBoxSizer(wxHORIZONTAL);
    m_assign_btn = new wxButton(m_kbd_grp, wxID_ANY, "Assign Key", wxDefaultPosition, wxSize(100, 25));
    row->Add(m_assign_btn, 0, wxALL, 2);
    sizer->Add(row, 0, wxALL, 2);

    m_current_binding_box = new wxStaticText(m_kbd_grp, wxID_ANY, "No key assigned");
    sizer->Add(m_current_binding_box, 0, wxALL, 2);

    m_kbd_grp->SetSizer(sizer);
    m_sub_tabs->AddPage(m_kbd_grp, "Keyboard");
}

void SettingsPanel::init_misc_grp(int x, int y, int w, int h) {
    m_misc_grp = new wxPanel(m_sub_tabs, wxID_ANY);
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(new wxStaticText(m_misc_grp, wxID_ANY, "Misc settings coming soon"), 0, wxALL, 2);
    m_misc_grp->SetSizer(sizer);
    m_sub_tabs->AddPage(m_misc_grp, "Misc");
}

void SettingsPanel::on_reinit_audio(wxCommandEvent& event) {
    // In a real scenario we would re-init the audio backend
    m_engine.m_num_ins = m_audio_ins->GetValue();
    m_engine.m_num_outs = m_audio_outs->GetValue();
    wxMessageBox("Audio settings updated (restart required for some changes).", "Info", wxOK | wxICON_INFORMATION);
}

void SettingsPanel::on_reinit_midi(wxCommandEvent& event) {
    wxMessageBox("Reinitializing MIDI...", "Info", wxOK | wxICON_INFORMATION);
}

void SettingsPanel::on_gui_theme(wxCommandEvent& event) {
    int idx = event.GetSelection();
    if (idx != wxNOT_FOUND) {
        ThemeManager::apply_theme((ThemeType)idx);
        ThemeManager::apply_theme_and_settings(m_engine);
        // Refresh entire UI
        wxWindow* top = wxGetTopLevelParent(this);
        if (top) top->Refresh();
    }
}

void SettingsPanel::on_gui_btn_h(wxCommandEvent& event) {}
void SettingsPanel::on_gui_font_size(wxCommandEvent& event) {}

void SettingsPanel::on_waveform_color(wxCommandEvent& event) {
    wxColourData data;
    data.SetColour(wxColour(m_engine.m_waveform_color));
    wxColourDialog dlg(this, &data);
    if (dlg.ShowModal() == wxID_OK) {
        wxColour c = dlg.GetColourData().GetColour();
        m_engine.m_waveform_color = (c.Red() << 24) | (c.Green() << 16) | (c.Blue() << 8) | 255;
    }
}

void SettingsPanel::on_bg_color(wxCommandEvent& event) {
    wxColourData data;
    data.SetColour(wxColour(m_engine.m_bg_color));
    wxColourDialog dlg(this, &data);
    if (dlg.ShowModal() == wxID_OK) {
        wxColour c = dlg.GetColourData().GetColour();
        m_engine.m_bg_color = (c.Red() << 24) | (c.Green() << 16) | (c.Blue() << 8) | 255;
        ThemeManager::apply_theme(Theme{}); // Switch to Custom
        wxWindow* top = wxGetTopLevelParent(this);
        if (top) top->Refresh();
    }
}

void SettingsPanel::on_fg_color(wxCommandEvent& event) {
    wxColourData data;
    data.SetColour(wxColour(m_engine.m_fg_color));
    wxColourDialog dlg(this, &data);
    if (dlg.ShowModal() == wxID_OK) {
        wxColour c = dlg.GetColourData().GetColour();
        m_engine.m_fg_color = (c.Red() << 24) | (c.Green() << 16) | (c.Blue() << 8) | 255;
        ThemeManager::apply_theme(Theme{}); // Switch to Custom
        wxWindow* top = wxGetTopLevelParent(this);
        if (top) top->Refresh();
    }
}

void SettingsPanel::on_kbd_layout(wxCommandEvent& event) {
    int idx = event.GetSelection();
    if (idx == 0) m_engine.m_key_bindings.set_layout(KeyboardLayout::QWERTY);
    else if (idx == 1) /* AZERTY not fully implemented in KeyBindings yet, maybe? */;
    else if (idx == 2) m_engine.m_key_bindings.set_layout(KeyboardLayout::QWERTZ);
    
    m_engine.save_config();
}

void SettingsPanel::on_assign_key(wxCommandEvent& event) {}

} // namespace disgrace_ns
