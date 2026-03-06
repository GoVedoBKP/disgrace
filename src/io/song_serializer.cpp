#include "song_serializer.h"
#include "../core/engine.h"
#include "../sequencer/sequencer.h"
#include "../sequencer/pattern.h"
#include "../mixer/track.h"
#include "../instrument/sample_instrument.h"
#include "../instrument/soundfont_instrument.h"
#include "audio_file.h"

#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace disgrace_ns
{

    bool SongSerializer::save(const Engine& engine, const ::std::string& folder)
    {
        fs::path base_path(folder);
        if (!fs::exists(base_path)) {
            fs::create_directories(base_path);
        }

        fs::path samples_dir = base_path / "samples";
        fs::path soundfonts_dir = base_path / "soundfonts";
        fs::create_directories(samples_dir);
        fs::create_directories(soundfonts_dir);

        json j;
        j["tempo"] = engine.tempo();
        j["lpb"]   = engine.lpb();
        j["order"] = engine.order_list();

        // Save instruments
        j["instruments"] = json::array();
        for (size_t i = 0; i < engine.instrument_count(); ++i) {
            const Instrument& inst = engine.instrument(i);
            json jinst;
            jinst["name"] = inst.name();
            jinst["type"] = (int)inst.type();

            if (inst.type() == InstrumentType::Sampler) {
                const auto& sampler = static_cast<const SampleInstrument&>(inst);
                json jsamples = json::array();
                for (size_t s = 0; s < sampler.sample_count(); ++s) {
                    const auto& sample = sampler.get_sample(s);
                    std::string sample_filename = "inst_" + std::to_string(i) + "_s" + std::to_string(s) + ".wav";
                    fs::path sample_path = samples_dir / sample_filename;
                    
                    AudioFile::save_wav(sample_path.string(), sample.data->left, sample.data->right, sample.data->sample_rate);
                    
                    json js;
                    js["name"] = sample.name;
                    js["file"] = "samples/" + sample_filename;
                    jsamples.push_back(js);
                }
                jinst["samples"] = jsamples;
            } else if (inst.type() == InstrumentType::SoundFont) {
                const auto& sf = static_cast<const SoundFontInstrument&>(inst);
                if (!sf.path().empty()) {
                    fs::path sf_src(sf.path());
                    std::string sf_filename = "inst_" + std::to_string(i) + "_" + sf_src.filename().string();
                    fs::path sf_dest = soundfonts_dir / sf_filename;
                    
                    try {
                        if (fs::exists(sf_src)) {
                            fs::copy_file(sf_src, sf_dest, fs::copy_options::overwrite_existing);
                            jinst["soundfont"] = "soundfonts/" + sf_filename;
                        }
                    } catch (...) {}
                }
                jinst["preset"] = sf.current_preset();
            }
            j["instruments"].push_back(jinst);
        }

        // Save tracks and their assigned instruments
        j["tracks"] = json::array();
        for (size_t t = 0; t < engine.track_count(); ++t) {
            json jt;
            jt["name"] = engine.track(t).name();
            jt["instrument_index"] = engine.get_instrument_index(engine.track(t).instrument());
            jt["volume"] = engine.track(t).volume();
            jt["pan"] = engine.track(t).pan;
            jt["output_bus"] = engine.track(t).output_bus();
            j["tracks"].push_back(jt);
        }

        // Save buses
        j["buses"] = json::array();
        for (size_t b = 0; b < engine.bus_count(); ++b) {
            json jb;
            jb["name"] = engine.bus(b).name();
            jb["volume"] = engine.bus(b).volume();
            jb["pan"] = engine.bus(b).pan();
            jb["output_bus"] = engine.bus(b).output_bus();
            j["buses"].push_back(jb);
        }

        // Save patterns
        j["patterns"] = json::array();
        for (size_t p = 0; p < engine.pattern_count(); ++p)
        {
            const Pattern& pat = engine.pattern(p);
            json jp;
            jp["rows"] = pat.row_count();
            jp["tracks"] = json::array();

            for (size_t t = 0; t < pat.track_count(); ++t)
            {
                json jtrack;
                jtrack["cols"] = pat.column_count(t);
                json jdata = json::array();

                for (size_t r = 0; r < pat.row_count(); ++r)
                {
                    json jrow = json::array();
                    for (size_t c = 0; c < pat.column_count(t); ++c) {
                        const TrackEvent& ev = pat.event(t, r, c);
                        json jev;
                        jev["note"]       = ev.note;
                        jev["sample"]     = ev.sample_idx;
                        jev["volume"]     = ev.volume;
                        jev["fx1"]        = ev.effect1;
                        jev["p1"]         = ev.param1;
                        jev["fx2"]        = ev.effect2;
                        jev["p2"]         = ev.param2;
                        jrow.push_back(jev);
                    }
                    jdata.push_back(jrow);
                }
                jtrack["data"] = jdata;
                jp["tracks"].push_back(jtrack);
            }
            j["patterns"].push_back(jp);
        }

        ::std::ofstream file(base_path / "song.json");
        if (!file) return false;
        file << j.dump(2);
        return true;
    }

    bool SongSerializer::load(Engine& engine, const ::std::string& folder)
    {
        fs::path base_path(folder);
        ::std::ifstream file(base_path / "song.json");
        if (!file) return false;

        json j;
        file >> j;

        engine.new_project(); // Clear current state

        engine.set_tempo(j["tempo"]);
        engine.set_lpb(j["lpb"]);
        engine.set_order(j["order"].get<::std::vector<uint8_t>>());

        // Load instruments
        if (j.contains("instruments")) {
            engine.m_instruments.clear();
            for (auto& ji : j["instruments"]) {
                InstrumentType type = (InstrumentType)ji["type"];
                engine.add_instrument();
                size_t idx = engine.instrument_count() - 1;
                engine.set_instrument_type(idx, type);
                Instrument& inst = engine.instrument(idx);
                inst.set_name(ji["name"]);

                if (type == InstrumentType::Sampler && ji.contains("samples")) {
                    SampleInstrument& sampler = static_cast<SampleInstrument&>(inst);
                    for (auto& js : ji["samples"]) {
                        std::shared_ptr<SampleData> sd = std::make_shared<SampleData>();
                        fs::path sample_path = base_path / js["file"].get<std::string>();
                        uint32_t rate;
                        if (AudioFile::load_audio(sample_path.string(), sd->left, sd->right, rate)) {
                            sd->sample_rate = rate;
                            sampler.add_sample(js["name"], sd);
                        }
                    }
                } else if (type == InstrumentType::SoundFont && ji.contains("soundfont")) {
                    SoundFontInstrument& sf = static_cast<SoundFontInstrument&>(inst);
                    fs::path sf_path = base_path / ji["soundfont"].get<std::string>();
                    if (sf.load_soundfont(sf_path.string())) {
                        sf.set_preset(ji["preset"]);
                    }
                }
            }
        }

        // Load buses
        if (j.contains("buses")) {
            for (auto& jb : j["buses"]) {
                engine.add_bus();
                MixerBus& bus = engine.bus(engine.bus_count() - 1);
                bus.set_name(jb["name"]);
                bus.set_volume(jb["volume"]);
                bus.set_pan(jb["pan"]);
                bus.set_output_bus(jb["output_bus"]);
            }
        }

        // Load tracks
        if (j.contains("tracks")) {
            for (auto& jt : j["tracks"]) {
                engine.add_track();
                Track& track = engine.track(engine.track_count() - 1);
                track.set_name(jt["name"]);
                track.set_volume(jt["volume"]);
                track.pan = jt["pan"];
                track.set_output_bus(jt.value("output_bus", -1));
                int inst_idx = jt["instrument_index"];
                if (inst_idx >= 0 && inst_idx < (int)engine.instrument_count()) {
                    track.set_instrument(&engine.instrument(inst_idx));
                }
            }
        }

        auto& patterns = j["patterns"];
        engine.m_patterns.clear();
        for (size_t p = 0; p < patterns.size(); ++p)
        {
            size_t rows = patterns[p]["rows"];
            engine.m_patterns.emplace_back(rows, engine.track_count());
            Pattern& pat = engine.m_patterns.back();

            auto& jtracks = patterns[p]["tracks"];
            for (size_t t = 0; t < jtracks.size() && t < pat.track_count(); ++t)
            {
                size_t cols = jtracks[t]["cols"];
                pat.set_column_count(t, cols);
                auto& jdata = jtracks[t]["data"];
                
                for (size_t r = 0; r < jdata.size() && r < pat.row_count(); ++r)
                {
                    auto& jrow = jdata[r];
                    for (size_t c = 0; c < jrow.size() && c < pat.column_count(t); ++c) {
                        auto& jev = jrow[c];
                        auto& ev = pat.event(t, r, c);
                        ev.note       = jev["note"];
                        ev.sample_idx = jev["sample"];
                        ev.volume     = jev["volume"];
                        ev.effect1    = jev["fx1"];
                        ev.param1     = jev["p1"];
                        ev.effect2    = jev["fx2"];
                        ev.param2     = jev["p2"];
                    }
                }
            }
        }
        return true;
    }

} // namespace disgrace_ns
