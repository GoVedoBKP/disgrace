#include "project_panel.h"
#include "main_window.h"
#include "../core/engine.h"
#include "../io/lilypond_exporter.h"
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/filename.H>
#include <FL/fl_ask.H>

namespace disgrace_ns {

ProjectPanel::ProjectPanel(int x, int y, int w, int h, Engine& engine)
    : Fl_Group(x, y, w, h), m_engine(engine) {
    
    begin();

    int left_w = 300;
    int margin = 10;

    // Buttons at top left
    int cur_y = margin;
    int btn_w = (left_w - 4 * margin) / 3;

    m_new_btn = new Fl_Button(x + margin, y + cur_y, btn_w, 25, "New");
    m_new_btn->callback(cb_new, this);

    m_load_btn = new Fl_Button(x + 2 * margin + btn_w, y + cur_y, btn_w, 25, "Load");
    m_load_btn->callback(cb_load, this);

    m_save_btn = new Fl_Button(x + 3 * margin + 2 * btn_w, y + cur_y, btn_w, 25, "Save");
    m_save_btn->callback(cb_save, this);

    cur_y += 25 + margin;

    // Fixed control space at the bottom (Export button, 2 checkboxes, sample rate)
    int controls_h = 25 * 3 + 10 * 3 + margin; 
    int browser_h = h - cur_y - controls_h - margin;
    
    m_file_browser = new Fl_File_Browser(x + margin, y + cur_y, left_w - 2 * margin, browser_h);
    m_file_browser->type(FL_HOLD_BROWSER);
    m_file_browser->load(".");
    m_file_browser->callback(cb_file_select, this);

    cur_y += browser_h + margin;

    // Export controls
    Fl_Box* sr_label = new Fl_Box(x + margin, y + cur_y, 160, 25, "Export Sample Rate:");
    sr_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    m_sample_rate_ch = new Fl_Choice(x + margin + 170, y + cur_y, 100, 25);
    m_sample_rate_ch->add("44100");
    m_sample_rate_ch->add("48000");
    m_sample_rate_ch->add("88200");
    m_sample_rate_ch->add("96000");
    m_sample_rate_ch->add("192000");
    m_sample_rate_ch->value(0);

    cur_y += 25 + margin / 2;
    m_separate_tracks_btn = new Fl_Check_Button(x + margin, y + cur_y, 200, 25, "Separate Files (per track)");
    
    cur_y += 25 + margin / 2;
    m_realtime_btn = new Fl_Check_Button(x + margin, y + cur_y, 200, 25, "Realtime Export (for MIDI/HW)");

    cur_y += 25 + margin / 2;
    m_export_progress_bar = new Fl_Progress(x + margin, y + cur_y, left_w - 2 * margin, 15);
    m_export_progress_bar->minimum(0.0f);
    m_export_progress_bar->maximum(1.0f);
    m_export_progress_bar->value(0.0f);
    m_export_progress_bar->hide();

    m_export_btn = new Fl_Button(x + margin, y + h - 65, left_w - 2 * margin, 25, "Export to WAV");
    m_export_btn->callback(cb_export, this);

    m_export_ly_btn = new Fl_Button(x + margin, y + h - 35, left_w - 2 * margin, 25, "Export to LilyPond");
    m_export_ly_btn->callback(cb_export_ly, this);

    // Right side: Tracks
    int rcur_y = margin;
    m_add_track_btn = new Fl_Button(x + left_w + margin, y + rcur_y, 100, 25, "+ Add Track");
    m_add_track_btn->callback(cb_add_track, this);

    m_add_bus_btn = new Fl_Button(x + left_w + margin + 110, y + rcur_y, 100, 25, "+ Add Bus");
    m_add_bus_btn->callback(cb_add_bus, this);

    rcur_y += 25 + margin;

    m_track_scroll = new Fl_Scroll(x + left_w + margin, y + rcur_y, w - left_w - 2 * margin, h - rcur_y - margin);
    m_track_scroll->type(Fl_Scroll::VERTICAL);
    m_track_container = new Fl_Group(x + left_w + margin, y + rcur_y, w - left_w - 40, 1000);
    m_track_container->end();
    m_track_scroll->add(m_track_container);
    m_track_scroll->end();

    resizable(m_track_scroll);

    end();

    update_track_list();
}

void ProjectPanel::cb_new(Fl_Widget*, void* data) {
    auto* self = static_cast<ProjectPanel*>(data);
    if (fl_ask("Create new project? All unsaved changes will be lost.")) {
        self->m_engine.new_project();
        for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
            MainWindow* mw = dynamic_cast<MainWindow*>(win);
            if (mw) mw->request_update();
        }
    }
}

void ProjectPanel::cb_load(Fl_Widget*, void* data) {
    auto* self = static_cast<ProjectPanel*>(data);
    Fl_Native_File_Chooser fnfc;
    fnfc.title("Load Project");
    fnfc.type(Fl_Native_File_Chooser::BROWSE_FILE);
    fnfc.filter("Disgrace Projects\t*.dg\n");
    if (fnfc.show() == 0) {
        self->m_engine.load_project(fnfc.filename());
        for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
            MainWindow* mw = dynamic_cast<MainWindow*>(win);
            if (mw) mw->update_all_uis(); // Immediate, synchronous update
        }
    }
}

void ProjectPanel::cb_save(Fl_Widget*, void* data) {
    auto* self = static_cast<ProjectPanel*>(data);
    Fl_Native_File_Chooser fnfc;
    fnfc.title("Save Project");
    fnfc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    fnfc.filter("Disgrace Projects\t*.dg\n");
    if (fnfc.show() == 0) {
        self->m_engine.save_project(fnfc.filename());
    }
}

void ProjectPanel::cb_export(Fl_Widget*, void* data) {
    auto* self = static_cast<ProjectPanel*>(data);
    Fl_Native_File_Chooser fnfc;
    fnfc.title("Export to WAV");
    fnfc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    fnfc.filter("WAV Files\t*.wav\n");
    fnfc.preset_file("output.wav");
    if (fnfc.show() == 0) {
        Engine::ExportOptions opts;
        opts.sample_rate = std::stoul(self->m_sample_rate_ch->text(self->m_sample_rate_ch->value()));
        opts.separate_tracks = self->m_separate_tracks_btn->value();
        opts.realtime = self->m_realtime_btn->value();
        
        std::string path = fnfc.filename();
        self->m_export_btn->deactivate();
        self->m_export_progress_bar->value(0.0f);
        self->m_export_progress_bar->show();
        
        std::thread([self, path, opts]() {
            self->m_engine.render_to_wav(path, opts);
        }).detach();
        
        Fl::add_timeout(0.1, cb_export_timeout, self);
    }
}

void ProjectPanel::cb_export_timeout(void* data) {
    auto* self = static_cast<ProjectPanel*>(data);
    float p = self->m_engine.m_export_progress.load();
    self->m_export_progress_bar->value(p);
    
    if (self->m_engine.m_is_exporting.load()) {
        Fl::repeat_timeout(0.1, cb_export_timeout, self);
    } else {
        self->m_export_btn->activate();
        self->m_export_progress_bar->hide();
    }
}

void ProjectPanel::update_track_list() {
    for (int i = 0; i < m_track_container->children(); ++i) {
        void* d = m_track_container->child(i)->user_data();
        if (d) {
            // We need to be careful as some children might not have this exact type, 
            // but in this container they all do (std::pair<ProjectPanel*, size_t>*)
            delete static_cast<std::pair<ProjectPanel*, size_t>*>(d);
        }
    }
    m_track_container->clear();
    m_track_container->begin();

    int row_h = 35;
    int cur_y = m_track_container->y(); // Corrected: use container's absolute y
    int label_w = 30;
    int input_w = 150;
    int choice_w = 150;
    int btn_w = 30;

    size_t num_tracks = m_engine.track_count();
    size_t num_insts = m_engine.instrument_count();
    size_t num_buses = m_engine.bus_count();

    for (size_t i = 0; i < num_tracks; ++i) {
        int cur_x = m_track_container->x();
        int row_y = cur_y + (int)(i * row_h);
        
        char idx_str[16];
        snprintf(idx_str, 16, "TRK %zu:", i + 1);
        new Fl_Box(cur_x, row_y, label_w + 30, row_h, strdup(idx_str));
        cur_x += label_w + 35;

        Fl_Input* name_in = new Fl_Input(cur_x, row_y + 5, input_w - 50, 25);
        name_in->value(m_engine.track(i).name().c_str());
        name_in->maximum_size(32);
        name_in->callback(cb_track_name, new std::pair<ProjectPanel*, size_t>(this, i));
        name_in->when(FL_WHEN_ENTER_KEY | FL_WHEN_RELEASE);
        cur_x += input_w - 50 + 5;

        Fl_Choice* inst_ch = new Fl_Choice(cur_x, row_y + 5, choice_w - 20, 25);
        inst_ch->add("None");
        for (size_t j = 0; j < num_insts; ++j) {
            inst_ch->add(m_engine.instrument(j).name().c_str());
        }
        
        int inst_idx = m_engine.get_instrument_index(m_engine.track(i).instrument());
        inst_ch->value(inst_idx + 1);
        inst_ch->callback(cb_track_inst, new std::pair<ProjectPanel*, size_t>(this, i));
        cur_x += choice_w - 20 + 5;

        // Notation Type Dropdown
        Fl_Choice* notation_ch = new Fl_Choice(cur_x, row_y + 5, 100, 25);
        notation_ch->add("Violin|Bass|Violin+Bass|Drums");
        notation_ch->value((int)m_engine.track(i).notation());
        notation_ch->callback(cb_track_notation, new std::pair<ProjectPanel*, size_t>(this, i));
        
        Instrument* inst_ptr = m_engine.track(i).instrument();
        if (inst_ptr && (inst_ptr->type() == InstrumentType::SoundFont || 
                         inst_ptr->type() == InstrumentType::Plugin || 
                         inst_ptr->type() == InstrumentType::Midi)) {
            notation_ch->activate();
        } else {
            notation_ch->deactivate();
        }
        cur_x += 100 + 5;

        Fl_Choice* out_ch = new Fl_Choice(cur_x, row_y + 5, 80, 25);
        out_ch->add("Master");
        for (size_t j = 0; j < num_buses; ++j) {
            char b_name[32]; snprintf(b_name, 32, "Bus %zu", j + 1);
            out_ch->add(b_name);
        }
        out_ch->value(m_engine.track(i).output_bus() + 1);
        out_ch->callback(cb_track_output, new std::pair<ProjectPanel*, size_t>(this, i));
        cur_x += 80 + 5;

        Fl_Button* up = new Fl_Button(cur_x, row_y + 5, btn_w, 25, "@8<");
        up->callback(cb_move_track_up, new std::pair<ProjectPanel*, size_t>(this, i));
        cur_x += btn_w + 2;

        Fl_Button* down = new Fl_Button(cur_x, row_y + 5, btn_w, 25, "@2<");
        down->callback(cb_move_track_down, new std::pair<ProjectPanel*, size_t>(this, i));
        cur_x += btn_w + 5;

        Fl_Button* rem = new Fl_Button(cur_x, row_y + 5, btn_w, 25, "X");
        rem->callback(cb_remove_track, new std::pair<ProjectPanel*, size_t>(this, i));
    }

    cur_y += (int)(num_tracks * row_h) + 10;

    for (size_t i = 0; i < num_buses; ++i) {
        int cur_x = m_track_container->x();
        int row_y = cur_y + (int)(i * row_h);

        char idx_str[16];
        snprintf(idx_str, 16, "BUS %zu:", i + 1);
        new Fl_Box(cur_x, row_y, label_w + 30, row_h, strdup(idx_str));
        cur_x += label_w + 35;

        Fl_Input* name_in = new Fl_Input(cur_x, row_y + 5, input_w - 50, 25);
        name_in->value(m_engine.bus(i).name().c_str());
        name_in->callback(cb_bus_name, new std::pair<ProjectPanel*, size_t>(this, i));
        name_in->when(FL_WHEN_ENTER_KEY | FL_WHEN_RELEASE);
        cur_x += input_w - 50 + 5;
        
        // Buses can route to other buses (forward only) or Master
        Fl_Choice* out_ch = new Fl_Choice(cur_x, row_y + 5, 80, 25);
        out_ch->add("Master");
        for (size_t j = 0; j < num_buses; ++j) {
            char b_name[32]; snprintf(b_name, 32, "Bus %zu", j + 1);
            out_ch->add(b_name);
        }
        out_ch->value(m_engine.bus(i).output_bus() + 1);
        out_ch->callback(cb_bus_output, new std::pair<ProjectPanel*, size_t>(this, i));
        cur_x += 80 + 5 + 130 + 5; // Skip inst and up/down for now or keep space

        Fl_Button* rem = new Fl_Button(cur_x, row_y + 5, btn_w, 25, "X");
        rem->callback(cb_remove_bus, new std::pair<ProjectPanel*, size_t>(this, i));
    }

    m_track_container->end();
    m_track_container->size(m_track_container->w(), (int)((num_tracks + num_buses) * row_h + 50));
    m_track_scroll->redraw();
}

void ProjectPanel::cb_add_track(Fl_Widget*, void* data) {
    auto* self = static_cast<ProjectPanel*>(data);
    self->m_engine.add_track();
    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
        MainWindow* mw = dynamic_cast<MainWindow*>(win);
        if (mw) mw->request_update();
    }
}

void ProjectPanel::cb_remove_track(Fl_Widget*, void* data) {
    auto* pair = static_cast<std::pair<ProjectPanel*, size_t>*>(data);
    if (fl_ask("Remove track %zu?", pair->second + 1)) {
        pair->first->m_engine.remove_track(pair->second);
        for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
            MainWindow* mw = dynamic_cast<MainWindow*>(win);
            if (mw) mw->request_update();
        }
    }
}

void ProjectPanel::cb_track_name(Fl_Widget* w, void* data) {
    auto* pair = static_cast<std::pair<ProjectPanel*, size_t>*>(data);
    Fl_Input* in = static_cast<Fl_Input*>(w);
    pair->first->m_engine.track(pair->second).set_name(in->value());
    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
        MainWindow* mw = dynamic_cast<MainWindow*>(win);
        if (mw) mw->request_update();
    }
}

void ProjectPanel::cb_track_inst(Fl_Widget* w, void* data) {
    auto* pair = static_cast<std::pair<ProjectPanel*, size_t>*>(data);
    Fl_Choice* ch = static_cast<Fl_Choice*>(w);
    int idx = ch->value();
    if (idx <= 0) {
        pair->first->m_engine.track(pair->second).set_instrument(nullptr);
    } else {
        pair->first->m_engine.track(pair->second).set_instrument(&pair->first->m_engine.instrument(idx - 1));
    }
    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
        MainWindow* mw = dynamic_cast<MainWindow*>(win);
        if (mw) mw->request_update();
    }
}

void ProjectPanel::cb_track_notation(Fl_Widget* w, void* data) {
    auto* pair = static_cast<std::pair<ProjectPanel*, size_t>*>(data);
    Fl_Choice* ch = static_cast<Fl_Choice*>(w);
    pair->first->m_engine.track(pair->second).set_notation((NotationType)ch->value());
}

void ProjectPanel::cb_track_output(Fl_Widget* w, void* data) {
    auto* pair = static_cast<std::pair<ProjectPanel*, size_t>*>(data);
    Fl_Choice* ch = static_cast<Fl_Choice*>(w);
    pair->first->m_engine.track(pair->second).set_output_bus(ch->value() - 1);
}

void ProjectPanel::cb_add_bus(Fl_Widget*, void* data) {
    auto* self = static_cast<ProjectPanel*>(data);
    self->m_engine.add_bus();
    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
        MainWindow* mw = dynamic_cast<MainWindow*>(win);
        if (mw) mw->request_update();
    }
}

void ProjectPanel::cb_remove_bus(Fl_Widget*, void* data) {
    auto* pair = static_cast<std::pair<ProjectPanel*, size_t>*>(data);
    if (fl_ask("Remove bus %zu?", pair->second + 1)) {
        pair->first->m_engine.remove_bus(pair->second);
        for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
            MainWindow* mw = dynamic_cast<MainWindow*>(win);
            if (mw) mw->request_update();
        }
    }
}

void ProjectPanel::cb_bus_name(Fl_Widget* w, void* data) {
    auto* pair = static_cast<std::pair<ProjectPanel*, size_t>*>(data);
    Fl_Input* in = static_cast<Fl_Input*>(w);
    pair->first->m_engine.bus(pair->second).set_name(in->value());
}

void ProjectPanel::cb_bus_output(Fl_Widget* w, void* data) {
    auto* pair = static_cast<std::pair<ProjectPanel*, size_t>*>(data);
    Fl_Choice* ch = static_cast<Fl_Choice*>(w);
    pair->first->m_engine.bus(pair->second).set_output_bus(ch->value() - 1);
}

void ProjectPanel::cb_move_track_up(Fl_Widget*, void* data) {
    auto* pair = static_cast<std::pair<ProjectPanel*, size_t>*>(data);
    if (pair->second > 0) {
        pair->first->m_engine.move_track(pair->second, pair->second - 1);
        for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
            MainWindow* mw = dynamic_cast<MainWindow*>(win);
            if (mw) mw->request_update();
        }
    }
}

void ProjectPanel::cb_move_track_down(Fl_Widget*, void* data) {
    auto* pair = static_cast<std::pair<ProjectPanel*, size_t>*>(data);
    if (pair->second < pair->first->m_engine.track_count() - 1) {
        pair->first->m_engine.move_track(pair->second, pair->second + 1);
        for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
            MainWindow* mw = dynamic_cast<MainWindow*>(win);
            if (mw) mw->request_update();
        }
    }
}

void ProjectPanel::cb_export_ly(Fl_Widget*, void* data) {
    auto* self = static_cast<ProjectPanel*>(data);
    Fl_Native_File_Chooser fnfc;
    fnfc.title("Export to LilyPond");
    fnfc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    fnfc.filter("LilyPond Files\t*.ly\n");
    fnfc.preset_file("score.ly");
    if (fnfc.show() == 0) {
        if (LilyPondExporter::export_project(self->m_engine, fnfc.filename())) {
            fl_message("Successfully exported to %s", fnfc.filename());
        } else {
            fl_alert("Failed to export to %s", fnfc.filename());
        }
    }
}

void ProjectPanel::cb_file_select(Fl_Widget*, void*) {}

} // namespace disgrace_ns
