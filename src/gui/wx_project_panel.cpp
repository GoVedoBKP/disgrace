#include "wx_project_panel.h"
#include "wx_main_window.h"
#include "../core/engine.h"
#include "../io/lilypond_exporter.h"

#include <wx/sizer.h>
#include <wx/msgdlg.h>
#include <wx/filedlg.h>
#include <wx/dir.h>

namespace disgrace_ns {

wxBEGIN_EVENT_TABLE(ProjectPanel, wxPanel)
    EVT_BUTTON(wxID_NEW, ProjectPanel::on_new)
    EVT_BUTTON(wxID_OPEN, ProjectPanel::on_load)
    EVT_BUTTON(wxID_FILE1, ProjectPanel::on_import)
    EVT_BUTTON(wxID_SAVE, ProjectPanel::on_save)
    EVT_BUTTON(wxID_FILE2, ProjectPanel::on_export)
    EVT_BUTTON(wxID_FILE3, ProjectPanel::on_export_ly)
wxEND_EVENT_TABLE()

ProjectPanel::ProjectPanel(wxWindow* parent, WxMainWindow* main_window, Engine& engine)
    : wxPanel(parent, wxID_ANY), m_main_window(main_window), m_engine(engine)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxHORIZONTAL);

    m_left_panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* left_sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* file_btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_new_btn = new wxButton(m_left_panel, wxID_NEW, "New", wxDefaultPosition, wxSize(60, 25));
    m_load_btn = new wxButton(m_left_panel, wxID_OPEN, "Load", wxDefaultPosition, wxSize(60, 25));
    m_import_btn = new wxButton(m_left_panel, wxID_FILE1, "Import", wxDefaultPosition, wxSize(60, 25));
    m_save_btn = new wxButton(m_left_panel, wxID_SAVE, "Save", wxDefaultPosition, wxSize(60, 25));
    m_export_btn = new wxButton(m_left_panel, wxID_FILE2, "Export", wxDefaultPosition, wxSize(60, 25));
    m_export_ly_btn = new wxButton(m_left_panel, wxID_FILE3, "Export LY", wxDefaultPosition, wxSize(60, 25));

    file_btn_sizer->Add(m_new_btn, 0, wxALL, 2);
    file_btn_sizer->Add(m_load_btn, 0, wxALL, 2);
    file_btn_sizer->Add(m_import_btn, 0, wxALL, 2);
    file_btn_sizer->Add(m_save_btn, 0, wxALL, 2);
    file_btn_sizer->Add(m_export_btn, 0, wxALL, 2);
    file_btn_sizer->Add(m_export_ly_btn, 0, wxALL, 2);
    left_sizer->Add(file_btn_sizer, 0, wxEXPAND | wxALL, 2);

    wxBoxSizer* browser_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_dir_picker = new wxDirPickerCtrl(m_left_panel, wxID_ANY, ".", "Choose Directory", wxDefaultPosition, wxSize(200, 25));
    m_dir_picker->Bind(wxEVT_DIRPICKER_CHANGED, &ProjectPanel::on_dir_changed, this);
    browser_sizer->Add(m_dir_picker, 0, wxALL, 2);
    left_sizer->Add(browser_sizer, 0, wxEXPAND | wxALL, 2);

    m_file_list = new wxListBox(m_left_panel, wxID_ANY, wxDefaultPosition, wxSize(400, 120));
    m_file_list->Bind(wxEVT_LISTBOX_DCLICK, &ProjectPanel::on_file_dclick, this);
    left_sizer->Add(m_file_list, 0, wxEXPAND | wxALL, 2);

    update_file_list(".");

    wxBoxSizer* export_opts_sizer = new wxBoxSizer(wxHORIZONTAL);
    export_opts_sizer->Add(new wxStaticText(m_left_panel, wxID_ANY, "Sample Rate:"), 0, wxALL, 2);
    m_sample_rate_ch = new wxChoice(m_left_panel, wxID_ANY, wxDefaultPosition, wxSize(80, 25));
    m_sample_rate_ch->Append("44100");
    m_sample_rate_ch->Append("48000");
    m_sample_rate_ch->Append("96000");
    m_sample_rate_ch->Select(1);
    export_opts_sizer->Add(m_sample_rate_ch, 0, wxALL, 2);
    left_sizer->Add(export_opts_sizer, 0, wxEXPAND | wxALL, 2);

    wxBoxSizer* check_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_separate_tracks_btn = new wxCheckBox(m_left_panel, wxID_ANY, "Separate Tracks");
    m_realtime_btn = new wxCheckBox(m_left_panel, wxID_ANY, "Realtime");
    check_sizer->Add(m_separate_tracks_btn, 0, wxALL, 2);
    check_sizer->Add(m_realtime_btn, 0, wxALL, 2);
    left_sizer->Add(check_sizer, 0, wxEXPAND | wxALL, 2);

    m_export_progress_bar = new wxGauge(m_left_panel, wxID_ANY, 100);
    left_sizer->Add(m_export_progress_bar, 0, wxEXPAND | wxALL, 2);

    m_left_panel->SetSizer(left_sizer);
    main_sizer->Add(m_left_panel, 0, wxEXPAND | wxALL, 2);

    m_right_panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* right_sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* track_btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_add_track_btn = new wxButton(m_right_panel, wxID_ANY, "Add Track", wxDefaultPosition, wxSize(80, 25));
    m_add_bus_btn = new wxButton(m_right_panel, wxID_ANY, "Add Bus", wxDefaultPosition, wxSize(80, 25));
    m_add_track_btn->Bind(wxEVT_BUTTON, &ProjectPanel::on_add_track, this);
    m_add_bus_btn->Bind(wxEVT_BUTTON, &ProjectPanel::on_add_bus, this);
    track_btn_sizer->Add(m_add_track_btn, 0, wxALL, 2);
    track_btn_sizer->Add(m_add_bus_btn, 0, wxALL, 2);
    right_sizer->Add(track_btn_sizer, 0, wxEXPAND | wxALL, 2);

    m_track_scroll = new wxScrolledWindow(m_right_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    m_track_container = new wxPanel(m_track_scroll, wxID_ANY);
    right_sizer->Add(m_track_scroll, 1, wxEXPAND | wxALL, 2);

    m_right_panel->SetSizer(right_sizer);
    main_sizer->Add(m_right_panel, 1, wxEXPAND | wxALL, 2);

    SetSizer(main_sizer);

    update_track_list();
}

void ProjectPanel::update_track_list() {
    m_track_container->DestroyChildren();

    int row_h = 35;
    int start_y = 2;
    int label_w = 60;
    int input_w = 120;
    int choice_w = 100;
    int btn_w = 30;

    size_t num_tracks = m_engine.track_count();
    size_t num_insts = m_engine.instrument_count();
    size_t num_buses = m_engine.bus_count();

    for (size_t i = 0; i < num_tracks; ++i) {
        auto& track_obj = m_engine.track(i);

        wxPanel* track_row = new wxPanel(m_track_container, wxID_ANY, wxPoint(2, start_y + (int)i * row_h), wxSize(700, row_h));
        wxBoxSizer* row_sizer = new wxBoxSizer(wxHORIZONTAL);

        wxString idx_str;
        idx_str.Printf("TRK %zu:", i + 1);
        wxStaticText* label = new wxStaticText(track_row, wxID_ANY, idx_str, wxDefaultPosition, wxSize(label_w, 25));
        row_sizer->Add(label, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);

        wxTextCtrl* name_in = new wxTextCtrl(track_row, wxID_ANY, track_obj.name(), wxDefaultPosition, wxSize(input_w, 25));
        name_in->Bind(wxEVT_TEXT, [this, i, name_in](wxCommandEvent&) {
            m_engine.track(i).set_name(name_in->GetValue().ToStdString());
        });
        row_sizer->Add(name_in, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);

        wxChoice* inst_ch = new wxChoice(track_row, wxID_ANY, wxDefaultPosition, wxSize(choice_w, 25));
        inst_ch->Append("None");
        for (size_t j = 0; j < num_insts; ++j) {
            inst_ch->Append(m_engine.instrument(j).name());
        }
        int inst_idx = m_engine.get_instrument_index(track_obj.instrument());
        inst_ch->Select(inst_idx + 1);
        inst_ch->Bind(wxEVT_CHOICE, [this, i, inst_ch](wxCommandEvent&) {
            int sel = inst_ch->GetSelection();
            if (sel == 0) {
                m_engine.track(i).set_instrument(nullptr);
            } else {
                m_engine.track(i).set_instrument(&m_engine.instrument(sel - 1));
            }
            update_track_list();
        });
        row_sizer->Add(inst_ch, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);

        wxChoice* notation_ch = new wxChoice(track_row, wxID_ANY, wxDefaultPosition, wxSize(100, 25));
        notation_ch->Append("Violin");
        notation_ch->Append("Bass");
        notation_ch->Append("Violin+Bass");
        notation_ch->Append("Drums");
        notation_ch->Select((int)track_obj.notation());
        notation_ch->Bind(wxEVT_CHOICE, [this, i, notation_ch](wxCommandEvent&) {
            m_engine.track(i).set_notation((NotationType)notation_ch->GetSelection());
        });
        
        Instrument* inst_ptr = track_obj.instrument();
        if (inst_ptr && (inst_ptr->type() == InstrumentType::SoundFont || 
                         inst_ptr->type() == InstrumentType::Plugin || 
                         inst_ptr->type() == InstrumentType::Midi)) {
            notation_ch->Enable(true);
        } else {
            notation_ch->Enable(false);
        }
        row_sizer->Add(notation_ch, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);

        wxChoice* out_ch = new wxChoice(track_row, wxID_ANY, wxDefaultPosition, wxSize(80, 25));
        out_ch->Append("Master");
        for (size_t j = 0; j < num_buses; ++j) {
            wxString b_name;
            b_name.Printf("Bus %zu", j + 1);
            out_ch->Append(b_name);
        }
        out_ch->Select(track_obj.output_bus() + 1);
        out_ch->Bind(wxEVT_CHOICE, [this, i, out_ch](wxCommandEvent&) {
            m_engine.track(i).set_output_bus(out_ch->GetSelection() - 1);
        });
        row_sizer->Add(out_ch, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);

        wxButton* up_btn = new wxButton(track_row, wxID_ANY, "^", wxDefaultPosition, wxSize(btn_w, 25));
        up_btn->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&) {
            if (i > 0) {
                m_engine.move_track(i, i - 1);
                update_track_list();
            }
        });
        row_sizer->Add(up_btn, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);

        wxButton* down_btn = new wxButton(track_row, wxID_ANY, "v", wxDefaultPosition, wxSize(btn_w, 25));
        down_btn->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&) {
            if (i < m_engine.track_count() - 1) {
                m_engine.move_track(i, i + 1);
                update_track_list();
            }
        });
        row_sizer->Add(down_btn, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);

        wxButton* del_btn = new wxButton(track_row, wxID_ANY, "X", wxDefaultPosition, wxSize(btn_w, 25));
        del_btn->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&) {
            m_engine.remove_track(i);
            update_track_list();
            if (m_main_window) m_main_window->update_all_uis();
        });
        row_sizer->Add(del_btn, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);

        track_row->SetSizer(row_sizer);
    }

    int total_h = std::max((int)num_tracks * row_h + 10, 200);
    m_track_container->SetSize(wxSize(720, total_h));
    m_track_scroll->SetVirtualSize(720, total_h);
}

void ProjectPanel::on_new(wxCommandEvent& event) {
    m_engine.new_project();
    update_track_list();
    if (m_main_window) m_main_window->update_all_uis();
    wxMessageBox("New project created", "Info", wxOK | wxICON_INFORMATION);
}

void ProjectPanel::on_load(wxCommandEvent& event) {
    wxFileDialog dlg(this, "Load Project", "", "", "Disgrace Projects\t*.dg", wxFD_OPEN);
    if (dlg.ShowModal() == wxID_OK) {
        m_engine.load_project(dlg.GetPath().ToStdString());
        if (m_main_window) m_main_window->update_all_uis();
    }
}

void ProjectPanel::on_import(wxCommandEvent& event) {
    wxFileDialog dlg(this, "Import Audio", "", "", "Audio Files\t*.{wav,flac,ogg,mp3,aiff,sf2,xrns}", wxFD_OPEN);
    if (dlg.ShowModal() == wxID_OK) {
        m_engine.import_audio(dlg.GetPath().ToStdString());
        if (m_main_window) m_main_window->update_all_uis();
    }
}

void ProjectPanel::on_save(wxCommandEvent& event) {
    wxFileDialog dlg(this, "Save Project", "", "", "Disgrace Projects\t*.dg", wxFD_SAVE);
    if (dlg.ShowModal() == wxID_OK) {
        m_engine.save_project(dlg.GetPath().ToStdString());
    }
}

void ProjectPanel::on_export(wxCommandEvent& event) {
    wxFileDialog dlg(this, "Export to WAV", "", "", "WAV Files|*.wav", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        Engine::ExportOptions opts;
        wxString sr = m_sample_rate_ch->GetStringSelection();
        if (!sr.empty()) opts.sample_rate = wxAtoi(sr);
        opts.separate_tracks = m_separate_tracks_btn->GetValue();
        opts.realtime = m_realtime_btn->GetValue();
        
        if (m_engine.render_to_wav(dlg.GetPath().ToStdString(), opts)) {
            wxMessageBox("Export completed successfully", "Success", wxOK | wxICON_INFORMATION);
        } else {
            wxMessageBox("Export failed", "Error", wxOK | wxICON_ERROR);
        }
    }
}

void ProjectPanel::on_export_ly(wxCommandEvent& event) {
    wxFileDialog dlg(this, "Export to LilyPond", "", "", "LilyPond Files|*.ly", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        if (LilyPondExporter::export_project(m_engine, dlg.GetPath().ToStdString())) {
            wxMessageBox("LilyPond export completed", "Success", wxOK | wxICON_INFORMATION);
        } else {
            wxMessageBox("LilyPond export failed", "Error", wxOK | wxICON_ERROR);
        }
    }
}

void ProjectPanel::on_file_select(wxCommandEvent& event) {}

void ProjectPanel::on_dir_changed(wxFileDirPickerEvent& event) {
    update_file_list(event.GetPath());
}

void ProjectPanel::on_file_dclick(wxCommandEvent& event) {
    int sel = m_file_list->GetSelection();
    if (sel != wxNOT_FOUND) {
        wxString filename = m_file_list->GetString(sel);
        wxString dir = m_dir_picker->GetPath();
        wxString full_path = dir + "/" + filename;
        
        if (wxMessageBox("Load project " + filename + "? All unsaved changes will be lost.", 
                        "Confirm", wxYES_NO | wxICON_QUESTION) == wxYES) {
            m_engine.load_project(full_path.ToStdString());
            if (m_main_window) m_main_window->update_all_uis();
        }
    }
}

void ProjectPanel::update_file_list(const wxString& dir) {
    m_file_list->Clear();
    wxDir directory(dir);
    if (directory.IsOpened()) {
        wxString filename;
        bool cont = directory.GetFirst(&filename, "*.dg", wxDIR_FILES);
        while (cont) {
            m_file_list->Append(filename);
            cont = directory.GetNext(&filename);
        }
    }
}

void ProjectPanel::on_add_track(wxCommandEvent& event) {
    m_engine.add_track();
    update_track_list();
    if (m_main_window) m_main_window->update_all_uis();
}

void ProjectPanel::on_remove_track(wxCommandEvent& event) {}

void ProjectPanel::on_track_name(wxCommandEvent& event) {}

void ProjectPanel::on_track_inst(wxCommandEvent& event) {}

void ProjectPanel::on_track_notation(wxCommandEvent& event) {}

void ProjectPanel::on_track_output(wxCommandEvent& event) {}

void ProjectPanel::on_move_track_up(wxCommandEvent& event) {}

void ProjectPanel::on_move_track_down(wxCommandEvent& event) {}

void ProjectPanel::on_add_bus(wxCommandEvent& event) {
    m_engine.add_bus();
    update_track_list();
    if (m_main_window) m_main_window->update_all_uis();
}

void ProjectPanel::on_remove_bus(wxCommandEvent& event) {}

void ProjectPanel::on_bus_name(wxCommandEvent& event) {}

void ProjectPanel::on_bus_output(wxCommandEvent& event) {}

} // namespace disgrace_ns
