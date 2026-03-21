#include <wx/wxprec.h>
#include <wx/app.h>
#include <wx/msgdlg.h>

#include "gui/wx_main_window.h"
#include "core/engine.h"

namespace disgrace_ns {
class DisgraceApp : public wxApp {
public:
    virtual bool OnInit() override;

private:
    Engine* m_engine = nullptr;
    WxMainWindow* m_window = nullptr;
};

wxIMPLEMENT_APP_CONSOLE(DisgraceApp);

bool DisgraceApp::OnInit() {
    if (!wxApp::OnInit())
        return false;

    try {
        m_engine = new Engine();

        if (!m_engine->initialize()) {
            delete m_engine;
            return false;
        }

        m_window = new WxMainWindow(1024, 768, "Disgrace", *m_engine);
        m_window->Show(true);

        return true;
    }
    catch (const std::exception& e) {
        wxMessageBox(wxString::Format("Fatal error: %s", e.what()), "Error", wxOK | wxICON_ERROR);
        return false;
    }
}

} // namespace disgrace_ns

int main(int argc, char** argv) {
    return wxEntry(argc, argv);
}
