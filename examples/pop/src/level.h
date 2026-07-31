// Level geometry for the Prince of Persia example.
//
// All functions in this header are pure (no engine, no KG). Headless-
// testable. The KG-coupled wrappers live in world.h.
//
// Tile model (see GAME_DESIGN.md S2). The level is one flat grid, laid
// out side-on: y grows downward, so "the tile below (x, y)" is
// (x, y + 1). Cells split into two disjoint roles, exactly as in the
// original game:
//
//   passable  - a character's body occupies this cell (Empty, Exit,
//               GateOpen)
//   support   - floor-level material a character stands on top of; a
//               character at (x, y) is grounded when (x, y + 1) is a
//               support tile (Floor, LooseFloor, Spikes, Potion,
//               PressurePlate, Wall)
//
// A character therefore never occupies a floor cell, which is what makes
// "walk along the floor and hit the spikes in it" fall out naturally.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pop {

// Values mirror the TileKind enum in schema/pop.yaml.
enum class TileKind : uint8_t {
    Empty,
    Floor,
    Wall,
    Spikes,
    LooseFloor,
    Gate,           // closed: blocks passage
    GateOpen,       // raised: passable
    Exit,
    Potion,
    PressurePlate,
};

struct Level {
    int w = 0;
    int h = 0;
    std::vector<TileKind> tiles;

    bool in_bounds(int x, int y) const {
        return x >= 0 && x < w && y >= 0 && y < h;
    }

    // Out-of-bounds reads return Wall, so the level is implicitly sealed
    // and callers never need their own bounds checks.
    TileKind at(int x, int y) const {
        if (!in_bounds(x, y)) return TileKind::Wall;
        return tiles[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
    }

    void set(int x, int y, TileKind k) {
        if (!in_bounds(x, y)) return;
        tiles[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = k;
    }
};

// A character's body can occupy this cell.
bool is_passable(TileKind k);

// Floor-level material a character can stand on top of.
bool is_support(TileKind k);

// Convenience wrappers reading through the grid.
bool is_passable_at(const Level& lv, int x, int y);
bool is_grounded_at(const Level& lv, int x, int y);   // support directly below

// ASCII map legend, shared by parse_level and the renderer:
//   '.' or ' ' Empty      '#' Floor        'X' Wall
//   '^' Spikes            '~' LooseFloor   '|' Gate (closed)
//   '/' GateOpen          'E' Exit         '!' Potion
//   '_' PressurePlate
// Unknown characters parse as Empty.
TileKind char_to_tile(char c);
char tile_to_char(TileKind k);

// Rows are laid out top to bottom. Short rows are padded with Empty, so
// trailing spaces in a source literal are optional.
Level parse_level(const std::vector<std::string>& rows);

// Raise every gate in the level. The vertical slice has one plate and one
// gate, so a plate opens all gates; GAME_DESIGN.md S4 notes the
// per-gate linkage a fuller game would want.
void open_all_gates(Level& lv);

} // namespace pop
