#include "level.h"

#include <algorithm>

namespace pop {

bool is_passable(TileKind k) {
    switch (k) {
        case TileKind::Empty:
        case TileKind::Exit:
        case TileKind::GateOpen:
            return true;
        default:
            return false;
    }
}

bool is_support(TileKind k) {
    switch (k) {
        case TileKind::Floor:
        case TileKind::LooseFloor:
        case TileKind::Spikes:
        case TileKind::Potion:
        case TileKind::PressurePlate:
        case TileKind::Wall:
            return true;
        default:
            return false;
    }
}

bool is_passable_at(const Level& lv, int x, int y) {
    return is_passable(lv.at(x, y));
}

bool is_grounded_at(const Level& lv, int x, int y) {
    return is_support(lv.at(x, y + 1));
}

TileKind char_to_tile(char c) {
    switch (c) {
        case '#': return TileKind::Floor;
        case 'X': return TileKind::Wall;
        case '^': return TileKind::Spikes;
        case '~': return TileKind::LooseFloor;
        case '|': return TileKind::Gate;
        case '/': return TileKind::GateOpen;
        case 'E': return TileKind::Exit;
        case '!': return TileKind::Potion;
        case '_': return TileKind::PressurePlate;
        case '.':
        case ' ':
        default:  return TileKind::Empty;
    }
}

char tile_to_char(TileKind k) {
    switch (k) {
        case TileKind::Floor:         return '#';
        case TileKind::Wall:          return 'X';
        case TileKind::Spikes:        return '^';
        case TileKind::LooseFloor:    return '~';
        case TileKind::Gate:          return '|';
        case TileKind::GateOpen:      return '/';
        case TileKind::Exit:          return 'E';
        case TileKind::Potion:        return '!';
        case TileKind::PressurePlate: return '_';
        case TileKind::Empty:
        default:                      return '.';
    }
}

Level parse_level(const std::vector<std::string>& rows) {
    Level lv;
    lv.h = static_cast<int>(rows.size());
    for (const auto& r : rows) {
        lv.w = std::max(lv.w, static_cast<int>(r.size()));
    }
    lv.tiles.assign(static_cast<size_t>(lv.w) * static_cast<size_t>(lv.h), TileKind::Empty);

    for (int y = 0; y < lv.h; ++y) {
        const std::string& row = rows[static_cast<size_t>(y)];
        for (int x = 0; x < static_cast<int>(row.size()); ++x) {
            lv.set(x, y, char_to_tile(row[static_cast<size_t>(x)]));
        }
    }
    return lv;
}

void open_all_gates(Level& lv) {
    for (auto& t : lv.tiles) {
        if (t == TileKind::Gate) t = TileKind::GateOpen;
    }
}

} // namespace pop
