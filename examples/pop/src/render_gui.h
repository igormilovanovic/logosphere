// GUI rendering for the Prince of Persia example.
//
// The windowed counterpart to render_ascii.h: same inputs (Level,
// Character, seconds remaining), drawn instead of stringified. Only
// depends on IDrawSurface (include/logosphere/rendering/i_draw_surface.h),
// not on GLFW or Metal directly, so the exact same render_gui() call
// drives both the live Metal-backed window (pop_app.cpp) and the
// SVG-emitting demo recorder (tools/record_demo.cpp) -- one drawing, two
// backends.
#pragma once

#include "level.h"
#include "prince.h"

#include "logosphere/rendering/i_draw_surface.h"

namespace pop {

constexpr int kDefaultTilePx = 40;
constexpr int kHudHeight = 40;

// Pixel dimensions a full frame needs at the given tile size, so window
// creation and the SVG canvas agree with what render_gui() actually draws.
int gui_frame_width(const Level& lv, int tile_px = kDefaultTilePx);
int gui_frame_height(const Level& lv, int tile_px = kDefaultTilePx);

// Draws the tile grid, both characters, and a HUD strip below the level.
// Pass nullptr for guard when there is none.
void render_gui(IDrawSurface& surface, const Level& lv, const Character& prince,
                 const Character* guard, double seconds_remaining,
                 int tile_px = kDefaultTilePx);

} // namespace pop
