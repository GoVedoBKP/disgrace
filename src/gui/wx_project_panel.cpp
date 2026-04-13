#include "wx_project_panel.h"
#include "wx_main_window.h"
#include "../core/engine.h"
#include "../io/lilypond_exporter.h"

#include <wx/sizer.h>
#include <wx/msgdlg.h>
#include <wx/filedlg.h>
#include <wx/dir.h>
#include <wx/artprov.h>
#include <wx/spinctrl.h>
#include <wx/timer.h>
#include <thread>
#include <atomic>

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

    // File Operations Group
    wxStaticBoxSizer* file_group = new wxStaticBoxSizer(wxVERTICAL, m_left_panel, "File Operations");
    wxFlexGridSizer* file_btn_grid = new wxFlexGridSizer(2, 3, 5, 5);
    file_btn_grid->AddGrowableCol(0);
    file_btn_grid->AddGrowableCol(1);
    file_btn_grid->AddGrowableCol(2);

    auto create_btn = [&](wxWindowID id, const wxString& label) {
        return new wxButton(m_left_panel, id, label, wxDefaultPosition, wxSize(-1, 30));
    };

    m_new_btn = create_btn(wxID_NEW, "New");
    m_load_btn = create_btn(wxID_OPEN, "Load");
    m_import_btn = create_btn(wxID_FILE1, "Import");
    m_save_btn = create_btn(wxID_SAVE, "Save");
    m_export_btn = create_btn(wxID_FILE2, "Export");
    m_export_ly_btn = create_btn(wxID_FILE3, "Export LY");

    file_btn_grid->Add(m_new_btn, 1, wxEXPAND);
    file_btn_grid->Add(m_load_btn, 1, wxEXPAND);
    file_btn_grid->Add(m_import_btn, 1, wxEXPAND);
    file_btn_grid->Add(m_save_btn, 1, wxEXPAND);
    file_btn_grid->Add(m_export_btn, 1, wxEXPAND);
    file_btn_grid->Add(m_export_ly_btn, 1, wxEXPAND);
    
    file_group->Add(file_btn_grid, 0, wxEXPAND | wxALL, 5);
    left_sizer->Add(file_group, 0, wxEXPAND | wxALL, 5);

    // Browser Group
    wxStaticBoxSizer* browser_group = new wxStaticBoxSizer(wxVERTICAL, m_left_panel, "Project Browser");
    m_dir_picker = new wxDirPickerCtrl(m_left_panel, wxID_ANY, ".", "Choose Directory", wxDefaultPosition, wxDefaultSize, wxDIRP_DEFAULT_STYLE | wxDIRP_SMALL);
    m_dir_picker->Bind(wxEVT_DIRPICKER_CHANGED, &ProjectPanel::on_dir_changed, this);
    browser_group->Add(m_dir_picker, 0, wxEXPAND | wxALL, 2);

    m_file_list = new wxListBox(m_left_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    m_file_list->Bind(wxEVT_LISTBOX_DCLICK, &ProjectPanel::on_file_dclick, this);
    browser_group->Add(m_file_list, 1, wxEXPAND | wxALL, 2);
    left_sizer->Add(browser_group, 1, wxEXPAND | wxALL, 5);

    update_file_list(".");

    // Project Metadata Fields
    wxStaticBoxSizer* meta_sizer = new wxStaticBoxSizer(wxVERTICAL, m_left_panel, "Project Metadata");
    wxFlexGridSizer* meta_grid = new wxFlexGridSizer(4, 2, 5, 5);
    meta_grid->AddGrowableCol(1);
    
    auto add_meta_field = [&](const wxString& label, wxTextCtrl** ctrl, const std::string& initial_val, auto setter) {
        meta_grid->Add(new wxStaticText(m_left_panel, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        *ctrl = new wxTextCtrl(m_left_panel, wxID_ANY, initial_val);
        (*ctrl)->Bind(wxEVT_TEXT, [this, setter, ctrl](wxCommandEvent&) {
            (m_engine.*setter)((*ctrl)->GetValue().ToStdString());
        });
        meta_grid->Add(*ctrl, 1, wxEXPAND);
    };

    add_meta_field("Title:", &m_title_in, m_engine.project_title(), &Engine::set_project_title);
    add_meta_field("Artist:", &m_artist_in, m_engine.project_artist(), &Engine::set_project_artist);
    add_meta_field("Album:", &m_album_in, m_engine.project_album(), &Engine::set_project_album);
    add_meta_field("Year:", &m_year_in, m_engine.project_year(), &Engine::set_project_year);

    meta_sizer->Add(meta_grid, 1, wxEXPAND | wxALL, 5);
    left_sizer->Add(meta_sizer, 0, wxEXPAND | wxALL, 5);

    // Export Options Group
    wxStaticBoxSizer* export_group = new wxStaticBoxSizer(wxVERTICAL, m_left_panel, "Export Settings");
    
    wxBoxSizer* sr_row = new wxBoxSizer(wxHORIZONTAL);
    sr_row->Add(new wxStaticText(m_left_panel, wxID_ANY, "Sample Rate:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    m_sample_rate_ch = new wxChoice(m_left_panel, wxID_ANY);
    m_sample_rate_ch->Append("44100");
    m_sample_rate_ch->Append("48000");
    m_sample_rate_ch->Append("96000");
    m_sample_rate_ch->Select(1);
    sr_row->Add(m_sample_rate_ch, 1, wxEXPAND);
    export_group->Add(sr_row, 0, wxEXPAND | wxALL, 5);

    wxBoxSizer* check_row = new wxBoxSizer(wxHORIZONTAL);
    m_separate_tracks_btn = new wxCheckBox(m_left_panel, wxID_ANY, "Separate Tracks");
    m_realtime_btn = new wxCheckBox(m_left_panel, wxID_ANY, "Realtime Render");
    check_row->Add(m_separate_tracks_btn, 1, wxEXPAND);
    check_row->Add(m_realtime_btn, 1, wxEXPAND);
    export_group->Add(check_row, 0, wxEXPAND | wxALL, 5);

    m_export_progress_bar = new wxGauge(m_left_panel, wxID_ANY, 100);
    export_group->Add(m_export_progress_bar, 0, wxEXPAND | wxALL, 5);
    
    left_sizer->Add(export_group, 0, wxEXPAND | wxALL, 5);

    m_left_panel->SetSizer(left_sizer);
    main_sizer->Add(m_left_panel, 3, wxEXPAND | wxALL, 2);

    m_right_panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer* right_sizer = new wxBoxSizer(wxVERTICAL);

    wxBoxSizer* track_btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_add_track_btn = new wxButton(m_right_panel, wxID_ANY, "Add Track", wxDefaultPosition, wxSize(-1, 30));
    m_add_track_btn->SetBitmap(wxArtProvider::GetBitmap(wxART_PLUS, wxART_BUTTON, wxSize(16, 16)));
    m_add_bus_btn = new wxButton(m_right_panel, wxID_ANY, "Add Bus", wxDefaultPosition, wxSize(-1, 30));
    m_add_bus_btn->SetBitmap(wxArtProvider::GetBitmap(wxART_PLUS, wxART_BUTTON, wxSize(16, 16)));
    m_add_track_btn->Bind(wxEVT_BUTTON, &ProjectPanel::on_add_track, this);
    m_add_bus_btn->Bind(wxEVT_BUTTON, &ProjectPanel::on_add_bus, this);
    track_btn_sizer->Add(m_add_track_btn, 1, wxEXPAND | wxALL, 2);
    track_btn_sizer->Add(m_add_bus_btn, 1, wxEXPAND | wxALL, 2);
    right_sizer->Add(track_btn_sizer, 0, wxEXPAND | wxALL, 5);

    m_track_scroll = new wxScrolledWindow(m_right_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    m_track_scroll->SetScrollRate(0, 5);
    m_track_container = new wxPanel(m_track_scroll, wxID_ANY);
    right_sizer->Add(m_track_scroll, 1, wxEXPAND | wxALL, 2);

    m_right_panel->SetSizer(right_sizer);
    main_sizer->Add(m_right_panel, 7, wxEXPAND | wxALL, 2);

    SetSizer(main_sizer);

    update_track_list();
}

void ProjectPanel::update_metadata() {
    if (m_title_in) m_title_in->ChangeValue(m_engine.project_title());
    if (m_artist_in) m_artist_in->ChangeValue(m_engine.project_artist());
    if (m_album_in) m_album_in->ChangeValue(m_engine.project_album());
    if (m_year_in) m_year_in->ChangeValue(m_engine.project_year());
}

void ProjectPanel::update_track_list() {
    m_track_container->DestroyChildren();

    wxFlexGridSizer* grid_sizer = new wxFlexGridSizer(0, 10, 5, 5);
    grid_sizer->AddGrowableCol(1); // Track name should grow

    size_t num_tracks = m_engine.track_count();
    size_t num_insts = m_engine.instrument_count();
    size_t num_buses = m_engine.bus_count();

    // Header row
    auto make_hdr = [&](const wxString& text, const wxString& tip = wxEmptyString) {
        wxStaticText* lbl = new wxStaticText(m_track_container, wxID_ANY, text);
        wxFont f = lbl->GetFont();
        f.SetWeight(wxFONTWEIGHT_BOLD);
        lbl->SetFont(f);
        if (!tip.IsEmpty()) lbl->SetToolTip(tip);
        return lbl;
    };
    grid_sizer->Add(make_hdr(""),          0, wxALIGN_CENTER_VERTICAL | wxALL, 2); // index
    grid_sizer->Add(make_hdr("Name"),      0, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    grid_sizer->Add(make_hdr("Instrument"),0, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    grid_sizer->Add(make_hdr("Notation"),  0, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    grid_sizer->Add(make_hdr("Output"),    0, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    grid_sizer->Add(make_hdr("V\u00b1",   "Velocity humanization: \u00b1 spread in units (0 = off, SF/Plugin/MIDI only)"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    grid_sizer->Add(make_hdr("T ms",       "Timing humanization: max random onset delay in ms (0 = off, SF/Plugin/MIDI only)"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 2);
    grid_sizer->AddSpacer(1); // up
    grid_sizer->AddSpacer(1); // down
    grid_sizer->AddSpacer(1); // remove

    // Display tracks
    for (size_t i = 0; i < num_tracks; ++i) {
        auto& track_obj = m_engine.track(i);

        // Column 0: Index Label
        wxString idx_str;
        idx_str.Printf("TRK %zu:", i + 1);
        grid_sizer->Add(new wxStaticText(m_track_container, wxID_ANY, idx_str), 0, wxALIGN_CENTER_VERTICAL | wxALL, 2);

        // Column 1: Name Input
        wxTextCtrl* name_in = new wxTextCtrl(m_track_container, wxID_ANY, track_obj.name(), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
        name_in->Bind(wxEVT_TEXT, [this, i, name_in](wxCommandEvent&) {
            m_engine.track(i).set_name(name_in->GetValue().ToStdString());
        });
        grid_sizer->Add(name_in, 1, wxEXPAND | wxALL, 2);

        // Column 2: Instrument Choice
        wxChoice* inst_ch = new wxChoice(m_track_container, wxID_ANY);
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
            CallAfter([this]() { update_track_list(); });
        });
        grid_sizer->Add(inst_ch, 0, wxEXPAND | wxALL, 2);

        // Column 3: Notation Choice
        wxChoice* notation_ch = new wxChoice(m_track_container, wxID_ANY);
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
                         inst_ptr->type() == InstrumentType::SFZ ||
                         inst_ptr->type() == InstrumentType::XRNI ||
                         inst_ptr->type() == InstrumentType::Plugin || 
                         inst_ptr->type() == InstrumentType::Midi)) {
            notation_ch->Enable(true);
        } else {
            notation_ch->Enable(false);
        }
        grid_sizer->Add(notation_ch, 0, wxEXPAND | wxALL, 2);

        // Column 4: Output Bus Choice
        wxChoice* out_ch = new wxChoice(m_track_container, wxID_ANY);
        out_ch->Append("Master");
        for (size_t j = 1; j < num_buses; ++j) {
            wxString b_name = m_engine.bus(j).name();
            out_ch->Append(b_name);
        }
        
        int current_out = track_obj.output_bus();
        if (current_out == MixerBus::ROUTE_MASTER) {
            out_ch->Select(0);
        } else if (current_out >= 0 && (size_t)current_out < num_buses) {
            out_ch->Select(current_out + 1);
        }
        
        out_ch->Bind(wxEVT_CHOICE, [this, i, out_ch](wxCommandEvent&) {
            int sel = out_ch->GetSelection();
            if (sel == 0) {
                m_engine.track(i).set_output_bus(MixerBus::ROUTE_MASTER);
            } else {
                m_engine.track(i).set_output_bus(sel - 1);
            }
        });
        grid_sizer->Add(out_ch, 0, wxEXPAND | wxALL, 2);

        // Columns 5, 6: Velocity & timing humanization - active only for SoundFont/SFZ/Plugin/MIDI
        bool hum_enabled = inst_ptr &&
                           (inst_ptr->type() == InstrumentType::SoundFont ||
                            inst_ptr->type() == InstrumentType::SFZ ||
                            inst_ptr->type() == InstrumentType::XRNI ||
                            inst_ptr->type() == InstrumentType::Plugin ||
                            inst_ptr->type() == InstrumentType::Midi);

        wxSpinCtrl* vel_spin = new wxSpinCtrl(m_track_container, wxID_ANY, wxEmptyString,
                                               wxDefaultPosition, wxSize(55, -1),
                                               wxSP_ARROW_KEYS, 0, 64, track_obj.humanize_vel());
        vel_spin->SetToolTip("Velocity humanization: +/- spread in velocity units (0 = off)");
        vel_spin->Enable(hum_enabled);
        vel_spin->Bind(wxEVT_SPINCTRL, [this, i](wxSpinEvent& ev) {
            m_engine.track(i).set_humanize_vel((uint8_t)ev.GetValue());
            m_engine.mark_dirty();
        });
        grid_sizer->Add(vel_spin, 0, wxALIGN_CENTER_VERTICAL | wxALL, 2);

        wxSpinCtrl* tim_spin = new wxSpinCtrl(m_track_container, wxID_ANY, wxEmptyString,
                                               wxDefaultPosition, wxSize(55, -1),
                                               wxSP_ARROW_KEYS, 0, 100, track_obj.humanize_timing());
        tim_spin->SetToolTip("Timing humanization: max random onset delay in ms (0 = off)");
        tim_spin->Enable(hum_enabled);
        tim_spin->Bind(wxEVT_SPINCTRL, [this, i](wxSpinEvent& ev) {
            m_engine.track(i).set_humanize_timing((uint8_t)ev.GetValue());
            m_engine.mark_dirty();
        });
        grid_sizer->Add(tim_spin, 0, wxALIGN_CENTER_VERTICAL | wxALL, 2);

        // Columns 7, 8, 9: Control Buttons
        auto create_small_btn = [&](const wxArtID& art, const wxString& backup_label, auto func) {
            wxButton* btn = new wxButton(m_track_container, wxID_ANY, backup_label, wxDefaultPosition, wxSize(30, 25));
            btn->SetBitmap(wxArtProvider::GetBitmap(art, wxART_BUTTON, wxSize(14, 14)));
            btn->Bind(wxEVT_BUTTON, func);
            return btn;
        };

        grid_sizer->Add(create_small_btn(wxART_GO_UP, "^", [this, i](wxCommandEvent&) {
            if (i > 0) { m_engine.move_track(i, i - 1); CallAfter([this]() { update_track_list(); }); }
        }), 0, wxALL, 1);

        grid_sizer->Add(create_small_btn(wxART_GO_DOWN, "v", [this, i](wxCommandEvent&) {
            if (i < m_engine.track_count() - 1) { m_engine.move_track(i, i + 1); CallAfter([this]() { update_track_list(); }); }
        }), 0, wxALL, 1);

        wxButton* rem_btn = create_small_btn(wxART_DELETE, "X", [this, i](wxCommandEvent&) {
            m_engine.remove_track(i); CallAfter([this]() { update_track_list(); });
        });
        rem_btn->SetForegroundColour(*wxRED);
        grid_sizer->Add(rem_btn, 0, wxALL, 1);
    }

    // Display buses (skip master bus at index 0)
    for (size_t i = 1; i < num_buses; ++i) {
        auto& bus_obj = m_engine.bus(i);

        // Column 0: Index Label
        wxString idx_str;
        idx_str.Printf("BUS %zu:", i);
        grid_sizer->Add(new wxStaticText(m_track_container, wxID_ANY, idx_str), 0, wxALIGN_CENTER_VERTICAL | wxALL, 2);

        // Column 1: Name Input
        wxTextCtrl* name_in = new wxTextCtrl(m_track_container, wxID_ANY, bus_obj.name(), wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
        name_in->Bind(wxEVT_TEXT, [this, i, name_in](wxCommandEvent&) {
            m_engine.bus(i).set_name(name_in->GetValue().ToStdString());
        });
        grid_sizer->Add(name_in, 1, wxEXPAND | wxALL, 2);

        // Columns 2, 3: Spacers (instrument/notation - N/A for buses)
        grid_sizer->AddSpacer(10);
        grid_sizer->AddSpacer(10);

        // Column 4: Output Choice (Hierarchical Buses + Hardware)
        wxChoice* out_ch = new wxChoice(m_track_container, wxID_ANY);
        std::vector<int> bus_outputs;
        
        // 1. Master Bus
        out_ch->Append("Master Bus");
        bus_outputs.push_back(MixerBus::ROUTE_MASTER);
        
        // 2. Hierarchical Buses (only those with index < current bus to avoid loops)
        for (size_t j = 1; j < i; ++j) {
            out_ch->Append("Bus: " + m_engine.bus(j).name());
            bus_outputs.push_back((int)j);
        }
        
        // 3. Hardware Stereo Pairs
        uint32_t num_outs = m_engine.m_num_outs;
        for (uint32_t j = 0; j < num_outs; j += 2) {
            if (j + 1 < num_outs) {
                out_ch->Append(wxString::Format("Hardware: Out %u/%u", j+1, j+2));
                bus_outputs.push_back(MixerBus::ROUTE_HW_STEREO_BASE - (int)(j/2));
            } else {
                out_ch->Append(wxString::Format("Hardware: Out %u", j+1));
                bus_outputs.push_back(MixerBus::ROUTE_HW_MONO_BASE - (int)j);
            }
        }
        
        // 4. Hardware Mono Outputs
        for (uint32_t j = 0; j < num_outs; ++j) {
            out_ch->Append(wxString::Format("Hardware: Mono Out %u", j+1));
            bus_outputs.push_back(MixerBus::ROUTE_HW_MONO_BASE - (int)j);
        }

        // Set current selection
        int current_out = bus_obj.output_bus();
        for (size_t k = 0; k < bus_outputs.size(); ++k) {
            if (bus_outputs[k] == current_out) {
                out_ch->Select((int)k);
                break;
            }
        }
        
        out_ch->Bind(wxEVT_CHOICE, [this, i, out_ch, bus_outputs](wxCommandEvent&) {
            int sel = out_ch->GetSelection();
            if (sel >= 0 && sel < (int)bus_outputs.size()) {
                m_engine.bus(i).set_output_bus(bus_outputs[sel]);
            }
        });
        grid_sizer->Add(out_ch, 0, wxEXPAND | wxALL, 2);
        // Columns 5, 6: Spacers (humanization - N/A for buses)
        grid_sizer->AddSpacer(10);
        grid_sizer->AddSpacer(10);

        // Columns 7, 8, 9: Control Buttons
        auto create_small_btn = [&](const wxArtID& art, const wxString& backup_label, auto func) {
            wxButton* btn = new wxButton(m_track_container, wxID_ANY, backup_label, wxDefaultPosition, wxSize(30, 25));
            btn->SetBitmap(wxArtProvider::GetBitmap(art, wxART_BUTTON, wxSize(14, 14)));
            btn->Bind(wxEVT_BUTTON, func);
            return btn;
        };

        grid_sizer->Add(create_small_btn(wxART_GO_UP, "^", [this, i](wxCommandEvent&) {
            if (i > 1) { m_engine.move_bus(i, i - 1); update_track_list(); }
        }), 0, wxALL, 1);

        grid_sizer->Add(create_small_btn(wxART_GO_DOWN, "v", [this, i](wxCommandEvent&) {
            if (i < m_engine.bus_count() - 1) { m_engine.move_bus(i, i + 1); update_track_list(); }
        }), 0, wxALL, 1);

        wxButton* rem_btn = create_small_btn(wxART_DELETE, "X", [this, i](wxCommandEvent&) {
            m_engine.remove_bus(i); update_track_list();
        });
        rem_btn->SetForegroundColour(*wxRED);
        grid_sizer->Add(rem_btn, 0, wxALL, 1);
    }

    m_track_container->SetSizer(grid_sizer);
    grid_sizer->SetSizeHints(m_track_container);
    m_track_container->Layout();
    
    // Update scrolled window virtual size
    wxSize best_size = grid_sizer->GetMinSize();
    m_track_scroll->SetVirtualSize(best_size);
}

void ProjectPanel::on_new(wxCommandEvent& event) {
    m_engine.new_project();
    update_track_list();
    if (m_main_window) m_main_window->update_all_uis();
    wxMessageBox("New project created", "Info", wxOK | wxICON_INFORMATION);
}

void ProjectPanel::on_load(wxCommandEvent& event) {
    wxFileDialog dlg(this, "Load Project", "", "", "Disgrace Projects (*.dg;*.DG)|*.dg;*.DG", wxFD_OPEN);
    if (dlg.ShowModal() == wxID_OK) {
        m_engine.load_project(dlg.GetPath().ToStdString());
        if (m_main_window) m_main_window->update_all_uis();
    }
}

void ProjectPanel::on_import(wxCommandEvent& event) {
    wxFileDialog dlg(this, "Import Audio", "", "", "Audio Files (*.wav;*.flac;*.ogg;*.mp3;*.aiff;*.sf2;*.xrns)|*.wav;*.flac;*.ogg;*.mp3;*.aiff;*.sf2;*.xrns;*.WAV;*.FLAC;*.OGG;*.MP3;*.AIFF;*.SF2;*.XRNS", wxFD_OPEN);
    if (dlg.ShowModal() == wxID_OK) {
        m_engine.import_audio(dlg.GetPath().ToStdString());
        if (m_main_window) m_main_window->update_all_uis();
    }
}

void ProjectPanel::on_save(wxCommandEvent& event) {
    wxFileDialog dlg(this, "Save Project", "", "", "Disgrace Projects (*.dg)|*.dg", wxFD_SAVE);
    if (dlg.ShowModal() == wxID_OK) {
        m_engine.save_project(dlg.GetPath().ToStdString());
    }
}

void ProjectPanel::on_export(wxCommandEvent& event) {
    wxFileDialog dlg(this, "Export to WAV", "", "", "WAV Files|*.wav", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;

    Engine::ExportOptions opts;
    wxString sr = m_sample_rate_ch->GetStringSelection();
    if (!sr.empty()) opts.sample_rate = wxAtoi(sr);
    opts.separate_tracks = m_separate_tracks_btn->GetValue();
    opts.realtime = m_realtime_btn->GetValue();

    std::string path = dlg.GetPath().ToStdString();
    m_export_btn->Disable();
    m_export_progress_bar->SetValue(0);
    m_export_result.store(-1);

    std::thread([this, path, opts]() {
        bool ok = m_engine.render_to_wav(path, opts);
        m_export_result.store(ok ? 1 : 0);
    }).detach();

    if (!m_export_timer) {
        m_export_timer = new wxTimer(this, wxID_ANY);
        Bind(wxEVT_TIMER, &ProjectPanel::on_export_timer, this, m_export_timer->GetId());
    }
    m_export_timer->Start(200);
}

void ProjectPanel::on_export_timer(wxTimerEvent& /*event*/) {
    float prog = m_engine.m_export_progress.load();
    m_export_progress_bar->SetValue((int)(prog * 100.0f));

    int result = m_export_result.load();
    if (result >= 0) {
        m_export_timer->Stop();
        m_export_btn->Enable();
        m_export_progress_bar->SetValue(result > 0 ? 100 : 0);
        if (result > 0)
            wxMessageBox("Export completed successfully", "Success", wxOK | wxICON_INFORMATION);
        else
            wxMessageBox("Export failed", "Error", wxOK | wxICON_ERROR);
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
        cont = directory.GetFirst(&filename, "*.DG", wxDIR_FILES);
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
