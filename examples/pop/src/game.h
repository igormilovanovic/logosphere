// Shared game loop for the Prince of Persia example.
//
// The console frontend (main.cpp), the windowed GUI frontend (pop_app.cpp)
// and the demo recorder (tools/record_demo.cpp) all drive the identical
// simulation through this file, so there is exactly one place that decides
// what a tick does. Like the tiers it wraps (level.h, prince.h, combat.h),
// this only reaches world.h -- no GLFW, no core/engine.h -- so it stays
// safe to link into logosphere_core-only targets.
#pragma once

#include "combat.h"
#include "level.h"
#include "prince.h"
#include "world.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace pop {

// One tick of the countdown. The original gives you an hour; a clean run
// of this level takes well under two minutes of game time.
constexpr double kTickSeconds = 1.0;

// Flavor text a tick produced (a hit landed, a floor gave way, the game
// ended, ...). Frontends decide what to do with it: the console prints it,
// the GUI/demo tooling may ignore it.
using NarrateFn = std::function<void(const std::string&)>;

struct Game {
    Level lv;
    std::unique_ptr<World> world;
    Character prince;
    Character guard;
    Fighter prince_fighter;
    Fighter guard_fighter;
    uint32_t rng = 0x1234567u;
    bool finished = false;
};

// Build a fresh game on the shipped level, with the Prince and guard at
// their documented starting tiles.
Game new_game(uint32_t seed = 0x1234567u);

// One tick. Returns false once the game has ended.
bool tick(Game& g, Action action, CombatAction combat_action,
          const NarrateFn& narrate = {});

} // namespace pop
