// Prince of Persia knowledge-graph integration tests.
//
// This is the tier that actually exercises the engine: ontology
// extension, body plans, the capability system driving movement speed,
// the DamageSystem, the event bus, and GameTime. Links logosphere_core.
//
// Usage:
//   ./build/test_pop_world

#include "level.h"
#include "level_one.h"
#include "prince.h"
#include "world.h"

#include "core/game_time.h"

#include <iostream>
#include <string>

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

kg::EntityID part_named(kg::KGModule& kg, kg::EntityID who, const std::string& name) {
    for (auto p : kg.getRelated(who, "HAS_PART")) {
        if (kg.getProperty(p, "body_part_name") == name) return p;
    }
    return kg::INVALID_ENTITY;
}

// GameTime is a process-wide static clock, so every case that reads it
// must start from a known point.
void reset_clock() {
    GameTime::reset();
    GameTime::initialize(0.0);
}

} // namespace

void test_ontology_is_extended() {
    Level lv = level_one();
    auto w = create_world(lv);

    const auto& reg = w->kg.getRegistry();
    ASSERT(reg.hasEntityType("Level"), "Level type registered");
    ASSERT(reg.hasEntityType("Prince"), "Prince type registered");
    ASSERT(reg.hasEntityType("Guard"), "Guard type registered");
    ASSERT(reg.hasEntityType("Sword"), "Sword type registered");
    ASSERT(reg.hasEntityType("Gate"), "Gate type registered");

    // Engine types survive the extension.
    ASSERT(reg.hasEntityType("Humanoid"), "engine types still present");
    ASSERT(reg.isSubtypeOf("Prince", "Humanoid"), "the Prince is a Humanoid");
    ASSERT(reg.isSubtypeOf("Gate", "Structure"), "a Gate is a Structure");

    // The game reuses the engine's relations rather than inventing its
    // own, because generate_registry.py only emits the engine's set.
    ASSERT(reg.hasRelationType("CONTAINS"), "CONTAINS available");
    ASSERT(reg.hasRelationType("HAS_PART"), "HAS_PART available");
    ASSERT(reg.hasRelationType("SUPPORTS"), "SUPPORTS available");
    ASSERT(reg.hasRelationType("MANAGES"), "MANAGES available");
}

void test_level_entity_carries_its_dimensions() {
    Level lv = level_one();
    auto w = create_world(lv);
    ASSERT(w->kg.getProperty(w->level_e, "grid_width") == std::to_string(lv.w),
           "grid width recorded");
    ASSERT(w->kg.getProperty(w->level_e, "grid_height") == std::to_string(lv.h),
           "grid height recorded");
}

void test_interesting_tiles_become_entities() {
    Level lv = level_one();
    auto w = create_world(lv);

    auto contained = w->kg.getRelated(w->level_e, "CONTAINS");
    ASSERT(!contained.empty(), "the level contains tile entities");

    // Plain floor and empty air stay out of the graph, as logotron keeps
    // arena geometry out of it.
    int floor_or_empty = 0;
    for (int y = 0; y < lv.h; ++y) {
        for (int x = 0; x < lv.w; ++x) {
            const TileKind k = lv.at(x, y);
            if (k == TileKind::Floor || k == TileKind::Empty || k == TileKind::Wall) {
                floor_or_empty++;
            }
        }
    }
    ASSERT(static_cast<int>(contained.size()) < floor_or_empty,
           "only the interesting tiles are materialised");

    ASSERT(!w->kg.findByType("Gate").empty(), "the gate exists in the graph");
    ASSERT(!w->kg.findByType("PressurePlate").empty(), "the plate exists");
    ASSERT(!w->kg.findByType("LooseFloor").empty(), "the loose tile exists");
    ASSERT(!w->kg.findByType("Spikes").empty(), "the spikes exist");
    ASSERT(!w->kg.findByType("Potion").empty(), "the potion exists");
}

void test_plate_manages_gate() {
    Level lv = level_one();
    auto w = create_world(lv);

    auto plates = w->kg.findByType("PressurePlate");
    ASSERT(!plates.empty(), "there is a plate");
    if (!plates.empty()) {
        auto managed = w->kg.getRelated(plates[0], "MANAGES");
        ASSERT(!managed.empty(), "the plate manages at least one gate");
        if (!managed.empty()) {
            ASSERT(w->kg.getType(managed[0]) == "Gate", "and what it manages is a gate");
            ASSERT(w->kg.getProperty(managed[0], "gate_state") == "CLOSED",
                   "which starts closed");
        }
    }
}

void test_characters_have_body_plans() {
    Level lv = level_one();
    auto w = create_world(lv);

    for (kg::EntityID who : {w->prince_e, w->guard_e}) {
        ASSERT(part_named(w->kg, who, "left_leg") != kg::INVALID_ENTITY, "has a left leg");
        ASSERT(part_named(w->kg, who, "right_leg") != kg::INVALID_ENTITY, "has a right leg");
        ASSERT(part_named(w->kg, who, "left_arm") != kg::INVALID_ENTITY, "has a left arm");
        ASSERT(part_named(w->kg, who, "right_arm") != kg::INVALID_ENTITY, "has a right arm");
        ASSERT(part_named(w->kg, who, "torso") != kg::INVALID_ENTITY, "has a torso");
        ASSERT(part_named(w->kg, who, "head") != kg::INVALID_ENTITY, "has a head");
    }
}

void test_sword_hangs_off_the_sword_arm() {
    Level lv = level_one();
    auto w = create_world(lv);

    ASSERT(w->kg.getType(w->sword_e) == "Sword", "the sword exists");
    ASSERT(w->kg.getProperty(w->sword_e, "cap_list") == "manipulation",
           "and carries a manipulation capability");

    const kg::EntityID arm = part_named(w->kg, w->prince_e, "right_arm");
    auto supported = w->kg.getRelated(arm, "SUPPORTS");
    bool found = false;
    for (auto s : supported) {
        if (s == w->sword_e) found = true;
    }
    ASSERT(found, "the sword arm SUPPORTS the sword");
    ASSERT(w->kg.getProperty(arm, "rule.0.cascade") == "relation:SUPPORTS:0.5",
           "and a cascade rule degrades the blade with the arm");
}

void test_healthy_prince_moves_at_full_speed() {
    Level lv = level_one();
    auto w = create_world(lv);
    const float scale = prince_speed_scale(*w);
    ASSERT(scale > 0.99f, "an unhurt Prince runs at the baseline speed");
    ASSERT(ticks_per_tile(scale) == kMoveTicksPerTile, "which is the base cost per tile");
}

// The headline integration: damage recorded in the knowledge graph flows
// through the engine's own capability rules into how fast the Prince
// moves. No game code reads the rule; CapabilityProfile evaluates it.
void test_wounded_legs_slow_the_prince_down() {
    Level lv = level_one();
    auto w = create_world(lv);
    const float healthy = prince_speed_scale(*w);

    // Enough to trip the health_below:50 limp rule on both legs.
    apply_fall_damage(*w, 120);

    const float wounded = prince_speed_scale(*w);
    ASSERT(wounded < healthy, "wounded legs reduce movement speed");
    ASSERT(ticks_per_tile(wounded) > ticks_per_tile(healthy),
           "which costs real ticks per tile in the movement code");

    const kg::EntityID leg = part_named(w->kg, w->prince_e, "left_leg");
    ASSERT(w->kg.getProperty(leg, "rule.0.effect") == "speed_cap:0.6",
           "the limp is an engine response rule, not game logic");
}

void test_potion_clears_the_limp() {
    Level lv = level_one();
    auto w = create_world(lv);

    apply_fall_damage(*w, 120);
    const float wounded = prince_speed_scale(*w);

    heal_prince_fully(*w);
    const float healed = prince_speed_scale(*w);

    ASSERT(healed > wounded, "healing restores speed");
    ASSERT(healed > 0.99f, "all the way back to the baseline");
}

void test_sword_damage_flows_through_the_damage_system() {
    Level lv = level_one();
    auto w = create_world(lv);

    const float before = w->damage.get_hp(w->guard_e);
    apply_sword_damage(*w, w->guard_e, kStrikeDamage);
    const float after = w->damage.get_hp(w->guard_e);

    ASSERT(after < before, "the guard's entity health drops");

    const kg::EntityID torso = part_named(w->kg, w->guard_e, "torso");
    const std::string health = w->kg.getProperty(torso, "health");
    ASSERT(!health.empty(), "the torso carries a health property");
    ASSERT(std::stof(health) < kLimbMaxHp * 1.5f, "which the blow reduced");
}

void test_damage_is_observable_on_the_event_bus() {
    Level lv = level_one();
    auto w = create_world(lv);

    int damage_events = 0;
    w->bus.damage().subscribe(
        [&damage_events](const logosphere::ontology::DamageEvent&) { damage_events++; });

    apply_sword_damage(*w, w->guard_e, kStrikeDamage);
    ASSERT(damage_events > 0, "the engine emits damage on the bus without the game asking");
}

void test_death_reaches_the_deaths_channel() {
    Level lv = level_one();
    auto w = create_world(lv);

    int deaths = 0;
    w->bus.deaths().subscribe(
        [&deaths](const logosphere::ontology::DeathEvent&) { deaths++; });

    for (int i = 0; i < 10 && !w->damage.is_dead(w->guard_e); ++i) {
        apply_sword_damage(*w, w->guard_e, kStrikeDamage);
    }
    ASSERT(w->damage.is_dead(w->guard_e), "enough blows kill the guard");
    ASSERT(deaths > 0, "and a death event is emitted");
}

void test_character_state_is_mirrored_into_the_graph() {
    Level lv = level_one();
    auto w = create_world(lv);

    Character c;
    c.x = 7;
    c.y = 1;
    c.facing = -1;
    c.state = PrinceState::Running;
    sync_character(*w, w->prince_e, c);

    ASSERT(w->kg.getProperty(w->prince_e, "tile_x") == "7", "x mirrored");
    ASSERT(w->kg.getProperty(w->prince_e, "tile_y") == "1", "y mirrored");
    ASSERT(w->kg.getProperty(w->prince_e, "facing") == "-1", "facing mirrored");
    ASSERT(w->kg.getProperty(w->prince_e, "movement_state") == "RUNNING", "state mirrored");
}

void test_property_writes_emit_state_changes() {
    Level lv = level_one();
    auto w = create_world(lv);

    int changes = 0;
    w->bus.state_changes().subscribe(
        [&changes](const logosphere::ontology::WorldEvent&) { changes++; });

    Character c;
    c.x = 3;
    sync_character(*w, w->prince_e, c);
    ASSERT(changes > 0, "KG writes auto-emit once a bus is attached");
}

void test_countdown() {
    reset_clock();
    Level lv = level_one();
    auto w = create_world(lv, 10.0);

    ASSERT(seconds_remaining(*w) > 9.0, "the clock starts full");
    ASSERT(!time_expired(*w), "and has not expired");

    advance_clock(6.0);
    ASSERT(seconds_remaining(*w) < 5.0, "advancing the clock consumes the limit");
    ASSERT(!time_expired(*w), "with time still on it");

    advance_clock(6.0);
    ASSERT(seconds_remaining(*w) == 0.0, "the remainder floors at zero");
    ASSERT(time_expired(*w), "and the level is out of time");
    reset_clock();
}

int main() {
    std::cout << "=== Prince of Persia: knowledge-graph tests ===" << std::endl;

    reset_clock();
    test_ontology_is_extended();
    test_level_entity_carries_its_dimensions();
    test_interesting_tiles_become_entities();
    test_plate_manages_gate();
    test_characters_have_body_plans();
    test_sword_hangs_off_the_sword_arm();
    test_healthy_prince_moves_at_full_speed();
    test_wounded_legs_slow_the_prince_down();
    test_potion_clears_the_limp();
    test_sword_damage_flows_through_the_damage_system();
    test_damage_is_observable_on_the_event_bus();
    test_death_reaches_the_deaths_channel();
    test_character_state_is_mirrored_into_the_graph();
    test_property_writes_emit_state_changes();
    test_countdown();

    std::cout << std::endl;
    std::cout << tests_passed << " passed, " << tests_failed << " failed" << std::endl;
    return tests_failed > 0 ? 1 : 0;
}
