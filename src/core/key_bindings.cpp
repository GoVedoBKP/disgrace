#include "key_bindings.h"
#include <cstring>

namespace disgrace_ns {

static const int WXK_SPACE = 32;
static const int WXK_DELETE = 127;
static const int WXK_UP = 315;
static const int WXK_DOWN = 317;
static const int WXK_LEFT = 314;
static const int WXK_RIGHT = 316;

static const int WXK_F1 = 340;
static const int WXK_F2 = 341;
static const int WXK_F3 = 342;
static const int WXK_F4 = 343;
static const int WXK_F5 = 344;
static const int WXK_F6 = 345;
static const int WXK_F7 = 346;
static const int WXK_F8 = 347;
static const int WXK_F9 = 348;
static const int WXK_F10 = 349;
static const int WXK_F11 = 350;
static const int WXK_F12 = 351;

static const int WXK_CTRL = 0x1000;
static const int WXK_ALT = 0x2000;
static const int WXK_SHIFT = 0x4000;

static int get_key_code(int key) {
    if (key >= 'a' && key <= 'z') return key - 'a' + 65;
    if (key >= 'A' && key <= 'Z') return key - 'A' + 65;
    if (key >= '0' && key <= '9') return key;
    switch (key) {
        case ' ': return WXK_SPACE;
        case '\t': return 9;
        case '\r': return 13;
        case '\b': return 8;
        case ',': return 44;
        case '.': return 46;
        case '/': return 47;
        case ';': return 59;
        case '\'': return 39;
        case '[': return 91;
        case ']': return 93;
        case '\\': return 92;
        case '-': return 45;
        case '=': return 61;
        case '`': return 96;
        default: return key;
    }
}

KeyBindings::KeyBindings() {
    detect_layout();
    set_defaults();
}

KeyboardLayout KeyBindings::detect_layout() {
    const char* lang = getenv("LANG");
    if (lang && (strstr(lang, "de_DE") || strstr(lang, "de_AT") || strstr(lang, "de_CH"))) {
        m_layout = KeyboardLayout::QWERTZ;
    } else {
        m_layout = KeyboardLayout::QWERTY;
    }
    return m_layout;
}

void KeyBindings::set_layout(KeyboardLayout layout) {
    m_layout = layout;
    if (m_layout == KeyboardLayout::Auto) detect_layout();
    set_defaults();
}

void KeyBindings::set_defaults() {
    m_action_to_key.clear();
    m_key_to_action.clear();

    auto add_default = [&](Action a, int k, int m = 0) {
        int norm_k = k;
        if (k >= 'a' && k <= 'z') norm_k = k - 'a' + 'A';
        m_action_to_key[a] = {norm_k, m};
        m_key_to_action[{norm_k, m}] = a;
    };

    add_default(Action::Play, ' ');
    add_default(Action::PlaySong, WXK_F5);
    add_default(Action::PlayPattern, WXK_F6);
    add_default(Action::PlayFromPosition, WXK_F7);
    add_default(Action::Stop, WXK_F8);
    add_default(Action::Record, 'r');
    add_default(Action::ToggleMetronome, 'm');
    add_default(Action::Undo, 'z', WXK_CTRL);
    add_default(Action::Redo, 'y', WXK_CTRL);
    add_default(Action::Copy, 'c', WXK_CTRL);
    add_default(Action::Cut, 'x', WXK_CTRL);
    add_default(Action::Paste, 'v', WXK_CTRL);
    add_default(Action::Clear, WXK_DELETE);
    add_default(Action::MoveUp, WXK_UP);
    add_default(Action::MoveDown, WXK_DOWN);
    add_default(Action::MoveLeft, WXK_LEFT);
    add_default(Action::MoveRight, WXK_RIGHT);

    bool is_qwertz = (m_layout == KeyboardLayout::QWERTZ);
    add_default(Action::NoteC,  is_qwertz ? 'y' : 'z');
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

    add_default(Action::NoteOff, '^');
    add_default(Action::NoteOff, '`');
    add_default(Action::NoteC2,  'q');
    add_default(Action::NoteCs2, '2');
    add_default(Action::NoteD2,  'w');
    add_default(Action::NoteDs2, '3');
    add_default(Action::NoteE2,  'e');
    add_default(Action::NoteF2,  'r');
    add_default(Action::NoteFs2, '5');
    add_default(Action::NoteG2,  't');
    add_default(Action::NoteGs2, '6');
    add_default(Action::NoteA2,  is_qwertz ? 'z' : 'y');
    add_default(Action::NoteAs2, '7');
    add_default(Action::NoteB2,  'u');
    add_default(Action::NoteC3,  'i');

    add_default(Action::NextPattern, WXK_F12);
    add_default(Action::PrevPattern, WXK_F11);
    add_default(Action::OctaveUp,    ']');
    add_default(Action::OctaveDown,  '[');
    add_default(Action::NextOrderPos, WXK_RIGHT, WXK_ALT);
    add_default(Action::PrevOrderPos, WXK_LEFT, WXK_ALT);
}

Action KeyBindings::get_action(int key, int modifiers) const {
    int norm_key = key;
    if (key >= 'a' && key <= 'z') norm_key = key - 'a' + 'A';
    auto it = m_key_to_action.find({norm_key, modifiers});
    if (it != m_key_to_action.end()) return it->second;
    return static_cast<Action>(-1);
}

void KeyBindings::assign(Action action, int key, int modifiers) {
    int norm_key = key;
    if (key >= 'a' && key <= 'z') norm_key = key - 'a' + 'A';

    auto it = m_action_to_key.find(action);
    if (it != m_action_to_key.end()) {
        m_key_to_action.erase(it->second);
    }

    m_key_to_action.erase({norm_key, modifiers});

    m_action_to_key[action] = {norm_key, modifiers};
    m_key_to_action[{norm_key, modifiers}] = action;
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
        case Action::NoteOff: return "Note Off";
        case Action::NoteC2: return "Note C (2nd Octave)";
        case Action::NoteCs2: return "Note C# (2nd Octave)";
        case Action::NoteD2: return "Note D (2nd Octave)";
        case Action::NoteDs2: return "Note D# (2nd Octave)";
        case Action::NoteE2: return "Note E (2nd Octave)";
        case Action::NoteF2: return "Note F (2nd Octave)";
        case Action::NoteFs2: return "Note F# (2nd Octave)";
        case Action::NoteG2: return "Note G (2nd Octave)";
        case Action::NoteGs2: return "Note G# (2nd Octave)";
        case Action::NoteA2: return "Note A (2nd Octave)";
        case Action::NoteAs2: return "Note A# (2nd Octave)";
        case Action::NoteB2: return "Note B (2nd Octave)";
        case Action::NoteC3: return "Note C (3rd Octave)";
        case Action::NextPattern: return "Next Pattern";
        case Action::PrevPattern: return "Previous Pattern";
        case Action::OctaveUp: return "Increase Octave";
        case Action::OctaveDown: return "Decrease Octave";
        case Action::NextOrderPos: return "Next Order Position";
        case Action::PrevOrderPos: return "Previous Order Position";
    }
    return "Unknown";
}

std::string KeyBindings::get_key_name(Action action) const {
    auto it = m_action_to_key.find(action);
    if (it == m_action_to_key.end()) return "None";

    std::string name;
    int key = it->second.key;
    int mods = it->second.modifiers;

    if (mods & WXK_CTRL) name += "Ctrl+";
    if (mods & WXK_ALT) name += "Alt+";
    if (mods & WXK_SHIFT) name += "Shift+";

    if (key >= 'a' && key <= 'z') name += char('A' + (key - 'a'));
    else if (key >= 'A' && key <= 'Z') name += char(key);
    else if (key >= '0' && key <= '9') name += char(key);
    else {
        switch (key) {
            case WXK_SPACE: name += "Space"; break;
            case WXK_DELETE: name += "Delete"; break;
            case WXK_UP: name += "Up"; break;
            case WXK_DOWN: name += "Down"; break;
            case WXK_LEFT: name += "Left"; break;
            case WXK_RIGHT: name += "Right"; break;
            case WXK_F1: name += "F1"; break;
            case WXK_F2: name += "F2"; break;
            case WXK_F3: name += "F3"; break;
            case WXK_F4: name += "F4"; break;
            case WXK_F5: name += "F5"; break;
            case WXK_F6: name += "F6"; break;
            case WXK_F7: name += "F7"; break;
            case WXK_F8: name += "F8"; break;
            case WXK_F9: name += "F9"; break;
            case WXK_F10: name += "F10"; break;
            case WXK_F11: name += "F11"; break;
            case WXK_F12: name += "F12"; break;
            default: name += char(key); break;
        }
    }
    return name;
}

} // namespace disgrace_ns
