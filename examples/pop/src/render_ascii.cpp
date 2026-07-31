#include "render_ascii.h"

#include <algorithm>
#include <cstdio>

namespace pop {

std::string render_level(const Level& lv, const Character& prince, const Character* guard) {
    std::string out;
    out.reserve(static_cast<size_t>(lv.w + 1) * static_cast<size_t>(lv.h));

    for (int y = 0; y < lv.h; ++y) {
        for (int x = 0; x < lv.w; ++x) {
            char c = tile_to_char(lv.at(x, y));
            if (guard && guard->alive() && guard->x == x && guard->y == y) {
                c = 'G';
            }
            if (prince.alive() && prince.x == x && prince.y == y) {
                c = '@';
            }
            out.push_back(c);
        }
        out.push_back('\n');
    }
    return out;
}

std::string format_clock(double seconds_remaining) {
    if (seconds_remaining < 0.0) seconds_remaining = 0.0;
    const int total = static_cast<int>(seconds_remaining);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", total / 60, total % 60);
    return std::string(buf);
}

std::string render_bar(int value, int max_value, int width) {
    if (max_value <= 0 || width <= 0) return std::string("[]");
    const int clamped = std::max(0, std::min(value, max_value));
    const int filled = (clamped * width) / max_value;

    std::string out;
    out.reserve(static_cast<size_t>(width) + 2);
    out.push_back('[');
    out.append(static_cast<size_t>(filled), '#');
    out.append(static_cast<size_t>(width - filled), '.');
    out.push_back(']');
    return out;
}

std::string render_hud(const Character& prince, const Character* guard,
                       double seconds_remaining) {
    char buf[192];
    std::string out;

    std::snprintf(buf, sizeof(buf), "Prince %s %3d/%-3d   %-13s   sands %s\n",
                  render_bar(prince.hp, prince.max_hp, 10).c_str(),
                  prince.hp, prince.max_hp,
                  to_string(prince.state),
                  format_clock(seconds_remaining).c_str());
    out += buf;

    if (guard && guard->alive()) {
        std::snprintf(buf, sizeof(buf), "Guard  %s %3d/%-3d\n",
                      render_bar(guard->hp, guard->max_hp, 10).c_str(),
                      guard->hp, guard->max_hp);
        out += buf;
    }
    return out;
}

std::string render_frame(const Level& lv, const Character& prince,
                         const Character* guard, double seconds_remaining) {
    return render_level(lv, prince, guard) +
           render_hud(prince, guard, seconds_remaining);
}

} // namespace pop
