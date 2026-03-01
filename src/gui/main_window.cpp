#include "main_window.h"
#include "transport_panel.h"
#include "tracker_panel.h"
#include "mixer_panel.h"
#include "instrument_panel.h"
#include "project_panel.h"
#include "settings_panel.h"
#include "../core/engine.h"

#include <FL/fl_draw.H>
#include <FL/Fl_Native_File_Chooser.H> // ADD THIS LINE for fl_file_chooser

namespace disgrace_ns
{

  disgrace_ns::MainWindow::MainWindow(int w, int h,
                         const char* title,
                         disgrace_ns::Engine& engine)
  : Fl_Double_Window(w, h, title),
  m_engine(engine),
  m_transport(nullptr),
  m_status(nullptr),
  m_tabs(nullptr),
  m_project_tab(nullptr),
  m_tracker_tab(nullptr),
  m_mixer_tab(nullptr),
  m_instrument_tab(nullptr),
  m_settings_tab(nullptr),
  m_loop_btn(nullptr),
  m_tracker_panel(nullptr),
  m_instrument_panel(nullptr),
  m_mixer_panel(nullptr),
  m_settings_panel(nullptr),
  m_project_panel(nullptr)
  {
    begin();

    m_transport = new disgrace_ns::TransportPanel(0, 0, w, 40, m_engine);
    m_loop_btn = new Fl_Check_Button(w - 70, 10, 60, 25, "Loop");
    m_loop_btn->value(1);
    m_loop_btn->callback(
      [](Fl_Widget* w, void* data)
      {
        auto* engine =
        static_cast<disgrace_ns::Engine*>(data);

        engine->set_loop(
          static_cast<Fl_Check_Button*>(w)
          ->value());
      },
      &m_engine);

    m_tabs = new Fl_Tabs(0, 40, w, h - 75);
    
    init_project_tab(w, h);
    init_tracker_tab(w, h);
    init_instrument_tab(w, h);
    init_mixer_tab(w, h);
    init_settings_tab(w, h);

    m_tabs->end();

    resizable(m_tabs);
    end();
  }

  disgrace_ns::MainWindow::~MainWindow() {}

  disgrace_ns::Track& disgrace_ns::MainWindow::track(size_t index)
  {
      return m_engine.track(index);
  }

void disgrace_ns::MainWindow::update_all_uis() {
    if (m_mixer_panel) m_mixer_panel->update_mixer_ui();
    if (m_tracker_panel) m_tracker_panel->update_pattern_list_browser();
    if (m_project_panel) m_project_panel->update_track_list();
    if (m_instrument_panel) m_instrument_panel->update_instrument_list();
}

void disgrace_ns::MainWindow::timer_cb(void* data)
  {
    auto* self = static_cast<disgrace_ns::MainWindow*>(data);

    // This check is no longer valid as m_tracker is now in TrackerPanel
    // if (self->m_tracker == nullptr) {
    //     fprintf(stderr, "DEBUG: m_tracker is nullptr in timer_cb! Skipping set_current_row.\n");
    //     Fl::repeat_timeout(0.03, timer_cb, data);
    //     return;
    // }
    
    Fl::repeat_timeout(0.03, timer_cb, data);
  }

  int disgrace_ns::MainWindow::handle(int event){
    if (event == FL_KEYDOWN)
    {
      Action action = m_engine.m_key_bindings.get_action(Fl::event_key(), Fl::event_state() & (FL_CTRL | FL_SHIFT | FL_ALT | FL_META));
      
      switch (action)
      {
        case Action::Play:
          m_engine.play();
          return 1;

        case Action::Record:
          m_engine.record();
          return 1;

        case Action::ToggleMetronome:
          m_engine.toggle_metronome();
          return 1;
        
        case Action::Cut:
          if (m_tabs->value() == m_instrument_tab && m_instrument_panel) {
              m_instrument_panel->cb_cut(nullptr, m_instrument_panel);
              return 1;
          }
          break;

        case Action::Copy:
          if (m_tabs->value() == m_instrument_tab && m_instrument_panel) {
              m_instrument_panel->cb_copy(nullptr, m_instrument_panel);
              return 1;
          }
          break;

        case Action::Paste:
          if (m_tabs->value() == m_instrument_tab && m_instrument_panel) {
              m_instrument_panel->cb_paste(nullptr, m_instrument_panel);
              return 1;
          }
          break;

        default:
          break;
      }
    }
    return Fl_Double_Window::handle(event); // Call base class handle for unhandled events
  }

} // namespace disgrace_ns
