#include "transportbar.h"
#include "../core/engine.h"
#include <FL/Fl_Check_Button.H>

namespace disgrace_ns
{

TransportBar::TransportBar(int X, int Y, int W, int H, Engine& engine)
: Fl_Group(X, Y, W, H), m_engine(engine)
{
    int x = X + 10;

    m_record = new Fl_Light_Button(x, Y+5, 80, 25, "Record");
    m_record->callback(cb_record, this);
    x += 90;

    m_play = new Fl_Button(x, Y+5, 60, 25, "Play");
    m_play->callback(cb_play, this);
    x += 70;

    m_stop = new Fl_Button(x, Y+5, 60, 25, "Stop");
    m_stop->callback(cb_stop, this);
    x += 70;

    Fl_Check_Button* loop_btn = new Fl_Check_Button(x, Y+5, 70, 25, "Loop");
    loop_btn->value(1);
    loop_btn->callback([](Fl_Widget* w, void* d){ 
        ((Engine*)d)->set_loop(((Fl_Check_Button*)w)->value()); 
    }, &m_engine);
    x += 80;

    m_metronome = new Fl_Light_Button(x, Y+5, 100, 25, "Metronome");
    m_metronome->value(1);
    m_metronome->callback(cb_metronome, this);
    x += 110;

    m_tempo = new Fl_Value_Input(x, Y+5, 60, 25, "BPM");
    m_tempo->range(30, 300);
    m_tempo->step(1);
    m_tempo->value(m_engine.tempo());
    m_tempo->callback(cb_tempo, this);
    x += 100;

    Fl_Value_Input* octave_in = new Fl_Value_Input(x, Y+5, 40, 25, "Octave");
    octave_in->range(0, 9);
    octave_in->step(1);
    octave_in->value(m_engine.base_octave());
    octave_in->callback([](Fl_Widget* w, void* d){
        ((Engine*)d)->set_base_octave((int)((Fl_Value_Input*)w)->value());
    }, &m_engine);
    x += 80;

    m_status = new Fl_Box(x, Y+5, 100, 25, "Stopped");
    x += 110;
    m_meter_l = new VUMeter(x, Y+5, 100, 10, nullptr, true);
    m_meter_r = new VUMeter(x, Y+18, 100, 10, nullptr, true);

    end();
}

void TransportBar::cb_tempo(Fl_Widget* w, void* data)
{
    auto* self = static_cast<disgrace_ns::TransportBar*>(data);
    self->m_engine.set_tempo(static_cast<Fl_Value_Input*>(w)->value());
}

void TransportBar::cb_play(Fl_Widget*, void* data)
{
    auto* self = static_cast<disgrace_ns::TransportBar*>(data);
    self->m_engine.play();
}

void TransportBar::cb_stop(Fl_Widget*, void* data)
{
    auto* self = static_cast<disgrace_ns::TransportBar*>(data);
    self->m_engine.stop();
}

void TransportBar::cb_record(Fl_Widget*, void* data)
{
    auto* self = static_cast<disgrace_ns::TransportBar*>(data);
    bool rec = self->m_record->value();
    self->m_engine.enable_record(rec);
}

void TransportBar::cb_metronome(Fl_Widget*, void* data)
{
    auto* self = static_cast<disgrace_ns::TransportBar*>(data);
    self->m_engine.set_metronome_enabled(self->m_metronome->value());
}

void TransportBar::update()
{
    double engine_bpm = m_engine.tempo();
    if (m_tempo->value() != engine_bpm) m_tempo->value(engine_bpm);

    if (m_record->value() != (int)m_engine.m_record_enabled) 
        m_record->value(m_engine.m_record_enabled);

    auto state = m_engine.transport_state();
    switch (state)
    {
        case disgrace_ns::TransportState::Stopped: m_status->label("Stopped"); break;
        case disgrace_ns::TransportState::Playing: m_status->label("Playing"); break;
    }
    if (m_meter_l) m_meter_l->level(m_engine.master_meter_l());
    if (m_meter_r) m_meter_r->level(m_engine.master_meter_r());
    redraw();
}

} // namespace disgrace_ns
