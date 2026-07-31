// Sword fighting for the Prince of Persia example.
//
// All functions in this header are pure (no engine, no KG). Headless-
// testable. The KG-coupled wrappers live in world.h.
//
// The guard's choices come from a caller-owned xorshift state rather than
// a global RNG, so a fight replays identically from the same seed and
// tests can assert exact outcomes. See GAME_DESIGN.md S5.
//
// Health lives on Character (prince.h), not here, so movement and combat
// share one source of truth. Fighter carries only what a duel needs.
// world.h mirrors every point of damage into the engine's DamageSystem so
// the knowledge graph and the capability rules see it too.
#pragma once

#include "prince.h"

#include <cstdint>

namespace pop {

enum class CombatAction {
    Idle,
    Strike,
    Parry,
};

enum class CombatState {
    Ready,
    Recovering,   // committed to a swing, cannot act
};

struct Fighter {
    float skill = 0.5f;             // 0..1, drives how often the guard parries
    CombatState state = CombatState::Ready;
    int recovery_ticks = 0;

    bool ready() const { return state == CombatState::Ready && recovery_ticks == 0; }
};

// Tuning. See GAME_DESIGN.md S5.
constexpr int kStrikeDamage         = 34;   // three clean hits kill
constexpr int kHitRecoveryTicks     = 1;    // recovery after a landed blow
constexpr int kParriedRecoveryTicks = 2;    // being parried leaves you open

struct ExchangeResult {
    CombatAction guard_action = CombatAction::Idle;
    bool prince_hit_guard = false;
    bool guard_hit_prince = false;
    bool prince_was_parried = false;
    bool guard_was_parried = false;
    int damage_to_guard = 0;
    int damage_to_prince = 0;
    bool guard_died = false;
    bool prince_died = false;
};

// Deterministic xorshift32. Exposed so tests and the game loop share one
// reproducible stream.
uint32_t next_rand(uint32_t& state);

// The guard's move for this tick. Parries roughly skill*50% of the time
// and swings otherwise, so a skilled guard is defensive but never inert.
CombatAction choose_guard_action(const Character& guard, const Fighter& gf, uint32_t& rng);

// True when the two are toe to toe and both still standing.
bool in_sword_range(const Character& a, const Character& b);

// Resolve one tick of the duel. Mutates both characters' hp and both
// fighters' recovery, and reports what happened so the caller can mirror
// the damage into the engine and narrate it.
ExchangeResult combat_tick(Character& prince, Fighter& pf,
                           Character& guard, Fighter& gf,
                           CombatAction prince_action, uint32_t& rng);

const char* to_string(CombatAction a);

} // namespace pop
