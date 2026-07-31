// Prince of Persia level-geometry tests.
//
// Pure tier: links nothing at all (see the add_pop_pure_test helper in
// the root CMakeLists.txt), mirroring test_logotron_cycle.
//
// Usage:
//   ./build/test_pop_level

#include "level.h"
#include "level_one.h"

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

void test_parse_dimensions() {
    Level lv = parse_level({"###", "...", "XXX"});
    ASSERT(lv.w == 3, "width from longest row");
    ASSERT(lv.h == 3, "height from row count");
    ASSERT(lv.tiles.size() == 9, "grid sized w*h");
}

void test_short_rows_pad_with_empty() {
    Level lv = parse_level({"#####", "#"});
    ASSERT(lv.w == 5, "width taken from the longest row");
    ASSERT(lv.at(0, 1) == TileKind::Floor, "explicit character kept");
    ASSERT(lv.at(4, 1) == TileKind::Empty, "short row padded with Empty");
}

void test_legend_round_trip() {
    const TileKind kinds[] = {
        TileKind::Empty, TileKind::Floor, TileKind::Wall, TileKind::Spikes,
        TileKind::LooseFloor, TileKind::Gate, TileKind::GateOpen,
        TileKind::Exit, TileKind::Potion, TileKind::PressurePlate,
    };
    for (TileKind k : kinds) {
        ASSERT(char_to_tile(tile_to_char(k)) == k, "legend round-trips");
    }
    ASSERT(char_to_tile(' ') == TileKind::Empty, "space parses as Empty");
    ASSERT(char_to_tile('?') == TileKind::Empty, "unknown parses as Empty");
}

void test_out_of_bounds_reads_as_wall() {
    Level lv = parse_level({"..."});
    ASSERT(lv.at(-1, 0) == TileKind::Wall, "left of the grid is Wall");
    ASSERT(lv.at(3, 0) == TileKind::Wall, "right of the grid is Wall");
    ASSERT(lv.at(0, -1) == TileKind::Wall, "above the grid is Wall");
    ASSERT(lv.at(0, 9) == TileKind::Wall, "below the grid is Wall");
    ASSERT(!lv.in_bounds(3, 0), "in_bounds agrees");
}

void test_set_ignores_out_of_bounds() {
    Level lv = parse_level({"..."});
    lv.set(99, 99, TileKind::Floor);   // must not corrupt or crash
    ASSERT(lv.tiles.size() == 3, "grid untouched by out-of-bounds set");
}

void test_passable_and_support_are_disjoint() {
    const TileKind kinds[] = {
        TileKind::Empty, TileKind::Floor, TileKind::Wall, TileKind::Spikes,
        TileKind::LooseFloor, TileKind::Gate, TileKind::GateOpen,
        TileKind::Exit, TileKind::Potion, TileKind::PressurePlate,
    };
    for (TileKind k : kinds) {
        ASSERT(!(is_passable(k) && is_support(k)),
               "a tile is never both passable and support");
    }
    ASSERT(is_passable(TileKind::Empty), "Empty is passable");
    ASSERT(is_passable(TileKind::Exit), "Exit is passable");
    ASSERT(is_passable(TileKind::GateOpen), "an open gate is passable");
    ASSERT(!is_passable(TileKind::Gate), "a closed gate blocks");
    ASSERT(is_support(TileKind::Floor), "Floor supports");
    ASSERT(is_support(TileKind::Spikes), "Spikes are floor-level material");
    ASSERT(is_support(TileKind::Potion), "a potion rests on solid ground");
}

void test_grounded_reads_the_tile_below() {
    Level lv = parse_level({
        "...",
        "#.#",
    });
    ASSERT(is_grounded_at(lv, 0, 0), "supported by the floor below");
    ASSERT(!is_grounded_at(lv, 1, 0), "nothing below the gap");
    ASSERT(is_grounded_at(lv, 0, 1), "out-of-grid floor counts as Wall support");
}

void test_open_all_gates() {
    Level lv = parse_level({"|.|"});
    open_all_gates(lv);
    ASSERT(lv.at(0, 0) == TileKind::GateOpen, "first gate raised");
    ASSERT(lv.at(2, 0) == TileKind::GateOpen, "second gate raised");
    ASSERT(lv.at(1, 0) == TileKind::Empty, "non-gate tiles untouched");
}

void test_shipped_level_shape() {
    Level lv = level_one();
    ASSERT(lv.w == 32, "level one is 32 tiles wide");
    ASSERT(lv.h == 4, "level one is 4 tiles tall");

    for (const auto& row : level_one_rows()) {
        ASSERT(static_cast<int>(row.size()) == lv.w, "every row is full width");
    }

    // The features the route depends on, at the columns level_one.h documents.
    ASSERT(is_grounded_at(lv, kPrinceStartX, kPrinceStartY), "the Prince starts on solid ground");
    ASSERT(lv.at(4, 2) == TileKind::Empty, "col 4 is the gap");
    ASSERT(lv.at(4, 3) == TileKind::Spikes, "spikes wait under the gap");
    ASSERT(lv.at(9, 2) == TileKind::LooseFloor, "col 9 is the loose tile");
    ASSERT(lv.at(15, 2) == TileKind::Potion, "col 15 holds the potion");
    ASSERT(lv.at(18, 2) == TileKind::PressurePlate, "col 18 is the plate");
    ASSERT(lv.at(21, 1) == TileKind::Gate, "col 21 is the gate, closed");
    ASSERT(lv.at(30, 1) == TileKind::Exit, "col 30 is the exit");
    ASSERT(is_grounded_at(lv, kGuardStartX, kGuardStartY), "the guard stands on solid ground");
}

int main() {
    std::cout << "=== Prince of Persia: level tests ===" << std::endl;

    test_parse_dimensions();
    test_short_rows_pad_with_empty();
    test_legend_round_trip();
    test_out_of_bounds_reads_as_wall();
    test_set_ignores_out_of_bounds();
    test_passable_and_support_are_disjoint();
    test_grounded_reads_the_tile_below();
    test_open_all_gates();
    test_shipped_level_shape();

    std::cout << std::endl;
    std::cout << tests_passed << " passed, " << tests_failed << " failed" << std::endl;
    return tests_failed > 0 ? 1 : 0;
}
