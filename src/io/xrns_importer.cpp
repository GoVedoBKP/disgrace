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
#include <map>
#include <set>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

namespace disgrace_ns {

struct RenoiseInstrument {
    std::string name;
    std::string type; 
    std::vector<std::pair<std::string, std::string>> samples;
};

struct RenoiseTrack {
    std::string name;
};

struct RawRenoiseNote {
    uint8_t note = 255;
    uint8_t instrument = 255;
    uint8_t volume = 255;
};

struct RawRenoisePattern {
    size_t num_lines = 64;
    std::vector<std::vector<std::vector<RawRenoiseNote>>> tracks_data;
};

static std::string get_node_content(xmlNodePtr node) {
    xmlChar* content = xmlNodeGetContent(node);
    std::string s;
    if (content) {
        s = (const char*)content;
        xmlFree(content);
    }
    return s;
}

static uint8_t parse_renoise_note(const std::string& note_str) {
    if (note_str == "OFF") return 254;
    if (note_str == "---" || note_str.empty() || note_str == "..") return 255;
    
    if (note_str.length() < 3) return 255;

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
        default: return 255;
    }
    
    int octave = 0;
    if (note_str[1] == '#') {
        note_num++;
        octave = std::atoi(note_str.substr(2).c_str());
    } else {
        octave = std::atoi(note_str.substr(2).c_str());
    }

    return note_num + (octave + 1) * 12;
}

bool XrnsImporter::import(Engine& engine, const std::string& path) {
    fs::path tmp_dir = fs::temp_directory_path() / "disgrace_xrns_import";
    
    try {
        if (fs::exists(tmp_dir)) fs::remove_all(tmp_dir);
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
        if (fs::exists(tmp_dir)) fs::remove_all(tmp_dir);
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
            if (!fs::exists(parent)) fs::create_directories(parent);
            
            if (pathname.back() != '/') {
                archive_entry_set_pathname(entry, dest_path.c_str());
                int ret = archive_read_extract(a, entry, 0);
                if (ret != ARCHIVE_OK) {
                    std::cerr << "Failed to extract: " << pathname << " Error: " << archive_error_string(a) << std::endl;
                }
            }
        }
        archive_read_data_skip(a);
    }
    
    archive_read_free(a);
    return true;
}

bool XrnsImporter::parse_song_xml(Engine& engine, const std::string& extracted_dir) {
    std::string song_xml_path = extracted_dir + "/Song.xml";
    LIBXML_TEST_VERSION
    xmlDocPtr doc = xmlReadFile(song_xml_path.c_str(), nullptr, 0);
    if (!doc) return false;
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) { xmlFreeDoc(doc); return false; }
    
    std::vector<RenoiseInstrument> ri_list;
    std::vector<RenoiseTrack> rt_list;
    std::vector<RawRenoisePattern> rp_list;
    std::vector<size_t> sequence;

    for (xmlNodePtr node = root->children; node; node = node->next) {
        if (xmlStrEqual(node->name, (const xmlChar*)"GlobalSongData")) {
            for (xmlNodePtr child = node->children; child; child = child->next) {
                if (xmlStrEqual(child->name, (const xmlChar*)"BeatsPerMinute")) engine.set_tempo(std::atof(get_node_content(child).c_str()));
            }
        } else if (xmlStrEqual(node->name, (const xmlChar*)"Instruments")) {
            for (xmlNodePtr inst_node = node->children; inst_node; inst_node = inst_node->next) {
                if (xmlStrEqual(inst_node->name, (const xmlChar*)"Instrument")) {
                    RenoiseInstrument inst; inst.type = "Sampler";
                    for (xmlNodePtr child = inst_node->children; child; child = child->next) {
                        if (xmlStrEqual(child->name, (const xmlChar*)"Name")) inst.name = get_node_content(child);
                        else if (xmlStrEqual(child->name, (const xmlChar*)"PluginProperties")) {
                            for (xmlNodePtr pchild = child->children; pchild; pchild = pchild->next) {
                                if (xmlStrEqual(pchild->name, (const xmlChar*)"PluginIdentifier")) {
                                    std::string id = get_node_content(pchild);
                                    if (!id.empty()) {
                                        inst.type = "Plugin";
                                        if (id.find("SoundFont") != std::string::npos || id.find(".sf2") != std::string::npos) inst.type = "SoundFont";
                                    }
                                }
                            }
                        } else if (xmlStrEqual(child->name, (const xmlChar*)"SampleList")) {
                            for (xmlNodePtr sample_node = child->children; sample_node; sample_node = sample_node->next) {
                                if (xmlStrEqual(sample_node->name, (const xmlChar*)"Sample")) {
                                    std::string s_name, s_path;
                                    for (xmlNodePtr s_child = sample_node->children; s_child; s_child = s_child->next) {
                                        if (xmlStrEqual(s_child->name, (const xmlChar*)"Name")) s_name = get_node_content(s_child);
                                        else if (xmlStrEqual(s_child->name, (const xmlChar*)"SamplePart")) {
                                            for (xmlNodePtr sp_child = s_child->children; sp_child; sp_child = sp_child->next) {
                                                if (xmlStrEqual(sp_child->name, (const xmlChar*)"File")) s_path = get_node_content(sp_child);
                                            }
                                        }
                                    }
                                    if (!s_path.empty()) inst.samples.push_back({s_name, s_path});
                                }
                            }
                        }
                    }
                    ri_list.push_back(inst);
                }
            }
        } else if (xmlStrEqual(node->name, (const xmlChar*)"Tracks")) {
            for (xmlNodePtr track_node = node->children; track_node; track_node = track_node->next) {
                if (xmlStrEqual(track_node->name, (const xmlChar*)"SequencerTrack")) {
                    RenoiseTrack track;
                    for (xmlNodePtr child = track_node->children; child; child = child->next) {
                        if (xmlStrEqual(child->name, (const xmlChar*)"Name")) track.name = get_node_content(child);
                    }
                    rt_list.push_back(track);
                } else if (xmlStrEqual(track_node->name, (const xmlChar*)"MasterTrack") || xmlStrEqual(track_node->name, (const xmlChar*)"SendTrack") || xmlStrEqual(track_node->name, (const xmlChar*)"GroupTrack")) {
                    rt_list.push_back({"Other Track"});
                }
            }
        } else if (xmlStrEqual(node->name, (const xmlChar*)"PatternPool")) {
            xmlNodePtr patterns_node = nullptr;
            for (xmlNodePtr child = node->children; child; child = child->next) {
                if (xmlStrEqual(child->name, (const xmlChar*)"Patterns")) { patterns_node = child; break; }
            }
            if (patterns_node) {
                for (xmlNodePtr pat_node = patterns_node->children; pat_node; pat_node = pat_node->next) {
                    if (xmlStrEqual(pat_node->name, (const xmlChar*)"Pattern")) {
                        RawRenoisePattern rp;
                        for (xmlNodePtr child = pat_node->children; child; child = child->next) {
                            if (xmlStrEqual(child->name, (const xmlChar*)"NumberOfLines")) rp.num_lines = std::atoi(get_node_content(child).c_str());
                            else if (xmlStrEqual(child->name, (const xmlChar*)"Tracks")) {
                                for (xmlNodePtr track_node = child->children; track_node; track_node = track_node->next) {
                                    if (xmlStrEqual(track_node->name, (const xmlChar*)"PatternTrack")) {
                                        std::vector<std::vector<RawRenoiseNote>> row_data(rp.num_lines);
                                        xmlNodePtr lines_node = nullptr;
                                        for (xmlNodePtr c = track_node->children; c; c = c->next) { if (xmlStrEqual(c->name, (const xmlChar*)"Lines")) { lines_node = c; break; } }
                                        if (lines_node) {
                                            for (xmlNodePtr line_node = lines_node->children; line_node; line_node = line_node->next) {
                                                if (xmlStrEqual(line_node->name, (const xmlChar*)"Line")) {
                                                    xmlChar* idx_attr = xmlGetProp(line_node, (const xmlChar*)"index");
                                                    int row_idx = idx_attr ? std::atoi((const char*)idx_attr) : -1;
                                                    if (idx_attr) xmlFree(idx_attr);
                                                    if (row_idx >= 0 && (size_t)row_idx < rp.num_lines) {
                                                        xmlNodePtr note_cols_node = nullptr;
                                                        for (xmlNodePtr c = line_node->children; c; c = c->next) { if (xmlStrEqual(c->name, (const xmlChar*)"NoteColumns")) { note_cols_node = c; break; } }
                                                        if (note_cols_node) {
                                                            for (xmlNodePtr nc_node = note_cols_node->children; nc_node; nc_node = nc_node->next) {
                                                                if (xmlStrEqual(nc_node->name, (const xmlChar*)"NoteColumn")) {
                                                                    RawRenoiseNote rn;
                                                                    for (xmlNodePtr ncc = nc_node->children; ncc; ncc = ncc->next) {
                                                                        if (xmlStrEqual(ncc->name, (const xmlChar*)"Note")) rn.note = parse_renoise_note(get_node_content(ncc));
                                                                        else if (xmlStrEqual(ncc->name, (const xmlChar*)"Instrument")) {
                                                                            std::string s = get_node_content(ncc); if (s != ".." && !s.empty()) rn.instrument = std::atoi(s.c_str());
                                                                        } else if (xmlStrEqual(ncc->name, (const xmlChar*)"Volume")) {
                                                                            std::string s = get_node_content(ncc); if (s != ".." && !s.empty()) { rn.volume = (uint8_t)std::strtol(s.c_str(), nullptr, 16); if (rn.volume > 127) rn.volume = 127; }
                                                                        }
                                                                    }
                                                                    row_data[row_idx].push_back(rn);
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        rp.tracks_data.push_back(row_data);
                                    }
                                }
                            }
                        }
                        rp_list.push_back(rp);
                    }
                }
            }
        } else if (xmlStrEqual(node->name, (const xmlChar*)"Sequencer")) {
            for (xmlNodePtr child = node->children; child; child = child->next) {
                if (xmlStrEqual(child->name, (const xmlChar*)"SequenceEntry")) {
                    for (xmlNodePtr sec = child->children; sec; sec = sec->next) {
                        if (xmlStrEqual(sec->name, (const xmlChar*)"Pattern")) sequence.push_back(std::atoi(get_node_content(sec).c_str()));
                    }
                }
            }
        }
    }
    xmlFreeDoc(doc); xmlCleanupParser();

    // CLEAR ENGINE
    engine.m_tracks.clear();
    engine.m_buses.clear();
    engine.m_instruments.clear();
    engine.clear_patterns();
    engine.m_order.clear();

    // Shared Plugin/SoundFont Instruments
    std::map<uint8_t, Instrument*> shared_instruments;
    for (size_t i = 0; i < ri_list.size(); ++i) {
        const auto& ri = ri_list[i];
        if (ri.type == "Plugin") {
            auto plugin = std::make_unique<DSSIInstrument>(engine.sample_rate());
            plugin->set_name(ri.name); plugin->set_type(InstrumentType::Plugin);
            shared_instruments[i] = plugin.get(); engine.add_instrument(std::move(plugin));
        } else if (ri.type == "SoundFont") {
            auto sf = std::make_unique<SoundFontInstrument>(engine.sample_rate());
            sf->set_name(ri.name); sf->set_type(InstrumentType::SoundFont);
            shared_instruments[i] = sf.get(); engine.add_instrument(std::move(sf));
        }
    }

    // Mappings
    std::map<std::pair<size_t, uint8_t>, size_t> plugin_track_map;
    std::map<size_t, size_t> sampler_track_map;
    std::map<size_t, std::map<uint8_t, uint8_t>> sampler_inst_to_sample_idx;

    size_t disgrace_track_idx = 0;
    for (size_t rt = 0; rt < rt_list.size(); rt++) {
        std::set<uint8_t> used_plugins, used_samplers;
        for (const auto& rp : rp_list) {
            if (rt >= rp.tracks_data.size()) continue;
            for (const auto& row : rp.tracks_data[rt]) {
                for (const auto& rn : row) {
                    if (rn.instrument != 255 && rn.instrument < ri_list.size()) {
                        if (ri_list[rn.instrument].type == "Sampler") used_samplers.insert(rn.instrument);
                        else used_plugins.insert(rn.instrument);
                    }
                }
            }
        }

        if (!used_samplers.empty()) {
            engine.add_track();
            size_t dt = disgrace_track_idx++;
            sampler_track_map[rt] = dt;
            auto sampler = std::make_unique<SampleInstrument>(engine.sample_rate());
            sampler->set_name(rt_list[rt].name + " (Sampler)");
            sampler->set_type(InstrumentType::Sampler);
            uint8_t next_s_idx = 1;
            for (uint8_t ri_idx : used_samplers) {
                sampler_inst_to_sample_idx[rt][ri_idx] = next_s_idx;
                const auto& ri = ri_list[ri_idx];
                for (const auto& s : ri.samples) {
                    std::string full_path = extracted_dir + "/" + s.second;
                    auto data = std::make_shared<SampleData>(); std::vector<float> l, r; uint32_t sr;
                    if (AudioFile::load_audio(full_path, l, r, sr)) {
                        data->left = l; data->right = r; data->sample_rate = sr;
                        sampler->add_sample(ri.name, data); break; 
                    }
                }
                next_s_idx++;
            }
            engine.track(dt).set_instrument(sampler.get());
            engine.track(dt).set_name(rt_list[rt].name + " (Sampler)");
            
            std::string tname = rt_list[rt].name;
            std::transform(tname.begin(), tname.end(), tname.begin(), ::tolower);
            if (tname.find("drum") != std::string::npos || tname.find("perc") != std::string::npos) {
                engine.track(dt).set_notation(NotationType::Drums);
            }

            engine.add_instrument(std::move(sampler));
        }

        for (uint8_t ri_idx : used_plugins) {
            engine.add_track();
            size_t dt = disgrace_track_idx++;
            plugin_track_map[{rt, ri_idx}] = dt;
            engine.track(dt).set_instrument(shared_instruments[ri_idx]);
            engine.track(dt).set_name(rt_list[rt].name + " (" + ri_list[ri_idx].name + ")");
        }

        if (used_samplers.empty() && used_plugins.empty()) {
            engine.add_track();
            size_t dt = disgrace_track_idx++;
            auto none = std::make_unique<NoneInstrument>();
            engine.track(dt).set_instrument(none.get());
            engine.track(dt).set_name(rt_list[rt].name);
            engine.add_instrument(std::move(none));
        }
    }

    if (sequence.empty()) { for (size_t i = 0; i < rp_list.size(); i++) sequence.push_back(i); }

    for (size_t i = 0; i < rp_list.size(); ++i) {
        const auto& rp = rp_list[i];
        engine.add_pattern(std::make_unique<Pattern>(rp.num_lines, engine.track_count()));
        Pattern& pat = engine.pattern(engine.pattern_count() - 1);
        for (size_t rt = 0; rt < rp.tracks_data.size(); rt++) {
            const auto& renoise_track_data = rp.tracks_data[rt];
            for (size_t row = 0; row < renoise_track_data.size() && row < MAX_ROWS; row++) {
                const auto& line = renoise_track_data[row];
                for (size_t col = 0; col < line.size(); col++) {
                    const auto& rn = line[col];
                    if (rn.instrument != 255 && rn.instrument < ri_list.size()) {
                        if (ri_list[rn.instrument].type == "Sampler") {
                            size_t dt = sampler_track_map[rt];
                            TrackEvent& ev = pat.event(dt, row, col < MAX_COLS ? col : 0);
                            ev.note = rn.note; ev.volume = rn.volume; ev.sample_idx = sampler_inst_to_sample_idx[rt][rn.instrument];
                        } else {
                            auto itm = plugin_track_map.find({rt, rn.instrument});
                            if (itm != plugin_track_map.end()) {
                                TrackEvent& ev = pat.event(itm->second, row, col < MAX_COLS ? col : 0);
                                ev.note = rn.note; ev.volume = rn.volume;
                            }
                        }
                    } else if (rn.note == 254) {
                        if (sampler_track_map.count(rt)) pat.event(sampler_track_map[rt], row, 0).note = 254;
                        for (auto const& [key, val] : plugin_track_map) { if (key.first == rt) pat.event(val, row, 0).note = 254; }
                    }
                }
            }
        }
    }

    // Populate order correctly
    for (size_t pat_idx : sequence) {
        if (pat_idx < engine.pattern_count()) engine.m_order.push_back(pat_idx);
    }
    if (engine.m_order.empty()) engine.m_order.push_back(0);
    
    engine.m_active_pattern.store(engine.m_order[0]);
    engine.m_order_start.store(0);
    engine.m_order_end.store(engine.m_order.size() - 1);
    engine.m_current_row = 0;
    engine.m_current_tick = 0;
    engine.m_edit_order_pos.store(0);
    engine.m_order_pos.store(0);

    std::cout << "XRNS import complete: " << engine.track_count() << " tracks, " << engine.pattern_count() << " patterns" << std::endl;
    return true;
}

} // namespace disgrace_ns
