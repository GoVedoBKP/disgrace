/*
 * Disgrace - Digital Audio Workstation
 * Copyright (C) 2025  Miroslav Shaltev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "xrni_instrument.h"
#include "../io/audio_file.h"

#include <archive.h>
#include <archive_entry.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

namespace disgrace_ns
{

// ─────────────────────────────────────────────────────────────── helpers ────

static std::string xml_content(xmlNodePtr node)
{
    xmlChar* raw = xmlNodeGetContent(node);
    std::string s;
    if (raw) { s = (const char*)raw; xmlFree(raw); }
    return s;
}

// Parse a Renoise note string: "C-4", "C#5", "G-9", or plain integer.
// Formula: note_num + (octave + 1) * 12   (C-4 → 60, same as MIDI middle C)
int XrniInstrument::parse_note(const std::string& s)
{
    if (s.empty()) return 60;

    // Plain integer?
    if (std::isdigit((unsigned char)s[0]) || s[0] == '-') {
        int v = std::atoi(s.c_str());
        return std::clamp(v, 0, 127);
    }

    char letter = (char)std::toupper((unsigned char)s[0]);
    int note_num = 0;
    switch (letter) {
        case 'C': note_num = 0;  break;
        case 'D': note_num = 2;  break;
        case 'E': note_num = 4;  break;
        case 'F': note_num = 5;  break;
        case 'G': note_num = 7;  break;
        case 'A': note_num = 9;  break;
        case 'B': note_num = 11; break;
        default:  return 60;
    }

    int octave = 0;
    if (s.size() >= 2 && s[1] == '#') {
        note_num++;
        if (s.size() >= 3) octave = std::atoi(s.substr(2).c_str());
    } else if (s.size() >= 2 && s[1] == 'b') {
        note_num--;
        if (note_num < 0) note_num = 11;
        if (s.size() >= 3) octave = std::atoi(s.substr(2).c_str());
    } else {
        if (s.size() >= 2) octave = std::atoi(s.substr(1).c_str());  // handles "C4" or "C-4" (substr skips '-')
        // For "C-4": substr(1) = "-4", atoi("-4") = -4 ... but we want octave=4.
        // Renoise uses '-' as separator: "C-4" means C octave 4. The '-' is NOT minus.
        // Check if the second char is '-' followed by a digit:
        if (s.size() >= 3 && s[1] == '-') {
            octave = std::atoi(s.substr(2).c_str());
        }
    }

    return std::clamp(note_num + (octave + 1) * 12, 0, 127);
}

float XrniInstrument::midi_note_to_hz(int note)
{
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

// ──────────────────────────────────────────────────────── extract zip ────

static bool extract_xrni(const std::string& zip_path, const std::string& dest_dir)
{
    struct archive* a = archive_read_new();
    archive_read_support_format_zip(a);
    archive_read_support_filter_all(a);

    if (archive_read_open_filename(a, zip_path.c_str(), 10240) != ARCHIVE_OK) {
        archive_read_free(a);
        return false;
    }

    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        std::string name = archive_entry_pathname(entry);

        // Only extract Instrument.xml and Samples/
        if (name == "Instrument.xml" || name.rfind("Samples/", 0) == 0) {
            std::string dest = dest_dir + "/" + name;
            fs::path parent  = fs::path(dest).parent_path();
            if (!fs::exists(parent)) fs::create_directories(parent);

            if (!name.empty() && name.back() != '/') {
                archive_entry_set_pathname(entry, dest.c_str());
                int r = archive_read_extract(a, entry, 0);
                if (r != ARCHIVE_OK)
                    std::cerr << "xrni: extract failed: " << name << ": "
                              << archive_error_string(a) << "\n";
            }
        }
        archive_read_data_skip(a);
    }

    archive_read_close(a);
    archive_read_free(a);
    return true;
}

// ────────────────────────────────────────────── constructor / load ────

XrniInstrument::XrniInstrument(double sample_rate)
    : m_sample_rate(sample_rate)
{
    set_type(InstrumentType::XRNI);
}

bool XrniInstrument::load_xrni(const std::string& path)
{
    m_regions.clear();
    m_sample_names.clear();
    m_voices.clear();

    m_path = path;

    // Extract to temp dir
    fs::path tmp = fs::temp_directory_path() / "disgrace_xrni_load";
    try {
        if (fs::exists(tmp)) fs::remove_all(tmp);
        fs::create_directories(tmp);
    } catch (const std::exception& e) {
        std::cerr << "xrni: cannot create temp dir: " << e.what() << "\n";
        return false;
    }

    if (!extract_xrni(path, tmp.string())) {
        std::cerr << "xrni: failed to extract " << path << "\n";
        fs::remove_all(tmp);
        return false;
    }

    // Parse Instrument.xml
    std::string xml_path = (tmp / "Instrument.xml").string();
    if (!fs::exists(xml_path)) {
        std::cerr << "xrni: Instrument.xml not found inside " << path << "\n";
        fs::remove_all(tmp);
        return false;
    }

    LIBXML_TEST_VERSION
    xmlDocPtr doc = xmlReadFile(xml_path.c_str(), nullptr, 0);
    if (!doc) {
        std::cerr << "xrni: cannot parse Instrument.xml\n";
        fs::remove_all(tmp);
        return false;
    }

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root) { xmlFreeDoc(doc); fs::remove_all(tmp); return false; }

    // Grab instrument name
    for (xmlNodePtr n = root->children; n; n = n->next) {
        if (xmlStrEqual(n->name, (const xmlChar*)"Name")) {
            m_xrni_name = xml_content(n);
            break;
        }
    }
    if (m_xrni_name.empty()) {
        m_xrni_name = fs::path(path).stem().string();
    }

    // Find Samples/SampleList node
    xmlNodePtr samples_node = nullptr;
    for (xmlNodePtr n = root->children; n; n = n->next) {
        if (xmlStrEqual(n->name, (const xmlChar*)"Samples") ||
            xmlStrEqual(n->name, (const xmlChar*)"SampleList"))
        {
            samples_node = n;
            break;
        }
    }

    if (!samples_node) {
        std::cerr << "xrni: no Samples element found\n";
        xmlFreeDoc(doc);
        fs::remove_all(tmp);
        return false;
    }

    // Parse each Sample element
    for (xmlNodePtr sn = samples_node->children; sn; sn = sn->next) {
        if (!xmlStrEqual(sn->name, (const xmlChar*)"Sample")) continue;

        SfzRegion reg;
        std::string sample_filename;
        std::string sample_name;
        bool loop_forward = false;

        for (xmlNodePtr ch = sn->children; ch; ch = ch->next) {
            if (xmlStrEqual(ch->name, (const xmlChar*)"Name")) {
                sample_name = xml_content(ch);

            } else if (xmlStrEqual(ch->name, (const xmlChar*)"FileName") ||
                       xmlStrEqual(ch->name, (const xmlChar*)"SampleFile")) {
                sample_filename = xml_content(ch);

            } else if (xmlStrEqual(ch->name, (const xmlChar*)"Volume")) {
                float v = std::atof(xml_content(ch).c_str());
                // Renoise volume 0.0–4.0 (1.0 = unity). Convert to dB.
                if (v <= 0.0f) v = 0.0001f;
                reg.volume = 20.0f * std::log10(v);

            } else if (xmlStrEqual(ch->name, (const xmlChar*)"Panning")) {
                // 0.0=left, 0.5=centre, 1.0=right — ignored for now (mono sample routing)

            } else if (xmlStrEqual(ch->name, (const xmlChar*)"Transpose")) {
                reg.transpose = std::atoi(xml_content(ch).c_str());

            } else if (xmlStrEqual(ch->name, (const xmlChar*)"Finetune")) {
                // Renoise finetune: -127..127 (semitone/128 steps), convert to cents
                reg.tune = (float)std::atoi(xml_content(ch).c_str()) / 1.27f;

            } else if (xmlStrEqual(ch->name, (const xmlChar*)"LoopMode")) {
                std::string lm = xml_content(ch);
                loop_forward = (lm == "Forward" || lm == "PingPong");
                reg.loop_enabled = loop_forward;

            } else if (xmlStrEqual(ch->name, (const xmlChar*)"LoopStart")) {
                reg.loop_start = (int64_t)std::atoll(xml_content(ch).c_str());

            } else if (xmlStrEqual(ch->name, (const xmlChar*)"LoopEnd")) {
                reg.loop_end = (int64_t)std::atoll(xml_content(ch).c_str());

            } else if (xmlStrEqual(ch->name, (const xmlChar*)"SampleMapping") ||
                       xmlStrEqual(ch->name, (const xmlChar*)"KeyMapping")) {
                // Parse key/velocity mapping
                for (xmlNodePtr mp = ch->children; mp; mp = mp->next) {
                    if (xmlStrEqual(mp->name, (const xmlChar*)"NoteRangeLow") ||
                        xmlStrEqual(mp->name, (const xmlChar*)"KeyZoneLow")) {
                        reg.lokey = parse_note(xml_content(mp));
                    } else if (xmlStrEqual(mp->name, (const xmlChar*)"NoteRangeHigh") ||
                               xmlStrEqual(mp->name, (const xmlChar*)"KeyZoneHigh")) {
                        reg.hikey = parse_note(xml_content(mp));
                    } else if (xmlStrEqual(mp->name, (const xmlChar*)"BaseNote") ||
                               xmlStrEqual(mp->name, (const xmlChar*)"RootNote") ||
                               xmlStrEqual(mp->name, (const xmlChar*)"MapKey")) {
                        reg.pitch_keycenter = parse_note(xml_content(mp));
                    } else if (xmlStrEqual(mp->name, (const xmlChar*)"VelocityRangeLow") ||
                               xmlStrEqual(mp->name, (const xmlChar*)"VelocityZoneLow")) {
                        reg.lovel = std::atoi(xml_content(mp).c_str());
                    } else if (xmlStrEqual(mp->name, (const xmlChar*)"VelocityRangeHigh") ||
                               xmlStrEqual(mp->name, (const xmlChar*)"VelocityZoneHigh")) {
                        reg.hivel = std::atoi(xml_content(mp).c_str());
                    } else if (xmlStrEqual(mp->name, (const xmlChar*)"TuningSemitones")) {
                        reg.transpose += std::atoi(xml_content(mp).c_str());
                    } else if (xmlStrEqual(mp->name, (const xmlChar*)"TuningCents")) {
                        reg.tune += (float)std::atoi(xml_content(mp).c_str());
                    }
                }
            }
        }

        // Resolve sample path: "Samples/00 Piano C4.wav" → tmp / "Samples/00 Piano C4.wav"
        if (sample_filename.empty()) {
            std::cerr << "xrni: sample with no filename, skipping\n";
            continue;
        }

        fs::path audio_path = tmp / sample_filename;
        if (!fs::exists(audio_path)) {
            // Some XRNI files use just the basename without the "Samples/" prefix
            audio_path = tmp / "Samples" / fs::path(sample_filename).filename();
        }

        if (!fs::exists(audio_path)) {
            std::cerr << "xrni: sample file not found: " << sample_filename << "\n";
            continue;
        }

        auto data = std::make_shared<SampleData>();
        uint32_t sr = 0;
        if (!AudioFile::load_audio(audio_path.string(), data->left, data->right, sr)) {
            std::cerr << "xrni: failed to load audio: " << audio_path << "\n";
            continue;
        }
        if (data->right.empty()) data->right = data->left; // mono → stereo
        data->sample_rate = (int)sr;

        reg.data = data;
        reg.sample = sample_filename;

        m_regions.push_back(reg);
        m_sample_names.push_back(sample_name.empty() ? fs::path(sample_filename).stem().string()
                                                      : sample_name);
    }

    xmlFreeDoc(doc);
    fs::remove_all(tmp);

    std::cerr << "xrni: loaded " << m_regions.size() << " samples from " << path << "\n";
    return !m_regions.empty();
}

// ──────────────────────────────────────────────────────── playback ────

void XrniInstrument::note_on(uint8_t note, uint8_t velocity,
                              size_t column_index, size_t offset_samples, uint8_t)
{
    for (auto& sv : m_voices)
        if (sv.column == (int)column_index) sv.voice->panic();

    float note_hz = midi_note_to_hz(note);
    float vel_f   = velocity / 127.0f;

    for (const auto& reg : m_regions) {
        if (note < reg.lokey || note > reg.hikey) continue;
        if (velocity < reg.lovel || velocity > reg.hivel) continue;
        if (!reg.data) continue;

        float keycenter_hz = midi_note_to_hz(reg.pitch_keycenter + reg.transpose);
        float tune_factor  = std::pow(2.0f, reg.tune / 1200.0f);
        float start_freq   = note_hz / keycenter_hz * 440.0f * tune_factor;

        float gain = std::pow(10.0f, reg.volume / 20.0f) * m_volume * vel_f;

        auto sv = std::make_unique<SampleVoice>(reg.data, m_sample_rate);
        sv->set_adsr(reg.ampeg_attack, reg.ampeg_decay, reg.ampeg_sustain, reg.ampeg_release);
        sv->set_volume(gain);

        size_t end_pos = (reg.loop_end > 0) ? (size_t)reg.loop_end : 0;
        sv->set_region(end_pos, reg.loop_enabled, (size_t)reg.loop_start);
        sv->start(note, velocity, start_freq, offset_samples);

        if (m_voices.size() >= MAX_VOICES) {
            auto it = std::find_if(m_voices.begin(), m_voices.end(),
                [](const SfzVoice& v){ return !v.voice->active(); });
            if (it == m_voices.end()) it = m_voices.begin();
            m_voices.erase(it);
        }

        m_voices.push_back({ std::move(sv), (int)column_index, note });
    }
}

void XrniInstrument::note_off(size_t column_index)
{
    for (auto& sv : m_voices)
        if (sv.column == (int)column_index) sv.voice->stop();
}

void XrniInstrument::panic()
{
    for (auto& sv : m_voices) sv.voice->panic();
    m_voices.clear();
}

void XrniInstrument::set_volume(float vol)
{
}

void XrniInstrument::process(float* l, float* r, size_t nframes)
{
    for (auto& sv : m_voices)
        if (sv.voice->active()) sv.voice->process(l, r, nframes);

    m_voices.erase(
        std::remove_if(m_voices.begin(), m_voices.end(),
                       [](const SfzVoice& v){ return !v.voice->active(); }),
        m_voices.end());
}

} // namespace disgrace_ns
