// Prince of Persia on Logosphere - headless/console example.
//
// The game loop only orchestrates: the pure tiers (level.h, prince.h,
// combat.h, render_ascii.h) decide what happens, game.h drives one tick at
// a time, and world.h projects it into the engine's knowledge graph,
// damage system and clock. Nothing here touches rendering, GLFW or Metal,
// so this builds under every LOGOSPHERE_PROFILE. See POP.md for the
// windowed macOS path.
//
// Usage:
//   ./pop [--seed N]

#include "game.h"
#include "render_ascii.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void print_help() {
    std::cout <<
        "Commands (each takes an optional repeat count, e.g. \"d 6\"):\n"
        "  a  step left        d  step right\n"
        "  w  climb up         s  climb down / crouch\n"
        "  j  jump             .  wait\n"
        "  k  strike           p  parry\n"
        "  q  quit             ?  this help\n";
}

void narrate(const std::string& text) {
    std::cout << "  " << text << "\n";
}

} // namespace

int main(int argc, char** argv) {
    uint32_t seed = 0x1234567u;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--seed" && i + 1 < argc) {
            seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        }
    }

    pop::Game g = pop::new_game(seed);

    // The knowledge graph emits on every property and relation write once
    // a bus is attached, so the game observes rather than logs by hand.
    g.world->bus.damage().subscribe(
        [](const logosphere::ontology::DamageEvent& e) {
            std::cout << "  [kg] damage " << (e.damage_amount ? *e.damage_amount : 0.0f)
                      << " to entity " << (e.target_entity_id ? *e.target_entity_id : "?")
                      << "\n";
        });

    std::cout << "Prince of Persia on Logosphere\n\n";
    print_help();
    std::cout << "\n" << pop::render_frame(g.lv, g.prince, &g.guard,
                                           pop::seconds_remaining(*g.world));

    std::string line;
    while (!g.finished && std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd;
        if (!(in >> cmd)) continue;

        int count = 1;
        if (!(in >> count) || count < 1) count = 1;
        if (count > 200) count = 200;

        if (cmd == "q") break;
        if (cmd == "?") { print_help(); continue; }

        pop::Action action = pop::Action::None;
        pop::CombatAction combat_action = pop::CombatAction::Idle;
        if (cmd == "a")      action = pop::Action::Left;
        else if (cmd == "d") action = pop::Action::Right;
        else if (cmd == "w") action = pop::Action::Up;
        else if (cmd == "s") action = pop::Action::Down;
        else if (cmd == "j") action = pop::Action::Jump;
        else if (cmd == "k") { action = pop::Action::Attack; combat_action = pop::CombatAction::Strike; }
        else if (cmd == "p") { action = pop::Action::Block;  combat_action = pop::CombatAction::Parry; }
        else if (cmd == ".") action = pop::Action::None;
        else {
            std::cout << "Unknown command \"" << cmd << "\". Type ? for help.\n";
            continue;
        }

        for (int i = 0; i < count; ++i) {
            if (!pop::tick(g, action, combat_action, narrate)) break;
        }

        std::cout << "\n" << pop::render_frame(g.lv, g.prince, &g.guard,
                                               pop::seconds_remaining(*g.world));
    }

    if (!g.finished) std::cout << "\nYou abandon the attempt.\n";
    return 0;
}
