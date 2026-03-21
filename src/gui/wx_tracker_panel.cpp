#include "wx_tracker_panel.h"
#include "wx_tracker_view.h"
#include "../core/engine.h"

#include <wx/sizer.h>
#include <wx/textctrl.h>
#include <wx/dialog.h>

namespace disgrace_ns {

wxBEGIN_EVENT_TABLE(TrackerPanel, wxPanel)
    EVT_BUTTON(wxID_ANY, TrackerPanel::on_add_pattern)
    EVT_BUTTON(wxID_ANY, TrackerPanel::on_remove_pattern)
    EVT_BUTTON(wxID_ANY, TrackerPanel::on_copy_pattern)
    EVT_BUTTON(wxID_ANY, TrackerPanel::on_inc_pattern)
    EVT_BUTTON(wxID_ANY, TrackerPanel::on_dec_pattern)
    EVT_TOGGLEBUTTON(wxID_ANY, TrackerPanel::on_follow_playback)
    EVT_BUTTON(wxID_ANY, TrackerPanel::on_detach)
wxEND_EVENT_TABLE()

TrackerPanel::TrackerPanel(wxWindow* parent, Engine& engine)
    : wxPanel(parent, wxID_ANY), m_engine(engine)
{
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    int btn_h = 25;

    wxBoxSizer* btn_sizer = new wxBoxSizer(wxHORIZONTAL);

    m_detach_btn = new wxButton(this, wxID_ANY, "[]", wxDefaultPosition, wxSize(30, 20));
    btn_sizer->Add(m_detach_btn, 0, wxALL, 2);

    m_add_pattern_btn = new wxButton(this, wxID_ANY, "Add", wxDefaultPosition, wxSize(40, btn_h));
    m_add_pattern_btn->SetWindowStyleFlag(wxBU_EXACTFIT);
    btn_sizer->Add(m_add_pattern_btn, 0, wxALL, 2);

    m_remove_pattern_btn = new wxButton(this, wxID_ANY, "Rem", wxDefaultPosition, wxSize(40, btn_h));
    m_remove_pattern_btn->SetWindowStyleFlag(wxBU_EXACTFIT);
    btn_sizer->Add(m_remove_pattern_btn, 0, wxALL, 2);

    m_copy_pattern_btn = new wxButton(this, wxID_ANY, "Copy", wxDefaultPosition, wxSize(40, btn_h));
    m_copy_pattern_btn->SetWindowStyleFlag(wxBU_EXACTFIT);
    btn_sizer->Add(m_copy_pattern_btn, 0, wxALL, 2);

    m_dec_pattern_btn = new wxButton(this, wxID_ANY, "-", wxDefaultPosition, wxSize(60, btn_h));
    btn_sizer->Add(m_dec_pattern_btn, 0, wxALL, 2);

    m_inc_pattern_btn = new wxButton(this, wxID_ANY, "+", wxDefaultPosition, wxSize(60, btn_h));
    btn_sizer->Add(m_inc_pattern_btn, 0, wxALL, 2);

    m_follow_btn = new wxToggleButton(this, wxID_ANY, "Follow", wxDefaultPosition, wxSize(120, btn_h));
    m_follow_btn->SetValue(true);
    m_follow_playback = true;
    btn_sizer->Add(m_follow_btn, 0, wxALL, 2);

    main_sizer->Add(btn_sizer, 0, wxEXPAND | wxALL, 2);

    wxBoxSizer* content_sizer = new wxBoxSizer(wxHORIZONTAL);

    int pattern_list_width = 120;

    m_pattern_scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxSize(pattern_list_width, -1), wxVSCROLL);
    m_pattern_scroll->SetScrollRate(0, 25);
    m_pattern_list_container = new wxPanel(m_pattern_scroll, wxID_ANY);
    content_sizer->Add(m_pattern_scroll, 0, wxEXPAND | wxALL, 2);

    m_tracker = new TrackerView(this, wxID_ANY, m_engine.pattern(), m_engine);
    content_sizer->Add(m_tracker, 1, wxEXPAND | wxALL, 2);

    main_sizer->Add(content_sizer, 1, wxEXPAND | wxALL, 0);

    SetSizer(main_sizer);

    update_pattern_list();
}

void TrackerPanel::on_add_pattern(wxCommandEvent& event) {
    m_engine.add_pattern_to_order();
    update_pattern_list();
}

void TrackerPanel::on_remove_pattern(wxCommandEvent& event) {
    const auto& order = m_engine.order_list();
    if (!order.empty()) {
        m_engine.remove_pattern_from_order(order.size() - 1);
        update_pattern_list();
    }
}

void TrackerPanel::on_copy_pattern(wxCommandEvent& event) {
    const auto& order = m_engine.order_list();
    if (!order.empty()) {
        m_engine.copy_pattern_in_order(order.size() - 1);
        update_pattern_list();
    }
}

void TrackerPanel::on_inc_pattern(wxCommandEvent& event) {
    size_t pos = m_engine.m_edit_order_pos.load();
    auto order = m_engine.order_list();
    if (pos < order.size()) {
        if (order[pos] < m_engine.pattern_count() - 1) {
            order[pos]++;
            m_engine.set_order(order);
            m_engine.set_active_pattern(order[pos]);
            m_tracker->set_pattern(m_engine.pattern());
            update_pattern_list();
        }
    }
}

void TrackerPanel::on_dec_pattern(wxCommandEvent& event) {
    size_t pos = m_engine.m_edit_order_pos.load();
    auto order = m_engine.order_list();
    if (pos < order.size()) {
        if (order[pos] > 0) {
            order[pos]--;
            m_engine.set_order(order);
            m_engine.set_active_pattern(order[pos]);
            m_tracker->set_pattern(m_engine.pattern());
            update_pattern_list();
        }
    }
}

void TrackerPanel::on_follow_playback(wxCommandEvent& event) {
    m_follow_playback = m_follow_btn->GetValue();
}

void TrackerPanel::on_detach(wxCommandEvent& event) {
    Hide();
}

void TrackerPanel::update_pattern_list() {
    m_tracker->set_pattern(m_engine.pattern());
    const auto& order = m_engine.order_list();
    m_last_order_size = order.size();
    m_last_pattern_count = m_engine.pattern_count();

    m_pattern_list_container->DestroyChildren();
    m_order_buttons.clear();
    m_pattern_length_inputs.clear();

    int row_h = 25;
    int start_x = 0;
    int start_y = 0;

    for (size_t i = 0; i < order.size(); ++i) {
        int cur_y = start_y + (int)(i * row_h);

        wxString pos_str;
        pos_str.Printf("%02zu:", i);
        wxButton* b = new wxButton(m_pattern_list_container, wxID_ANY, pos_str, wxPoint(start_x, cur_y), wxSize(30, row_h));
        if (m_selected_order_idx == (int)i) {
            b->SetBackgroundColour(wxColour(0, 120, 215));
            b->SetForegroundColour(*wxWHITE);
        }
        b->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent& ev) {
            auto& eng = this->m_engine;
            auto ord = eng.order_list();
            if (i < ord.size()) {
                eng.m_edit_order_pos.store(i);
                eng.set_active_pattern(ord[i]);
                this->m_selected_order_idx = (int)i;
                this->m_tracker->set_pattern(eng.pattern());
                this->update_pattern_list();
            }
        });
        m_order_buttons[i] = b;

        wxString pat_str;
        pat_str.Printf("%02u", order[i]);
        wxStaticText* p = new wxStaticText(m_pattern_list_container, wxID_ANY, pat_str, wxPoint(start_x + 30, cur_y), wxSize(30, row_h));
        p->SetForegroundColour(wxColour(255, 255, 0));

        int pat_idx = order[i];
        size_t len = m_engine.pattern(pat_idx).row_count();
        wxString len_str;
        len_str.Printf("%zu", len);
        wxTextCtrl* len_inp = new wxTextCtrl(m_pattern_list_container, wxID_ANY, len_str, wxPoint(start_x + 65, cur_y + 2), wxSize(40, 20), wxTE_PROCESS_ENTER);
        len_inp->Bind(wxEVT_TEXT_ENTER, [this, pat_idx](wxCommandEvent& ev) {
            wxTextCtrl* inp = dynamic_cast<wxTextCtrl*>(ev.GetEventObject());
            if (inp) {
                long new_len = 0;
                if (inp->GetValue().ToLong(&new_len) && new_len > 0 && new_len <= 512) {
                    this->m_engine.resize_pattern(pat_idx, (size_t)new_len);
                    inp->SetValue(wxString::Format("%zu", this->m_engine.pattern(pat_idx).row_count()));
                    this->m_tracker->recalculate_size();
                    this->m_tracker->Refresh();
                }
            }
        });
        m_pattern_length_inputs[pat_idx] = len_inp;
    }

    m_pattern_list_container->SetSize(wxSize(120, (int)(order.size() * row_h)));
    m_pattern_scroll->SetVirtualSize(120, (int)(order.size() * row_h));

    m_pattern_scroll->Refresh();
}

void TrackerPanel::update() {
    if (m_tracker) {
        if (m_follow_playback) {
            size_t engine_order_pos = m_engine.m_order_pos.load();
            if (m_engine.m_edit_order_pos.load() != engine_order_pos) {
                m_engine.m_edit_order_pos.store(engine_order_pos);
                m_engine.set_active_pattern(m_engine.m_order[engine_order_pos]);
                m_tracker->set_pattern(m_engine.pattern());
            }
        }

        size_t current_edit_pos = m_engine.m_edit_order_pos.load();
        size_t current_order_size = m_engine.order_list().size();
        size_t current_pattern_count = m_engine.pattern_count();

        if ((int)current_edit_pos != m_selected_order_idx ||
            current_order_size != m_last_order_size ||
            current_pattern_count != m_last_pattern_count) {

            m_selected_order_idx = (int)current_edit_pos;
            update_pattern_list();
        }

        if (m_engine.transport_state() != TransportState::Stopped) {
            m_tracker->ensure_cursor_visible();
        }
        m_tracker->Refresh();
    }
}

void TrackerPanel::grab_focus() {
    if (m_tracker) {
        m_tracker->SetFocus();
    }
}

int TrackerPanel::get_cursor_row() const {
    if (m_tracker) return m_tracker->get_cursor_row();
    return 0;
}

void TrackerPanel::handle_note_action(Action action) {
    if (m_tracker) {
        m_tracker->handle_action(action);
    }
}

} // namespace disgrace_ns
