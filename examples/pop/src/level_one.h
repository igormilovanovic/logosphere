// The shipped level for the Prince of Persia example.
//
// Header-only and pure, so the game and the tests build the identical
// level and the test suite can assert that what ships is winnable.
//
// Reading the map (legend in level.h): the Prince walks along row 1 and
// stands on the material in row 2. Row 3 is bedrock, except for the
// spikes waiting under the gap at column 4.
//
//   col  4   gap in the floor, spikes below - must be jumped
//   col  9   loose tile: crossable at full speed, not while limping
//   col 15   potion
//   col 18   pressure plate, raises the gate
//   col 21   gate, closed until the plate is stepped on
//   col 27   guard
//   col 30   exit
//
// See GAME_DESIGN.md S7 for the intended route.
#pragma once

#include "level.h"

#include <string>
#include <vector>

namespace pop {

constexpr int kPrinceStartX = 1;
constexpr int kPrinceStartY = 1;
constexpr int kGuardStartX  = 27;
constexpr int kGuardStartY  = 1;
constexpr float kGuardSkill = 0.6f;

inline std::vector<std::string> level_one_rows() {
    return {
        "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
        "X....................|........EX",
        "X###.####~#####!##_##X#########X",
        "X###^##########################X",
    };
}

inline Level level_one() {
    return parse_level(level_one_rows());
}

} // namespace pop
