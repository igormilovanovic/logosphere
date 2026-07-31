// Knowledge-graph bridge for the Prince of Persia example.
//
// This is the tier that touches the engine. It follows the read / mutate
// / write shape that examples/logotron/src/arena.h uses: the pure tiers
// (level.h, prince.h, combat.h) own per-tick game state, and this file
// projects it into the knowledge graph, applies damage through the
// engine's DamageSystem, and reads movement speed back out of the
// capability system.
//
// Engine facilities exercised here, all available in the headless `core`
// profile (see POP.md "How it maps onto the engine"):
//
//   kg::KGModule            entities, relations, properties, auto-events
//   body_plan               biped declaration + capability-bearing parts
//   CapabilityProfile       body-part health -> capability factors
//   DynamicsParams          capability factors -> movement speed
//   DamageSystem            Slash damage, per-body-part damage, deaths
//   GameTime                the countdown
//   EventBus                everything above, observable
//
// Damage contract: the pure tiers are authoritative for hp during a tick;
// every point of it is mirrored here into DamageSystem, which writes the
// knowledge graph and drives the capability rules. Callers must not write
// health into the KG directly.
#pragma once

#include "combat.h"
#include "level.h"
#include "prince.h"

#include "logosphere/damage/damage_system.h"
#include "logosphere/events/event_bus.h"
#include "logosphere/kg/kg_module.h"

#include <memory>
#include <string>

namespace pop {

// Physical constants for a lightly-built swordsman, fed to
// CapabilityProfile / DynamicsParams. See GAME_DESIGN.md S6.
constexpr float kPrinceMass       = 68.0f;   // kg
constexpr float kPrinceLegLength  = 0.92f;   // m
constexpr float kPrinceHeight     = 1.78f;   // m
constexpr float kPrinceReflexesMs = 210.0f;  // quicker than the 250ms reference human
constexpr float kPrinceGritW      = 520.0f;

constexpr double kDefaultTimeLimitSeconds = 3600.0;   // the original's sixty minutes

// Health each limb carries. Fall damage lands on the legs, sword blows on
// the torso, so the two hazards degrade different capabilities.
constexpr float kLimbMaxHp = 100.0f;

struct World {
    kg::KGModule kg;
    logosphere::EventBus bus;
    DamageSystem damage;

    kg::EntityID level_e  = kg::INVALID_ENTITY;
    kg::EntityID prince_e = kg::INVALID_ENTITY;
    kg::EntityID guard_e  = kg::INVALID_ENTITY;
    kg::EntityID sword_e  = kg::INVALID_ENTITY;

    double time_limit_seconds = kDefaultTimeLimitSeconds;

    // Cached so speed_scale is only recomputed when health actually
    // changes: CapabilityProfile::compute_from_kg re-fires any
    // emit_event rules on every call (tests/test_capability_system.cpp
    // documents this), and it prints a [CAP] line each time.
    float cached_speed_scale = 1.0f;

    World();
};

// Build the knowledge graph for a parsed level: the Level entity, one
// entity per interesting tile (gates, plates, loose floors, spikes,
// potions, the exit), the Prince and the Guard with full biped body
// plans, and the Prince's sword wired for the capability cascade.
std::unique_ptr<World> create_world(const Level& lv, double time_limit_seconds = kDefaultTimeLimitSeconds);

// Mirror a character's tile position, facing and movement state into the
// knowledge graph. Every write auto-emits on bus.state_changes().
void sync_character(World& w, kg::EntityID who, const Character& c);

// Movement speed for the Prince, derived from body-part health through
// CapabilityProfile -> DynamicsParams and normalised against an unhurt
// baseline. 1.0 when healthy, lower when a leg is wounded.
float prince_speed_scale(World& w);

// A fall landed: damage the legs, so locomotion (and therefore speed)
// degrades through the engine's own capability rules.
void apply_fall_damage(World& w, int hp_cost);

// A blade landed: Slash damage to the torso of whoever was hit.
void apply_sword_damage(World& w, kg::EntityID who, int hp_cost);

// A potion was drunk: restore the entity and every body part, which also
// clears any limp, since the speed cap comes from a rule on leg health.
void heal_prince_fully(World& w);

// Countdown helpers. GameTime is a process-wide static clock; advance it
// once per tick from the game loop.
void advance_clock(double dt_seconds);
double seconds_remaining(const World& w);
bool time_expired(const World& w);

} // namespace pop
