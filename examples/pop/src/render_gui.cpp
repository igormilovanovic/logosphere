#include "render_gui.h"

#include "render_ascii.h"

#include <algorithm>
#include <cstdio>

namespace pop {

namespace {

struct Rgb { uint8_t r, g, b; };

// Palette follows the GitHub-dark colors already used in demo.svg, so the
// GUI and the recorded demo read as one visual family with the console's
// terminal recording.
constexpr Rgb kEmptyBg      {13, 17, 23};
constexpr Rgb kFloorBg      {48, 54, 61};
constexpr Rgb kWallBg       {33, 38, 45};
constexpr Rgb kSpikesBg     {255, 123, 114};
constexpr Rgb kLooseFloorBg {210, 153, 34};
constexpr Rgb kGateBg       {139, 148, 158};
constexpr Rgb kGateOpenEdge {126, 231, 135};
constexpr Rgb kExitBg       {210, 168, 255};
constexpr Rgb kPotionMarker {126, 231, 135};
constexpr Rgb kPlateMarker  {201, 209, 217};
constexpr Rgb kPrinceColor  {247, 129, 102};
constexpr Rgb kGuardColor   {121, 192, 255};
constexpr Rgb kHudText      {201, 209, 217};
constexpr Rgb kHudDim       {139, 148, 158};
constexpr Rgb kHudFilled    {126, 231, 135};
constexpr Rgb kHudEmpty     {45, 51, 59};

Rgb tile_bg(TileKind k) {
    switch (k) {
        case TileKind::Empty:         return kEmptyBg;
        case TileKind::Floor:         return kFloorBg;
        case TileKind::Wall:          return kWallBg;
        case TileKind::Spikes:        return kSpikesBg;
        case TileKind::LooseFloor:    return kLooseFloorBg;
        case TileKind::Gate:          return kGateBg;
        case TileKind::GateOpen:      return kEmptyBg;
        case TileKind::Exit:          return kExitBg;
        case TileKind::Potion:        return kFloorBg;
        case TileKind::PressurePlate: return kFloorBg;
    }
    return kEmptyBg;
}

// Potion/plate/open-gate share a background with a neighboring tile kind,
// so they get a small marker on top to stay distinguishable.
void draw_tile_marker(IDrawSurface& s, TileKind k, int px, int py, int tile_px) {
    const int inset = tile_px / 4;
    const int size = tile_px - inset * 2;
    if (k == TileKind::Potion) {
        s.fill_rect(px + inset, py + inset, size, size,
                    kPotionMarker.r, kPotionMarker.g, kPotionMarker.b, 255);
    } else if (k == TileKind::PressurePlate) {
        const int bar_h = std::max(2, tile_px / 5);
        s.fill_rect(px + inset, py + tile_px - inset - bar_h, size, bar_h,
                    kPlateMarker.r, kPlateMarker.g, kPlateMarker.b, 255);
    } else if (k == TileKind::GateOpen) {
        s.draw_rectangle_outline(px + 2, py + 2, tile_px - 4, tile_px - 4,
                                  kGateOpenEdge.r, kGateOpenEdge.g, kGateOpenEdge.b);
    }
}

void draw_character(IDrawSurface& s, const Character& c, int tile_px, Rgb color) {
    const int px = c.x * tile_px;
    const int py = c.y * tile_px;
    const int inset = tile_px / 6;
    s.fill_rect(px + inset, py + inset, tile_px - inset * 2, tile_px - inset * 2,
                color.r, color.g, color.b, 255);

    // A small white notch on the side the character faces.
    const int nose_w = std::max(2, tile_px / 6);
    const int nose_x = c.facing > 0 ? px + tile_px - inset - nose_w : px + inset;
    s.fill_rect(nose_x, py + tile_px / 2 - nose_w / 2, nose_w, nose_w, 255, 255, 255, 255);
}

void draw_health_bar(IDrawSurface& s, int x, int y, int width, int height,
                      int value, int max_value) {
    s.fill_rect(x, y, width, height, kHudEmpty.r, kHudEmpty.g, kHudEmpty.b, 255);
    if (max_value <= 0) return;
    const int clamped = std::max(0, std::min(value, max_value));
    const int filled = (clamped * width) / max_value;
    if (filled > 0) {
        s.fill_rect(x, y, filled, height, kHudFilled.r, kHudFilled.g, kHudFilled.b, 255);
    }
}

} // namespace

int gui_frame_width(const Level& lv, int tile_px) {
    return lv.w * tile_px;
}

int gui_frame_height(const Level& lv, int tile_px) {
    return lv.h * tile_px + kHudHeight;
}

void render_gui(IDrawSurface& surface, const Level& lv, const Character& prince,
                 const Character* guard, double seconds_remaining, int tile_px) {
    for (int y = 0; y < lv.h; ++y) {
        for (int x = 0; x < lv.w; ++x) {
            const TileKind k = lv.at(x, y);
            const Rgb bg = tile_bg(k);
            const int px = x * tile_px;
            const int py = y * tile_px;
            surface.fill_rect(px, py, tile_px, tile_px, bg.r, bg.g, bg.b, 255);
            draw_tile_marker(surface, k, px, py, tile_px);
        }
    }

    if (guard && guard->alive()) draw_character(surface, *guard, tile_px, kGuardColor);
    if (prince.alive()) draw_character(surface, prince, tile_px, kPrinceColor);

    const int hud_y = lv.h * tile_px;
    const int hud_w = lv.w * tile_px;
    surface.fill_rect(0, hud_y, hud_w, kHudHeight, kWallBg.r, kWallBg.g, kWallBg.b, 255);

    char buf[128];
    std::snprintf(buf, sizeof(buf), "Prince %d/%d  %s",
                  prince.hp, prince.max_hp, to_string(prince.state));
    surface.draw_string(8, hud_y + 14, buf, kHudText.r, kHudText.g, kHudText.b);
    draw_health_bar(surface, 8, hud_y + 24, 120, 8, prince.hp, prince.max_hp);

    if (guard && guard->alive()) {
        std::snprintf(buf, sizeof(buf), "Guard %d/%d", guard->hp, guard->max_hp);
        surface.draw_string(160, hud_y + 14, buf, kHudDim.r, kHudDim.g, kHudDim.b);
        draw_health_bar(surface, 160, hud_y + 24, 120, 8, guard->hp, guard->max_hp);
    }

    const std::string clock = "sands " + format_clock(seconds_remaining);
    surface.draw_string(hud_w - 90, hud_y + 14, clock, kHudText.r, kHudText.g, kHudText.b);
}

} // namespace pop
