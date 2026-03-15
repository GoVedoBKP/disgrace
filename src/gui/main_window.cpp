#include "main_window.h"
#include "theme.h"
#include "transportbar.h"
#include "tracker_panel.h"
#include "tracks_panel.h"
#include "notation_panel.h"
#include "mixer_panel.h"
#include "instrument_panel.h"
#include "project_panel.h"
#include "settings_panel.h"
#include "../core/engine.h"
#include "tracker_view.h"

#include <FL/fl_draw.H>
#include <FL/Fl_Native_File_Chooser.H> 

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
  m_tracks_tab(nullptr),
  m_notation_tab(nullptr),
  m_mixer_tab(nullptr),
  m_instrument_tab(nullptr),
  m_settings_tab(nullptr),
  m_loop_btn(nullptr),
  m_tracker_panel(nullptr),
  m_tracks_panel(nullptr),
  m_notation_panel(nullptr),
  m_instrument_panel(nullptr),
  m_mixer_panel(nullptr),
  m_settings_panel(nullptr),
  m_project_panel(nullptr)
  {
    ThemeManager::apply_theme_and_settings(m_engine);
    begin();

    m_transport = new disgrace_ns::TransportBar(0, 0, w, 40, m_engine);

    m_tabs = new Fl_Tabs(0, 40, w, h - 75);
    
    init_project_tab(w, h);
    if (m_project_tab) m_tabs->add(m_project_tab);
    
    init_tracker_tab(w, h);
    if (m_tracker_tab) m_tabs->add(m_tracker_tab);
    
    init_tracks_tab(w, h);
    if (m_tracks_tab) m_tabs->add(m_tracks_tab);
    
    init_notation_tab(w, h);
    if (m_notation_tab) m_tabs->add(m_notation_tab);
    
    init_instrument_tab(w, h);
    if (m_instrument_tab) m_tabs->add(m_instrument_tab);
    
    init_mixer_tab(w, h);
    if (m_mixer_tab) m_tabs->add(m_mixer_tab);
    
    init_settings_tab(w, h);
    if (m_settings_tab) m_tabs->add(m_settings_tab);
    
    init_help_tab(w, h);
    if (m_help_tab) m_tabs->add(m_help_tab);

    m_tabs->end();

    m_tabs->callback([](Fl_Widget* w, void* d) {
        MainWindow* self = (MainWindow*)d;
        Fl_Tabs* tabs = (Fl_Tabs*)w;
        if (tabs->value() == self->m_tracker_tab && self->m_tracker_panel) {
            self->m_tracker_panel->grab_focus();
        }
        self->update_all_uis();
    }, this);

    Fl::add_timeout(0.03, timer_cb, this);

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
    if (m_instrument_panel) {
        m_instrument_panel->update_instrument_list();
        m_instrument_panel->update_editor();
    }
}

void disgrace_ns::MainWindow::request_update() {
    Fl::remove_timeout(request_update_cb, this);
    Fl::add_timeout(0.05, request_update_cb, this);
}

void disgrace_ns::MainWindow::request_update_cb(void* data) {
    auto* self = static_cast<disgrace_ns::MainWindow*>(data);
    self->update_all_uis();
}

void disgrace_ns::MainWindow::timer_cb(void* data)
  {
    auto* self = static_cast<disgrace_ns::MainWindow*>(data);

    if (self->m_transport) self->m_transport->update();
    if (self->m_mixer_panel) self->m_mixer_panel->update_meters();
    if (self->m_tracker_panel && self->m_engine.transport_state() != TransportState::Stopped) 
        self->m_tracker_panel->update();
    
    if (self->m_tracks_panel && (self->m_engine.transport_state() != TransportState::Stopped || self->m_tabs->value() == self->m_tracks_tab))
        self->m_tracks_panel->update();
    
    if (self->m_notation_panel && (self->m_engine.transport_state() != TransportState::Stopped || self->m_tabs->value() == self->m_notation_tab))
        self->m_notation_panel->update();
    
    Fl::repeat_timeout(0.03, timer_cb, data);
  }

  int disgrace_ns::MainWindow::handle(int event){
    if (event == FL_KEYDOWN || event == FL_KEYUP)
    {
      Action action = m_engine.m_key_bindings.get_action(Fl::event_key(), Fl::event_state() & (FL_CTRL | FL_SHIFT | FL_ALT | FL_META));
      
      auto is_note_action = [](Action a) -> bool {
          return ((int)a >= (int)Action::NoteC && (int)a <= (int)Action::NoteB) ||
                 ((int)a >= (int)Action::NoteC2 && (int)a <= (int)Action::NoteB2) ||
                 (a == Action::NoteC3) || (a == Action::NoteOff);
      };

      if (is_note_action(action) && m_tabs->value() == m_tracker_tab && m_tracker_panel) {
          if (Fl::focus() == m_tracker_panel->tracker_view()) return m_tracker_panel->tracker_view()->handle(event);
      }

      if (event == FL_KEYDOWN) {
          switch (action)
          {
            case Action::Play:
              m_engine.play();
              return 1;

            case Action::PlaySong:
              m_engine.play_song();
              return 1;

            case Action::PlayPattern:
              m_engine.play_pattern();
              return 1;

            case Action::PlayFromPosition:
              m_engine.play_from_position(m_engine.current_row());
              return 1;

            case Action::Stop:
              m_engine.panic();
              return 1;

            case Action::Record:
              m_engine.enable_record(!m_engine.m_record_enabled);
              if (m_transport) m_transport->update();
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
    }
    return Fl_Double_Window::handle(event); 
  }

} // namespace disgrace_ns
