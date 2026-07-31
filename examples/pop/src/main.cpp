// Prince of Persia on Logosphere - headless/console example.
//
// The game loop only orchestrates: the pure tiers (level.h, prince.h,
// combat.h, render_ascii.h) decide what happens, and world.h projects it
// into the engine's knowledge graph, damage system and clock. Nothing
// here touches rendering, GLFW or Metal, so this builds under every
// LOGOSPHERE_PROFILE. See POP.md for the windowed macOS path.
//
// Usage:
//   ./pop [--seed N]

#include "combat.h"
#include "level.h"
#include "level_one.h"
#include "prince.h"
#include "render_ascii.h"
#include "world.h"

#include "core/game_time.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace {

// One tick of the countdown. The original gives you an hour; a clean run
// of this level takes well under two minutes of game time.
constexpr double kTickSeconds = 1.0;

void print_help() {
    std::cout <<
        "Commands (each takes an optional repeat count, e.g. \"d 6\"):\n"
        "  a  step left        d  step right\n"
        "  w  climb up         s  climb down / crouch\n"
        "  j  jump             .  wait\n"
        "  k  strike           p  parry\n"
        "  q  quit             ?  this help\n";
}

struct Game {
    pop::Level lv;
    std::unique_ptr<pop::World> world;
    pop::Character prince;
    pop::Character guard;
    pop::Fighter prince_fighter;
    pop::Fighter guard_fighter;
    uint32_t rng = 0x1234567u;
    bool finished = false;
};

void narrate(const std::string& text) {
    std::cout << "  " << text << "\n";
}

// One tick. Returns false once the game has ended.
bool tick(Game& g, pop::Action action, pop::CombatAction combat_action) {
    if (g.finished) return false;

    if (pop::in_sword_range(g.prince, g.guard)) {
        pop::ExchangeResult r = pop::combat_tick(
            g.prince, g.prince_fighter, g.guard, g.guard_fighter, combat_action, g.rng);

        if (r.prince_was_parried) narrate("your blade is turned aside");
        if (r.guard_was_parried) narrate("you parry the guard's swing");
        if (r.prince_hit_guard) {
            pop::apply_sword_damage(*g.world, g.world->guard_e, r.damage_to_guard);
            narrate("you cut the guard");
        }
        if (r.guard_hit_prince) {
            pop::apply_sword_damage(*g.world, g.world->prince_e, r.damage_to_prince);
            narrate("the guard's blade finds you");
        }
        if (r.guard_died) narrate("the guard falls");
        if (r.prince_died) narrate("you are slain");
    } else {
        pop::StepResult r = pop::step_character(
            g.prince, g.lv, action, g.world->cached_speed_scale);

        switch (r.outcome) {
            case pop::StepOutcome::Idle:
            case pop::StepOutcome::Moved:
            case pop::StepOutcome::Turned:
                break;
            case pop::StepOutcome::DrankPotion:
                pop::heal_prince_fully(*g.world);
                narrate("you drink the potion; your wounds close");
                break;
            case pop::StepOutcome::Landed:
                if (r.damage > 0) {
                    pop::apply_fall_damage(*g.world, r.damage);
                    narrate("you land hard (" + std::to_string(r.fall_height) +
                            " storeys), and favour a leg");
                } else {
                    narrate("you land lightly");
                }
                break;
            case pop::StepOutcome::Died:
                pop::apply_fall_damage(*g.world, r.damage);
                narrate("the fall kills you");
                break;
            default:
                narrate(pop::to_string(r.outcome));
                break;
        }
    }

    pop::advance_clock(kTickSeconds);
    pop::sync_character(*g.world, g.world->prince_e, g.prince);
    pop::sync_character(*g.world, g.world->guard_e, g.guard);

    if (g.prince.state == pop::PrinceState::Escaped) {
        std::cout << "\nYou slip through the door. The palace is behind you.\n";
        g.finished = true;
    } else if (!g.prince.alive()) {
        std::cout << "\nThe Prince is dead.\n";
        g.finished = true;
    } else if (pop::time_expired(*g.world)) {
        std::cout << "\nThe last grain falls. Out of time.\n";
        g.finished = true;
    }
    return !g.finished;
}

} // namespace

int main(int argc, char** argv) {
    Game g;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--seed" && i + 1 < argc) {
            g.rng = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        }
    }

    g.lv = pop::level_one();
    g.world = pop::create_world(g.lv);

    GameTime::reset();
    GameTime::initialize(0.0);

    g.prince.x = pop::kPrinceStartX;
    g.prince.y = pop::kPrinceStartY;
    g.prince.facing = 1;
    g.guard.x = pop::kGuardStartX;
    g.guard.y = pop::kGuardStartY;
    g.guard.facing = -1;
    g.guard_fighter.skill = pop::kGuardSkill;

    // The knowledge graph emits on every property and relation write once
    // a bus is attached, so the game observes rather than logs by hand.
    g.world->bus.damage().subscribe(
        [](const logosphere::ontology::DamageEvent& e) {
            std::cout << "  [kg] damage " << (e.damage_amount ? *e.damage_amount : 0.0f)
                      << " to entity " << (e.target_entity_id ? *e.target_entity_id : "?")
                      << "\n";
        });

    pop::sync_character(*g.world, g.world->prince_e, g.prince);
    pop::sync_character(*g.world, g.world->guard_e, g.guard);

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
            if (!tick(g, action, combat_action)) break;
        }

        std::cout << "\n" << pop::render_frame(g.lv, g.prince, &g.guard,
                                               pop::seconds_remaining(*g.world));
    }

    if (!g.finished) std::cout << "\nYou abandon the attempt.\n";
    return 0;
}
