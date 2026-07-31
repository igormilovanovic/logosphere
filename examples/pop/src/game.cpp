#include "game.h"

#include "level_one.h"

#include "core/game_time.h"

namespace pop {

namespace {
void say(const NarrateFn& narrate, const std::string& text) {
    if (narrate) narrate(text);
}
} // namespace

Game new_game(uint32_t seed) {
    Game g;
    g.rng = seed;
    g.lv = level_one();
    g.world = create_world(g.lv);

    GameTime::reset();
    GameTime::initialize(0.0);

    g.prince.x = kPrinceStartX;
    g.prince.y = kPrinceStartY;
    g.prince.facing = 1;
    g.guard.x = kGuardStartX;
    g.guard.y = kGuardStartY;
    g.guard.facing = -1;
    g.guard_fighter.skill = kGuardSkill;

    sync_character(*g.world, g.world->prince_e, g.prince);
    sync_character(*g.world, g.world->guard_e, g.guard);
    return g;
}

bool tick(Game& g, Action action, CombatAction combat_action, const NarrateFn& narrate) {
    if (g.finished) return false;

    if (in_sword_range(g.prince, g.guard)) {
        ExchangeResult r = combat_tick(
            g.prince, g.prince_fighter, g.guard, g.guard_fighter, combat_action, g.rng);

        if (r.prince_was_parried) say(narrate, "your blade is turned aside");
        if (r.guard_was_parried) say(narrate, "you parry the guard's swing");
        if (r.prince_hit_guard) {
            apply_sword_damage(*g.world, g.world->guard_e, r.damage_to_guard);
            say(narrate, "you cut the guard");
        }
        if (r.guard_hit_prince) {
            apply_sword_damage(*g.world, g.world->prince_e, r.damage_to_prince);
            say(narrate, "the guard's blade finds you");
        }
        if (r.guard_died) say(narrate, "the guard falls");
        if (r.prince_died) say(narrate, "you are slain");
    } else {
        StepResult r = step_character(g.prince, g.lv, action, g.world->cached_speed_scale);

        switch (r.outcome) {
            case StepOutcome::Idle:
            case StepOutcome::Moved:
            case StepOutcome::Turned:
                break;
            case StepOutcome::DrankPotion:
                heal_prince_fully(*g.world);
                say(narrate, "you drink the potion; your wounds close");
                break;
            case StepOutcome::Landed:
                if (r.damage > 0) {
                    apply_fall_damage(*g.world, r.damage);
                    say(narrate, "you land hard (" + std::to_string(r.fall_height) +
                            " storeys), and favour a leg");
                } else {
                    say(narrate, "you land lightly");
                }
                break;
            case StepOutcome::Died:
                apply_fall_damage(*g.world, r.damage);
                say(narrate, "the fall kills you");
                break;
            default:
                say(narrate, to_string(r.outcome));
                break;
        }
    }

    advance_clock(kTickSeconds);
    sync_character(*g.world, g.world->prince_e, g.prince);
    sync_character(*g.world, g.world->guard_e, g.guard);

    if (g.prince.state == PrinceState::Escaped) {
        say(narrate, "You slip through the door. The palace is behind you.");
        g.finished = true;
    } else if (!g.prince.alive()) {
        say(narrate, "The Prince is dead.");
        g.finished = true;
    } else if (time_expired(*g.world)) {
        say(narrate, "The last grain falls. Out of time.");
        g.finished = true;
    }
    return !g.finished;
}

} // namespace pop
