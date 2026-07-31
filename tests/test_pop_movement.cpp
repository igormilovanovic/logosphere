// Prince of Persia movement state-machine tests.
//
// Pure tier: links nothing at all. Every case is a deterministic
// transition, so the whole platformer is verifiable without an engine,
// a window, or a physics solver.
//
// Usage:
//   ./build/test_pop_movement

#include "level.h"
#include "level_one.h"
#include "prince.h"

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

namespace {

// Run `n` ticks of the same action, returning the last result.
StepResult run(Character& c, Level& lv, Action a, int n, float speed = 1.0f) {
    StepResult r;
    for (int i = 0; i < n; ++i) r = step_character(c, lv, a, speed);
    return r;
}

// Walk right until the character reaches `target_x` or the budget runs
// out. Returns true if the target was reached.
bool walk_to(Character& c, Level& lv, int target_x, float speed = 1.0f, int budget = 400) {
    for (int i = 0; i < budget && c.x != target_x && c.alive(); ++i) {
        step_character(c, lv, target_x > c.x ? Action::Right : Action::Left, speed);
    }
    return c.x == target_x;
}

Character standing_at(int x, int y) {
    Character c;
    c.x = x;
    c.y = y;
    c.facing = 1;
    return c;
}

} // namespace

void test_ticks_per_tile_scales_with_speed() {
    ASSERT(ticks_per_tile(1.0f) == kMoveTicksPerTile, "full speed costs the base ticks");
    ASSERT(ticks_per_tile(0.5f) == kMoveTicksPerTile * 2, "half speed costs twice as many");
    ASSERT(ticks_per_tile(10.0f) == 1, "never faster than one tick per tile");
    ASSERT(ticks_per_tile(0.0f) > kMoveTicksPerTile, "zero speed is slow but finite");
}

void test_fall_damage_table() {
    ASSERT(fall_damage_for(0) == 0, "no fall, no damage");
    ASSERT(fall_damage_for(kSafeFallHeight) == 0, "a short drop is free");
    ASSERT(fall_damage_for(2) == kFallDamage, "two storeys costs one wound");
    ASSERT(fall_damage_for(kFatalFallHeight) >= 100, "three storeys is lethal");
}

void test_walking_takes_time_and_advances_one_tile() {
    Level lv = parse_level({"....", "####"});
    Character c = standing_at(0, 0);

    for (int i = 0; i < kMoveTicksPerTile - 1; ++i) {
        StepResult r = step_character(c, lv, Action::Right, 1.0f);
        ASSERT(r.outcome == StepOutcome::Idle, "mid-stride ticks report Idle");
        ASSERT(c.x == 0, "the character has not crossed yet");
    }
    StepResult r = step_character(c, lv, Action::Right, 1.0f);
    ASSERT(r.outcome == StepOutcome::Moved, "the final tick completes the step");
    ASSERT(c.x == 1, "advanced exactly one tile");
    ASSERT(c.state == PrinceState::Running, "state reflects the movement");
}

void test_wounded_character_is_slower() {
    Level lv = parse_level({"....", "####"});
    Character fast = standing_at(0, 0);
    Character slow = standing_at(0, 0);

    run(fast, lv, Action::Right, 4, 1.0f);
    run(slow, lv, Action::Right, 4, 0.5f);
    ASSERT(fast.x > slow.x, "the slowed character covers less ground in the same time");
}

void test_turning_costs_a_tick_before_moving() {
    Level lv = parse_level({"....", "####"});
    Character c = standing_at(2, 0);

    StepResult r = step_character(c, lv, Action::Left, 1.0f);
    ASSERT(r.outcome == StepOutcome::Turned, "first tick turns around");
    ASSERT(c.facing == -1, "now facing left");
    ASSERT(c.x == 2, "turning does not move");
}

void test_blocked_by_wall() {
    Level lv = parse_level({".X..", "####"});
    Character c = standing_at(0, 0);
    StepResult r = run(c, lv, Action::Right, kMoveTicksPerTile);
    ASSERT(r.outcome == StepOutcome::Blocked, "walking into a wall is blocked");
    ASSERT(c.x == 0, "and does not move");
}

void test_walking_off_a_ledge_starts_a_fall() {
    Level lv = parse_level({
        "...",
        "#..",
        "...",
        "###",
    });
    Character c = standing_at(0, 0);
    StepResult r = run(c, lv, Action::Right, kMoveTicksPerTile);
    ASSERT(r.outcome == StepOutcome::StartedFalling, "stepping into open air starts a fall");
    ASSERT(c.state == PrinceState::Falling, "state is Falling");
    ASSERT(c.fall_start_y == 0, "the fall is measured from where it began");
}

void test_landing_applies_height_based_damage() {
    // Fall from row 0 to row 2: two storeys.
    Level lv = parse_level({
        "..",
        "..",
        "..",
        "##",
    });
    Character c = standing_at(0, 0);
    c.state = PrinceState::Falling;
    c.fall_start_y = 0;

    StepResult r;
    for (int i = 0; i < 6 && c.state == PrinceState::Falling; ++i) {
        r = step_character(c, lv, Action::None, 1.0f);
    }
    ASSERT(r.outcome == StepOutcome::Landed, "the fall ends in a landing");
    ASSERT(r.fall_height == 2, "two storeys fallen");
    ASSERT(r.damage == kFallDamage, "and the matching damage reported");
    ASSERT(c.hp == 100 - kFallDamage, "hp reduced");
    ASSERT(c.state == PrinceState::Standing, "back on their feet");
}

void test_long_fall_is_fatal() {
    Level lv = parse_level({"..", "..", "..", "..", "..", "##"});
    Character c = standing_at(0, 0);
    c.state = PrinceState::Falling;
    c.fall_start_y = 0;

    StepResult r;
    for (int i = 0; i < 10 && c.alive(); ++i) {
        r = step_character(c, lv, Action::None, 1.0f);
    }
    ASSERT(r.outcome == StepOutcome::Died, "a long fall kills");
    ASSERT(!c.alive(), "and the character is dead");
}

void test_spikes_kill_on_arrival() {
    Level lv = parse_level({"..", "#^"});
    Character c = standing_at(0, 0);
    StepResult r = run(c, lv, Action::Right, kMoveTicksPerTile);
    ASSERT(r.outcome == StepOutcome::HitSpikes, "stepping onto spiked floor is fatal");
    ASSERT(!c.alive(), "the character dies");
}

void test_loose_floor_collapses_after_its_timer() {
    Level lv = parse_level({"..", "~#"});
    Character c = standing_at(0, 0);

    ASSERT(c.armed_loose_x < 0, "nothing armed to begin with");
    step_character(c, lv, Action::None, 1.0f);   // arrival was at construction; arm by standing
    // Re-arm deterministically by arriving on the tile.
    c.armed_loose_x = 0;
    c.armed_loose_y = 1;
    c.armed_loose_timer = kLooseCollapseTicks;

    for (int i = 0; i < kLooseCollapseTicks - 1; ++i) {
        step_character(c, lv, Action::None, 1.0f);
        ASSERT(lv.at(0, 1) == TileKind::LooseFloor, "still holding");
    }
    step_character(c, lv, Action::None, 1.0f);
    ASSERT(lv.at(0, 1) == TileKind::Empty, "the tile gives way when the timer expires");
}

void test_loose_floor_is_crossable_at_speed_but_not_when_slowed() {
    // The loose tile sits under col 1; the route runs left to right.
    const std::vector<std::string> rows = {"....", "#~##"};

    Level fast_lv = parse_level(rows);
    Character fast = standing_at(0, 0);
    walk_to(fast, fast_lv, 2, 1.0f);
    ASSERT(fast.x == 2 && fast.alive() && fast.state != PrinceState::Falling,
           "at full speed the loose tile is crossed in time");
    run(fast, fast_lv, Action::None, kLooseCollapseTicks + 1, 1.0f);
    ASSERT(fast_lv.at(1, 1) == TileKind::Empty, "and it drops away behind them");
    ASSERT(fast.alive() && fast.state != PrinceState::Falling, "leaving them safe on the far side");

    Level slow_lv = parse_level(rows);
    Character slow = standing_at(0, 0);
    for (int i = 0; i < 12; ++i) step_character(slow, slow_lv, Action::Right, 0.35f);
    ASSERT(slow.state == PrinceState::Falling || slow.y > 0 || !slow.alive(),
           "while slowed, the tile gives way underfoot");
}

void test_potion_heals_and_becomes_floor() {
    Level lv = parse_level({"..", "#!"});
    Character c = standing_at(0, 0);
    c.hp = 20;
    StepResult r = run(c, lv, Action::Right, kMoveTicksPerTile);
    ASSERT(r.outcome == StepOutcome::DrankPotion, "standing over a potion drinks it");
    ASSERT(c.hp == c.max_hp, "health restored");
    ASSERT(lv.at(1, 1) == TileKind::Floor, "the potion is consumed, leaving plain floor");
}

void test_pressure_plate_opens_the_gate() {
    Level lv = parse_level({"..|", "#_#"});
    Character c = standing_at(0, 0);
    ASSERT(lv.at(2, 0) == TileKind::Gate, "the gate starts closed");
    StepResult r = run(c, lv, Action::Right, kMoveTicksPerTile);
    ASSERT(r.outcome == StepOutcome::PlateTriggered, "the plate fires");
    ASSERT(lv.at(2, 0) == TileKind::GateOpen, "and raises the gate");
}

void test_closed_gate_blocks_until_opened() {
    Level lv = parse_level({".|.", "###"});
    Character c = standing_at(0, 0);
    StepResult r = run(c, lv, Action::Right, kMoveTicksPerTile);
    ASSERT(r.outcome == StepOutcome::Blocked, "a closed gate blocks");

    open_all_gates(lv);
    r = run(c, lv, Action::Right, kMoveTicksPerTile);
    ASSERT(r.outcome == StepOutcome::Moved, "an open gate lets you through");
    ASSERT(c.x == 1, "now standing in the gateway");
}

void test_standing_jump_clears_a_one_tile_gap() {
    Level lv = parse_level({"....", "#.##"});
    Character c = standing_at(0, 0);
    StepResult r = step_character(c, lv, Action::Jump, 1.0f);
    ASSERT(r.outcome == StepOutcome::Jumped, "the jump resolves");
    ASSERT(c.x == kStandJumpReach, "cleared the gap");
    ASSERT(c.state == PrinceState::Standing, "landed on the far side");
}

void test_running_jump_reaches_further() {
    Level lv = parse_level({".....", "#...#"});
    Character stand = standing_at(0, 0);
    step_character(stand, lv, Action::Jump, 1.0f);

    Level lv2 = parse_level({".....", "#...#"});
    Character runner = standing_at(0, 0);
    runner.state = PrinceState::Running;
    step_character(runner, lv2, Action::Jump, 1.0f);

    ASSERT(runner.x > stand.x, "a running jump carries further than a standing one");
    ASSERT(runner.x == kRunJumpReach, "and reaches its documented distance");
}

void test_jump_into_a_wall_stops_short() {
    Level lv = parse_level({".X..", "####"});
    Character c = standing_at(0, 0);
    step_character(c, lv, Action::Jump, 1.0f);
    ASSERT(c.x == 0, "a wall in the arc stops the jump dead");
}

void test_climb_up_a_ledge() {
    Level lv = parse_level({
        "...",
        ".#.",
        "##.",
    });
    Character c = standing_at(0, 1);
    StepResult r = step_character(c, lv, Action::Up, 1.0f);
    ASSERT(r.outcome == StepOutcome::Climbed, "pulled up onto the ledge");
    ASSERT(c.x == 1 && c.y == 0, "now standing on top of it");
}

void test_ledge_grab_cancels_fall_damage() {
    // Falling past a ledge on the facing side.
    Level lv = parse_level({
        "..",
        ".#",
        "..",
        "..",
        "##",
    });
    Character c = standing_at(0, 1);
    c.state = PrinceState::Falling;
    c.fall_start_y = 0;

    StepResult r = step_character(c, lv, Action::Up, 1.0f);
    ASSERT(r.outcome == StepOutcome::GrabbedLedge, "the ledge is caught");
    ASSERT(c.state == PrinceState::Hanging, "now hanging");
    ASSERT(c.fall_start_y == c.y, "the fall is reset, so no damage carries over");

    r = step_character(c, lv, Action::Up, 1.0f);
    ASSERT(r.outcome == StepOutcome::Climbed, "and can be climbed");
    ASSERT(c.hp == c.max_hp, "arriving unhurt");
}

void test_hanging_can_be_released() {
    Level lv = parse_level({"..", ".#", "..", "##"});
    Character c = standing_at(0, 1);
    c.state = PrinceState::Hanging;
    StepResult r = step_character(c, lv, Action::Down, 1.0f);
    ASSERT(r.outcome == StepOutcome::StartedFalling, "letting go resumes the fall");
}

void test_exit_wins() {
    Level lv = parse_level({".E", "##"});
    Character c = standing_at(0, 0);
    StepResult r = run(c, lv, Action::Right, kMoveTicksPerTile);
    ASSERT(r.outcome == StepOutcome::ReachedExit, "reaching the exit is reported");
    ASSERT(c.state == PrinceState::Escaped, "and ends the level");
}

void test_terminal_states_absorb_input() {
    Level lv = parse_level({"...", "###"});
    Character dead = standing_at(0, 0);
    dead.state = PrinceState::Dead;
    StepResult r = run(dead, lv, Action::Right, 10);
    ASSERT(r.outcome == StepOutcome::Idle, "the dead do not move");
    ASSERT(dead.x == 0, "position unchanged");

    Character escaped = standing_at(0, 0);
    escaped.state = PrinceState::Escaped;
    run(escaped, lv, Action::Right, 10);
    ASSERT(escaped.x == 0, "neither do the escaped");
}

// The whole point of shipping a level: prove the documented route works.
void test_shipped_level_is_winnable() {
    Level lv = level_one();
    Character c = standing_at(kPrinceStartX, kPrinceStartY);

    ASSERT(walk_to(c, lv, 3), "walk to the lip of the gap");
    // Arriving at a run, so this is a running jump and carries further
    // than the gap strictly needs.
    step_character(c, lv, Action::Jump, 1.0f);
    ASSERT(c.x > 4, "the jump clears the gap at col 4");
    ASSERT(c.alive() && c.state != PrinceState::Falling, "landing safely on the far side");

    ASSERT(walk_to(c, lv, 15), "cross the loose tile and reach the potion");
    ASSERT(c.alive() && c.state != PrinceState::Falling, "still on the upper floor");

    ASSERT(walk_to(c, lv, 18), "reach the pressure plate");
    ASSERT(lv.at(21, 1) == TileKind::GateOpen, "the plate raised the gate");

    ASSERT(walk_to(c, lv, 26), "advance to the guard");

    // The guard is settled by combat, not movement; clear the way and
    // confirm the exit is then reachable.
    ASSERT(walk_to(c, lv, 30), "reach the exit once the way is clear");
    ASSERT(c.state == PrinceState::Escaped, "the level is winnable");
}

void test_shipped_level_gap_is_lethal_if_not_jumped() {
    Level lv = level_one();
    Character c = standing_at(kPrinceStartX, kPrinceStartY);
    for (int i = 0; i < 40 && c.alive(); ++i) {
        step_character(c, lv, Action::Right, 1.0f);
    }
    ASSERT(!c.alive(), "walking into the gap drops you onto the spikes");
}

int main() {
    std::cout << "=== Prince of Persia: movement tests ===" << std::endl;

    test_ticks_per_tile_scales_with_speed();
    test_fall_damage_table();
    test_walking_takes_time_and_advances_one_tile();
    test_wounded_character_is_slower();
    test_turning_costs_a_tick_before_moving();
    test_blocked_by_wall();
    test_walking_off_a_ledge_starts_a_fall();
    test_landing_applies_height_based_damage();
    test_long_fall_is_fatal();
    test_spikes_kill_on_arrival();
    test_loose_floor_collapses_after_its_timer();
    test_loose_floor_is_crossable_at_speed_but_not_when_slowed();
    test_potion_heals_and_becomes_floor();
    test_pressure_plate_opens_the_gate();
    test_closed_gate_blocks_until_opened();
    test_standing_jump_clears_a_one_tile_gap();
    test_running_jump_reaches_further();
    test_jump_into_a_wall_stops_short();
    test_climb_up_a_ledge();
    test_ledge_grab_cancels_fall_damage();
    test_hanging_can_be_released();
    test_exit_wins();
    test_terminal_states_absorb_input();
    test_shipped_level_is_winnable();
    test_shipped_level_gap_is_lethal_if_not_jumped();

    std::cout << std::endl;
    std::cout << tests_passed << " passed, " << tests_failed << " failed" << std::endl;
    return tests_failed > 0 ? 1 : 0;
}
