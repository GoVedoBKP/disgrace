#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace disgrace_ns {

class Engine;

class LilyPondExporter {
public:
    static bool export_project(const Engine& engine, const std::string& path);
    static std::string generate_ly_source(const Engine& engine, int track_index = -1);

private:
    static std::string midi_to_lily(uint8_t note);
    static std::string get_clef_string(int notation_type);
};

} // namespace disgrace_ns
