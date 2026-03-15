#include "instrument_panel.h"
#include "detached_window.h"
#include "main_window.h"
#include "../core/engine.h"
#include "../instrument/sample_instrument.h"
#include "../instrument/soundfont_instrument.h"
#include "../instrument/dssi_instrument.h"
#include "../instrument/lv2_instrument.h"
#include "../instrument/midi_instrument.h"
#include "../io/audio_file.h"
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/fl_ask.H>
#include <FL/fl_message.H>
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>

namespace disgrace_ns {

InstrumentPanel::InstrumentPanel(int x, int y, int w, int h, Engine& engine)
    : Fl_Group(x, y, w, h), m_engine(engine) {
    
    begin();

    int left_w = 350;
    int margin = 10;

    m_detach_btn = new Fl_Button(x + w - 30, y, 30, 20, "[]");
    m_detach_btn->callback(cb_detach, this);

    // Left Panel
    m_left_panel = new Fl_Group(x, y, left_w, h, "Instruments");
    m_left_panel->box(FL_ENGRAVED_FRAME);
    m_left_panel->align(FL_ALIGN_TOP_LEFT);
    m_left_panel->begin();

    int cur_y = margin;
    int btn_w = (left_w - 5 * margin) / 4;

    m_new_btn = new Fl_Button(x + margin, y + cur_y, btn_w, 25, "New");
    m_new_btn->callback(cb_new, this);

    m_load_btn = new Fl_Button(x + 2 * margin + btn_w, y + cur_y, btn_w, 25, "Load");
    m_load_btn->callback(cb_load, this);

    m_save_btn = new Fl_Button(x + 3 * margin + 2 * btn_w, y + cur_y, btn_w, 25, "Save");
    m_save_btn->callback(cb_save, this);

    m_delete_btn = new Fl_Button(x + 4 * margin + 3 * btn_w, y + cur_y, btn_w, 25, "Del");
    m_delete_btn->callback(cb_delete, this);

    cur_y += 25 + margin;

    m_inst_scroll = new Fl_Scroll(x + margin, y + cur_y, left_w - 2 * margin, 200);
    m_inst_scroll->type(Fl_Scroll::VERTICAL);
    m_inst_container = new Fl_Group(x + margin, y + cur_y, left_w - 40, 1000);
    m_inst_container->end();
    m_inst_scroll->add(m_inst_container);
    m_inst_scroll->end();

    cur_y += 200 + margin;

    m_file_browser = new Fl_File_Browser(x + margin, y + cur_y, left_w - 2 * margin, y + h - (y + cur_y) - margin);
    m_file_browser->load(".");

    m_left_panel->end();

    // Right Panel (Editor)
    m_right_panel = new Fl_Group(x + left_w, y, w - left_w, h, "Editor");
    m_right_panel->box(FL_ENGRAVED_FRAME);
    m_right_panel->align(FL_ALIGN_TOP_LEFT);
    m_right_panel->begin();
    
    // --- Sampler Editor ---
    m_sampler_editor = new Fl_Group(x + left_w, y, w - left_w, h);
    m_sampler_editor->begin();
    
    int middle_w = 150; 
    int split_x = x + left_w + middle_w;

    m_sampler_list_grp = new Fl_Group(x + left_w, y, middle_w, h, "Samples");
    m_sampler_list_grp->box(FL_ENGRAVED_FRAME);
    m_sampler_list_grp->align(FL_ALIGN_TOP_LEFT);
    m_sampler_list_grp->begin();
    m_add_sample_btn = new Fl_Button(x + left_w + margin, y + margin, middle_w - 2 * margin, 25, "Add Sample");
    m_add_sample_btn->callback(cb_add_sample, this);
    
    m_sample_scroll = new Fl_Scroll(x + left_w + margin, y + margin + 35, middle_w - 2 * margin, h - margin*2 - 35);
    m_sample_scroll->type(Fl_Scroll::VERTICAL);
    m_sample_container = new Fl_Group(x + left_w + margin, y + margin + 35, middle_w - 40, 1000);
    m_sample_container->end();
    m_sample_scroll->add(m_sample_container);
    m_sample_scroll->end();
    m_sampler_list_grp->end();

    m_sampler_rec_grp = new Fl_Group(split_x, y, w - left_w - middle_w, h, "Waveform");
    m_sampler_rec_grp->box(FL_ENGRAVED_FRAME);
    m_sampler_rec_grp->align(FL_ALIGN_TOP_LEFT);
    m_sampler_rec_grp->begin();
    int rec_y = margin;
    m_sample_play_btn = new Fl_Button(split_x + margin, y + rec_y, 30, 25, "@>");
    m_sample_play_btn->callback(cb_sample_play, this);
    m_sample_stop_btn = new Fl_Button(split_x + margin + 32, y + rec_y, 30, 25, "@||");
    m_sample_stop_btn->callback(cb_sample_stop, this);
    m_rec_btn = new Fl_Light_Button(split_x + margin + 64, y + rec_y, 80, 25, "Record");
    m_rec_btn->callback(cb_record_sample, this);
    m_rec_btn->labelcolor(FL_RED);
    m_rec_mode_ch = new Fl_Choice(split_x + margin + 149, y + rec_y, 80, 25);
    m_rec_mode_ch->add("Free|Synced");
    m_rec_mode_ch->value(0);
    m_rec_input_ch = new Fl_Choice(split_x + margin + 234, y + rec_y, 120, 25);
    rec_y += 25 + margin;
    m_mono_btn = new Fl_Check_Button(split_x + margin, y + rec_y, 80, 25, "Mono");
    m_mono_btn->callback(cb_mono_toggle, this);
    m_sample_fmt_ch = new Fl_Choice(split_x + margin + 100, y + rec_y, 150, 25, "Format:");
    m_sample_fmt_ch->add("Stereo|Stereo -> Mono (L)|Stereo -> Mono (R)|Stereo -> Mono (Mix)|Mono -> Stereo");
    m_sample_fmt_ch->callback(cb_sample_fmt, this);
    m_sample_fmt_ch->value(0); // Stereo as default
    rec_y += 25 + margin;
    m_waveform_view = new WaveformView(split_x + margin, y + rec_y, w - split_x - 2 * margin, 400, m_engine);
    rec_y += 400 + margin;
    m_zoom_in_btn = new Fl_Button(split_x + margin, y + rec_y, 70, 25, "Zoom +");
    m_zoom_in_btn->callback(cb_zoom_in, this);
    m_zoom_out_btn = new Fl_Button(split_x + margin + 72, y + rec_y, 70, 25, "Zoom -");
    m_zoom_out_btn->callback(cb_zoom_out, this);
    m_view_all_btn = new Fl_Button(split_x + margin + 144, y + rec_y, 70, 25, "View All");
    m_view_all_btn->callback(cb_view_all, this);
    m_view_sel_btn = new Fl_Button(split_x + margin + 216, y + rec_y, 70, 25, "View Sel");
    m_view_sel_btn->callback(cb_view_sel, this);
    m_view_mode_ch = new Fl_Choice(split_x + margin + 288, y + rec_y, 80, 25, "View:");
    m_view_mode_ch->add("Both|Left|Right");
    m_view_mode_ch->value(0);
    m_view_mode_ch->callback(cb_view_mode, this);
    rec_y += 25 + margin;
    Fl_Group* ops_grp = new Fl_Group(split_x + margin, y + rec_y, w - split_x - 2 * margin, 100, "Operations");
    ops_grp->box(FL_ENGRAVED_FRAME);
    ops_grp->align(FL_ALIGN_TOP_LEFT);
    ops_grp->begin();
    m_cut_btn = new Fl_Button(split_x + 2*margin, y + rec_y + margin, 50, 25, "Cut"); m_cut_btn->callback(cb_cut, this);
    m_copy_btn = new Fl_Button(split_x + 2*margin + 52, y + rec_y + margin, 50, 25, "Copy"); m_copy_btn->callback(cb_copy, this);
    m_paste_btn = new Fl_Button(split_x + 2*margin + 104, y + rec_y + margin, 50, 25, "Paste"); m_paste_btn->callback(cb_paste, this);
    m_silence_btn = new Fl_Button(split_x + 2*margin + 156, y + rec_y + margin, 60, 25, "Silence"); m_silence_btn->callback(cb_silence, this);
    m_norm_btn = new Fl_Button(split_x + 2*margin + 218, y + rec_y + margin, 80, 25, "Normalize"); m_norm_btn->callback(cb_normalize, this);
    m_vol_btn = new Fl_Button(split_x + 2*margin + 300, y + rec_y + margin, 50, 25, "Gain"); m_vol_btn->callback(cb_adjust_vol, this);
    m_vol_input = new Fl_Value_Input(split_x + 2*margin + 352, y + rec_y + margin, 40, 25); m_vol_input->value(1.0);
    int f_y = margin + 30;
    m_fade_in_lin_btn = new Fl_Button(split_x + 2*margin, y + rec_y + f_y, 80, 25, "Fade In Lin"); m_fade_in_lin_btn->callback(cb_fade_in_lin, this);
    m_fade_in_log_btn = new Fl_Button(split_x + 2*margin + 85, y + rec_y + f_y, 80, 25, "Fade In Log"); m_fade_in_log_btn->callback(cb_fade_in_log, this);
    m_fade_out_lin_btn = new Fl_Button(split_x + 2*margin + 170, y + rec_y + f_y, 80, 25, "Fade Out Lin"); m_fade_out_lin_btn->callback(cb_fade_out_lin, this);
    m_fade_out_log_btn = new Fl_Button(split_x + 2*margin + 255, y + rec_y + f_y, 80, 25, "Fade Out Log"); m_fade_out_log_btn->callback(cb_fade_out_log, this);
    ops_grp->end();
    m_sampler_rec_grp->end();
    m_sampler_editor->end();
    m_sampler_editor->hide();

    // --- SoundFont Editor ---
    m_sfont_editor = new Fl_Group(x + left_w, y, w - left_w, h);
    m_sfont_editor->begin();
    int sf_y = margin;
    m_sfont_load_btn = new Fl_Button(x + left_w + margin, y + sf_y, 100, 25, "Load SoundFont");
    m_sfont_load_btn->callback(cb_sfont_load, this);
    sf_y += 25 + margin;
    m_sfont_browser = new Fl_Browser(x + left_w + margin, y + sf_y, w - left_w - 2 * margin, h - sf_y - 60);
    m_sfont_browser->type(FL_HOLD_BROWSER);
    m_sfont_browser->callback(cb_sfont_select, this);
    sf_y = h - 45;
    m_sfont_vol_slider = new Fl_Value_Slider(x + left_w + 100, y + sf_y, 200, 25, "Volume");
    m_sfont_vol_slider->type(FL_HOR_NICE_SLIDER);
    m_sfont_vol_slider->range(0, 128);
    m_sfont_vol_slider->value(100);
    m_sfont_vol_slider->callback(cb_sfont_vol, this);
    m_sfont_editor->end();
    m_sfont_editor->hide();

    // --- Plugin Editor ---
    m_plugin_editor = new Fl_Group(x + left_w, y, w - left_w, h);
    m_plugin_editor->begin();
    int pl_y = margin;
    m_plugin_scan_btn = new Fl_Button(x + left_w + margin, y + pl_y, 100, 25, "Scan Plugins");
    m_plugin_scan_btn->callback(cb_plugin_scan, this);
    pl_y += 25 + margin;
    int pl_split_x = x + left_w + (w - left_w) / 2;
    m_plugin_browser = new Fl_Browser(x + left_w + margin, y + pl_y, (w - left_w) / 2 - 2 * margin, h - pl_y - margin);
    m_plugin_browser->type(FL_HOLD_BROWSER);
    m_plugin_browser->callback(cb_plugin_select, this);
    m_plugin_controls_grp = new Fl_Group(pl_split_x, y + pl_y, (w - left_w) / 2, h - pl_y, "Controls");
    m_plugin_controls_grp->box(FL_ENGRAVED_FRAME);
    m_plugin_controls_grp->align(FL_ALIGN_TOP_LEFT);
    m_plugin_controls_grp->begin();
    m_plugin_scroll = new Fl_Scroll(pl_split_x + 5, y + pl_y + 5, (w - left_w) / 2 - 10, h - pl_y - 10);
    m_plugin_scroll->type(Fl_Scroll::VERTICAL);
    m_plugin_controls_container = new Fl_Group(pl_split_x + 5, y + pl_y + 5, (w - left_w) / 2 - 30, 1000);
    m_plugin_controls_container->begin(); m_plugin_controls_container->end();
    m_plugin_scroll->add(m_plugin_controls_container);
    m_plugin_scroll->end();
    m_plugin_controls_grp->end();

    // --- ZynAddSubFX Editor (Embedded in Plugin Editor right pane) ---
    m_zyn_editor = new Fl_Group(pl_split_x, y + pl_y, (w - left_w) / 2, 220);
    m_zyn_editor->begin();
    int z_y = 5;
    m_zyn_bank_ch = new Fl_Choice(pl_split_x + 60, y + pl_y + z_y, (w - left_w) / 2 - 70, 25, "Bank:");
    m_zyn_bank_ch->callback(cb_zyn_bank, this);
    z_y += 30;
    m_zyn_prev_btn = new Fl_Button(pl_split_x + 10, y + pl_y + z_y, 40, 25, "@<");
    m_zyn_prev_btn->callback(cb_zyn_prev, this);
    m_zyn_next_btn = new Fl_Button(pl_split_x + 55, y + pl_y + z_y, 40, 25, "@>");
    m_zyn_next_btn->callback(cb_zyn_next, this);
    z_y += 30;
    m_zyn_preset_browser = new Fl_Browser(pl_split_x + 10, y + pl_y + z_y, (w - left_w) / 2 - 20, 150);
    m_zyn_preset_browser->type(FL_HOLD_BROWSER);
    m_zyn_preset_browser->callback(cb_zyn_preset, this);
    m_zyn_editor->end();
    m_zyn_editor->hide();

    m_plugin_editor->end();
    m_plugin_editor->hide();

    // --- MIDI Editor ---
    m_midi_editor = new Fl_Group(x + left_w, y, w - left_w, h);
    m_midi_editor->begin();
    int m_y = margin;
    m_midi_channel = new Fl_Value_Input(x + left_w + 120, y + m_y, 50, 25, "MIDI Channel:");
    m_midi_channel->minimum(1);
    m_midi_channel->maximum(16);
    m_midi_channel->step(1);
    m_midi_channel->callback(cb_midi_ch, this);
    m_y += 30;
    m_midi_program = new Fl_Value_Input(x + left_w + 120, y + m_y, 50, 25, "MIDI Program:");
    m_midi_program->minimum(0);
    m_midi_program->maximum(127);
    m_midi_program->step(1);
    m_midi_program->callback(cb_midi_pg, this);
    m_midi_editor->end();
    m_midi_editor->hide();

    m_right_panel->end();
    resizable(m_right_panel);
    end();

    update_instrument_list();
    update_editor();
    update_rec_inputs();
}

void InstrumentPanel::update_rec_inputs() {
    m_rec_input_ch->clear();
    bool mono = m_mono_btn->value();
    uint32_t num_ins = m_engine.m_num_ins;
    if (mono) {
        for (uint32_t i = 0; i < num_ins; ++i) {
            char buf[32]; snprintf(buf, 32, "Channel %u", i + 1);
            m_rec_input_ch->add(strdup(buf));
        }
    } else {
        for (uint32_t i = 0; i < num_ins; i += 2) {
            char buf[32];
            if (i + 1 < num_ins) snprintf(buf, 32, "Channels %u/%u", i + 1, i + 2);
            else snprintf(buf, 32, "Channel %u (L)", i + 1);
            m_rec_input_ch->add(strdup(buf));
        }
    }
    m_rec_input_ch->value(0);
}

void InstrumentPanel::update_instrument_list() {
    for (int i = 0; i < m_inst_container->children(); ++i) {
        void* d = m_inst_container->child(i)->user_data();
        if (d) delete static_cast<std::pair<InstrumentPanel*, size_t>*>(d);
    }
    m_inst_container->clear();
    m_inst_container->begin();
    int row_h = 35;
    size_t num_insts = m_engine.instrument_count();
    int start_x = m_inst_container->x();
    int start_y = m_inst_container->y();
    for (size_t i = 0; i < num_insts; ++i) {
        int cur_y = start_y + (int)(i * row_h);
        auto& inst = m_engine.instrument(i);
        Fl_Button* sel_btn = new Fl_Button(start_x, cur_y, 30, row_h, strdup((std::to_string(i+1) + ":").c_str()));
        sel_btn->box(FL_NO_BOX);
        if ((int)i == m_selected_instrument) sel_btn->labelcolor(FL_YELLOW);
        sel_btn->callback(cb_inst_select, new std::pair<InstrumentPanel*, size_t>(this, i));
        Fl_Input* name_in = new Fl_Input(start_x + 35, cur_y + 5, 120, 25);
        name_in->value(inst.name().c_str());
        name_in->callback(cb_inst_name, new std::pair<InstrumentPanel*, size_t>(this, i));
        Fl_Choice* type_ch = new Fl_Choice(start_x + 160, cur_y + 5, 100, 25);
        type_ch->add("None|Sampler|SoundFont|Plugin|MIDI");
        type_ch->value((int)inst.type());
        type_ch->callback(cb_inst_type, new std::pair<InstrumentPanel*, size_t>(this, i));
    }
    m_inst_container->end();
    m_inst_container->size(m_inst_container->w(), (int)(num_insts * row_h + 20));
    m_inst_scroll->redraw();
}

void InstrumentPanel::update_editor() {
    m_sampler_editor->hide();
    m_sfont_editor->hide();
    m_plugin_editor->hide();
    m_zyn_editor->hide();
    m_midi_editor->hide();
    m_sample_play_btn->hide();
    m_sample_stop_btn->hide();

    if (m_selected_instrument < 0 || m_selected_instrument >= (int)m_engine.instrument_count()) {
        m_right_panel->redraw();
        return;
    }

    auto& inst = m_engine.instrument(m_selected_instrument);
    if (inst.type() == InstrumentType::Sampler) {
        m_sampler_editor->show();
        m_sample_play_btn->show();
        m_sample_stop_btn->show();
        for (int i = 0; i < m_sample_container->children(); ++i) {
            void* d = m_sample_container->child(i)->user_data();
            if (d) delete static_cast<std::pair<InstrumentPanel*, size_t>*>(d);
        }
        m_sample_container->clear();
        m_sample_container->begin();
        SampleInstrument* sampler = static_cast<SampleInstrument*>(&inst);
        int row_h = 35;
        int start_x = m_sample_container->x();
        int start_y = m_sample_container->y();
        for (size_t i = 0; i < sampler->sample_count(); ++i) {
            int cur_y = start_y + (int)(i * row_h);
            const auto& entry = sampler->get_sample(i);
            Fl_Button* sel = new Fl_Button(start_x, cur_y, 25, 25, strdup((std::to_string(i+1) + ":").c_str()));
            sel->box(FL_NO_BOX);
            if ((int)i == m_selected_sample) sel->labelcolor(FL_YELLOW);
            sel->callback(cb_sample_select, new std::pair<InstrumentPanel*, size_t>(this, i));
            Fl_Input* name_in = new Fl_Input(start_x + 30, cur_y + 5, 60, 25);
            name_in->value(entry.name.c_str());
            name_in->callback(cb_sample_name, new std::pair<InstrumentPanel*, size_t>(this, i));
            Fl_Button* load = new Fl_Button(start_x + 95, cur_y + 5, 20, 25, "L"); load->callback(cb_load_sample, new std::pair<InstrumentPanel*, size_t>(this, i));
            Fl_Button* up = new Fl_Button(start_x + 117, cur_y + 5, 20, 25, "U"); up->callback(cb_move_sample_up, new std::pair<InstrumentPanel*, size_t>(this, i));
            Fl_Button* down = new Fl_Button(start_x + 139, cur_y + 5, 20, 25, "D"); down->callback(cb_move_sample_down, new std::pair<InstrumentPanel*, size_t>(this, i));
            Fl_Button* rem = new Fl_Button(start_x + 161, cur_y + 5, 20, 25, "X"); rem->callback(cb_remove_sample, new std::pair<InstrumentPanel*, size_t>(this, i));
        }
        m_sample_container->end();
        m_sample_container->size(m_sample_container->w(), (int)(sampler->sample_count() * row_h + 20));
        m_sample_scroll->redraw();
        m_waveform_view->set_color(m_engine.m_waveform_color);
        if (m_selected_sample >= 0 && m_selected_sample < (int)sampler->sample_count()) m_waveform_view->set_sample(sampler->get_sample(m_selected_sample).data);
        else m_waveform_view->set_sample(nullptr);
    } else if (inst.type() == InstrumentType::SoundFont) {
        m_sfont_editor->show();
        SoundFontInstrument* sf = static_cast<SoundFontInstrument*>(&inst);
        m_sfont_browser->clear();
        for (const auto& p : sf->presets()) {
            char buf[256]; snprintf(buf, 256, "[%03d:%03d] %s", p.bank, p.num, p.name.c_str());
            m_sfont_browser->add(strdup(buf));
        }
        if (sf->current_preset() >= 0) m_sfont_browser->select(sf->current_preset() + 1);
    } else if (inst.type() == InstrumentType::Plugin) {
        m_plugin_editor->show();
        if (m_plugin_browser->size() == 0) {
            cb_plugin_scan(nullptr, this);
        }
        
        // Highlight current plugin
        m_plugin_browser->deselect();
        std::string current_path;
        if (auto* dssi = dynamic_cast<DSSIInstrument*>(&inst)) {
            current_path = dssi->path();
            for (auto const& [idx, info] : m_plugin_map) {
                if (info.path == current_path && info.index == dssi->index()) {
                    m_plugin_browser->select(idx);
                    break;
                }
            }
        }

        bool is_zyn = (inst.plugin_name().find("ZynAddSubFX") != std::string::npos);
        if (is_zyn) {
            m_zyn_editor->show();
            int zyn_h = 220;
            m_plugin_controls_grp->resize(m_plugin_controls_grp->x(), m_sampler_editor->y() + zyn_h + 5, 
                                        m_plugin_controls_grp->w(), m_right_panel->h() - (m_zyn_editor->y() - m_right_panel->y()) - zyn_h - 15);
            if (m_zyn_bank_ch->size() == 0) {
                std::vector<std::string> bank_paths = {"/usr/share/zynaddsubfx/banks", "/usr/local/share/zynaddsubfx/banks"};
                for (const auto& bp : bank_paths) {
                    DIR* dir = opendir(bp.c_str()); if (!dir) continue;
                    struct dirent* entry; while ((entry = readdir(dir)) != nullptr) {
                        if (entry->d_type == DT_DIR && entry->d_name[0] != '.') m_zyn_bank_ch->add(strdup(entry->d_name));
                    }
                    closedir(dir);
                }
                if (m_zyn_bank_ch->size() > 0) { m_zyn_bank_ch->value(0); cb_zyn_bank(nullptr, this); }
            }
        } else {
            m_zyn_editor->hide();
            m_plugin_controls_grp->resize(m_plugin_controls_grp->x(), m_sampler_editor->y(), 
                                        m_plugin_controls_grp->w(), m_sampler_editor->h());
        }
        m_plugin_controls_container->clear(); m_plugin_controls_container->begin();
        int row_h = 45; int cur_y = m_plugin_controls_container->y(); int start_x = m_plugin_controls_container->x();
        for (int i = 0; i < m_plugin_controls_container->children(); ++i) {
            void* d = m_plugin_controls_container->child(i)->user_data();
            if (d) delete static_cast<std::pair<InstrumentPanel*, size_t>*>(d);
        }
        m_plugin_controls_container->clear();
        m_plugin_controls_container->begin();
        size_t num_params = inst.parameter_count();
        for (size_t i = 0; i < num_params; ++i) {
            auto param = inst.get_parameter(i);
            Fl_Value_Slider* slider = new Fl_Value_Slider(start_x, cur_y + 15, m_plugin_controls_container->w(), 20, strdup(param.name.c_str()));
            slider->type(FL_HOR_NICE_SLIDER); slider->align(FL_ALIGN_TOP_LEFT); slider->labelsize(10);
            slider->range(param.min, param.max); slider->value(param.value);
            slider->callback(cb_plugin_param, new std::pair<InstrumentPanel*, size_t>(this, i));
            cur_y += row_h;
        }
        m_plugin_controls_container->end(); m_plugin_controls_container->size(m_plugin_controls_container->w(), (int)(num_params * row_h + 20));
        m_plugin_scroll->redraw();
    } else if (inst.type() == InstrumentType::Midi) {
        m_midi_editor->show();
        MidiInstrument* midi = static_cast<MidiInstrument*>(&inst);
        m_midi_channel->value(midi->channel() + 1);
        m_midi_program->value(midi->program());
    }
    m_right_panel->redraw();
}

void InstrumentPanel::cb_new(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data);
    self->m_engine.add_instrument();
    self->m_selected_instrument = (int)self->m_engine.instrument_count() - 1;
    self->m_selected_sample = -1;
    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
        MainWindow* mw = dynamic_cast<MainWindow*>(win);
        if (mw) mw->request_update();
    }
}

void InstrumentPanel::cb_inst_select(Fl_Widget*, void* data) {
    auto* pair = static_cast<std::pair<InstrumentPanel*, size_t>*>(data);
    pair->first->m_selected_instrument = (int)pair->second;
    pair->first->m_selected_sample = -1;
    pair->first->update_editor();
    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
        MainWindow* mw = dynamic_cast<MainWindow*>(win);
        if (mw) mw->request_update();
    }
}

void InstrumentPanel::cb_sample_select(Fl_Widget*, void* data) {
    auto* pair = static_cast<std::pair<InstrumentPanel*, size_t>*>(data);
    InstrumentPanel* self = pair->first;
    self->m_selected_sample = (int)pair->second;
    
    auto& inst = self->m_engine.instrument(self->m_selected_instrument);
    if (inst.type() == InstrumentType::Sampler) {
        static_cast<SampleInstrument*>(&inst)->set_selected_sample(pair->second);
    }
    
    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
        MainWindow* mw = dynamic_cast<MainWindow*>(win);
        if (mw) mw->request_update();
    }
}

void InstrumentPanel::cb_sample_play(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data);
    if (self->m_selected_instrument >= 0) {
        auto& inst = self->m_engine.instrument(self->m_selected_instrument);
        // Stop current play first
        inst.note_off();
        // Play at middle C (60)
        inst.note_on(60, 100);
    }
}

void InstrumentPanel::cb_sample_stop(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data);
    if (self->m_selected_instrument >= 0) {
        auto& inst = self->m_engine.instrument(self->m_selected_instrument);
        inst.note_off();
    }
}

void InstrumentPanel::cb_load(Fl_Widget*, void*) {}
void InstrumentPanel::cb_save(Fl_Widget*, void*) {}

void InstrumentPanel::cb_delete(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data);
    if (self->m_selected_instrument >= 0) {
        if (fl_ask("Delete selected instrument?")) {
            self->m_engine.remove_instrument(self->m_selected_instrument);
            self->m_selected_instrument = -1; self->m_selected_sample = -1;
            for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
                MainWindow* mw = dynamic_cast<MainWindow*>(win);
                if (mw) mw->request_update();
            }
        }
    }
}

void InstrumentPanel::cb_inst_name(Fl_Widget* w, void* data) {
    auto* pair = static_cast<std::pair<InstrumentPanel*, size_t>*>(data);
    pair->first->m_engine.instrument(pair->second).set_name(static_cast<Fl_Input*>(w)->value());
    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
        MainWindow* mw = dynamic_cast<MainWindow*>(win);
        if (mw) mw->request_update();
    }
}

void InstrumentPanel::cb_inst_type(Fl_Widget* w, void* data) {
    auto* pair = static_cast<std::pair<InstrumentPanel*, size_t>*>(data);
    pair->first->m_engine.set_instrument_type(pair->second, (InstrumentType)static_cast<Fl_Choice*>(w)->value());
    pair->first->m_selected_instrument = (int)pair->second;
    pair->first->m_selected_sample = -1;
    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
        MainWindow* mw = dynamic_cast<MainWindow*>(win);
        if (mw) mw->request_update();
    }
}

void InstrumentPanel::cb_add_sample(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data);
    if (self->m_selected_instrument < 0) return;
    auto& inst = self->m_engine.instrument(self->m_selected_instrument);
    if (inst.type() == InstrumentType::Sampler) {
        static_cast<SampleInstrument*>(&inst)->add_sample("New Sample", std::make_shared<SampleData>());
        self->update_editor();
    }
}

void InstrumentPanel::cb_load_sample(Fl_Widget*, void* data) {
    auto* pair = static_cast<std::pair<InstrumentPanel*, size_t>*>(data);
    InstrumentPanel* self = pair->first;
    Fl_Native_File_Chooser fnfc; fnfc.title("Load Sample"); fnfc.type(Fl_Native_File_Chooser::BROWSE_FILE);
    fnfc.filter("Audio Files\t*.{wav,flac,mp3}\n");
    if (fnfc.show() == 0) {
        auto& inst = self->m_engine.instrument(self->m_selected_instrument);
        if (inst.type() == InstrumentType::Sampler) {
            auto* sampler = static_cast<SampleInstrument*>(&inst);
            auto data_ptr = std::make_shared<SampleData>();
            uint32_t sr = 0;
            if (AudioFile::load_audio(fnfc.filename(), data_ptr->left, data_ptr->right, sr)) {
                data_ptr->sample_rate = (int)sr;
                std::string name = fnfc.filename(); size_t last = name.find_last_of("/\\"); if (last != std::string::npos) name = name.substr(last + 1);
                sampler->set_sample_name(pair->second, name);
                sampler->get_sample(pair->second).data = data_ptr;
                self->m_selected_sample = (int)pair->second;
                for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
                    MainWindow* mw = dynamic_cast<MainWindow*>(win);
                    if (mw) mw->request_update();
                }
            }
        }
    }
}

void InstrumentPanel::cb_remove_sample(Fl_Widget*, void* data) {
    auto* pair = static_cast<std::pair<InstrumentPanel*, size_t>*>(data);
    static_cast<SampleInstrument*>(&pair->first->m_engine.instrument(pair->first->m_selected_instrument))->remove_sample(pair->second);
    pair->first->m_selected_sample = -1; 
    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
        MainWindow* mw = dynamic_cast<MainWindow*>(win);
        if (mw) mw->request_update();
    }
}

void InstrumentPanel::cb_move_sample_up(Fl_Widget*, void* data) {
    auto* pair = static_cast<std::pair<InstrumentPanel*, size_t>*>(data);
    if (pair->second > 0) {
        static_cast<SampleInstrument*>(&pair->first->m_engine.instrument(pair->first->m_selected_instrument))->move_sample(pair->second, pair->second - 1);
        pair->first->m_selected_sample = (int)pair->second - 1; 
        for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
            MainWindow* mw = dynamic_cast<MainWindow*>(win);
            if (mw) mw->request_update();
        }
    }
}

void InstrumentPanel::cb_move_sample_down(Fl_Widget*, void* data) {
    auto* pair = static_cast<std::pair<InstrumentPanel*, size_t>*>(data);
    SampleInstrument* sampler = static_cast<SampleInstrument*>(&pair->first->m_engine.instrument(pair->first->m_selected_instrument));
    if (pair->second < sampler->sample_count() - 1) {
        sampler->move_sample(pair->second, pair->second + 1);
        pair->first->m_selected_sample = (int)pair->second + 1; 
        for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
            MainWindow* mw = dynamic_cast<MainWindow*>(win);
            if (mw) mw->request_update();
        }
    }
}

void InstrumentPanel::cb_save_sample(Fl_Widget*, void*) {}
void InstrumentPanel::cb_sample_name(Fl_Widget* w, void* data) {
    auto* pair = static_cast<std::pair<InstrumentPanel*, size_t>*>(data);
    static_cast<SampleInstrument*>(&pair->first->m_engine.instrument(pair->first->m_selected_instrument))->set_sample_name(pair->second, static_cast<Fl_Input*>(w)->value());
    for (Fl_Window* win = Fl::first_window(); win; win = Fl::next_window(win)) {
        MainWindow* mw = dynamic_cast<MainWindow*>(win);
        if (mw) mw->request_update();
    }
}
void InstrumentPanel::cb_record_sample(Fl_Widget* w, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data);
    auto* btn = static_cast<Fl_Light_Button*>(w);
    
    if (btn->value()) {
        // Start recording
        uint32_t channel = (uint32_t)self->m_rec_input_ch->value();
        bool mono = self->m_mono_btn->value();
        Engine::SampleRecordMode mode = (Engine::SampleRecordMode)self->m_rec_mode_ch->value();
        
        // In JACK, stereo inputs are often consecutive
        if (!mono) channel *= 2; 

        self->m_engine.start_recording_sample(mode, channel, mono);
    } else {
        // Stop recording
        self->m_engine.stop_recording_sample();
        
        // Add recorded sample to the current instrument
        if (self->m_selected_instrument >= 0) {
            auto& inst = self->m_engine.instrument(self->m_selected_instrument);
            if (inst.type() == InstrumentType::Sampler) {
                auto* sampler = static_cast<SampleInstrument*>(&inst);
                if (self->m_engine.m_recording_sample_data && !self->m_engine.m_recording_sample_data->left.empty()) {
                    sampler->add_sample("Recorded Sample", self->m_engine.m_recording_sample_data);
                    self->m_selected_sample = (int)sampler->sample_count() - 1;
                    self->update_editor();
                }
            }
        }
    }
}
void InstrumentPanel::cb_mono_toggle(Fl_Widget*, void* data) { static_cast<InstrumentPanel*>(data)->update_rec_inputs(); }
void InstrumentPanel::cb_zoom_in(Fl_Widget*, void* data) { static_cast<InstrumentPanel*>(data)->m_waveform_view->zoom_in(); }
void InstrumentPanel::cb_zoom_out(Fl_Widget*, void* data) { static_cast<InstrumentPanel*>(data)->m_waveform_view->zoom_out(); }
void InstrumentPanel::cb_view_all(Fl_Widget*, void* data) { static_cast<InstrumentPanel*>(data)->m_waveform_view->view_all(); }
void InstrumentPanel::cb_view_sel(Fl_Widget*, void* data) { static_cast<InstrumentPanel*>(data)->m_waveform_view->view_selection(); }
void InstrumentPanel::cb_view_mode(Fl_Widget* w, void* data) { static_cast<InstrumentPanel*>(data)->m_waveform_view->set_channel_mode((ChannelMode)static_cast<Fl_Choice*>(w)->value()); }
void InstrumentPanel::cb_sample_fmt(Fl_Widget* w, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data); if (self->m_selected_sample < 0) return;
    static_cast<SampleInstrument*>(&self->m_engine.instrument(self->m_selected_instrument))->convert_sample_format(self->m_selected_sample, (SampleFormatAction)static_cast<Fl_Choice*>(w)->value());
    self->update_editor();
}

void InstrumentPanel::cb_sfont_load(Fl_Widget*, void* data) {
    InstrumentPanel* self = static_cast<InstrumentPanel*>(data);
    Fl_Native_File_Chooser fnfc; fnfc.title("Load SoundFont"); fnfc.type(Fl_Native_File_Chooser::BROWSE_FILE); fnfc.filter("SoundFont Files\t*.{sf2,sf3}\n");
    if (fnfc.show() == 0) {
        auto& inst = self->m_engine.instrument(self->m_selected_instrument);
        if (inst.type() == InstrumentType::SoundFont) {
            static_cast<SoundFontInstrument*>(&inst)->load_soundfont(fnfc.filename());
            self->update_editor();
        }
    }
}

void InstrumentPanel::cb_sfont_select(Fl_Widget* w, void* data) {
    InstrumentPanel* self = static_cast<InstrumentPanel*>(data);
    Fl_Browser* b = static_cast<Fl_Browser*>(w);
    int idx = b->value();
    if (idx > 0) {
        auto& inst = self->m_engine.instrument(self->m_selected_instrument);
        if (inst.type() == InstrumentType::SoundFont) {
            static_cast<SoundFontInstrument*>(&inst)->set_preset(idx - 1);
        }
    }
}

void InstrumentPanel::cb_sfont_vol(Fl_Widget* w, void* data) {
    InstrumentPanel* self = static_cast<InstrumentPanel*>(data);
    auto& inst = self->m_engine.instrument(self->m_selected_instrument);
    if (inst.type() == InstrumentType::SoundFont) {
        static_cast<SoundFontInstrument*>(&inst)->set_volume((float)static_cast<Fl_Value_Slider*>(w)->value() / 128.0f);
    }
}

void InstrumentPanel::cb_plugin_scan(Fl_Widget*, void* data) {
    InstrumentPanel* self = static_cast<InstrumentPanel*>(data);
    self->m_plugin_browser->clear(); self->m_plugin_map.clear();
    const char* dssi_header = "@b@C4--- DSSI Plugins ---";
    self->m_plugin_browser->add(dssi_header);
    std::vector<std::string> dssi_paths = {"/usr/lib/dssi", "/usr/local/lib/dssi", "/usr/lib/x86_64-linux-gnu/dssi"};
    for (const auto& path : dssi_paths) {
        DIR* dir = opendir(path.c_str()); if (!dir) continue;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string full_path = path + "/" + entry->d_name;
            if (full_path.find(".so") != std::string::npos) {
                void* handle = dlopen(full_path.c_str(), RTLD_LAZY);
                if (handle) {
                    DSSI_Descriptor_Function df = (DSSI_Descriptor_Function)dlsym(handle, "dssi_descriptor");
                    if (df) {
                        for (int i = 0; ; ++i) {
                            const DSSI_Descriptor* desc = df(i); if (!desc) break;
                            std::string label = std::string(desc->LADSPA_Plugin->Name) + " (DSSI)";
                            int item_idx = self->m_plugin_browser->size() + 1;
                            self->m_plugin_browser->add(label.c_str());
                            self->m_plugin_map[item_idx] = {desc->LADSPA_Plugin->Name, full_path, i, false};
                        }
                    }
                    dlclose(handle);
                }
            }
        }
        closedir(dir);
    }
    self->m_plugin_browser->add("@b@C4--- LV2 Plugins (Stub) ---");
}

void InstrumentPanel::cb_plugin_select(Fl_Widget* w, void* data) {
    InstrumentPanel* self = static_cast<InstrumentPanel*>(data);
    int idx = self->m_plugin_browser->value();
    if (idx <= 0 || self->m_plugin_map.find(idx) == self->m_plugin_map.end()) return;
    auto info = self->m_plugin_map[idx];
    auto& inst = self->m_engine.instrument(self->m_selected_instrument);
    if (inst.type() == InstrumentType::Plugin) {
        if (static_cast<DSSIInstrument*>(&inst)->load_plugin(info.path, info.index)) { self->update_editor(); }
    }
}

void InstrumentPanel::cb_plugin_param(Fl_Widget* w, void* data) {
    auto* pair = static_cast<std::pair<InstrumentPanel*, size_t>*>(data);
    pair->first->m_engine.instrument(pair->first->m_selected_instrument).set_parameter(pair->second, (float)static_cast<Fl_Value_Slider*>(w)->value());
}

void InstrumentPanel::cb_zyn_bank(Fl_Widget*, void* data) {
    InstrumentPanel* self = static_cast<InstrumentPanel*>(data);
    self->m_zyn_preset_browser->clear();
    int bidx = self->m_zyn_bank_ch->value(); if (bidx < 0) return;
    std::string bank_name = self->m_zyn_bank_ch->text(bidx);
    std::vector<std::string> bank_paths = {"/usr/share/zynaddsubfx/banks", "/usr/local/share/zynaddsubfx/banks"};
    for (const auto& bp : bank_paths) {
        std::string full_path = bp + "/" + bank_name;
        DIR* dir = opendir(full_path.c_str()); if (!dir) continue;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string fname = entry->d_name;
            if (fname.size() > 4 && fname.substr(fname.size() - 4) == ".xiz") self->m_zyn_preset_browser->add(strdup(fname.c_str()));
        }
        closedir(dir);
    }
}

void InstrumentPanel::cb_zyn_preset(Fl_Widget* w, void* data) {
    InstrumentPanel* self = static_cast<InstrumentPanel*>(data);
    int bidx = self->m_zyn_bank_ch->value();
    int pidx = self->m_zyn_preset_browser->value(); 
    if (bidx < 0 || pidx <= 0) return;

    if (self->m_selected_instrument >= 0) {
        auto& inst = self->m_engine.instrument(self->m_selected_instrument);
        if (inst.type() == InstrumentType::Plugin) {
            static_cast<DSSIInstrument*>(&inst)->load_program((unsigned long)bidx, (unsigned long)(pidx - 1));
        }
    }
}

void InstrumentPanel::cb_zyn_prev(Fl_Widget*, void* data) {
    InstrumentPanel* self = static_cast<InstrumentPanel*>(data);
    int v = self->m_zyn_preset_browser->value(); if (v > 1) { self->m_zyn_preset_browser->value(v - 1); cb_zyn_preset(self->m_zyn_preset_browser, self); }
}

void InstrumentPanel::cb_zyn_next(Fl_Widget*, void* data) {
    InstrumentPanel* self = static_cast<InstrumentPanel*>(data);
    int v = self->m_zyn_preset_browser->value(); if (v > 0 && v < self->m_zyn_preset_browser->size()) { self->m_zyn_preset_browser->value(v + 1); cb_zyn_preset(self->m_zyn_preset_browser, self); }
}

void InstrumentPanel::cb_cut(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data); if (self->m_selected_sample < 0) return;
    SampleInstrument* sampler = static_cast<SampleInstrument*>(&self->m_engine.instrument(self->m_selected_instrument));
    auto& sample = sampler->get_sample(self->m_selected_sample); if (!sample.data) return;
    size_t s1 = self->m_waveform_view->selection_start(), s2 = self->m_waveform_view->selection_end();
    if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); } else if (s1 > s2) std::swap(s1, s2);
    self->m_engine.sample_clipboard().data = std::make_shared<SampleData>(sample.data->cut(s1, s2));
    self->update_editor();
}

void InstrumentPanel::cb_copy(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data); if (self->m_selected_sample < 0) return;
    SampleInstrument* sampler = static_cast<SampleInstrument*>(&self->m_engine.instrument(self->m_selected_instrument));
    auto& sample = sampler->get_sample(self->m_selected_sample); if (!sample.data) return;
    size_t s1 = self->m_waveform_view->selection_start(), s2 = self->m_waveform_view->selection_end();
    if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); } else if (s1 > s2) std::swap(s1, s2);
    auto cb = std::make_shared<SampleData>(); cb->sample_rate = sample.data->sample_rate;
    cb->left.assign(sample.data->left.begin() + s1, sample.data->left.begin() + s2);
    if (!sample.data->right.empty()) cb->right.assign(sample.data->right.begin() + s1, sample.data->right.begin() + s2);
    self->m_engine.sample_clipboard().data = cb;
}

void InstrumentPanel::cb_paste(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data); if (self->m_selected_sample < 0 || !self->m_engine.sample_clipboard().data) return;
    SampleInstrument* sampler = static_cast<SampleInstrument*>(&self->m_engine.instrument(self->m_selected_instrument));
    auto& sample = sampler->get_sample(self->m_selected_sample); if (!sample.data) return;
    sample.data->paste_at(self->m_waveform_view->selection_start(), *self->m_engine.sample_clipboard().data);
    self->update_editor();
}

void InstrumentPanel::cb_silence(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data); if (self->m_selected_sample < 0) return;
    SampleInstrument* sampler = static_cast<SampleInstrument*>(&self->m_engine.instrument(self->m_selected_instrument));
    auto& sample = sampler->get_sample(self->m_selected_sample); if (!sample.data) return;
    size_t s1 = self->m_waveform_view->selection_start(), s2 = self->m_waveform_view->selection_end();
    if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); } else if (s1 > s2) std::swap(s1, s2);
    sample.data->silence(s1, s2); self->update_editor();
}

void InstrumentPanel::cb_fade_in_lin(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data); if (self->m_selected_sample < 0) return;
    SampleInstrument* sampler = static_cast<SampleInstrument*>(&self->m_engine.instrument(self->m_selected_instrument));
    auto& sample = sampler->get_sample(self->m_selected_sample); if (!sample.data) return;
    size_t s1 = self->m_waveform_view->selection_start(), s2 = self->m_waveform_view->selection_end();
    if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); } else if (s1 > s2) std::swap(s1, s2);
    sample.data->fade_in(s1, s2, false); self->update_editor();
}

void InstrumentPanel::cb_fade_in_log(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data); if (self->m_selected_sample < 0) return;
    SampleInstrument* sampler = static_cast<SampleInstrument*>(&self->m_engine.instrument(self->m_selected_instrument));
    auto& sample = sampler->get_sample(self->m_selected_sample); if (!sample.data) return;
    size_t s1 = self->m_waveform_view->selection_start(), s2 = self->m_waveform_view->selection_end();
    if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); } else if (s1 > s2) std::swap(s1, s2);
    sample.data->fade_in(s1, s2, true); self->update_editor();
}

void InstrumentPanel::cb_fade_out_lin(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data); if (self->m_selected_sample < 0) return;
    SampleInstrument* sampler = static_cast<SampleInstrument*>(&self->m_engine.instrument(self->m_selected_instrument));
    auto& sample = sampler->get_sample(self->m_selected_sample); if (!sample.data) return;
    size_t s1 = self->m_waveform_view->selection_start(), s2 = self->m_waveform_view->selection_end();
    if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); } else if (s1 > s2) std::swap(s1, s2);
    sample.data->fade_out(s1, s2, false); self->update_editor();
}

void InstrumentPanel::cb_fade_out_log(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data); if (self->m_selected_sample < 0) return;
    SampleInstrument* sampler = static_cast<SampleInstrument*>(&self->m_engine.instrument(self->m_selected_instrument));
    auto& sample = sampler->get_sample(self->m_selected_sample); if (!sample.data) return;
    size_t s1 = self->m_waveform_view->selection_start(), s2 = self->m_waveform_view->selection_end();
    if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); } else if (s1 > s2) std::swap(s1, s2);
    sample.data->fade_out(s1, s2, true); self->update_editor();
}

void InstrumentPanel::cb_normalize(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data); if (self->m_selected_sample < 0) return;
    SampleInstrument* sampler = static_cast<SampleInstrument*>(&self->m_engine.instrument(self->m_selected_instrument));
    auto& sample = sampler->get_sample(self->m_selected_sample); if (!sample.data) return;
    size_t s1 = self->m_waveform_view->selection_start(), s2 = self->m_waveform_view->selection_end();
    if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); } else if (s1 > s2) std::swap(s1, s2);
    sample.data->normalize(s1, s2); self->update_editor();
}

void InstrumentPanel::cb_adjust_vol(Fl_Widget*, void* data) {
    auto* self = static_cast<InstrumentPanel*>(data); if (self->m_selected_sample < 0) return;
    SampleInstrument* sampler = static_cast<SampleInstrument*>(&self->m_engine.instrument(self->m_selected_instrument));
    auto& sample = sampler->get_sample(self->m_selected_sample); if (!sample.data) return;
    size_t s1 = self->m_waveform_view->selection_start(), s2 = self->m_waveform_view->selection_end();
    if (s1 == s2) { s1 = 0; s2 = sample.data->left.size(); } else if (s1 > s2) std::swap(s1, s2);
    sample.data->adjust_volume(s1, s2, (float)self->m_vol_input->value()); self->update_editor();
}

void InstrumentPanel::cb_midi_ch(Fl_Widget* w, void* data) {
    InstrumentPanel* self = static_cast<InstrumentPanel*>(data);
    if (self->m_selected_instrument < 0) return;
    auto& inst = self->m_engine.instrument(self->m_selected_instrument);
    if (inst.type() == InstrumentType::Midi) static_cast<MidiInstrument*>(&inst)->set_channel((int)static_cast<Fl_Value_Input*>(w)->value() - 1);
}

void InstrumentPanel::cb_midi_pg(Fl_Widget* w, void* data) {
    InstrumentPanel* self = static_cast<InstrumentPanel*>(data);
    if (self->m_selected_instrument < 0) return;
    auto& inst = self->m_engine.instrument(self->m_selected_instrument);
    if (inst.type() == InstrumentType::Midi) static_cast<MidiInstrument*>(&inst)->set_program((int)static_cast<Fl_Value_Input*>(w)->value());
}

void InstrumentPanel::cb_detach(Fl_Widget*, void* data) {
    InstrumentPanel* self = static_cast<InstrumentPanel*>(data); Fl_Group* parent_grp = self->parent();
    if (self->m_detached_window) self->m_detached_window->show();
    else { self->m_detached_window = new DetachedWindow(800, 600, "Instruments", self, parent_grp); self->m_detached_window->show(); }
    self->hide();
}

} // namespace disgrace_ns
