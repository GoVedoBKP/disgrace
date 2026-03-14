#pragma once

#include <string>
#include <memory>

namespace disgrace_ns {

class Engine;

class XrnsImporter {
public:
    static bool import(Engine& engine, const std::string& path);

private:
    static bool extract_zip(const std::string& zip_path, const std::string& dest_dir);
    static bool parse_song_xml(Engine& engine, const std::string& extracted_dir);
};

} // namespace disgrace_ns
