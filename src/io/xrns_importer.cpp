#include "xrns_importer.h"
#include "../core/engine.h"
#include "../audio/sample_data.h"
#include "../instrument/sample_instrument.h"
#include "../instrument/soundfont_instrument.h"
#include "../instrument/dssi_instrument.h"
#include "../instrument/midi_instrument.h"
#include "../mixer/track.h"
#include "../sequencer/pattern.h"
#include "audio_file.h"
#include <archive.h>
#include <archive_entry.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <libxml/parser.h>
#include <libxml/tree.h>

namespace fs = std::filesystem;

namespace disgrace_ns {

bool XrnsImporter::import(Engine& engine, const std::string& path) {
    fs::path tmp_dir = fs::temp_directory_path() / "disgrace_xrns_import";
    
    try {
        if (fs::exists(tmp_dir)) {
            fs::remove_all(tmp_dir);
        }
        fs::create_directories(tmp_dir);
        
        if (!extract_zip(path, tmp_dir.string())) {
            std::cerr << "Failed to extract xrns file" << std::endl;
            fs::remove_all(tmp_dir);
            return false;
        }
        
        if (!parse_song_xml(engine, tmp_dir.string())) {
            std::cerr << "Failed to parse Song.xml" << std::endl;
            fs::remove_all(tmp_dir);
            return false;
        }
        
        fs::remove_all(tmp_dir);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception during xrns import: " << e.what() << std::endl;
        if (fs::exists(tmp_dir)) {
            fs::remove_all(tmp_dir);
        }
        return false;
    }
}

bool XrnsImporter::extract_zip(const std::string& zip_path, const std::string& dest_dir) {
    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);
    archive_read_support_filter_all(a);
    
    if (archive_read_open_filename(a, zip_path.c_str(), 10240) != ARCHIVE_OK) {
        archive_read_free(a);
        return false;
    }
    
    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string pathname = archive_entry_pathname(entry);
        
        if (pathname == "Song.xml" || 
            pathname.rfind("Samples/", 0) == 0 ||
            pathname.rfind("PluginData/", 0) == 0) {
            
            std::string dest_path = dest_dir + "/" + pathname;
            fs::path parent = fs::path(dest_path).parent_path();
            
            if (!fs::exists(parent)) {
                fs::create_directories(parent);
            }
            
            if (pathname.back() != '/') {
                int ret = archive_read_extract(a, entry, 0);
                if (ret != ARCHIVE_OK) {
                    std::cerr << "Failed to extract: " << pathname << std::endl;
                }
            }
        }
        
        archive_read_data_skip(a);
    }
    
    archive_read_free(a);
    return true;
}

struct RenoiseInstrument {
    std::string name;
    std::string type; // "Sampler", "Plugin", "SoundFont"
    std::string plugin_path;
    std::string plugin_name;
    std::vector<std::pair<std::string, std::string>> samples; // name -> relative path
};

struct RenoiseTrack {
    std::string name;
    std::vector<size_t> instrument_indices;
};

struct RenoiseNote {
    uint8_t note;
    uint8_t velocity;
    size_t instrument_index;
    size_t column;
};

struct RenoisePattern {
    size_t length;
    std::vector<std::vector<std::vector<RenoiseNote>>> tracks; // track -> column -> row -> note
};

struct RenoiseSequencerEntry {
    size_t pattern_index;
    size_t track;
};

bool XrnsImporter::parse_song_xml(Engine& engine, const std::string& extracted_dir) {
    std::string song_xml_path = extracted_dir + "/Song.xml";
    
    LIBXML_TEST_VERSION
    
    xmlDocPtr doc = xmlReadFile(song_xml_path.c_str(), nullptr, 0);
    if (!doc) {
        return false;
    }
    
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root || !xmlStrEqual(root->name, (const xmlChar*)"Song")) {
        xmlFreeDoc(doc);
        return false;
    }
    
    std::vector<RenoiseInstrument> instruments;
    std::vector<RenoiseTrack> tracks;
    std::vector<RenoisePattern> patterns;
    std::vector<RenoiseSequencerEntry> sequencer;
    
    for (xmlNodePtr node = root->children; node; node = node->next) {
        if (xmlStrEqual(node->name, (const xmlChar*)"Sequencer")) {
            for (xmlNodePtr seq_node = node->children; seq_node; seq_node = seq_node->next) {
                if (xmlStrEqual(seq_node->name, (const xmlChar*)"SequenceEntry")) {
                    RenoiseSequencerEntry entry;
                    entry.pattern_index = 0;
                    entry.track = 0;
                    
                    for (xmlNodePtr child = seq_node->children; child; child = child->next) {
                        if (xmlStrEqual(child->name, (const xmlChar*)"Pattern")) {
                            xmlChar* content = xmlNodeGetContent(child);
                            if (content) {
                                entry.pattern_index = std::atoi((const char*)content);
                                xmlFree(content);
                            }
                        }
                    }
                    sequencer.push_back(entry);
                }
            }
        } else if (xmlStrEqual(node->name, (const xmlChar*)"PatternPool")) {
            for (xmlNodePtr pool_node = node->children; pool_node; pool_node = pool_node->next) {
                if (xmlStrEqual(pool_node->name, (const xmlChar*)"Patterns")) {
                    for (xmlNodePtr pat_node = pool_node->children; pat_node; pat_node = pat_node->next) {
                        if (xmlStrEqual(pat_node->name, (const xmlChar*)"Pattern")) {
                            RenoisePattern pat;
                            pat.length = 64;
                            pat.tracks.clear();
                            
                            for (xmlNodePtr child = pat_node->children; child; child = child->next) {
                                if (xmlStrEqual(child->name, (const xmlChar*)"Length")) {
                                    xmlChar* content = xmlNodeGetContent(child);
                                    if (content) {
                                        pat.length = std::atoi((const char*)content);
                                        xmlFree(content);
                                    }
                                } else if (xmlStrEqual(child->name, (const xmlChar*)"Tracks")) {
                                    for (xmlNodePtr track_node = child->children; track_node; track_node = track_node->next) {
                                        if (xmlStrEqual(track_node->name, (const xmlChar*)"Track")) {
                                            std::vector<std::vector<RenoiseNote>> track_columns;
                                            
                                            for (xmlNodePtr line_node = track_node->children; line_node; line_node = line_node->next) {
                                                if (xmlStrEqual(line_node->name, (const xmlChar*)"Line")) {
                                                    std::vector<RenoiseNote> line_notes;
                                                    
                                                    for (xmlNodePtr note_col_node = line_node->children; note_col_node; note_col_node = note_col_node->next) {
                                                        if (xmlStrEqual(note_col_node->name, (const xmlChar*)"NoteColumn")) {
                                                            RenoiseNote note;
                                                            note.note = 255;
                                                            note.velocity = 0;
                                                            note.instrument_index = 0;
                                                            note.column = track_columns.size();
                                                            
                                                            for (xmlNodePtr nc_child = note_col_node->children; nc_child; nc_child = nc_child->next) {
                                                                if (xmlStrEqual(nc_child->name, (const xmlChar*)"Note")) {
                                                                    xmlChar* content = xmlNodeGetContent(nc_child);
                                                                    if (content) {
                                                                        std::string note_str = (const char*)content;
                                                                        xmlFree(content);
                                                                        
                                                                        if (note_str == "OFF") {
                                                                            note.note = 254; // Note off
                                                                        } else if (note_str == "---") {
                                                                            note.note = 255; // Empty
                                                                        } else if (note_str.length() >= 2) {
                                                                            // Parse note like "C-4", "D#5", etc.
                                                                            int octave = std::atoi(note_str.substr(2).c_str());
                                                                            char note_char = std::toupper(note_str[0]);
                                                                            int note_num = 0;
                                                                            switch (note_char) {
                                                                                case 'C': note_num = 0; break;
                                                                                case 'D': note_num = 2; break;
                                                                                case 'E': note_num = 4; break;
                                                                                case 'F': note_num = 5; break;
                                                                                case 'G': note_num = 7; break;
                                                                                case 'A': note_num = 9; break;
                                                                                case 'B': note_num = 11; break;
                                                                            }
                                                                            if (note_str[1] == '#') note_num++;
                                                                            note.note = note_num + (octave + 1) * 12;
                                                                        }
                                                                    }
                                                                } else if (xmlStrEqual(nc_child->name, (const xmlChar*)"Instrument")) {
                                                                    xmlChar* content = xmlNodeGetContent(nc_child);
                                                                    if (content) {
                                                                        note.instrument_index = std::atoi((const char*)content);
                                                                        xmlFree(content);
                                                                    }
                                                                } else if (xmlStrEqual(nc_child->name, (const xmlChar*)"Volume")) {
                                                                    xmlChar* content = xmlNodeGetContent(nc_child);
                                                                    if (content) {
                                                                        std::string vol_str = (const char*)content;
                                                                        xmlFree(content);
                                                                        if (vol_str != "---") {
                                                                            note.velocity = std::atoi(vol_str.c_str());
                                                                            if (note.velocity > 127) note.velocity = 127;
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                            
                                                            if (note.note != 255) {
                                                                line_notes.push_back(note);
                                                            }
                                                        }
                                                    }
                                                    
                                                    if (!line_notes.empty() || track_columns.empty() || track_columns[0].size() < pat.length) {
                                                        track_columns.push_back(line_notes);
                                                    }
                                                }
                                            }
                                            
                                            pat.tracks.push_back(track_columns);
                                        }
                                    }
                                }
                            }
                            
                            patterns.push_back(pat);
                        }
                    }
                }
            }
        } else if (xmlStrEqual(node->name, (const xmlChar*)"Instruments")) {
            for (xmlNodePtr inst_node = node->children; inst_node; inst_node = inst_node->next) {
                if (xmlStrEqual(inst_node->name, (const xmlChar*)"Instrument")) {
                    RenoiseInstrument inst;
                    
                    for (xmlNodePtr child = inst_node->children; child; child = child->next) {
                        if (xmlStrEqual(child->name, (const xmlChar*)"Name")) {
                            xmlChar* content = xmlNodeGetContent(child);
                            if (content) {
                                inst.name = (const char*)content;
                                xmlFree(content);
                            }
                        } else if (xmlStrEqual(child->name, (const xmlChar*)"Type")) {
                            xmlChar* content = xmlNodeGetContent(child);
                            if (content) {
                                inst.type = (const char*)content;
                                xmlFree(content);
                            }
                        } else if (xmlStrEqual(child->name, (const xmlChar*)"PluginProperties")) {
                            for (xmlNodePtr plugin_child = child->children; plugin_child; plugin_child = plugin_child->next) {
                                if (xmlStrEqual(plugin_child->name, (const xmlChar*)"PluginIdentifier")) {
                                    xmlChar* content = xmlNodeGetContent(plugin_child);
                                    if (content) {
                                        inst.plugin_name = (const char*)content;
                                        inst.plugin_path = inst.plugin_name;
                                        xmlFree(content);
                                    }
                                }
                            }
                        } else if (xmlStrEqual(child->name, (const xmlChar*)"SampleList")) {
                            for (xmlNodePtr sample_node = child->children; sample_node; sample_node = sample_node->next) {
                                if (xmlStrEqual(sample_node->name, (const xmlChar*)"Sample")) {
                                    std::string sample_name;
                                    std::string sample_path;
                                    
                                    for (xmlNodePtr sample_child = sample_node->children; sample_child; sample_child = sample_child->next) {
                                        if (xmlStrEqual(sample_child->name, (const xmlChar*)"Name")) {
                                            xmlChar* content = xmlNodeGetContent(sample_child);
                                            if (content) {
                                                sample_name = (const char*)content;
                                                xmlFree(content);
                                            }
                                        } else if (xmlStrEqual(sample_child->name, (const xmlChar*)"Path")) {
                                            xmlChar* content = xmlNodeGetContent(sample_child);
                                            if (content) {
                                                sample_path = "Samples/" + std::string((const char*)content);
                                                xmlFree(content);
                                            }
                                        }
                                    }
                                    
                                    if (!sample_name.empty() && !sample_path.empty()) {
                                        inst.samples.push_back({sample_name, sample_path});
                                    }
                                }
                            }
                        }
                    }
                    
                    instruments.push_back(inst);
                }
            }
        } else if (xmlStrEqual(node->name, (const xmlChar*)"Tracks")) {
            for (xmlNodePtr track_node = node->children; track_node; track_node = track_node->next) {
                if (xmlStrEqual(track_node->name, (const xmlChar*)"Track")) {
                    RenoiseTrack track;
                    track.instrument_indices.clear();
                    
                    for (xmlNodePtr child = track_node->children; child; child = child->next) {
                        if (xmlStrEqual(child->name, (const xmlChar*)"Name")) {
                            xmlChar* content = xmlNodeGetContent(child);
                            if (content) {
                                track.name = (const char*)content;
                                xmlFree(content);
                            }
                        } else if (xmlStrEqual(child->name, (const xmlChar*)"AssignedInstrument")) {
                            xmlChar* content = xmlNodeGetContent(child);
                            if (content) {
                                std::string inst_ref = (const char*)content;
                                xmlFree(content);
                                
                                // Parse instrument reference (format: "Instrument XX" or "Instrument 0")
                                size_t inst_num = 0;
                                if (sscanf(inst_ref.c_str(), "Instrument %zu", &inst_num) == 1) {
                                    track.instrument_indices.push_back(inst_num);
                                }
                            }
                        }
                    }
                    
                    tracks.push_back(track);
                }
            }
        } else if (xmlStrEqual(node->name, (const xmlChar*)"BPM")) {
            xmlChar* content = xmlNodeGetContent(node);
            if (content) {
                double bpm = std::atof((const char*)content);
                engine.set_tempo(bpm);
                xmlFree(content);
            }
        }
    }
    
    xmlFreeDoc(doc);
    xmlCleanupParser();
    
    std::cout << "Found " << instruments.size() << " instruments and " << tracks.size() << " tracks" << std::endl;
    
    std::map<size_t, size_t> instrument_to_track_map;
    
    for (size_t t = 0; t < tracks.size(); t++) {
        const auto& renoise_track = tracks[t];
        
        if (renoise_track.instrument_indices.empty()) {
            continue;
        }
        
        for (size_t inst_idx : renoise_track.instrument_indices) {
            if (inst_idx >= instruments.size()) {
                continue;
            }
            
            const auto& renoise_inst = instruments[inst_idx];
            
            engine.add_track();
            size_t track_idx = engine.track_count() - 1;
            Track& track = engine.track(track_idx);
            
            if (!renoise_track.name.empty()) {
                track.set_name(renoise_track.name);
            } else {
                track.set_name("Track " + std::to_string(t + 1));
            }
            
            instrument_to_track_map[inst_idx] = track_idx;
            
            if (renoise_inst.type == "Sampler" || renoise_inst.type == "SamplerPlugin") {
                auto* sampler = new SampleInstrument(engine.sample_rate());
                
                for (const auto& sample_pair : renoise_inst.samples) {
                    std::string sample_full_path = extracted_dir + "/" + sample_pair.second;
                    auto sample_data = std::make_shared<SampleData>();
                    
                    std::vector<float> left, right;
                    uint32_t sample_rate = 44100;
                    
                    if (AudioFile::load_audio(sample_full_path, left, right, sample_rate)) {
                        sample_data->left = left;
                        sample_data->right = right;
                        sample_data->sample_rate = sample_rate;
                        sampler->add_sample(sample_pair.first, sample_data);
                    } else {
                        std::cerr << "Failed to load sample: " << sample_full_path << std::endl;
                    }
                }
                
                auto inst_ptr = static_cast<Instrument*>(sampler);
                track.set_instrument(inst_ptr);
                
            } else if (renoise_inst.type == "Plugin" || renoise_inst.type == "VST") {
                auto* plugin = new DSSIInstrument(engine.sample_rate());
                
                if (!renoise_inst.plugin_path.empty()) {
                    plugin->load_plugin(renoise_inst.plugin_path);
                } else if (!renoise_inst.plugin_name.empty()) {
                    plugin->load_plugin(renoise_inst.plugin_name);
                }
                
                auto inst_ptr = static_cast<Instrument*>(plugin);
                track.set_instrument(inst_ptr);
                
            } else if (renoise_inst.type == "SoundFont") {
                auto* sf = new SoundFontInstrument(engine.sample_rate());
                
                if (!renoise_inst.plugin_path.empty()) {
                    sf->load_soundfont(renoise_inst.plugin_path);
                }
                
                auto inst_ptr = static_cast<Instrument*>(sf);
                track.set_instrument(inst_ptr);
                
            } else if (renoise_inst.type == "Midi" || renoise_inst.type == "MidiPlugin") {
                auto* midi = new MidiInstrument(&engine);
                auto inst_ptr = static_cast<Instrument*>(midi);
                track.set_instrument(inst_ptr);
                
            } else {
                auto* placeholder = new DSSIInstrument(engine.sample_rate());
                placeholder->load_plugin(renoise_inst.plugin_name.empty() ? renoise_inst.name : renoise_inst.plugin_name);
                auto inst_ptr = static_cast<Instrument*>(placeholder);
                track.set_instrument(inst_ptr);
            }
        }
    }
    
    std::cout << "Created " << engine.track_count() << " tracks" << std::endl;
    
    if (!sequencer.empty() && !patterns.empty()) {
        engine.clear_patterns();
        
        for (size_t seq_idx = 0; seq_idx < sequencer.size(); seq_idx++) {
            size_t pat_idx = sequencer[seq_idx].pattern_index;
            
            if (pat_idx >= patterns.size()) continue;
            
            const auto& renoise_pat = patterns[pat_idx];
            
            engine.add_pattern(std::make_unique<Pattern>(renoise_pat.length, engine.track_count()));
            Pattern& pat = engine.pattern(engine.pattern_count() - 1);
            
            for (size_t t = 0; t < engine.track_count(); t++) {
                Track& tr = engine.track(t);
                
                for (size_t row = 0; row < renoise_pat.length && row < MAX_ROWS; row++) {
                    for (size_t col = 0; col < MAX_COLS; col++) {
                        TrackEvent& evt = pat.event(t, row, col);
                        evt.note = 255;
                        evt.volume = 255;
                        evt.sample_idx = 0;
                    }
                }
                
                if (t < renoise_pat.tracks.size()) {
                    const auto& renoise_track = renoise_pat.tracks[t];
                    
                    for (size_t row = 0; row < renoise_pat.length && row < MAX_ROWS; row++) {
                        for (size_t col = 0; col < renoise_track.size() && col < MAX_COLS; col++) {
                            const auto& line_notes = renoise_track[col];
                            
                            if (row < line_notes.size()) {
                                const auto& rn = line_notes[row];
                                
                                TrackEvent& evt = pat.event(t, row, col);
                                evt.note = rn.note;
                                evt.volume = (rn.velocity > 0) ? rn.velocity : 127;
                                evt.sample_idx = 0;
                            }
                        }
                    }
                }
            }
        }
        
        if (engine.pattern_count() == 0) {
            engine.add_pattern(std::make_unique<Pattern>(64, engine.track_count()));
        }
        
        std::cout << "Created " << engine.pattern_count() << " patterns from sequencer" << std::endl;
    }
    
    return true;
}

} // namespace disgrace_ns
