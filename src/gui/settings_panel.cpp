#include "settings_panel.h"
#include "main_window.h"
#include "../core/engine.h"
#include <FL/Fl_Button.H>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Double_Window.H>

namespace disgrace_ns {

// Helper function to recursively apply settings to all widgets
void apply_to_all_widgets(Fl_Widget* w, int font_size, int btn_h) {
    w->labelsize(font_size);
    
    // If it's a button, try to resize it
    if (dynamic_cast<Fl_Button*>(w)) {
        w->size(w->w(), btn_h); // Only height for now as width depends on layout context
    }

    // If it's a group, recurse into children
    Fl_Group* g = w->as_group();
    if (g) {
        for (int i = 0; i < g->children(); i++) {
            apply_to_all_widgets(g->child(i), font_size, btn_h);
        }
    }
}

void apply_gui_settings(Engine& engine) {
    FL_NORMAL_SIZE = engine.m_gui_font_size;
    
    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
        apply_to_all_widgets(win, engine.m_gui_font_size, engine.m_gui_button_height);
        win->redraw();
    }
}

SettingsPanel::SettingsPanel(int x, int y, int w, int h, Engine& engine)
    : Fl_Group(x, y, w, h), m_engine(engine) {
    
    begin();

    m_sub_tabs = new Fl_Tabs(x, y, w, h);
    
    init_audio_grp(x, y, w, h);
    init_midi_grp(x, y, w, h);
    init_mixer_grp(x, y, w, h);
    init_gui_grp(x, y, w, h);
    init_kbd_grp(x, y, w, h);
    init_misc_grp(x, y, w, h);

    m_sub_tabs->end();
    resizable(m_sub_tabs);

    end();
}

void SettingsPanel::init_audio_grp(int x, int y, int w, int h) {
    m_audio_grp = new Fl_Group(x, y + 25, w, h - 25, "Audio");
    m_audio_grp->begin();
    Fl_Box* title = new Fl_Box(x + 20, y + 40, 200, 25, "Audio Device: JACK");
    title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    
    m_audio_ins = new Fl_Value_Input(x + 140, y + 70, 40, 25, "Input Channels:");
    m_audio_ins->value(m_engine.m_num_ins);
    m_audio_ins->minimum(1);
    m_audio_ins->maximum(64);
    m_audio_ins->step(1);
    m_audio_ins->align(FL_ALIGN_LEFT);
    
    m_audio_outs = new Fl_Value_Input(x + 140, y + 100, 40, 25, "Output Channels:");
    m_audio_outs->value(m_engine.m_num_outs);
    m_audio_outs->minimum(1);
    m_audio_outs->maximum(64);
    m_audio_outs->step(1);
    m_audio_outs->align(FL_ALIGN_LEFT);

    m_reinit_audio_btn = new Fl_Button(x + 20, y + 140, 160, 25, "Reinitialize Audio");
    m_reinit_audio_btn->callback(cb_reinit_audio, this);

    m_audio_status = new Fl_Box(x + 20, y + 170, 200, 25, "Status: Unknown");
    m_audio_status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    m_audio_grp->end();
    update_audio_status();
}

void SettingsPanel::init_midi_grp(int x, int y, int w, int h) {
    m_midi_grp = new Fl_Group(x, y + 25, w, h - 25, "MIDI");
    m_midi_grp->begin();
    Fl_Box* title = new Fl_Box(x + 20, y + 40, 200, 25, "MIDI Config");
    title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    m_midi_ins = new Fl_Value_Input(x + 140, y + 70, 40, 25, "MIDI Inputs:");
    m_midi_ins->value(m_engine.m_num_midi_ins);
    m_midi_ins->minimum(0);
    m_midi_ins->maximum(16);
    m_midi_ins->step(1);
    m_midi_ins->align(FL_ALIGN_LEFT);

    m_midi_outs = new Fl_Value_Input(x + 140, y + 100, 40, 25, "MIDI Outputs:");
    m_midi_outs->value(m_engine.m_num_midi_outs);
    m_midi_outs->minimum(0);
    m_midi_outs->maximum(16);
    m_midi_outs->step(1);
    m_midi_outs->align(FL_ALIGN_LEFT);

    m_reinit_midi_btn = new Fl_Button(x + 20, y + 140, 160, 25, "Reinitialize MIDI");
    m_reinit_midi_btn->callback(cb_reinit_midi, this);

    m_midi_grp->end();
}

void SettingsPanel::init_mixer_grp(int x, int y, int w, int h) {
    m_mixer_grp = new Fl_Group(x, y + 25, w, h - 25, "Mixer");
    m_mixer_grp->begin();
    new Fl_Box(x + 20, y + 50, 200, 25, "Default Track Count: 8");
    m_mixer_grp->end();
}

void SettingsPanel::init_gui_grp(int x, int y, int w, int h) {
    m_gui_grp = new Fl_Group(x, y + 25, w, h - 25, "GUI");
    m_gui_grp->begin();
    
    Fl_Box* title = new Fl_Box(x + 20, y + 40, 200, 25, "GUI Settings");
    title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    m_gui_theme = new Fl_Choice(x + 120, y + 70, 100, 25, "Theme:");
    m_gui_theme->add("none");
    m_gui_theme->add("base");
    m_gui_theme->add("plastic");
    m_gui_theme->add("gtk+");
    m_gui_theme->add("gleam");
    m_gui_theme->value(0);
    m_gui_theme->callback(cb_gui_theme, this);
    m_gui_theme->align(FL_ALIGN_LEFT);

    m_gui_btn_h = new Fl_Choice(x + 120, y + 100, 100, 25, "Btn Height:");
    m_gui_btn_h->add("10");
    m_gui_btn_h->add("15");
    m_gui_btn_h->add("20");
    m_gui_btn_h->add("25");
    m_gui_btn_h->add("30");
    m_gui_btn_h->value(3); // Default 25
    m_gui_btn_h->callback(cb_gui_btn_h, this);
    m_gui_btn_h->align(FL_ALIGN_LEFT);

    m_gui_font_size = new Fl_Choice(x + 120, y + 130, 100, 25, "Font Size:");
    m_gui_font_size->add("8");
    m_gui_font_size->add("10");
    m_gui_font_size->add("12");
    m_gui_font_size->add("14");
    m_gui_font_size->add("16");
    m_gui_font_size->add("18");
    m_gui_font_size->add("20");
    m_gui_font_size->value(2); // Default 12
    m_gui_font_size->callback(cb_gui_font_size, this);
    m_gui_font_size->align(FL_ALIGN_LEFT);

    m_waveform_color_btn = new Fl_Button(x + 120, y + 160, 100, 25, "Waveform Color");
    m_waveform_color_btn->callback(cb_waveform_color, this);

    Fl_Check_Button* show_meters = new Fl_Check_Button(x + 20, y + 190, 150, 25, "Show Level Meters");
    show_meters->value(1);
    
    m_gui_grp->end();
}

void SettingsPanel::init_kbd_grp(int x, int y, int w, int h) {
    m_kbd_grp = new Fl_Group(x, y + 25, w, h - 25, "Keyboard");
    m_kbd_grp->begin();
    
    Fl_Box* title = new Fl_Box(x + 20, y + 40, 200, 25, "Keyboard Configuration");
    title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    m_action_choice = new Fl_Choice(x + 120, y + 70, 200, 25, "Select Action:");
    m_action_choice->align(FL_ALIGN_LEFT);
    
    // Add all actions to choice
    std::vector<Action> actions = {
        Action::Play, Action::Record, Action::ToggleMetronome,
        Action::Undo, Action::Redo, Action::Copy, Action::Cut, Action::Paste,
        Action::Clear, Action::MoveUp, Action::MoveDown, Action::MoveLeft, Action::MoveRight,
        Action::NoteC, Action::NoteCs, Action::NoteD, Action::NoteDs, Action::NoteE,
        Action::NoteF, Action::NoteFs, Action::NoteG, Action::NoteGs, Action::NoteA,
        Action::NoteAs, Action::NoteB
    };

    for (auto action : actions) {
        m_action_choice->add(m_engine.m_key_bindings.get_action_name(action).c_str(), 0, nullptr, (void*)(uintptr_t)action);
    }
    m_action_choice->value(0);
    m_action_choice->callback([](Fl_Widget* w, void* data) {
        static_cast<SettingsPanel*>(data)->update_kbd_list();
    }, this);

    m_current_binding_box = new Fl_Box(x + 20, y + 100, 300, 25, "Current Binding: ...");
    m_current_binding_box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    m_assign_btn = new Fl_Button(x + 20, y + 130, 150, 25, "Assign New Key...");
    m_assign_btn->callback(cb_assign_key, this);

    m_kbd_grp->end();
    update_kbd_list();
}

void SettingsPanel::update_kbd_list() {
    int idx = m_action_choice->value();
    if (idx < 0) return;

    Action action = (Action)(uintptr_t)m_action_choice->menu()[idx].user_data();
    std::string binding = m_engine.m_key_bindings.get_key_name(action);
    
    char buf[128];
    snprintf(buf, sizeof(buf), "Current Binding: %s", binding.c_str());
    m_current_binding_box->label(strdup(buf));
}

void SettingsPanel::init_misc_grp(int x, int y, int w, int h) {
    m_misc_grp = new Fl_Group(x, y + 25, w, h - 25, "Misc");
    m_misc_grp->begin();
    new Fl_Box(x + 20, y + 50, 200, 25, "Auto-save: Enabled");
    m_misc_grp->end();
}

void SettingsPanel::update_audio_status() {
    if (m_engine.audio_active()) {
        m_audio_status->label("Status: Connected to JACK");
        m_audio_status->labelcolor(FL_GREEN);
    } else {
        m_audio_status->label("Status: Disconnected");
        m_audio_status->labelcolor(FL_RED);
    }
    m_audio_status->redraw();
}

void SettingsPanel::cb_reinit_audio(Fl_Widget*, void* data) {
    auto* self = static_cast<SettingsPanel*>(data);
    self->m_engine.reinitialize_audio(
        static_cast<uint32_t>(self->m_audio_ins->value()),
        static_cast<uint32_t>(self->m_audio_outs->value()),
        static_cast<uint32_t>(self->m_midi_ins->value()),
        static_cast<uint32_t>(self->m_midi_outs->value())
    );
    self->update_audio_status();
}

void SettingsPanel::cb_reinit_midi(Fl_Widget*, void* data) {
    auto* self = static_cast<SettingsPanel*>(data);
    self->m_engine.reinitialize_audio(
        static_cast<uint32_t>(self->m_audio_ins->value()),
        static_cast<uint32_t>(self->m_audio_outs->value()),
        static_cast<uint32_t>(self->m_midi_ins->value()),
        static_cast<uint32_t>(self->m_midi_outs->value())
    );
    self->update_audio_status();
}

void SettingsPanel::cb_gui_theme(Fl_Widget* w, void* data) {
    auto* choice = static_cast<Fl_Choice*>(w);
    const char* theme = choice->text();
    if (theme) {
        Fl::scheme(theme);
        Fl::redraw();
    }
}

void SettingsPanel::cb_gui_btn_h(Fl_Widget* w, void* data) {
    auto* self = static_cast<SettingsPanel*>(data);
    auto* choice = static_cast<Fl_Choice*>(w);
    const char* val = choice->text();
    if (val) {
        self->m_engine.m_gui_button_height = atoi(val);
        apply_gui_settings(self->m_engine);
    }
}

void SettingsPanel::cb_gui_font_size(Fl_Widget* w, void* data) {
    auto* self = static_cast<SettingsPanel*>(data);
    auto* choice = static_cast<Fl_Choice*>(w);
    const char* val = choice->text();
    if (val) {
        self->m_engine.m_gui_font_size = atoi(val);
        apply_gui_settings(self->m_engine);
    }
}

void SettingsPanel::cb_waveform_color(Fl_Widget*, void* data) {
    auto* self = static_cast<SettingsPanel*>(data);
    unsigned char r, g, b;
    unsigned int c = self->m_engine.m_waveform_color;
    // Extract RGB from 0xRRGGBB00 (FLTK rgb format)
    r = (c >> 24) & 0xFF;
    g = (c >> 16) & 0xFF;
    b = (c >> 8) & 0xFF;
    
    if (fl_color_chooser("Waveform Color", r, g, b)) {
        self->m_engine.m_waveform_color = fl_rgb_color(r, g, b);
        for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
            MainWindow* mw = dynamic_cast<MainWindow*>(win);
            if (mw) mw->update_all_uis();
        }
    }
}

// Modal window to capture a key press
class KeyGrabber : public Fl_Double_Window {
public:
    KeyGrabber(Action action, KeyBindings& bindings) 
        : Fl_Double_Window(300, 100, "Press a key..."), m_action(action), m_bindings(bindings) {
        set_modal();
        m_box = new Fl_Box(0, 0, 300, 100, "Press new key for\n...");
        m_box->label(strdup(("Press new key for\n" + bindings.get_action_name(action)).c_str()));
    }

    int handle(int event) override {
        if (event == FL_KEYDOWN) {
            int key = Fl::event_key();
            int mods = Fl::event_state() & (FL_CTRL | FL_SHIFT | FL_ALT | FL_META);
            if (key == FL_Control_L || key == FL_Control_R || 
                key == FL_Shift_L || key == FL_Shift_R ||
                key == FL_Alt_L || key == FL_Alt_R ||
                key == FL_Meta_L || key == FL_Meta_R) {
                return 1;
            }
            m_bindings.assign(m_action, key, mods);
            m_pressed = true;
            hide();
            return 1;
        }
        return Fl_Double_Window::handle(event);
    }
    bool pressed() const { return m_pressed; }
private:
    Action m_action;
    KeyBindings& m_bindings;
    Fl_Box* m_box;
    bool m_pressed = false;
};

void SettingsPanel::cb_assign_key(Fl_Widget*, void* data) {
    SettingsPanel* self = static_cast<SettingsPanel*>(data);
    int idx = self->m_action_choice->value();
    if (idx < 0) return;
    Action action = (Action)(uintptr_t)self->m_action_choice->menu()[idx].user_data();
    KeyGrabber grabber(action, self->m_engine.m_key_bindings);
    grabber.show();
    while (grabber.shown()) { Fl::wait(); }
    if (grabber.pressed()) { self->update_kbd_list(); }
}

} // namespace disgrace_ns
