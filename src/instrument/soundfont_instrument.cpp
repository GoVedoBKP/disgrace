#include "soundfont_instrument.h"
#include <iostream>

namespace disgrace_ns
{

    SoundFontInstrument::SoundFontInstrument(double sample_rate)
    {
        m_fluid_settings = new_fluid_settings();
        fluid_settings_setnum(m_fluid_settings, "synth.sample-rate", sample_rate);
        m_fluid_synth = new_fluid_synth(m_fluid_settings);
    }

    SoundFontInstrument::~SoundFontInstrument()
    {
        delete_fluid_synth(m_fluid_synth);
        delete_fluid_settings(m_fluid_settings);
    }

    void SoundFontInstrument::note_on(uint8_t note, uint8_t velocity, size_t column_index, size_t, uint8_t)
    {
        // Simple tracker behavior: one note at a time per column
        // We could track last note per column to send noteoff
        fluid_synth_noteon(m_fluid_synth, (int)column_index % 16, note, velocity);
    }

    void SoundFontInstrument::note_off(size_t column_index)
    {
        fluid_synth_all_notes_off(m_fluid_synth, (int)column_index % 16);
    }

    void SoundFontInstrument::panic()
    {
        for (int i = 0; i < 16; ++i) {
            fluid_synth_all_sounds_off(m_fluid_synth, i);
        }
    }

    void SoundFontInstrument::set_volume(float vol)
    {
        m_volume = vol;
        fluid_synth_set_gain(m_fluid_synth, vol);
    }

    void SoundFontInstrument::set_pitch(float freq)
    {
        // Pitch control not implemented for SF via this method yet
    }

    void SoundFontInstrument::process(float* l, float* r, size_t nframes)
    {
        float* out[2] = {l, r};
        fluid_synth_process(m_fluid_synth, (int)nframes, 0, NULL, 2, out);
    }

    bool SoundFontInstrument::load_soundfont(const std::string& path)
    {
        if (m_sfont_id != -1) {
            fluid_synth_sfunload(m_fluid_synth, m_sfont_id, 1);
            m_sfont_id = -1;
            m_presets.clear();
        }

        m_sfont_id = fluid_synth_sfload(m_fluid_synth, path.c_str(), 1);
        if (m_sfont_id == -1) return false;

        m_path = path;

        fluid_sfont_t* sfont = fluid_synth_get_sfont_by_id(m_fluid_synth, m_sfont_id);
        if (sfont) {
            fluid_sfont_iteration_start(sfont);
            fluid_preset_t* preset;
            while ((preset = fluid_sfont_iteration_next(sfont))) {
                m_presets.push_back({
                    fluid_preset_get_banknum(preset),
                    fluid_preset_get_num(preset),
                    fluid_preset_get_name(preset)
                });
            }
        }

        if (!m_presets.empty()) {
            set_preset(0);
        }

        return true;
    }

    void SoundFontInstrument::set_preset(int index)
    {
        if (index >= 0 && index < (int)m_presets.size()) {
            m_selected_preset = index;
            // Apply preset to all 16 channels to ensure consistency across tracker columns
            for (int i = 0; i < 16; ++i) {
                // Use program_select to specify the exact soundfont and preset
                fluid_synth_program_select(m_fluid_synth, i, m_sfont_id, m_presets[index].bank, m_presets[index].num);
                // Also explicitly set bank and program as a secondary measure for better compatibility
                fluid_synth_bank_select(m_fluid_synth, i, m_presets[index].bank);
                fluid_synth_program_change(m_fluid_synth, i, m_presets[index].num);
            }
        }
    }

} // namespace disgrace_ns
