#include "combat.h"

#include <algorithm>
#include <cstdlib>

namespace pop {

uint32_t next_rand(uint32_t& state) {
    // xorshift32. Any non-zero seed works; zero would be a fixed point.
    if (state == 0) state = 0x9E3779B9u;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

bool in_sword_range(const Character& a, const Character& b) {
    return a.alive() && b.alive() && a.y == b.y && std::abs(a.x - b.x) == 1;
}

CombatAction choose_guard_action(const Character& guard, const Fighter& gf, uint32_t& rng) {
    if (!guard.alive() || !gf.ready()) return CombatAction::Idle;

    const uint32_t roll = next_rand(rng) % 100u;
    const uint32_t parry_chance =
        static_cast<uint32_t>(std::max(0.0f, std::min(1.0f, gf.skill)) * 50.0f);
    return (roll < parry_chance) ? CombatAction::Parry : CombatAction::Strike;
}

namespace {

void tick_recovery(Fighter& f) {
    if (f.recovery_ticks > 0) {
        f.recovery_ticks -= 1;
        if (f.recovery_ticks == 0) f.state = CombatState::Ready;
    }
}

void commit(Fighter& f, int ticks) {
    f.state = CombatState::Recovering;
    f.recovery_ticks = ticks;
}

// Combat deaths are decided here so hp and state never disagree.
bool wound(Character& c, int amount) {
    c.hp = std::max(0, c.hp - amount);
    if (c.hp == 0) {
        c.state = PrinceState::Dead;
        return true;
    }
    return false;
}

} // namespace

ExchangeResult combat_tick(Character& prince, Fighter& pf,
                           Character& guard, Fighter& gf,
                           CombatAction prince_action, uint32_t& rng) {
    ExchangeResult r;
    if (!prince.alive() || !guard.alive()) return r;

    prince.state = PrinceState::Fighting;
    guard.state = PrinceState::Fighting;

    tick_recovery(pf);
    tick_recovery(gf);

    const bool prince_could_act = pf.ready();
    r.guard_action = choose_guard_action(guard, gf, rng);

    // Both swings resolve against the opponent's stance this tick, so a
    // mutual strike trades blows.
    if (prince_action == CombatAction::Strike && prince_could_act) {
        if (r.guard_action == CombatAction::Parry) {
            r.prince_was_parried = true;
            commit(pf, kParriedRecoveryTicks);
        } else {
            r.prince_hit_guard = true;
            r.damage_to_guard = kStrikeDamage;
            r.guard_died = wound(guard, kStrikeDamage);
            commit(pf, kHitRecoveryTicks);
        }
    }

    if (r.guard_action == CombatAction::Strike) {
        if (prince_action == CombatAction::Parry && prince_could_act) {
            r.guard_was_parried = true;
            commit(gf, kParriedRecoveryTicks);
        } else {
            r.guard_hit_prince = true;
            r.damage_to_prince = kStrikeDamage;
            r.prince_died = wound(prince, kStrikeDamage);
            commit(gf, kHitRecoveryTicks);
        }
    }

    return r;
}

const char* to_string(CombatAction a) {
    switch (a) {
        case CombatAction::Strike: return "strike";
        case CombatAction::Parry:  return "parry";
        case CombatAction::Idle:
        default:                   return "wait";
    }
}

} // namespace pop
