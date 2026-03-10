#pragma once
#include <FL/Fl_Group.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Light_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Value_Input.H>
#include "vu_meter.h"
#include "../core/transport.h" // Add this for TransportState

namespace disgrace_ns
{

// Forward declare Engine within its namespace
class Engine;

class TransportBar : public Fl_Group
{
public:
    TransportBar(int X, int Y, int W, int H, Engine& engine); // Use unqualified Engine now that we are in the namespace

    void update();

private:
    static void cb_play(Fl_Widget*, void*);
    static void cb_stop(Fl_Widget*, void*);
    static void cb_record(Fl_Widget*, void*);
    static void cb_metronome(Fl_Widget*, void*);
    static void cb_tempo(Fl_Widget*, void*);
    static void cb_lpb(Fl_Widget*, void*);
    static void cb_step(Fl_Widget*, void*);

    Engine& m_engine; // Use unqualified Engine

    Fl_Button* m_play;
    Fl_Button* m_stop;
    Fl_Light_Button* m_record;
    Fl_Light_Button* m_metronome;
    Fl_Value_Input* m_tempo;
    Fl_Value_Input* m_lpb;
    Fl_Value_Input* m_step;
    Fl_Box* m_status;
    VUMeter* m_meter_l;
    VUMeter* m_meter_r;
};

} // namespace disgrace_ns
