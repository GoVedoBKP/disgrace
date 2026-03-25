#pragma once
#include "edit_command.h"
#include "../sequencer/pattern.h"
#include <vector>

namespace disgrace_ns
{
    class CmdEditBlock : public disgrace_ns::EditCommand
    {
    public:
        struct CellEdit {
            size_t track;
            size_t row;
            size_t field;
            uint8_t old_val;
            uint8_t new_val;
            
            CellEdit(size_t t, size_t r, size_t f, uint8_t ov, uint8_t nv)
                : track(t), row(r), field(f), old_val(ov), new_val(nv) {}
        };

        CmdEditBlock(disgrace_ns::Pattern& pat, const std::vector<CellEdit>& edits)
            : m_pattern(pat), m_edits(edits) {}

        void apply() override {
            for (const auto& edit : m_edits) {
                m_pattern.set_field(edit.track, edit.row, edit.field, edit.new_val);
            }
        }

        void undo() override {
            for (const auto& edit : m_edits) {
                m_pattern.set_field(edit.track, edit.row, edit.field, edit.old_val);
            }
        }

    private:
        disgrace_ns::Pattern& m_pattern;
        std::vector<CellEdit> m_edits;
    };
}
