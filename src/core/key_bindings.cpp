#include "key_bindings.h"
#include <FL/names.h>
#include <FL/Fl.H>
#include <FL/fl_draw.H>

namespace disgrace_ns {

KeyBindings::KeyBindings() {
    set_defaults();
}

void KeyBindings::set_defaults() {
    m_action_to_key.clear();
    m_key_to_action.clear();

    auto add_default = [&](Action a, int k, int m = 0) {
        m_action_to_key[a] = {k, m};
        m_key_to_action[{k, m}] = a;
    };

    add_default(Action::Play, ' ');
    add_default(Action::PlaySong, FL_F + 5);
    add_default(Action::PlayPattern, FL_F + 6);
    add_default(Action::PlayFromPosition, FL_F + 7);
    add_default(Action::Stop, FL_F + 8);
    add_default(Action::Record, 'r');
    add_default(Action::ToggleMetronome, 'm');
    add_default(Action::Undo, 'z', FL_CTRL);
    add_default(Action::Redo, 'y', FL_CTRL);
    add_default(Action::Copy, 'c', FL_CTRL);
    add_default(Action::Cut, 'x', FL_CTRL);
    add_default(Action::Paste, 'p', FL_CTRL);
    add_default(Action::Clear, FL_Delete);
    add_default(Action::MoveUp, FL_Up);
    add_default(Action::MoveDown, FL_Down);
    add_default(Action::MoveLeft, FL_Left);
    add_default(Action::MoveRight, FL_Right);

    // Notes
    add_default(Action::NoteC,  'z');
    add_default(Action::NoteCs, 's');
    add_default(Action::NoteD,  'x');
    add_default(Action::NoteDs, 'd');
    add_default(Action::NoteE,  'c');
    add_default(Action::NoteF,  'v');
    add_default(Action::NoteFs, 'g');
    add_default(Action::NoteG,  'b');
    add_default(Action::NoteGs, 'h');
    add_default(Action::NoteA,  'n');
    add_default(Action::NoteAs, 'j');
    add_default(Action::NoteB,  'm');
}

Action KeyBindings::get_action(int key, int modifiers) const {
    auto it = m_key_to_action.find({key, modifiers});
    if (it != m_key_to_action.end()) return it->second;
    
    // Also try without modifiers if not found (for simple keys)
    // Actually, FLTK includes some modifiers in event_state even if not explicitly pressed for the shortcut
    // But let's keep it simple for now.
    
    return static_cast<Action>(-1); // Not found
}

void KeyBindings::assign(Action action, int key, int modifiers) {
    // Remove old binding for this action
    auto it = m_action_to_key.find(action);
    if (it != m_action_to_key.end()) {
        m_key_to_action.erase(it->second);
    }

    // Remove any action currently assigned to this key combo
    m_key_to_action.erase({key, modifiers});

    // Assign new binding
    m_action_to_key[action] = {key, modifiers};
    m_key_to_action[{key, modifiers}] = action;
}

std::string KeyBindings::get_action_name(Action action) const {
    switch (action) {
        case Action::Play: return "Play/Pause";
        case Action::PlaySong: return "Play Song";
        case Action::PlayPattern: return "Play Pattern";
        case Action::PlayFromPosition: return "Play From Position";
        case Action::Stop: return "Stop";
        case Action::Record: return "Edit";
        case Action::ToggleMetronome: return "Toggle Metronome";
        case Action::Undo: return "Undo";
        case Action::Redo: return "Redo";
        case Action::Copy: return "Copy";
        case Action::Cut: return "Cut";
        case Action::Paste: return "Paste";
        case Action::Clear: return "Clear";
        case Action::MoveUp: return "Move Up";
        case Action::MoveDown: return "Move Down";
        case Action::MoveLeft: return "Move Left";
        case Action::MoveRight: return "Move Right";
        case Action::NoteC: return "Note C";
        case Action::NoteCs: return "Note C#";
        case Action::NoteD: return "Note D";
        case Action::NoteDs: return "Note D#";
        case Action::NoteE: return "Note E";
        case Action::NoteF: return "Note F";
        case Action::NoteFs: return "Note F#";
        case Action::NoteG: return "Note G";
        case Action::NoteGs: return "Note G#";
        case Action::NoteA: return "Note A";
        case Action::NoteAs: return "Note A#";
        case Action::NoteB: return "Note B";
    }
    return "Unknown";
}

std::string KeyBindings::get_key_name(Action action) const {
    auto it = m_action_to_key.find(action);
    if (it == m_action_to_key.end()) return "None";

    unsigned int shortcut = it->second.key;
    if (it->second.modifiers & FL_CTRL) shortcut |= FL_CTRL;
    if (it->second.modifiers & FL_ALT) shortcut |= FL_ALT;
    if (it->second.modifiers & FL_SHIFT) shortcut |= FL_SHIFT;
    if (it->second.modifiers & FL_META) shortcut |= FL_META;

    return fl_shortcut_label(shortcut);
}

} // namespace disgrace_ns
