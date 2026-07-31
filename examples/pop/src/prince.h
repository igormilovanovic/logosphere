// Movement state machine for the Prince of Persia example.
//
// All functions in this header are pure (no engine, no KG). Headless-
// testable. The KG-coupled wrappers live in world.h.
//
// The original game is tile-and-state-machine based rather than
// physically simulated, and so is this: a character occupies one cell,
// actions take a whole number of ticks, and every transition is a
// deterministic function of (character, level, action, speed_scale).
// See GAME_DESIGN.md S3.
//
// speed_scale is the one number the engine feeds in: it comes from the
// knowledge graph by way of CapabilityProfile -> DynamicsParams, so a
// wounded leg genuinely makes the Prince slower. 1.0 is unhurt.
#pragma once

#include "level.h"

namespace pop {

// Values mirror the PrinceState enum in schema/pop.yaml.
enum class PrinceState {
    Standing,
    Running,
    Crouching,
    StandJumping,
    RunJumping,
    Climbing,
    Hanging,
    Falling,
    Fighting,
    Dead,
    Escaped,
};

enum class Action {
    None,
    Left,
    Right,
    Up,
    Down,
    Jump,
    Attack,
    Block,
};

// What happened during one tick. The game loop turns these into KG
// writes, damage, and log lines; the state machine itself never touches
// anything outside the Level and the Character.
enum class StepOutcome {
    Idle,
    Moved,
    Turned,
    Blocked,
    Jumped,
    LooseCollapsed,
    HitSpikes,
    StartedFalling,
    Landed,
    GrabbedLedge,
    Climbed,
    DrankPotion,
    PlateTriggered,
    ReachedExit,
    Died,
};

struct StepResult {
    StepOutcome outcome = StepOutcome::Idle;
    int fall_height = 0;   // set on Landed and on a fatal fall
    int damage = 0;        // HP cost incurred this tick
};

struct Character {
    int x = 0;
    int y = 0;
    int facing = 1;                       // -1 left, +1 right
    PrinceState state = PrinceState::Standing;
    int hp = 100;
    int max_hp = 100;

    int move_progress = 0;                // ticks accumulated toward the next tile
    int fall_start_y = 0;
    int ticks_in_state = 0;

    // A loose tile this character has stepped on, counting down to its
    // collapse. (-1, -1) when nothing is armed.
    int armed_loose_x = -1;
    int armed_loose_y = -1;
    int armed_loose_timer = 0;

    bool alive() const { return state != PrinceState::Dead; }
};

// Tuning. See GAME_DESIGN.md S3 for why these values.
constexpr int kMoveTicksPerTile = 2;   // at speed_scale 1.0
constexpr int kStandJumpReach   = 2;   // clears a one-tile gap
constexpr int kRunJumpReach     = 3;   // clears a wider gap
constexpr int kSafeFallHeight   = 1;   // falls of this height cost nothing
constexpr int kFallDamage       = 34;  // per story beyond the safe height
constexpr int kFatalFallHeight  = 3;
// Ticks a loose tile survives after being stepped on. At full speed a
// character crosses in time; wounded and slowed, they do not - the
// capability system turning into a platforming consequence.
constexpr int kLooseCollapseTicks = 3;

// Ticks needed to cross one tile at the given speed scale. Slower when
// wounded; never less than one tick.
int ticks_per_tile(float speed_scale);

// HP cost of falling the given number of tiles. A fall of
// kFatalFallHeight or more returns enough to kill outright.
int fall_damage_for(int height);

// Advance one tick. The level is mutated in place for the effects that
// belong to the world rather than the character: loose floors collapse,
// potions are consumed, pressure plates raise gates.
StepResult step_character(Character& c, Level& lv, Action action, float speed_scale);

const char* to_string(PrinceState s);
const char* to_string(StepOutcome o);

} // namespace pop
