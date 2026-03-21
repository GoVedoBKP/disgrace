#include "theme.h"
#include "../core/engine.h"

namespace disgrace_ns {

std::vector<Theme> ThemeManager::m_themes;
ThemeType ThemeManager::m_current_theme = ThemeType::Classic;
bool ThemeManager::m_initialized = false;

void ThemeManager::init_themes() {
    if (m_initialized) return;
    
    m_themes.resize(13);
    
    m_themes[0].name = "Light";
    m_themes[0].background = 0xFFFFFFFF;
    m_themes[0].foreground = 0x000000FF;
    m_themes[0].selection = 0x0000FFFF;
    m_themes[0].inactive = 0xAAAAAAFF;
    m_themes[0].button_color = 0xDDDDDDFF;
    m_themes[0].input_background = 0xFFFFFFFF;
    m_themes[0].text_color = 0x000000FF;
    m_themes[0].label_color = 0x000000FF;
    m_themes[0].scheme = "none";
    m_themes[0].tracker_bg = 0xFFFFFFFF;
    m_themes[0].tracker_text = 0x000000FF;
    m_themes[0].tracker_cursor = 0xFF0000FF;
    m_themes[0].tracker_row_highlight = 0xE0E0FFFF;
    m_themes[0].tracker_lpb_highlight = 0xF0F0F0FF;
    m_themes[0].tracker_note = 0x0000FFFF;
    m_themes[0].tracker_sample = 0x00AA00FF;
    m_themes[0].tracker_volume = 0xAAAAAAFF;
    m_themes[0].tracker_effect = 0xAA5500FF;

    m_themes[1].name = "Dark";
    m_themes[1].background = 0x1E1E1EFF;
    m_themes[1].foreground = 0xC8C8C8FF;
    m_themes[1].selection = 0x3C3C3CFF;
    m_themes[1].inactive = 0x666666FF;
    m_themes[1].button_color = 0x3C3C3CFF;
    m_themes[1].input_background = 0x2D2D2DFF;
    m_themes[1].text_color = 0xC8C8C8FF;
    m_themes[1].label_color = 0xC8C8C8FF;
    m_themes[1].scheme = "none";
    m_themes[1].tracker_bg = 0x1E1E1EFF;
    m_themes[1].tracker_text = 0xC8C8C8FF;
    m_themes[1].tracker_cursor = 0xFFFF00FF;
    m_themes[1].tracker_row_highlight = 0x3C3C3CFF;
    m_themes[1].tracker_lpb_highlight = 0x2D2D37FF;
    m_themes[1].tracker_note = 0xB4B4FFFF;
    m_themes[1].tracker_sample = 0xB4FFB4FF;
    m_themes[1].tracker_volume = 0xFFFFB4FF;
    m_themes[1].tracker_effect = 0xFFB4FFFF;

    for (int i = 2; i < 13; ++i) {
        m_themes[i] = m_themes[1];
        m_themes[i].name = "Theme" + std::to_string(i);
    }
    
    m_initialized = true;
}

void ThemeManager::apply_theme(ThemeType type) {
    init_themes();
    m_current_theme = type;
}

void ThemeManager::apply_theme(const Theme& theme) {
    m_current_theme = ThemeType::Custom;
}

void ThemeManager::apply_theme_and_settings(Engine& engine) {
    init_themes();
    ThemeType type = engine.m_gui_theme;
    m_current_theme = type;
    
    size_t idx = static_cast<size_t>(type);
    if (idx < m_themes.size()) {
        const Theme& theme = m_themes[idx];
        
        engine.m_bg_color = theme.background;
        engine.m_fg_color = theme.foreground;
        engine.m_button_color = theme.button_color;
        
        engine.m_tracker_bg = theme.tracker_bg;
        engine.m_tracker_text = theme.tracker_text;
        engine.m_tracker_cursor = theme.tracker_cursor;
        engine.m_tracker_row_highlight = theme.tracker_row_highlight;
        engine.m_tracker_lpb_highlight = theme.tracker_lpb_highlight;
        engine.m_tracker_note = theme.tracker_note;
        engine.m_tracker_sample = theme.tracker_sample;
        engine.m_tracker_volume = theme.tracker_volume;
        engine.m_tracker_effect = theme.tracker_effect;
    }
}

const std::vector<Theme>& ThemeManager::get_available_themes() {
    init_themes();
    return m_themes;
}

wxColour ThemeManager::toWxColour(uint32_t c) {
    uint8_t a = c & 0xFF;
    if (a == 0) a = 0xFF; // Force opaque if alpha is 0
    return wxColour((c >> 24) & 0xFF, (c >> 16) & 0xFF, (c >> 8) & 0xFF, a);
}

} // namespace disgrace_ns
