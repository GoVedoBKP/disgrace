#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <wx/colour.h>

namespace disgrace_ns {

enum class ThemeType {
    Light,
    Dark,
    Pastel,
    PastelCyan,
    PastelBlue,
    PastelGreen,
    PastelGray,
    PastelRed,
    Classic,
    Plastic,
    GTK,
    Gleam,
    Custom
};

struct Theme {
    std::string name;
    uint32_t background;
    uint32_t foreground;
    uint32_t selection;
    uint32_t inactive;
    int boxtype;
    int button_boxtype;
    int label_boxtype;
    uint32_t button_color;
    uint32_t input_background;
    uint32_t text_color;
    uint32_t label_color;
    const char* scheme;

    uint32_t tracker_bg;
    uint32_t tracker_text;
    uint32_t tracker_cursor;
    uint32_t tracker_row_highlight;
    uint32_t tracker_lpb_highlight;
    uint32_t tracker_note;
    uint32_t tracker_sample;
    uint32_t tracker_volume;
    uint32_t tracker_effect;
};

class ThemeManager {
public:
    static void apply_theme(ThemeType type);
    static void apply_theme(const Theme& theme);
    static void apply_theme_and_settings(class Engine& engine);
    static const std::vector<Theme>& get_available_themes();
    static ThemeType get_current_theme_type() { return m_current_theme; }
    static wxColour toWxColour(uint32_t c);

private:
    static void init_themes();
    static std::vector<Theme> m_themes;
    static ThemeType m_current_theme;
    static bool m_initialized;
};

} // namespace disgrace_ns
