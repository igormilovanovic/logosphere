// Prince of Persia GUI-rendering tests.
//
// render_gui() only depends on IDrawSurface, so it's testable without an
// engine, a window, or Metal: SvgDrawSurface stands in as a pure test
// double. This is the same code path pop_gui (the live window) and
// pop_record_demo (the recorded demo) both draw through -- see
// render_gui.h. Links nothing but the two pop sources under test plus the
// project's IDrawSurface header.
//
// Usage:
//   ./build/test_pop_render_gui

#include "level.h"
#include "level_one.h"
#include "prince.h"
#include "render_gui.h"
#include "svg_draw_surface.h"

#include <iostream>

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "FAIL: " << msg << std::endl; \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while (0)

static int tests_passed = 0;
static int tests_failed = 0;

using namespace pop;

void test_frame_dimensions_match_level() {
    Level lv = level_one();
    ASSERT(gui_frame_width(lv, 10) == lv.w * 10, "width scales with tile size");
    ASSERT(gui_frame_height(lv, 10) == lv.h * 10 + kHudHeight, "height adds the HUD strip");
}

void test_renders_every_tile_kind_without_crashing() {
    // One of each TileKind, so a color/marker mapping gap can't hide.
    Level lv = parse_level({
        ".#X^~",
        "|/E!_",
    });
    Character prince;
    prince.x = 0;
    prince.y = 0;

    SvgDrawSurface surface;
    render_gui(surface, lv, prince, nullptr, 42.0);
    const std::string frame = surface.take_frame();
    ASSERT(!frame.empty(), "produces markup for a level covering every tile kind");
    ASSERT(frame.find("<rect") != std::string::npos, "draws tiles as rects");
}

void test_renders_with_and_without_guard() {
    Level lv = level_one();
    Character prince = Character();
    prince.x = kPrinceStartX;
    prince.y = kPrinceStartY;
    Character guard = Character();
    guard.x = kGuardStartX;
    guard.y = kGuardStartY;

    SvgDrawSurface with_guard;
    render_gui(with_guard, lv, prince, &guard, 3599.0);
    ASSERT(!with_guard.take_frame().empty(), "renders with a living guard");

    SvgDrawSurface no_guard;
    render_gui(no_guard, lv, prince, nullptr, 0.0);
    ASSERT(!no_guard.take_frame().empty(), "renders with guard == nullptr");

    Character dead_guard = guard;
    dead_guard.state = PrinceState::Dead;
    SvgDrawSurface dead_guard_surface;
    render_gui(dead_guard_surface, lv, prince, &dead_guard, 0.0);
    ASSERT(!dead_guard_surface.take_frame().empty(), "renders with a dead guard");
}

void test_dead_prince_still_renders_the_level() {
    Level lv = level_one();
    Character prince = Character();
    prince.state = PrinceState::Dead;

    SvgDrawSurface surface;
    render_gui(surface, lv, prince, nullptr, 0.0);
    const std::string frame = surface.take_frame();
    ASSERT(!frame.empty(), "still draws the level and HUD when the Prince has died");
}

int main() {
    std::cout << "=== Prince of Persia GUI-rendering tests ===\n";

    test_frame_dimensions_match_level();
    test_renders_every_tile_kind_without_crashing();
    test_renders_with_and_without_guard();
    test_dead_prince_still_renders_the_level();

    std::cout << "\n" << tests_passed << " passed, " << tests_failed << " failed\n";
    return tests_failed == 0 ? 0 : 1;
}
