#pragma once

#include <wx/wxprec.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#include <wx/timer.h>
#include <wx/menu.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/panel.h>
#include <wx/statusbr.h>

namespace disgrace_ns {

class Engine;
class TransportBar;
class TrackerPanel;
class TracksPanel;
class NotationPanel;
class InstrumentPanel;
class MixerPanel;
class SettingsPanel;
class ProjectPanel;
class Track;

class WxMainWindow : public wxFrame {
public:
    WxMainWindow(int w, int h, const wxString& title, Engine& engine);
    ~WxMainWindow();

    void update_all_uis();
    void request_update();

    Track& track(size_t index);
    int get_cursor_row() const;

    void OnTimer(wxTimerEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnCharHook(wxKeyEvent& event);

private:
    void init_menu();
    void init_panels();

    Engine& m_engine;

    wxTimer* m_timer;
    wxNotebook* m_tabs;
    wxStatusBar* m_status;

    TransportBar* m_transport;
    ProjectPanel* m_project_panel;
    TrackerPanel* m_tracker_panel;
    TracksPanel* m_tracks_panel;
    NotationPanel* m_notation_panel;
    InstrumentPanel* m_instrument_panel;
    MixerPanel* m_mixer_panel;
    SettingsPanel* m_settings_panel;

    int m_selected_tab;

    DECLARE_EVENT_TABLE()
};

} // namespace disgrace_ns
