#include "instrument_panel.h"
#include "detached_window.h"
#include "main_window.h"
#include "../core/engine.h"
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_ask.H>

namespace disgrace_ns {

InstrumentPanel::InstrumentPanel(int x, int y, int w, int h, Engine& engine)
    : Fl_Group(x, y, w, h), m_engine(engine) {
    
    begin();

    int left_w = 350;
    int margin = 10;

    m_detach_btn = new Fl_Button(x + w - 30, y, 30, 20, "[]");
    m_detach_btn->callback(cb_detach, this);

    int cur_y = margin;
    int btn_w = (left_w - 5 * margin) / 4;

    m_new_btn = new Fl_Button(x + margin, y + cur_y, btn_w, 25, "New");
    m_new_btn->callback(cb_new, this);

    m_load_btn = new Fl_Button(x + 2 * margin + btn_w, y + cur_y, btn_w, 25, "Load");
    m_load_btn->callback(cb_load, this);

    m_save_btn = new Fl_Button(x + 3 * margin + 2 * btn_w, y + cur_y, btn_w, 25, "Save");
    m_save_btn->callback(cb_save, this);

    m_delete_btn = new Fl_Button(x + 4 * margin + 3 * btn_w, y + cur_y, btn_w, 25, "Del");
    m_delete_btn->callback(cb_delete, this);

    cur_y += 25 + margin;

    // Instrument list scroll
    m_inst_scroll = new Fl_Scroll(x + margin, y + cur_y, left_w - 2 * margin, 200);
    m_inst_scroll->type(Fl_Scroll::VERTICAL);
    m_inst_container = new Fl_Group(x + margin, y + cur_y, left_w - 40, 1000);
    m_inst_container->end();
    m_inst_scroll->add(m_inst_container);
    m_inst_scroll->end();

    cur_y += 200 + margin;

    // File browser
    m_file_browser = new Fl_File_Browser(x + margin, y + cur_y, left_w - 2 * margin, y + h - (y + cur_y) - margin);
    m_file_browser->load(".");

    // Right side (Editor)
    Fl_Box* editor_box = new Fl_Box(x + left_w + margin, y + margin, w - left_w - 2 * margin, h - 2 * margin, "Instrument Editor");
    editor_box->box(FL_ENGRAVED_FRAME);
    
    resizable(editor_box);

    end();

    update_instrument_list();
}

void InstrumentPanel::update_instrument_list() {
    m_inst_container->clear();
    m_inst_container->begin();

    int row_h = 35;
    int start_y = m_inst_container->y();
    int start_x = m_inst_container->x();

    int label_w = 30;
    int input_w = 120;
    int choice_w = 100;

    size_t num_insts = m_engine.instrument_count();

    for (size_t i = 0; i < num_insts; ++i) {
        int cur_y = start_y + (int)(i * row_h);
        int cur_x = start_x;
        auto& inst = m_engine.instrument(i);

        // Index
        char idx_str[8];
        snprintf(idx_str, 8, "%zu:", i + 1);
        new Fl_Box(cur_x, cur_y, label_w, row_h, strdup(idx_str));
        cur_x += label_w + 5;

        // Name
        Fl_Input* name_in = new Fl_Input(cur_x, cur_y + 5, input_w, 25);
        name_in->value(inst.name().c_str());
        name_in->maximum_size(64);
        name_in->callback(cb_inst_name, new std::pair<InstrumentPanel*, size_t>(this, i));
        name_in->when(FL_WHEN_ENTER_KEY | FL_WHEN_RELEASE);
        cur_x += input_w + 5;

        // Type
        Fl_Choice* type_ch = new Fl_Choice(cur_x, cur_y + 5, choice_w, 25);
        type_ch->add("None");
        type_ch->add("Sampler");
        type_ch->add("SoundFont");
        type_ch->add("Plugin");
        type_ch->add("MIDI");
        type_ch->value((int)inst.type());
        type_ch->callback(cb_inst_type, new std::pair<InstrumentPanel*, size_t>(this, i));

        cur_y += row_h;
    }

    m_inst_container->end();
    m_inst_container->size(m_inst_container->w(), (int)(num_insts * row_h + 20));
    m_inst_scroll->redraw();
}

void InstrumentPanel::cb_new(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data);
    self->m_engine.add_instrument();
    self->update_instrument_list();
    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
        MainWindow* mw = dynamic_cast<MainWindow*>(win);
        if (mw) mw->update_all_uis();
    }
}

void InstrumentPanel::cb_load(Fl_Widget*, void* data) {}
void InstrumentPanel::cb_save(Fl_Widget*, void* data) {}

void InstrumentPanel::cb_delete(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data);
    if (self->m_engine.instrument_count() > 0) {
        size_t idx = self->m_engine.instrument_count() - 1;
        if (fl_ask("Delete instrument %zu?", idx + 1)) {
            self->m_engine.remove_instrument(idx);
            self->update_instrument_list();
            for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
                MainWindow* mw = dynamic_cast<MainWindow*>(win);
                if (mw) mw->update_all_uis();
            }
        }
    }
}

void InstrumentPanel::cb_inst_name(Fl_Widget* w, void* data) {
    auto* pair = static_cast<std::pair<InstrumentPanel*, size_t>*>(data);
    Fl_Input* in = static_cast<Fl_Input*>(w);
    pair->first->m_engine.instrument(pair->second).set_name(in->value());
    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
        MainWindow* mw = dynamic_cast<MainWindow*>(win);
        if (mw) mw->update_all_uis();
    }
}

void InstrumentPanel::cb_inst_type(Fl_Widget* w, void* data) {
    auto* pair = static_cast<std::pair<InstrumentPanel*, size_t>*>(data);
    Fl_Choice* ch = static_cast<Fl_Choice*>(w);
    pair->first->m_engine.instrument(pair->second).set_type((InstrumentType)ch->value());
}

void InstrumentPanel::cb_detach(Fl_Widget*, void* data) {
    InstrumentPanel* self = static_cast<InstrumentPanel*>(data);
    Fl_Group* parent_grp = self->parent();
    if (self->m_detached_window) {
        self->m_detached_window->show();
    } else {
        self->m_detached_window = new DetachedWindow(800, 600, "Instruments", self, parent_grp);
        self->m_detached_window->show();
    }
    self->hide();
}

} // namespace disgrace_ns
