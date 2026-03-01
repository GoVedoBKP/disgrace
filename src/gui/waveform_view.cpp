#include "waveform_view.h"
#include <FL/fl_draw.H>
#include <FL/Fl.H>
#include <algorithm>
#include <cmath>

namespace disgrace_ns
{

    disgrace_ns::WaveformView::WaveformView(int x, int y,
                               int w, int h)
    : Fl_Widget(x,y,w,h)
    {
    }

    void disgrace_ns::WaveformView::set_sample(
        ::std::shared_ptr<disgrace_ns::SampleData> s)
    {
        m_sample = s;
        m_sel_start = 0;
        m_sel_end = 0;
        m_zoom = 1.0;
        m_offset = 0;
        redraw();
    }

    void disgrace_ns::WaveformView::get_view_range(size_t& start, size_t& end) {
        if (!m_sample || m_sample->left.empty()) {
            start = 0; end = 0; return;
        }
        size_t total = m_sample->left.size();
        size_t view_size = (size_t)(total / m_zoom);
        start = m_offset;
        end = std::min(total, start + view_size);
    }

    void disgrace_ns::WaveformView::draw()
    {
        fl_push_clip(x(), y(), w(), h());
        fl_color(20,20,20);
        fl_rectf(x(), y(), w(), h());

        if (!m_sample || m_sample->left.empty()) {
            fl_color(60, 60, 60);
            fl_line(x(), y() + h()/2, x() + w(), y() + h()/2);
            fl_pop_clip();
            return;
        }

        size_t v_start, v_end;
        get_view_range(v_start, v_end);
        size_t v_size = v_end - v_start;
        if (v_size == 0) {
            fl_color(60, 60, 60);
            fl_line(x(), y() + h()/2, x() + w(), y() + h()/2);
            fl_pop_clip();
            return;
        }

        bool has_right = !m_sample->right.empty() && m_sample->right.size() == m_sample->left.size();
        
        // Draw Selection background
        if (m_sel_start != m_sel_end) {
            size_t s1 = std::min(m_sel_start, m_sel_end);
            size_t s2 = std::max(m_sel_start, m_sel_end);
            
            if (!(s2 < v_start || s1 > v_end)) {
                size_t ds1 = std::max(s1, v_start);
                size_t ds2 = std::min(s2, v_end);
                
                int x1 = (int)((double)(ds1 - v_start) * w() / v_size);
                int x2 = (int)((double)(ds2 - v_start) * w() / v_size);
                
                fl_color(60, 60, 100);
                fl_rectf(x() + x1, y(), x2 - x1, h());
            }
        }

        fl_color(m_color);

        if (!has_right || m_mode == ChannelMode::Left) {
            // Mono or Left Only - Draw center line
            fl_color(60, 60, 60);
            fl_line(x(), y() + h()/2, x() + w(), y() + h()/2);
            fl_color(m_color);

            for (int px = 0; px < w(); ++px) {
                size_t idx = v_start + (px * v_size) / w();
                if (idx >= v_end) break;
                float v = m_sample->left[idx];
                int yy = y() + h()/2;
                int amp = int(v * (h()/2 - 5));
                if (amp != 0) fl_line(x()+px, yy - amp, x()+px, yy + amp);
            }
            fl_draw("L", x() + 5, y() + 15);
        } else if (m_mode == ChannelMode::Right) {
            // Right Only - Draw center line
            fl_color(60, 60, 60);
            fl_line(x(), y() + h()/2, x() + w(), y() + h()/2);
            fl_color(m_color);

            for (int px = 0; px < w(); ++px) {
                size_t idx = v_start + (px * v_size) / w();
                if (idx >= v_end) break;
                float v = m_sample->right[idx];
                int yy = y() + h()/2;
                int amp = int(v * (h()/2 - 5));
                if (amp != 0) fl_line(x()+px, yy - amp, x()+px, yy + amp);
            }
            fl_draw("R", x() + 5, y() + 15);
        } else {
            // Stereo Both
            int half_h = h() / 2;
            
            // Draw center lines for both
            fl_color(60, 60, 60);
            fl_line(x(), y() + half_h / 2, x() + w(), y() + half_h / 2);
            fl_line(x(), y() + half_h + half_h / 2, x() + w(), y() + half_h + half_h / 2);
            
            fl_color(m_color);
            // Left
            for (int px = 0; px < w(); ++px) {
                size_t idx = v_start + (px * v_size) / w();
                if (idx >= v_end) break;
                float v = m_sample->left[idx];
                int yy = y() + half_h / 2;
                int amp = int(v * (half_h / 2 - 5));
                if (amp != 0) fl_line(x()+px, yy - amp, x()+px, yy + amp);
            }
            fl_draw("L", x() + 5, y() + 15);
            // Right
            for (int px = 0; px < w(); ++px) {
                size_t idx = v_start + (px * v_size) / w();
                if (idx >= v_end) break;
                float v = m_sample->right[idx];
                int yy = y() + half_h + half_h / 2;
                int amp = int(v * (half_h / 2 - 5));
                if (amp != 0) fl_line(x()+px, yy - amp, x()+px, yy + amp);
            }
            fl_draw("R", x() + 5, y() + half_h + 15);
            
            fl_color(60, 60, 60);
            fl_line(x(), y() + half_h, x() + w(), y() + half_h);
        }

        // Draw Time Markers
        fl_color(120, 120, 120);
        fl_font(FL_HELVETICA, 8);
        
        double sr = (double)m_sample->sample_rate;
        double view_start_sec = (double)v_start / sr;
        double view_end_sec = (double)v_end / sr;
        double sec_per_pixel = (view_end_sec - view_start_sec) / w();

        for (int px = 0; px < w(); px += 40) {
            double cur_sec = view_start_sec + px * sec_per_pixel;
            double tick_sec = floor(cur_sec * 10.0 + 0.5) / 10.0;
            int tick_px = (int)((tick_sec - view_start_sec) / sec_per_pixel);
            
            if (tick_px >= 0 && tick_px < w()) {
                bool major = (fmod(tick_sec + 0.001, 1.0) < 0.01);
                int tick_h = major ? 10 : 5;
                fl_line(x() + tick_px, y() + h(), x() + tick_px, y() + h() - tick_h);
                
                if (major) {
                    char buf[16];
                    snprintf(buf, 16, "%.1fs", tick_sec);
                    fl_draw(buf, x() + tick_px + 2, y() + h() - 2);
                }
            }
        }

        fl_pop_clip();
    }

    int disgrace_ns::WaveformView::handle(int event) {
        if (!m_sample || m_sample->left.empty()) return 0;

        size_t v_start, v_end;
        get_view_range(v_start, v_end);
        size_t v_size = v_end - v_start;

        switch(event) {
            case FL_PUSH: {
                int mx = Fl::event_x() - x();
                m_sel_start = v_start + (size_t)((double)mx * v_size / w());
                m_sel_end = m_sel_start;
                redraw();
                return 1;
            }
            case FL_DRAG: {
                int mx = Fl::event_x() - x();
                mx = std::max(0, std::min(w(), mx));
                m_sel_end = v_start + (size_t)((double)mx * v_size / w());
                redraw();
                return 1;
            }
        }
        return Fl_Widget::handle(event);
    }

    void disgrace_ns::WaveformView::zoom_in() {
        if (!m_sample || m_sample->left.empty()) return;
        m_zoom *= 1.5;
        redraw();
    }

    void disgrace_ns::WaveformView::zoom_out() {
        if (!m_sample || m_sample->left.empty()) return;
        m_zoom /= 1.5;
        if (m_zoom < 1.0) m_zoom = 1.0;
        size_t total = m_sample->left.size();
        size_t view_size = (size_t)(total / m_zoom);
        if (m_offset + view_size > total) {
            m_offset = (total > view_size) ? total - view_size : 0;
        }
        redraw();
    }

    void disgrace_ns::WaveformView::view_all() {
        m_zoom = 1.0;
        m_offset = 0;
        redraw();
    }

    void disgrace_ns::WaveformView::view_selection() {
        if (!m_sample || m_sample->left.empty() || m_sel_start == m_sel_end) return;
        size_t s1 = std::min(m_sel_start, m_sel_end);
        size_t s2 = std::max(m_sel_start, m_sel_end);
        size_t sel_size = s2 - s1;
        if (sel_size < 10) return; // Don't zoom too much
        
        m_zoom = (double)m_sample->left.size() / sel_size;
        m_offset = s1;
        redraw();
    }

} // namespace disgrace_ns
