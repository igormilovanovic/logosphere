// SvgDrawSurface: an IDrawSurface backend that records draw calls as SVG
// markup instead of touching pixels. This lets render_gui() -- and
// anything else written against IDrawSurface -- produce vector frames for
// the recorded demo (tools/record_demo.cpp) using the exact same drawing
// calls the live Metal-backed window uses, so there is no separate "what
// does this look like" logic to keep in sync. Pure: no platform deps, so
// it builds under every LOGOSPHERE_PROFILE.
#pragma once

#include "logosphere/rendering/i_draw_surface.h"

#include <string>

namespace pop {

class SvgDrawSurface : public IDrawSurface {
public:
    void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) override;

    void draw_line(int x1, int y1, int x2, int y2,
                   uint8_t r, uint8_t g, uint8_t b) override;
    void draw_line(int x1, int y1, int x2, int y2,
                   uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;

    void draw_rectangle(int x, int y, int width, int height,
                        uint8_t r, uint8_t g, uint8_t b) override;
    void draw_rectangle_outline(int x, int y, int width, int height,
                                uint8_t r, uint8_t g, uint8_t b) override;
    void draw_rectangle_alpha(int x, int y, int width, int height,
                              uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;
    void draw_rect(int x, int y, int width, int height,
                   uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;
    void fill_rect(int x, int y, int width, int height,
                   uint8_t r, uint8_t g, uint8_t b, uint8_t a) override;

    void draw_text(int x, int y, const char* text,
                   uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) override;
    void draw_string(int x, int y, const std::string& text,
                     uint8_t r, uint8_t g, uint8_t b) override;
    void draw_string_scaled(int x, int y, const std::string& text,
                            uint8_t r, uint8_t g, uint8_t b, float scale) override;

    // Returns the markup accumulated since the last call and clears the
    // buffer, ready for the next frame.
    std::string take_frame();

private:
    void emit_rect(int x, int y, int width, int height,
                   uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool outline_only);

    std::string buf_;
};

} // namespace pop
