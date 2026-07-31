#include "prince.h"

#include <algorithm>
#include <cmath>

namespace pop {

int ticks_per_tile(float speed_scale) {
    if (speed_scale <= 0.0f) return kMoveTicksPerTile * 4;   // barely mobile, still finite
    int t = static_cast<int>(std::ceil(static_cast<float>(kMoveTicksPerTile) / speed_scale));
    return std::max(1, t);
}

int fall_damage_for(int height) {
    if (height <= kSafeFallHeight) return 0;
    if (height >= kFatalFallHeight) return 1000;             // always lethal
    return kFallDamage * (height - kSafeFallHeight);
}

namespace {

void kill(Character& c) {
    c.hp = 0;
    c.state = PrinceState::Dead;
    c.move_progress = 0;
    c.ticks_in_state = 0;
}

void enter_fall(Character& c) {
    if (c.state != PrinceState::Falling) {
        c.fall_start_y = c.y;
        c.state = PrinceState::Falling;
        c.ticks_in_state = 0;
    }
    c.move_progress = 0;
}

// Effects that fire when a character arrives in a cell: the exit, and
// whatever floor material is now underneath. `base` is the outcome the
// move itself produced; arrival effects override it when they are more
// significant.
StepResult resolve_arrival(Character& c, Level& lv, StepOutcome base) {
    StepResult r;
    r.outcome = base;

    if (lv.at(c.x, c.y) == TileKind::Exit) {
        c.state = PrinceState::Escaped;
        r.outcome = StepOutcome::ReachedExit;
        return r;
    }

    const TileKind below = lv.at(c.x, c.y + 1);
    switch (below) {
        case TileKind::Spikes:
            kill(c);
            r.outcome = StepOutcome::HitSpikes;
            return r;
        case TileKind::Potion:
            c.hp = c.max_hp;
            lv.set(c.x, c.y + 1, TileKind::Floor);
            r.outcome = StepOutcome::DrankPotion;
            return r;
        case TileKind::PressurePlate:
            open_all_gates(lv);
            r.outcome = StepOutcome::PlateTriggered;
            return r;
        case TileKind::LooseFloor:
            // Arm it; it gives way once the timer runs out.
            c.armed_loose_x = c.x;
            c.armed_loose_y = c.y + 1;
            c.armed_loose_timer = kLooseCollapseTicks;
            return r;
        default:
            return r;
    }
}

StepResult land(Character& c, Level& lv) {
    StepResult r;
    const int height = c.y - c.fall_start_y;
    const int dmg = fall_damage_for(height);

    r.fall_height = height;
    r.damage = dmg;
    c.hp -= dmg;

    if (c.hp <= 0) {
        kill(c);
        r.outcome = StepOutcome::Died;
        return r;
    }

    c.state = PrinceState::Standing;
    c.ticks_in_state = 0;
    StepResult arrival = resolve_arrival(c, lv, StepOutcome::Landed);
    arrival.fall_height = height;
    arrival.damage = dmg;
    return arrival;
}

StepResult step_falling(Character& c, Level& lv, Action action) {
    // A ledge on the facing side can arrest the fall, which also cancels
    // the damage accumulated so far.
    if (action == Action::Up) {
        const int fx = c.x + c.facing;
        if (is_support(lv.at(fx, c.y)) && is_passable_at(lv, fx, c.y - 1)) {
            c.state = PrinceState::Hanging;
            c.ticks_in_state = 0;
            c.fall_start_y = c.y;
            StepResult r;
            r.outcome = StepOutcome::GrabbedLedge;
            return r;
        }
    }

    if (is_passable_at(lv, c.x, c.y + 1)) {
        c.y += 1;
    }
    if (is_grounded_at(lv, c.x, c.y)) {
        return land(c, lv);
    }

    StepResult r;
    r.outcome = StepOutcome::Idle;
    return r;
}

StepResult step_hanging(Character& c, Level& lv, Action action) {
    StepResult r;
    if (action == Action::Up) {
        c.x += c.facing;
        c.y -= 1;
        c.state = PrinceState::Standing;
        c.ticks_in_state = 0;
        return resolve_arrival(c, lv, StepOutcome::Climbed);
    }
    if (action == Action::Down) {
        enter_fall(c);
        r.outcome = StepOutcome::StartedFalling;
        return r;
    }
    r.outcome = StepOutcome::Idle;
    return r;
}

StepResult step_jump(Character& c, Level& lv, bool running) {
    const int reach = running ? kRunJumpReach : kStandJumpReach;
    c.state = running ? PrinceState::RunJumping : PrinceState::StandJumping;
    c.move_progress = 0;

    // Travel as far as the arc allows, stopping short of anything solid.
    int dest = c.x;
    for (int i = 1; i <= reach; ++i) {
        const int nx = c.x + c.facing * i;
        if (!is_passable_at(lv, nx, c.y)) break;
        dest = nx;
    }
    c.x = dest;

    if (is_grounded_at(lv, c.x, c.y)) {
        c.state = PrinceState::Standing;
        return resolve_arrival(c, lv, StepOutcome::Jumped);
    }

    enter_fall(c);
    StepResult r;
    r.outcome = StepOutcome::Jumped;
    return r;
}

StepResult step_grounded(Character& c, Level& lv, Action action, float speed_scale) {
    StepResult r;

    switch (action) {
        case Action::Left:
        case Action::Right: {
            const int dir = (action == Action::Left) ? -1 : 1;
            if (c.facing != dir) {
                c.facing = dir;
                c.state = PrinceState::Standing;
                c.move_progress = 0;
                r.outcome = StepOutcome::Turned;
                return r;
            }
            c.state = PrinceState::Running;
            c.move_progress += 1;
            if (c.move_progress < ticks_per_tile(speed_scale)) {
                r.outcome = StepOutcome::Idle;
                return r;
            }
            c.move_progress = 0;

            const int nx = c.x + dir;
            if (!is_passable_at(lv, nx, c.y)) {
                c.state = PrinceState::Standing;
                r.outcome = StepOutcome::Blocked;
                return r;
            }
            c.x = nx;
            StepResult moved = resolve_arrival(c, lv, StepOutcome::Moved);
            if (moved.outcome == StepOutcome::Moved && !is_grounded_at(lv, c.x, c.y)) {
                enter_fall(c);
                moved.outcome = StepOutcome::StartedFalling;
            }
            return moved;
        }

        case Action::Jump:
            return step_jump(c, lv, c.state == PrinceState::Running);

        case Action::Up: {
            // Pull up onto a ledge directly ahead.
            const int fx = c.x + c.facing;
            if (is_support(lv.at(fx, c.y)) &&
                is_passable_at(lv, fx, c.y - 1) &&
                is_passable_at(lv, c.x, c.y - 1)) {
                c.x = fx;
                c.y -= 1;
                c.state = PrinceState::Standing;
                c.move_progress = 0;
                return resolve_arrival(c, lv, StepOutcome::Climbed);
            }
            r.outcome = StepOutcome::Blocked;
            return r;
        }

        case Action::Down: {
            // Lower yourself off a ledge ahead, otherwise crouch.
            const int fx = c.x + c.facing;
            if (is_passable_at(lv, fx, c.y) && !is_grounded_at(lv, fx, c.y)) {
                c.x = fx;
                enter_fall(c);
                r.outcome = StepOutcome::StartedFalling;
                return r;
            }
            c.state = PrinceState::Crouching;
            r.outcome = StepOutcome::Idle;
            return r;
        }

        case Action::None:
        case Action::Attack:
        case Action::Block:
        default:
            // step_character only runs while out of sword range, so a
            // lingering Fighting stance is over by definition.
            c.state = PrinceState::Standing;
            c.move_progress = 0;
            r.outcome = StepOutcome::Idle;
            return r;
    }
}

} // namespace

StepResult step_character(Character& c, Level& lv, Action action, float speed_scale) {
    StepResult r;
    if (c.state == PrinceState::Dead || c.state == PrinceState::Escaped) {
        return r;   // terminal states absorb everything
    }
    c.ticks_in_state += 1;

    // A loose tile stepped on earlier gives way when its timer expires.
    bool collapsed = false;
    if (c.armed_loose_x >= 0 && c.armed_loose_y >= 0) {
        c.armed_loose_timer -= 1;
        if (c.armed_loose_timer <= 0) {
            lv.set(c.armed_loose_x, c.armed_loose_y, TileKind::Empty);
            c.armed_loose_x = -1;
            c.armed_loose_y = -1;
            collapsed = true;
        }
    }

    StepResult inner;
    switch (c.state) {
        case PrinceState::Falling:
            inner = step_falling(c, lv, action);
            break;
        case PrinceState::Hanging:
            inner = step_hanging(c, lv, action);
            break;
        default:
            if (!is_grounded_at(lv, c.x, c.y)) {
                enter_fall(c);
                inner.outcome = StepOutcome::StartedFalling;
            } else {
                inner = step_grounded(c, lv, action, speed_scale);
            }
            break;
    }

    // The collapse is the more informative event when nothing louder
    // happened; a death or a landing still wins.
    if (collapsed && (inner.outcome == StepOutcome::Idle ||
                      inner.outcome == StepOutcome::StartedFalling)) {
        inner.outcome = StepOutcome::LooseCollapsed;
    }
    return inner;
}

const char* to_string(PrinceState s) {
    switch (s) {
        case PrinceState::Standing:     return "STANDING";
        case PrinceState::Running:      return "RUNNING";
        case PrinceState::Crouching:    return "CROUCHING";
        case PrinceState::StandJumping: return "STAND_JUMPING";
        case PrinceState::RunJumping:   return "RUN_JUMPING";
        case PrinceState::Climbing:     return "CLIMBING";
        case PrinceState::Hanging:      return "HANGING";
        case PrinceState::Falling:      return "FALLING";
        case PrinceState::Fighting:     return "FIGHTING";
        case PrinceState::Dead:         return "DEAD";
        case PrinceState::Escaped:      return "ESCAPED";
    }
    return "UNKNOWN";
}

const char* to_string(StepOutcome o) {
    switch (o) {
        case StepOutcome::Idle:           return "idle";
        case StepOutcome::Moved:          return "moved";
        case StepOutcome::Turned:         return "turned";
        case StepOutcome::Blocked:        return "blocked";
        case StepOutcome::Jumped:         return "jumped";
        case StepOutcome::LooseCollapsed: return "loose floor gave way";
        case StepOutcome::HitSpikes:      return "impaled on spikes";
        case StepOutcome::StartedFalling: return "started falling";
        case StepOutcome::Landed:         return "landed";
        case StepOutcome::GrabbedLedge:   return "caught the ledge";
        case StepOutcome::Climbed:        return "climbed up";
        case StepOutcome::DrankPotion:    return "drank a potion";
        case StepOutcome::PlateTriggered: return "the gate grinds open";
        case StepOutcome::ReachedExit:    return "reached the exit";
        case StepOutcome::Died:           return "died";
    }
    return "?";
}

} // namespace pop
