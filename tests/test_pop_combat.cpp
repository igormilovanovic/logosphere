// Prince of Persia sword-fighting tests.
//
// Pure tier: links nothing at all. The guard's RNG state is caller-owned,
// so a duel replays exactly from a seed and these assertions are stable.
//
// Usage:
//   ./build/test_pop_combat

#include "combat.h"
#include "prince.h"

#include <iostream>
#include <vector>

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

namespace {

Character fighter_at(int x, int y) {
    Character c;
    c.x = x;
    c.y = y;
    return c;
}

} // namespace

void test_rng_is_deterministic_and_seed_sensitive() {
    uint32_t a = 42, b = 42, c = 43;
    std::vector<uint32_t> seq_a, seq_b, seq_c;
    for (int i = 0; i < 8; ++i) {
        seq_a.push_back(next_rand(a));
        seq_b.push_back(next_rand(b));
        seq_c.push_back(next_rand(c));
    }
    ASSERT(seq_a == seq_b, "the same seed replays the same stream");
    ASSERT(seq_a != seq_c, "a different seed diverges");

    uint32_t zero = 0;
    ASSERT(next_rand(zero) != 0, "a zero seed is escaped rather than stuck");
}

void test_sword_range() {
    Character a = fighter_at(5, 1);
    Character b = fighter_at(6, 1);
    ASSERT(in_sword_range(a, b), "adjacent on the same row is in range");
    ASSERT(in_sword_range(b, a), "and it is symmetric");

    Character far = fighter_at(8, 1);
    ASSERT(!in_sword_range(a, far), "two tiles apart is out of range");

    Character same = fighter_at(5, 1);
    ASSERT(!in_sword_range(a, same), "sharing a tile is not a duel");

    Character below = fighter_at(6, 2);
    ASSERT(!in_sword_range(a, below), "a different row is out of range");

    Character dead = fighter_at(6, 1);
    dead.state = PrinceState::Dead;
    ASSERT(!in_sword_range(a, dead), "the dead are not engaged");
}

void test_strike_lands_when_not_parried() {
    Character prince = fighter_at(0, 0), guard = fighter_at(1, 0);
    Fighter pf, gf;
    gf.skill = 0.0f;                 // never parries
    uint32_t rng = 7;

    ExchangeResult r = combat_tick(prince, pf, guard, gf, CombatAction::Strike, rng);
    ASSERT(r.prince_hit_guard, "the blow lands");
    ASSERT(r.damage_to_guard == kStrikeDamage, "for the documented damage");
    ASSERT(guard.hp == 100 - kStrikeDamage, "the guard's health drops");
    ASSERT(!pf.ready(), "the Prince is committed to the swing");
}

void test_parry_blocks_the_blow() {
    Character prince = fighter_at(0, 0), guard = fighter_at(1, 0);
    Fighter pf, gf;
    gf.skill = 1.0f;                 // parries as often as the model allows
    uint32_t rng = 1;

    // Find a tick where the guard chooses to parry, then verify it blocks.
    bool observed_parry = false;
    for (int i = 0; i < 32 && !observed_parry; ++i) {
        Character p = fighter_at(0, 0), g = fighter_at(1, 0);
        Fighter f1, f2;
        f2.skill = 1.0f;
        uint32_t seed = static_cast<uint32_t>(i + 1);
        ExchangeResult r = combat_tick(p, f1, g, f2, CombatAction::Strike, seed);
        if (r.guard_action == CombatAction::Parry) {
            observed_parry = true;
            ASSERT(r.prince_was_parried, "a parried strike is reported as such");
            ASSERT(!r.prince_hit_guard, "and does no damage");
            ASSERT(g.hp == 100, "the guard is untouched");
            ASSERT(f1.recovery_ticks == kParriedRecoveryTicks,
                   "being parried costs the longer recovery");
        }
    }
    ASSERT(observed_parry, "a skilled guard does parry sometimes");
    (void)pf; (void)gf; (void)rng;
}

void test_guard_skill_bounds_behaviour() {
    // Skill 0: always swings, never parries.
    int parries = 0;
    for (int i = 0; i < 50; ++i) {
        Character g = fighter_at(1, 0);
        Fighter gf;
        gf.skill = 0.0f;
        uint32_t seed = static_cast<uint32_t>(i + 1);
        if (choose_guard_action(g, gf, seed) == CombatAction::Parry) parries++;
    }
    ASSERT(parries == 0, "an unskilled guard never parries");

    // Skill 1: parries some of the time but still attacks.
    int strikes = 0, skilled_parries = 0;
    for (int i = 0; i < 50; ++i) {
        Character g = fighter_at(1, 0);
        Fighter gf;
        gf.skill = 1.0f;
        uint32_t seed = static_cast<uint32_t>(i + 1);
        CombatAction a = choose_guard_action(g, gf, seed);
        if (a == CombatAction::Parry) skilled_parries++;
        if (a == CombatAction::Strike) strikes++;
    }
    ASSERT(skilled_parries > 0, "a skilled guard parries");
    ASSERT(strikes > 0, "but is never purely defensive");
}

void test_recovering_fighter_cannot_act() {
    Character g = fighter_at(1, 0);
    Fighter gf;
    gf.state = CombatState::Recovering;
    gf.recovery_ticks = 2;
    uint32_t rng = 5;
    ASSERT(choose_guard_action(g, gf, rng) == CombatAction::Idle,
           "a recovering guard does nothing");
}

void test_recovery_expires() {
    Character prince = fighter_at(0, 0), guard = fighter_at(1, 0);
    Fighter pf, gf;
    gf.skill = 0.0f;
    uint32_t rng = 3;

    combat_tick(prince, pf, guard, gf, CombatAction::Strike, rng);
    ASSERT(!pf.ready(), "committed after striking");
    for (int i = 0; i < kHitRecoveryTicks + 1; ++i) {
        combat_tick(prince, pf, guard, gf, CombatAction::Idle, rng);
    }
    ASSERT(pf.ready(), "recovery expires and the Prince can act again");
}

void test_three_clean_hits_kill() {
    Character prince = fighter_at(0, 0), guard = fighter_at(1, 0);
    Fighter pf, gf;
    gf.skill = 0.0f;
    uint32_t rng = 11;

    int landed = 0;
    for (int i = 0; i < 40 && guard.alive(); ++i) {
        ExchangeResult r = combat_tick(prince, pf, guard, gf, CombatAction::Strike, rng);
        if (r.prince_hit_guard) landed++;
        if (r.guard_died) {
            ASSERT(!guard.alive(), "death is reflected in the character state");
            ASSERT(guard.hp == 0, "and health bottoms out at zero");
        }
    }
    ASSERT(!guard.alive(), "the guard eventually falls");
    ASSERT(landed == 3, "exactly three clean hits do it");
}

void test_dead_fighters_stop_exchanging() {
    Character prince = fighter_at(0, 0), guard = fighter_at(1, 0);
    Fighter pf, gf;
    guard.state = PrinceState::Dead;
    guard.hp = 0;
    uint32_t rng = 2;

    ExchangeResult r = combat_tick(prince, pf, guard, gf, CombatAction::Strike, rng);
    ASSERT(!r.prince_hit_guard && !r.guard_hit_prince, "no blows against a corpse");
    ASSERT(prince.hp == 100, "and none received");
}

void test_duel_replays_identically_from_a_seed() {
    auto run_duel = [](uint32_t seed) {
        Character prince = fighter_at(0, 0), guard = fighter_at(1, 0);
        Fighter pf, gf;
        gf.skill = 0.6f;
        uint32_t rng = seed;
        std::vector<int> trace;
        for (int i = 0; i < 30 && prince.alive() && guard.alive(); ++i) {
            // Alternate strike and parry, so both code paths are exercised.
            CombatAction a = (i % 2 == 0) ? CombatAction::Strike : CombatAction::Parry;
            ExchangeResult r = combat_tick(prince, pf, guard, gf, a, rng);
            trace.push_back(static_cast<int>(r.guard_action));
            trace.push_back(prince.hp);
            trace.push_back(guard.hp);
        }
        return trace;
    };

    ASSERT(run_duel(2024) == run_duel(2024), "the same seed replays the same duel");

    // The seed genuinely steers the fight. Individual pairs can coincide
    // -- a duel that ends in five ticks has little room to diverge -- so
    // assert across a spread rather than on one arbitrary pair.
    std::vector<std::vector<int>> traces;
    for (uint32_t seed : {7u, 99u, 2024u, 12345u, 555u}) {
        traces.push_back(run_duel(seed));
    }
    bool any_differ = false;
    for (size_t i = 1; i < traces.size(); ++i) {
        if (traces[i] != traces[0]) any_differ = true;
    }
    ASSERT(any_differ, "different seeds produce different duels");
}

void test_fighting_sets_the_movement_state() {
    Character prince = fighter_at(0, 0), guard = fighter_at(1, 0);
    Fighter pf, gf;
    uint32_t rng = 4;
    combat_tick(prince, pf, guard, gf, CombatAction::Idle, rng);
    ASSERT(prince.state == PrinceState::Fighting || !prince.alive(),
           "engaging puts the Prince in the fighting stance");
}

int main() {
    std::cout << "=== Prince of Persia: combat tests ===" << std::endl;

    test_rng_is_deterministic_and_seed_sensitive();
    test_sword_range();
    test_strike_lands_when_not_parried();
    test_parry_blocks_the_blow();
    test_guard_skill_bounds_behaviour();
    test_recovering_fighter_cannot_act();
    test_recovery_expires();
    test_three_clean_hits_kill();
    test_dead_fighters_stop_exchanging();
    test_duel_replays_identically_from_a_seed();
    test_fighting_sets_the_movement_state();

    std::cout << std::endl;
    std::cout << tests_passed << " passed, " << tests_failed << " failed" << std::endl;
    return tests_failed > 0 ? 1 : 0;
}
