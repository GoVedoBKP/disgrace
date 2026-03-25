#include "wx_analog_vu_meter.h"
#include "theme.h"
#include "../core/engine.h"

#include <wx/dcclient.h>
#include <cmath>
#include <algorithm>

namespace disgrace_ns {

wxBEGIN_EVENT_TABLE(AnalogVUMeter, wxPanel)
    EVT_PAINT(AnalogVUMeter::OnPaint)
wxEND_EVENT_TABLE()

AnalogVUMeter::AnalogVUMeter(wxWindow* parent, wxWindowID id, Engine& engine)
    : wxPanel(parent, id), m_engine(engine)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

void AnalogVUMeter::level(float l) {
    m_level = l;
    
    // VU Meter ballistics: Rise is usually faster than fall
    // Standard is roughly 300ms to reach 99%
    if (m_level > m_smooth_level) {
        m_smooth_level = 0.7f * m_smooth_level + 0.3f * m_level; // Faster rise
    } else {
        m_smooth_level = 0.92f * m_smooth_level + 0.08f * m_level; // Slower fall
    }
    
    Refresh(false);
}

void AnalogVUMeter::OnPaint(wxPaintEvent& event) {
    wxPaintDC dc(this);
    wxSize size = GetClientSize();
    int w = size.GetWidth();
    int h = size.GetHeight();

    // Background: White
    dc.SetBrush(*wxWHITE_BRUSH);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(0, 0, w, h);

    // Scaling: Increased sensitivity (-40 to +3)
    float db = 20.0f * log10f(m_smooth_level + 0.00001f);
    float db_max = 3.0f;
    float db_min = -40.0f; 
    float norm = (db - db_min) / (db_max - db_min);
    norm = std::max(0.0f, std::min(1.0f, norm));

    // Meter arc and scale - optimized for 180x130 or similar
    int centerX = w / 2;
    int centerY = h - 15;
    int radius = std::min(w / 2 - 10, h - 45);

    dc.SetPen(wxPen(*wxBLACK, 2));
    dc.DrawEllipticArc(centerX - radius, centerY - radius, radius * 2, radius * 2, 45, 135);

    const float pi = 3.1415926535f;

    // Tick marks and Labels
    float mark_dbs[] = { -40, -30, -20, -15, -10, -7, -5, -3, -1, 0, 1, 2, 3 };
    for (float mdb : mark_dbs) {
        float m_norm = (mdb - db_min) / (db_max - db_min);
        float angle = 135.0f - (m_norm * 90.0f);
        float rad = angle * pi / 180.0f;
        
        int x1 = centerX + (int)(radius * cos(rad));
        int y1 = centerY - (int)(radius * sin(rad));
        int x2 = centerX + (int)((radius - 12) * cos(rad));
        int y2 = centerY - (int)((radius - 12) * sin(rad));
        
        if (mdb >= 0) dc.SetPen(wxPen(*wxRED, 2)); 
        else dc.SetPen(wxPen(*wxBLACK, 1));
        
        dc.DrawLine(x1, y1, x2, y2);
        
        // Labels for major marks (avoiding too much overlap)
        if (mdb == -40 || mdb == -20 || mdb == -10 || mdb == 0 || mdb == 3) {
            wxString lbl;
            if (mdb > 0) lbl.Printf("+%d", (int)mdb);
            else lbl.Printf("%d", (int)mdb);
            
            // Move labels further inside the arc
            int tx = centerX + (int)((radius - 28) * cos(rad));
            int ty = centerY - (int)((radius - 28) * sin(rad));
            
            dc.SetFont(wxFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
            wxSize tsize = dc.GetTextExtent(lbl);
            dc.DrawText(lbl, tx - tsize.x/2, ty - tsize.y/2);
        }
    }

    // Minor ticks between major ones for more "analog" feel
    dc.SetPen(wxPen(*wxBLACK, 1));
    for (float db_tick = -40; db_tick <= 3; db_tick += 1.0f) {
        bool is_major = false;
        for (float mdb : mark_dbs) if (mdb == db_tick) is_major = true;
        if (is_major) continue;

        float t_norm = (db_tick - db_min) / (db_max - db_min);
        float angle = 135.0f - (t_norm * 90.0f);
        float rad = angle * pi / 180.0f;
        int x1 = centerX + (int)(radius * cos(rad));
        int y1 = centerY - (int)(radius * sin(rad));
        int x2 = centerX + (int)((radius - 6) * cos(rad));
        int y2 = centerY - (int)((radius - 6) * sin(rad));
        if (db_tick >= 0) dc.SetPen(wxPen(*wxRED, 1));
        else dc.SetPen(wxPen(*wxBLACK, 1));
        dc.DrawLine(x1, y1, x2, y2);
    }

    // Needle
    float angle = 135.0f - (norm * 90.0f);
    float rad = angle * pi / 180.0f;
    int needleX = centerX + (int)((radius - 2) * cos(rad));
    int needleY = centerY - (int)((radius - 2) * sin(rad));

    dc.SetPen(wxPen(wxColour(230, 40, 0), 3)); // Slightly thicker needle
    dc.DrawLine(centerX, centerY, needleX, needleY);

    // Center pivot
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.SetPen(*wxBLACK_PEN);
    dc.DrawCircle(centerX, centerY, 8);
    
    // "VU" text - repositioned
    dc.SetFont(wxFont(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    wxSize vu_size = dc.GetTextExtent("VU");
    dc.DrawText("VU", centerX - vu_size.x/2, centerY - (radius/2));
}

} // namespace disgrace_ns
