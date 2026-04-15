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

#include "track.h"
#include "../instrument/instrument.h"
#include <cmath> // Added for powf and fabs
#include <algorithm>

namespace disgrace_ns
{

disgrace_ns::Track::Track()
    : m_volume(0.5f), m_meter_l(0.0f), m_meter_r(0.0f), m_current_freq(440.0f), m_name("New Track"), m_output_bus(-1), m_notation(NotationType::Violin)
{
}

disgrace_ns::Track::Track(Track&& other) noexcept
    : m_fx_state(other.m_fx_state),
      m_frequency(other.m_frequency),
      pan(other.pan),
      last_volume(other.last_volume),
      m_volume(other.m_volume),
      m_mute(other.m_mute),
      m_solo(other.m_solo),
      m_notation(other.m_notation),
      m_output_bus(other.m_output_bus),
      m_name(std::move(other.m_name)),
      m_instrument(other.m_instrument),
      m_chain(std::move(other.m_chain)),
      m_meter_l(other.m_meter_l.load()),
      m_meter_r(other.m_meter_r.load()),
      m_current_freq(other.m_current_freq)
{
}

Track& disgrace_ns::Track::operator=(Track&& other) noexcept
{
    if (this != &other) {
        m_fx_state = other.m_fx_state;
        m_frequency = other.m_frequency;
        pan = other.pan;
        last_volume = other.last_volume;
        m_volume = other.m_volume;
        m_mute = other.m_mute;
        m_solo = other.m_solo;
        m_notation = other.m_notation;
        m_output_bus = other.m_output_bus;
        m_name = std::move(other.m_name);
        m_instrument = other.m_instrument;
        m_chain = std::move(other.m_chain);
        m_meter_l.store(other.m_meter_l.load());
        m_meter_r.store(other.m_meter_r.load());
        m_current_freq = other.m_current_freq;
    }
    return *this;
}

void disgrace_ns::Track::set_instrument(disgrace_ns::Instrument* inst)
{
    m_instrument = inst;
}

disgrace_ns::Instrument* disgrace_ns::Track::instrument()
{
    return m_instrument;
}

const disgrace_ns::Instrument* disgrace_ns::Track::instrument() const
{
    return m_instrument;
}

void disgrace_ns::Track::set_name(const std::string& name)
{
    m_name = name.substr(0, 32);
}

const std::string& disgrace_ns::Track::name() const
{
    return m_name;
}

void disgrace_ns::Track::process(float* out_l,
                    float* out_r,
                    size_t nframes,
                    const float* const* in_bufs)
{
    std::fill(out_l, out_l + nframes, 0.f);
    std::fill(out_r, out_r + nframes, 0.f);

    if (m_instrument)
        m_instrument->process(out_l, out_r, nframes);

    if (in_bufs && m_audio_input_l >= 0) {
        const float* in_l = in_bufs[m_audio_input_l];
        // If m_audio_input_r is valid, use it; otherwise, use in_l for mono.
        const float* in_r = (m_audio_input_r >= 0) ? in_bufs[m_audio_input_r] : in_l;

        if (m_input_delay_samples > 0 && !m_delay_buf_l.empty()) {
            size_t max_delay = m_delay_buf_l.size();
            for (size_t i = 0; i < nframes; ++i) {
                m_delay_buf_l[m_delay_ptr] = in_l[i];
                m_delay_buf_r[m_delay_ptr] = in_r[i];

                size_t read_ptr = (m_delay_ptr + max_delay - m_input_delay_samples) % max_delay;
                out_l[i] += m_delay_buf_l[read_ptr];
                out_r[i] += m_delay_buf_r[read_ptr];

                m_delay_ptr = (m_delay_ptr + 1) % max_delay;
            }
        } else {
            for (size_t i = 0; i < nframes; ++i) {
                out_l[i] += in_l[i];
                out_r[i] += in_r[i];
            }
        }
    }

    m_chain.process(out_l, out_r, nframes);

    // Apply volume + pan; measure peak in the same pass.
    const float vol      = volume();
    const float gain_l   = vol * (pan <= 0.f ? 1.0f : 1.0f - pan);
    const float gain_r   = vol * (pan >= 0.f ? 1.0f : 1.0f + pan);

    float peak_l = 0.f;
    float peak_r = 0.f;

    for (size_t i = 0; i < nframes; ++i) {
        out_l[i] *= gain_l;
        out_r[i] *= gain_r;
        float al = std::fabs(out_l[i]);
        float ar = std::fabs(out_r[i]);
        if (al > peak_l) peak_l = al;
        if (ar > peak_r) peak_r = ar;
    }

    // simple smoothing
    float prev_l = m_meter_l.load();
    float smoothed_l = 0.8f * prev_l + 0.2f * peak_l;
    m_meter_l.store(smoothed_l);

    float prev_r = m_meter_r.load();
    float smoothed_r = 0.8f * prev_r + 0.2f * peak_r;
    m_meter_r.store(smoothed_r);
}

float disgrace_ns::Track::note_to_frequency(uint8_t note) // Added namespace and class prefix
{
    return 440.0f * powf(2.0f, (int(note) - 69) / 12.0f);
}


void disgrace_ns::Track::note_on(uint8_t note, uint8_t velocity, size_t column_index, size_t offset_samples, uint8_t sample_index) // Added namespace and class prefix
{
    if (!m_instrument)
        return;

    float freq = note_to_frequency(note);

    if (m_fx_state.porta_active)
    {
        m_fx_state.porta_target = freq;
    }
    else
    {
        m_instrument->set_pitch(freq);
        m_instrument->note_on(note, velocity, column_index, offset_samples, sample_index);
        m_current_freq = freq; // Update m_current_freq on note_on
    }
}

void disgrace_ns::Track::note_off(size_t column_index)
{
    if (m_instrument)
        m_instrument->note_off(column_index);
}

void disgrace_ns::Track::panic()
{
    if (m_instrument)
        m_instrument->panic();
    
    // Reset FX states
    m_fx_state.vol_slide_up = 0;
    m_fx_state.vol_slide_down = 0;
    m_fx_state.porta_active = false;
    m_fx_state.retrig_ticks = 0;
    m_fx_state.note_cut_tick = -1;
}

size_t disgrace_ns::Track::total_latency() const
{
    size_t sum = 0;

    for (const auto& fx : m_chain.effects()) // Corrected m_fx_chain to m_chain
        sum += fx->latency();

    return sum;
}

void disgrace_ns::Track::set_volume(float v)
{
    m_volume = v;
}

float disgrace_ns::Track::volume() const
{
    return m_volume;
}

void disgrace_ns::Track::set_pan(float p)
{
    pan = std::max(-1.0f, std::min(1.0f, p));
}

float disgrace_ns::Track::get_pan() const
{
    return pan;
}

void disgrace_ns::Track::set_mute(bool m)
{
    m_mute = m;
}

bool disgrace_ns::Track::muted() const
{
    return m_mute;
}


void disgrace_ns::Track::process_tick(uint32_t engine_current_tick) // Added namespace and class prefix, updated parameter
{
    // --- volume slide ---
    if (m_fx_state.vol_slide_up > 0)
        m_volume += m_fx_state.vol_slide_up * 0.01f;

    if (m_fx_state.vol_slide_down > 0)
        m_volume -= m_fx_state.vol_slide_down * 0.01f;

    if (m_volume < 0.f) m_volume = 0.f;
    if (m_volume > 1.f) m_volume = 1.f;

    // --- portamento ---
    if (m_fx_state.porta_active)
    {
        float current = m_current_freq;

        if (current < m_fx_state.porta_target)
        {
            current += m_fx_state.porta_speed;
            if (current > m_fx_state.porta_target)
                current = m_fx_state.porta_target;
        }
        else
        {
            current -= m_fx_state.porta_speed;
            if (current < m_fx_state.porta_target)
                current = m_fx_state.porta_target;
        }

        m_current_freq = current;
        m_instrument->set_pitch(current);
    }


    // --- retrig ---
    if (m_fx_state.retrig_ticks > 0)
    {
        m_fx_state.retrig_counter++;

        if (m_fx_state.retrig_counter >=
            m_fx_state.retrig_ticks)
        {
            retrigger_note();
            m_fx_state.retrig_counter = 0;
        }
    }

    // --- note cut ---
    if (m_fx_state.note_cut_tick >= 0)
    {
        if (engine_current_tick == // Corrected from m_engine_current_tick
            m_fx_state.note_cut_tick)
        {
            note_off();
            m_fx_state.note_cut_tick = -1;
        }
    }
}

void disgrace_ns::Track::retrigger_note()
{
    if (m_instrument)
    {
        // Re-trigger the current note, assuming velocity of 100
        // A more robust solution might store the last note/velocity
        m_instrument->note_on(0, 100); // Placeholder, ideally should re-trigger last note
        m_instrument->set_pitch(m_current_freq);
    }
}

float disgrace_ns::Track::meter_level_l() const
{
    return m_meter_l.load();
}

float disgrace_ns::Track::meter_level_r() const
{
    return m_meter_r.load();
}

bool disgrace_ns::Track::solo() const
{
    return m_solo;
}

void disgrace_ns::Track::set_solo(bool s)
{
    m_solo = s;
}

void disgrace_ns::Track::set_output_bus(int bus_idx)
{
    m_output_bus = bus_idx;
}

int disgrace_ns::Track::output_bus() const
{
    return m_output_bus;
}

void disgrace_ns::Track::set_effect(size_t index, ::std::unique_ptr<disgrace_ns::DSP> dsp)
{
    m_chain.set(index, std::move(dsp));
}

void disgrace_ns::Track::enable_effect(size_t index, bool en)
{
    m_chain.enable(index, en);
}

void disgrace_ns::Track::move_effect_up(size_t index)
{
    m_chain.move_up(index);
}

void disgrace_ns::Track::move_effect_down(size_t index)
{
    m_chain.move_down(index);
}

void disgrace_ns::Track::remove_effect(size_t index)
{
    m_chain.remove(index);
}

void disgrace_ns::Track::load_effect_chain(const std::string& path)
{
    m_chain.load_chain(path);
}

void disgrace_ns::Track::save_effect_chain(const std::string& path)
{
    m_chain.save_chain(path);
}

disgrace_ns::DSP* disgrace_ns::Track::get_effect(size_t index) const
{
    if (index >= disgrace_ns::MAX_INSERTS) return nullptr;
    return m_chain.effects()[index].get();
}

bool disgrace_ns::Track::is_effect_enabled(size_t index) const
{
    // Need to add m_enabled to effects access in DSPChain if we want to check it easily,
    // or we assume it's always enabled if set for now.
    return true; 
}

void disgrace_ns::Track::set_audio_input(int channel_l, int channel_r)
{
    m_audio_input_l = channel_l;
    m_audio_input_r = channel_r;
}

void disgrace_ns::Track::get_audio_input(int& channel_l, int& channel_r) const
{
    channel_l = m_audio_input_l;
    channel_r = m_audio_input_r;
}

void disgrace_ns::Track::set_input_delay(float ms, uint32_t sample_rate)
{
    m_input_delay_ms = ms;
    m_input_delay_samples = (size_t)(ms * 0.001f * sample_rate);
    
    // Resize delay buffers to accommodate up to 1 second of delay
    if (m_delay_buf_l.size() < (size_t)sample_rate) {
        m_delay_buf_l.resize(sample_rate, 0.0f);
        m_delay_buf_r.resize(sample_rate, 0.0f);
        m_delay_ptr = 0;
    }
}

float disgrace_ns::Track::input_delay() const
{
    return m_input_delay_ms;
}

void disgrace_ns::Track::schedule_note_on(uint8_t note, uint8_t velocity,
                                           size_t column, uint8_t sample_idx,
                                           size_t samples_per_row)
{
    // Apply velocity humanization first (always; independent of timing).
    int vel = velocity;
    if (m_humanize_vel > 0) {
        int spread = m_humanize_vel;
        int delta  = (int)(rng_next() % (2 * spread + 1)) - spread;
        vel = std::max(1, std::min(127, vel + delta));
    }

    // If timing humanization is disabled, fire immediately.
    if (m_humanize_timing == 0) {
        note_on(note, (uint8_t)vel, column, 0, sample_idx);
        return;
    }

    // Compute random delay in samples (0 … max_ms * sr / 1000).
    // samples_per_row is passed as a proxy for sample_rate context;
    // cap delay to at most half a row so notes never arrive late for the next row.
    int32_t max_delay = (int32_t)((m_humanize_timing / 1000.0) * 48000.0);
    int32_t delay     = (int32_t)(rng_next() % (uint32_t)(max_delay + 1));
    delay = std::min(delay, (int32_t)(samples_per_row / 2));

    // Find a free slot in the pending queue.
    for (size_t s = 0; s < MAX_PENDING; ++s) {
        if (!m_pending[s].active) {
            m_pending[s] = {true, note, (uint8_t)vel, sample_idx, column, delay};
            return;
        }
    }
    // No free slot — fire immediately rather than dropping the note.
    note_on(note, (uint8_t)vel, column, 0, sample_idx);
}

void disgrace_ns::Track::fire_pending_notes(size_t frames)
{
    for (size_t s = 0; s < MAX_PENDING; ++s) {
        if (!m_pending[s].active) continue;
        m_pending[s].delay_samples -= (int32_t)frames;
        if (m_pending[s].delay_samples <= 0) {
            note_on(m_pending[s].note, m_pending[s].velocity,
                    m_pending[s].column, 0, m_pending[s].sample_idx);
            m_pending[s].active = false;
        }
    }
}

} // namespace disgrace_ns
