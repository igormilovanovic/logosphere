// Text rendering for the Prince of Persia example.
//
// All functions in this header are pure (no engine, no KG, no I/O).
// Headless-testable. The caller decides where the string goes.
//
// This is the whole "renderer" for the headless build. POP.md describes
// what the macOS windowed build would put here instead.
#pragma once

#include "combat.h"
#include "level.h"
#include "prince.h"

#include <string>

namespace pop {

// The level grid with the characters overlaid: '@' is the Prince, 'G' a
// living guard. Pass nullptr for guard when there is none. Ends with a
// newline.
std::string render_level(const Level& lv, const Character& prince, const Character* guard);

// "59:58" for 3598 seconds. Clamps at "00:00".
std::string format_clock(double seconds_remaining);

// A bar like "[########..]" with the given width.
std::string render_bar(int value, int max_value, int width);

// Status lines: the Prince's health, state and the countdown, plus the
// guard's health while a fight is live. Ends with a newline.
std::string render_hud(const Character& prince, const Character* guard,
                       double seconds_remaining);

// Everything the player sees for one tick.
std::string render_frame(const Level& lv, const Character& prince,
                         const Character* guard, double seconds_remaining);

} // namespace pop
