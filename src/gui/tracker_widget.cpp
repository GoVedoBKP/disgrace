#include "tracker_widget.h"
#include "../core/engine.h"
#include <imgui.h>
#include <SDL2/SDL_keycode.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace disgrace_ns {

static void note_to_name_buf(uint8_t note, char* buf) {
    static const char* names[] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
    if (note == 255) { strcpy(buf, "---"); return; }
    if (note == 254) { strcpy(buf, "==="); return; }
    if (note > 119) { strcpy(buf, "---"); return; }
    int octave = note / 12;
    int n = note % 12;
    snprintf(buf, 8, "%s%d", names[n], octave);
}

TrackerWidget::TrackerWidget(Engine& engine)
    : m_engine(engine)
{
    m_rowHeight = 18.0f;
    m_charWidth = 8.0f;
    m_headerHeight = 22.0f;
}

void TrackerWidget::render() {
    auto& pat = m_engine.pattern();
    clampCursor();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 viewSize = ImGui::GetContentRegionAvail();
    
    // Support horizontal scrolling
    float scrollX = ImGui::GetScrollX();
    
    size_t numTracks = m_engine.track_count();
    size_t numRows = pat.row_count();

    if (numTracks == 0) {
        ImGui::Text("No tracks. Click '+ Track' in the transport bar to add one.");
        return;
    }

    int playingRow = (int)m_engine.current_row();
    bool isPlaying = m_engine.transport_state() != TransportState::Stopped;
    int centerRow = isPlaying ? playingRow : m_cursorRow;

    // Calculate total width and track offsets
    float rowNumWidth = 40.0f;
    float totalWidth = rowNumWidth;
    m_trackUI.clear();
    for (size_t t = 0; t < numTracks; ++t) {
        size_t numCols = pat.column_count(t);
        // note(4) + sample(3) + vol(3) per col + 4 effects(3 each) + padding
        float trackW = (float)(numCols * 10 * m_charWidth + 4 * 3 * m_charWidth + 20.0f);
        m_trackUI.push_back({totalWidth, trackW});
        totalWidth += trackW;
    }

    // Tell ImGui about the content size for scrollbars
    ImGui::Dummy(ImVec2(totalWidth, viewSize.y));
    ImGui::SetCursorScreenPos(canvasPos);

    float midY = (viewSize.y - m_headerHeight) / 2.0f;
    
    auto get_ry = [&](int r) {
        return m_headerHeight + midY + (r - centerRow) * m_rowHeight;
    };

    // 1. Draw Background and Row Highlights
    int firstVisRow = centerRow - (int)(midY / m_rowHeight) - 1;
    int lastVisRow = centerRow + (int)(midY / m_rowHeight) + 1;
    firstVisRow = std::max(0, firstVisRow);
    lastVisRow = std::min((int)numRows - 1, lastVisRow);

    for (int r = firstVisRow; r <= lastVisRow; ++r) {
        float ry = get_ry(r);
        if (ry < m_headerHeight || ry > viewSize.y) continue;

        ImU32 rowCol = IM_COL32(35, 35, 40, 255);
        uint32_t lpb = m_engine.lpb();
        if (lpb > 0 && r % lpb == 0) rowCol = IM_COL32(45, 45, 55, 255);
        if (isPlaying && r == playingRow) rowCol = IM_COL32(60, 60, 80, 255);
        
        drawList->AddRectFilled(
            ImVec2(canvasPos.x, canvasPos.y + ry),
            ImVec2(canvasPos.x + totalWidth - scrollX, canvasPos.y + ry + m_rowHeight),
            rowCol);

        if (r == m_cursorRow) {
            drawList->AddRectFilled(
                ImVec2(canvasPos.x + rowNumWidth - scrollX, canvasPos.y + ry),
                ImVec2(canvasPos.x + totalWidth - scrollX, canvasPos.y + ry + m_rowHeight),
                IM_COL32(50, 50, 70, 255));
        }

        char rBuf[8];
        snprintf(rBuf, 8, "%03d", r);
        drawList->AddText(ImVec2(canvasPos.x + 5, canvasPos.y + ry + 2), IM_COL32(180, 180, 180, 255), rBuf);
    }

    // 2. Draw Tracks
    for (size_t t = 0; t < numTracks; ++t) {
        float tx = canvasPos.x + m_trackUI[t].x - scrollX;
        float tw = m_trackUI[t].w;
        
        if (tx + tw < canvasPos.x || tx > canvasPos.x + viewSize.x) continue;

        // Header
        drawList->AddRectFilled(ImVec2(tx, canvasPos.y), ImVec2(tx + tw - 2, canvasPos.y + m_headerHeight), IM_COL32(60, 60, 70, 255));
        std::string name = m_engine.track(t).name();
        drawList->AddText(ImVec2(tx + 4, canvasPos.y + 2), IM_COL32(220, 220, 220, 255), name.c_str());

        size_t numCols = pat.column_count(t);
        for (int r = firstVisRow; r <= lastVisRow; ++r) {
            float ry = get_ry(r);
            if (ry < m_headerHeight || ry > viewSize.y) continue;

            float cx = tx + 4;
            for (size_t c = 0; c < numCols; ++c) {
                const auto& ev = pat.event(t, r, c);
                
                // Note
                if ((int)t == m_cursorTrack && r == m_cursorRow && (int)c == m_cursorCol && m_cursorField == 0)
                    drawList->AddRectFilled(ImVec2(cx - 1, canvasPos.y + ry), ImVec2(cx + 3 * m_charWidth + 1, canvasPos.y + ry + m_rowHeight), IM_COL32(100, 100, 150, 255));
                
                char nBuf[8];
                note_to_name_buf(ev.note, nBuf);
                drawList->AddText(ImVec2(cx, canvasPos.y + ry + 2), ev.note == 255 ? IM_COL32(100, 100, 100, 255) : IM_COL32(180, 180, 255, 255), nBuf);
                cx += 4 * m_charWidth;

                // Sample
                if ((int)t == m_cursorTrack && r == m_cursorRow && (int)c == m_cursorCol && m_cursorField == 1)
                    drawList->AddRectFilled(ImVec2(cx - 1, canvasPos.y + ry), ImVec2(cx + 2 * m_charWidth + 1, canvasPos.y + ry + m_rowHeight), IM_COL32(100, 100, 150, 255));
                
                char sBuf[4];
                if (ev.sample_idx == 0) strcpy(sBuf, ".."); else snprintf(sBuf, 4, "%02X", ev.sample_idx);
                drawList->AddText(ImVec2(cx, canvasPos.y + ry + 2), IM_COL32(160, 200, 160, 255), sBuf);
                cx += 3 * m_charWidth;

                // Volume
                if ((int)t == m_cursorTrack && r == m_cursorRow && (int)c == m_cursorCol && m_cursorField == 2)
                    drawList->AddRectFilled(ImVec2(cx - 1, canvasPos.y + ry), ImVec2(cx + 2 * m_charWidth + 1, canvasPos.y + ry + m_rowHeight), IM_COL32(100, 100, 150, 255));
                
                if (ev.volume == 255) strcpy(sBuf, ".."); else snprintf(sBuf, 4, "%02X", ev.volume);
                drawList->AddText(ImVec2(cx, canvasPos.y + ry + 2), IM_COL32(200, 200, 160, 255), sBuf);
                cx += 3 * m_charWidth;
            }

            // Effects (using event from col 0 as per current design)
            const auto& ev0 = pat.event(t, r, 0);
            uint8_t vals[] = { (uint8_t)ev0.effect1, ev0.param1, (uint8_t)ev0.effect2, ev0.param2 };
            
            for (int f = 0; f < 4; ++f) {
                if ((int)t == m_cursorTrack && r == m_cursorRow && m_cursorField == (3 + f))
                    drawList->AddRectFilled(ImVec2(cx - 1, canvasPos.y + ry), ImVec2(cx + 2 * m_charWidth + 1, canvasPos.y + ry + m_rowHeight), IM_COL32(100, 100, 150, 255));
                
                char eBuf[4];
                snprintf(eBuf, 4, "%02X", vals[f]);
                drawList->AddText(ImVec2(cx, canvasPos.y + ry + 2), IM_COL32(200, 160, 160, 255), eBuf);
                cx += 3 * m_charWidth;
            }
        }
        
        // Vertical divider
        drawList->AddLine(ImVec2(tx + tw - 1, canvasPos.y), ImVec2(tx + tw - 1, canvasPos.y + viewSize.y), IM_COL32(80, 80, 80, 255));
    }
}

void TrackerWidget::clampCursor() {
    auto& pat = m_engine.pattern();
    int numTracks = (int)m_engine.track_count();
    int numRows = (int)pat.row_count();

    if (numTracks > 0) {
        if (m_cursorTrack >= numTracks) m_cursorTrack = numTracks - 1;
        if (m_cursorTrack < 0) m_cursorTrack = 0;
    } else {
        m_cursorTrack = 0;
    }

    if (numRows > 0) {
        if (m_cursorRow >= numRows) m_cursorRow = numRows - 1;
        if (m_cursorRow < 0) m_cursorRow = 0;
    } else {
        m_cursorRow = 0;
    }
}

void TrackerWidget::handleKeyEvent(int key, int mods) {
    auto& pat = m_engine.pattern();
    
    if (m_cursorTrack >= (int)m_engine.track_count()) return;
    int numCols = (int)pat.column_count(m_cursorTrack);
    int totalFields = numCols * 3 + 4;
    int absField = m_cursorCol * 3 + m_cursorField;

    switch (key) {
        case SDLK_UP:
            m_cursorRow -= m_engine.step_size();
            if (m_cursorRow < 0) m_cursorRow = (int)pat.row_count() - 1;
            break;
        case SDLK_DOWN:
            m_cursorRow += m_engine.step_size();
            if (m_cursorRow >= (int)pat.row_count()) m_cursorRow = 0;
            break;
        case SDLK_LEFT:
            absField--;
            if (absField < 0) {
                if (m_cursorTrack > 0) {
                    m_cursorTrack--;
                    numCols = (int)pat.column_count(m_cursorTrack);
                    absField = numCols * 3 + 4 - 1;
                } else {
                    absField = 0;
                }
            }
            if (absField < numCols * 3) {
                m_cursorCol = absField / 3;
                m_cursorField = absField % 3;
            } else {
                m_cursorCol = 0;
                m_cursorField = 3 + (absField - numCols * 3);
            }
            break;
        case SDLK_RIGHT:
            absField++;
            if (absField >= totalFields) {
                if (m_cursorTrack < (int)m_engine.track_count() - 1) {
                    m_cursorTrack++;
                    numCols = (int)pat.column_count(m_cursorTrack);
                    absField = 0;
                } else {
                    absField = totalFields - 1;
                }
            }
            if (absField < numCols * 3) {
                m_cursorCol = absField / 3;
                m_cursorField = absField % 3;
            } else {
                m_cursorCol = 0;
                m_cursorField = 3 + (absField - numCols * 3);
            }
            break;
        case SDLK_PAGEUP:
            m_cursorRow = std::max(0, m_cursorRow - 16);
            break;
        case SDLK_PAGEDOWN:
            m_cursorRow = std::min((int)pat.row_count() - 1, m_cursorRow + 16);
            break;
        case SDLK_HOME:
            m_cursorRow = 0;
            break;
        case SDLK_END:
            m_cursorRow = (int)pat.row_count() - 1;
            break;
        case SDLK_BACKSPACE:
            deleteCurrentField();
            m_cursorRow -= m_engine.step_size();
            if (m_cursorRow < 0) m_cursorRow = (int)pat.row_count() - 1;
            break;
        case SDLK_DELETE:
            deleteCurrentField();
            break;
        default:
            insertNoteFromAction(key, mods);
            break;
    }
    clampCursor();
}

void TrackerWidget::deleteCurrentField() {
    clampCursor();
    if (m_cursorTrack < (int)m_engine.track_count()) {
        auto& ev = m_engine.pattern().event(m_cursorTrack, m_cursorRow, m_cursorCol);
        switch (m_cursorField) {
            case 0: ev.note = 255; break;
            case 1: ev.sample_idx = 0; break;
            case 2: ev.volume = 255; break;
            case 3: ev.effect1 = EffectType::None; break;
            case 4: ev.param1 = 0; break;
            case 5: ev.effect2 = EffectType::None; break;
            case 6: ev.param2 = 0; break;
        }
    }
}

void TrackerWidget::insertNoteFromAction(int key, int mods) {
    auto action = m_engine.m_key_bindings.get_action(key, mods);
    
    auto action_to_note = [](Action a) -> int {
        switch(a) {
            case Action::NoteC: return 0;  case Action::NoteCs: return 1;
            case Action::NoteD: return 2;  case Action::NoteDs: return 3;
            case Action::NoteE: return 4;  case Action::NoteF: return 5;
            case Action::NoteFs: return 6; case Action::NoteG: return 7;
            case Action::NoteGs: return 8; case Action::NoteA: return 9;
            case Action::NoteAs: return 10; case Action::NoteB: return 11;
            case Action::NoteC2: return 12; case Action::NoteCs2: return 13;
            case Action::NoteD2: return 14; case Action::NoteDs2: return 15;
            case Action::NoteE2: return 16; case Action::NoteF2: return 17;
            case Action::NoteFs2: return 18; case Action::NoteG2: return 19;
            case Action::NoteGs2: return 20; case Action::NoteA2: return 21;
            case Action::NoteAs2: return 22; case Action::NoteB2: return 23;
            case Action::NoteC3: return 24;
            case Action::NoteOff: return 254;
            default: return -1;
        }
    };
    
    auto is_note_action = [](Action a) -> bool {
        return ((int)a >= (int)Action::NoteC && (int)a <= (int)Action::NoteB) ||
               ((int)a >= (int)Action::NoteC2 && (int)a <= (int)Action::NoteB2) ||
               (a == Action::NoteC3) || (a == Action::NoteOff);
    };

    if (is_note_action(action)) {
        int note = action_to_note(action);
        if (note != -1) {
            insertNote((uint8_t)note);
        }
    }
}

void TrackerWidget::insertNote(uint8_t note) {
    if (m_cursorTrack >= (int)m_engine.track_count()) return;
    if (m_cursorRow >= (int)m_engine.pattern().row_count()) return;

    auto& pat = m_engine.pattern();
    auto& event = pat.event(m_cursorTrack, m_cursorRow, m_cursorCol);
    
    uint8_t finalNote = note;
    if (note != 254) {
        int octaveNote = note + m_engine.base_octave() * 12;
        if (octaveNote > 119) octaveNote = 119;
        finalNote = (uint8_t)octaveNote;
    }
    
    event.note = finalNote;
    m_engine.preview_note(m_cursorTrack, finalNote, m_cursorCol);

    if (!m_engine.is_playing()) {
        m_cursorRow = std::min((int)pat.row_count() - 1, m_cursorRow + (int)m_engine.step_size());
    }
    clampCursor();
}

}