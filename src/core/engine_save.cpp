#include "engine.h"
#include "../io/song_serializer.h"
#include "../io/project_archive.h"
#include "../io/xrns_importer.h"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace disgrace_ns
{

void Engine::save_project(const ::std::string& path)
{
    try {
        fs::path tmp_dir = fs::temp_directory_path() / "disgrace_save_tmp";
        if (fs::exists(tmp_dir)) fs::remove_all(tmp_dir);
        fs::create_directories(tmp_dir);

        if (SongSerializer::save(*this, tmp_dir.string())) {
            if (ProjectArchive::save(path, tmp_dir.string())) {
                std::cout << "Project saved successfully to " << path << std::endl;
            } else {
                std::cerr << "Failed to archive project to " << path << std::endl;
            }
        } else {
            std::cerr << "Failed to serialize song to temporary directory" << std::endl;
        }

        fs::remove_all(tmp_dir);
    } catch (const std::exception& e) {
        std::cerr << "Exception during save: " << e.what() << std::endl;
    }
}

void Engine::load_project(const ::std::string& path)
{
    try {
        fs::path tmp_dir = fs::temp_directory_path() / "disgrace_load_tmp";
        if (fs::exists(tmp_dir)) fs::remove_all(tmp_dir);
        fs::create_directories(tmp_dir);

        if (ProjectArchive::extract(path, tmp_dir.string())) {
            if (SongSerializer::load(*this, tmp_dir.string())) {
                std::cout << "Project loaded successfully from " << path << std::endl;
            } else {
                std::cerr << "Failed to deserialize song from " << path << std::endl;
            }
        } else {
            std::cerr << "Failed to extract project archive from " << path << std::endl;
        }

        fs::remove_all(tmp_dir);
    } catch (const std::exception& e) {
        std::cerr << "Exception during load: " << e.what() << std::endl;
    }
}

void Engine::import_audio(const ::std::string& path)
{
    std::string path_str(path);
    
    if (path_str.size() >= 5 && path_str.compare(path_str.size() - 5, 5, ".xrns") == 0) {
        if (XrnsImporter::import(*this, path)) {
            std::cout << "XRNS import successful" << std::endl;
        } else {
            std::cerr << "Failed to import XRNS file" << std::endl;
        }
        return;
    }
    
    std::cout << "Import audio: " << path << std::endl;
}

} // namespace disgrace_ns
