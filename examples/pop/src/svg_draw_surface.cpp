#include "svg_draw_surface.h"

#include <cstdio>

namespace pop {

namespace {

std::string hex(uint8_t r, uint8_t g, uint8_t b) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    return std::string(buf);
}

std::string xml_escape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out.push_back(c);
        }
    }
    return out;
}

} // namespace

void SvgDrawSurface::emit_rect(int x, int y, int width, int height,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a, bool outline_only) {
    char buf[256];
    if (outline_only) {
        std::snprintf(buf, sizeof(buf),
            "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"none\" stroke=\"%s\"/>",
            x, y, width, height, hex(r, g, b).c_str());
    } else if (a >= 255) {
        std::snprintf(buf, sizeof(buf),
            "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"%s\"/>",
            x, y, width, height, hex(r, g, b).c_str());
    } else {
        std::snprintf(buf, sizeof(buf),
            "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"%s\" fill-opacity=\"%.3f\"/>",
            x, y, width, height, hex(r, g, b).c_str(), a / 255.0);
    }
    buf_ += buf;
}

void SvgDrawSurface::set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    emit_rect(x, y, 1, 1, r, g, b, a, false);
}

void SvgDrawSurface::draw_line(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b) {
    draw_line(x1, y1, x2, y2, r, g, b, 255);
}

void SvgDrawSurface::draw_line(int x1, int y1, int x2, int y2,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    char buf[192];
    std::snprintf(buf, sizeof(buf),
        "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"%s\" stroke-opacity=\"%.3f\"/>",
        x1, y1, x2, y2, hex(r, g, b).c_str(), a / 255.0);
    buf_ += buf;
}

void SvgDrawSurface::draw_rectangle(int x, int y, int width, int height,
                                    uint8_t r, uint8_t g, uint8_t b) {
    emit_rect(x, y, width, height, r, g, b, 255, false);
}

void SvgDrawSurface::draw_rectangle_outline(int x, int y, int width, int height,
                                            uint8_t r, uint8_t g, uint8_t b) {
    emit_rect(x, y, width, height, r, g, b, 255, true);
}

void SvgDrawSurface::draw_rectangle_alpha(int x, int y, int width, int height,
                                          uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    emit_rect(x, y, width, height, r, g, b, a, false);
}

void SvgDrawSurface::draw_rect(int x, int y, int width, int height,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    emit_rect(x, y, width, height, r, g, b, a, false);
}

void SvgDrawSurface::fill_rect(int x, int y, int width, int height,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    emit_rect(x, y, width, height, r, g, b, a, false);
}

void SvgDrawSurface::draw_text(int x, int y, const char* text,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    (void)a;
    draw_string(x, y, std::string(text ? text : ""), r, g, b);
}

void SvgDrawSurface::draw_string(int x, int y, const std::string& text,
                                 uint8_t r, uint8_t g, uint8_t b) {
    char buf[192];
    std::snprintf(buf, sizeof(buf),
        "<text x=\"%d\" y=\"%d\" fill=\"%s\" "
        "font-family=\"SFMono-Regular,Consolas,Liberation Mono,Menlo,monospace\" "
        "font-size=\"13\">",
        x, y, hex(r, g, b).c_str());
    buf_ += buf;
    buf_ += xml_escape(text);
    buf_ += "</text>";
}

void SvgDrawSurface::draw_string_scaled(int x, int y, const std::string& text,
                                        uint8_t r, uint8_t g, uint8_t b, float scale) {
    char buf[224];
    std::snprintf(buf, sizeof(buf),
        "<text x=\"%d\" y=\"%d\" fill=\"%s\" "
        "font-family=\"SFMono-Regular,Consolas,Liberation Mono,Menlo,monospace\" "
        "font-size=\"%.1f\">",
        x, y, hex(r, g, b).c_str(), 13.0f * scale);
    buf_ += buf;
    buf_ += xml_escape(text);
    buf_ += "</text>";
}

std::string SvgDrawSurface::take_frame() {
    std::string out;
    out.swap(buf_);
    return out;
}

} // namespace pop
