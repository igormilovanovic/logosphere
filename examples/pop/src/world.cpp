#include "world.h"

#include "generated/pop_ontology_registry.h"
#include "logosphere/capability/body_plan.h"
#include "logosphere/capability/capability_profile.h"
#include "logosphere/capability/dynamics_params.h"
#include "generated/logosphere_ontology_registry.h"
#include "core/game_time.h"

#include <algorithm>
#include <cmath>

namespace pop {

namespace {

// The unhurt run speed, used to normalise prince_speed_scale into a
// 0..1 factor the pure movement code can consume without knowing
// anything about metres per second.
float baseline_run_speed() {
    static const float kBaseline = [] {
        CapabilityProfile flat = CapabilityProfile::compute(
            kPrinceReflexesMs, kPrinceGritW, kPrinceMass, kPrinceLegLength, kPrinceHeight);
        return DynamicsParams::from_capability(flat).max_run_speed;
    }();
    return kBaseline;
}

// Give a character a full biped body plan. Legs carry locomotion (so
// wounding them slows the character down), arms carry manipulation (so
// wounding them degrades the sword through the SUPPORTS cascade).
void build_body(kg::KGModule& kg, kg::EntityID who) {
    body_plan::declare_biped(kg, who);
    kg.setProperty(who, "reflexes_ms", std::to_string(kPrinceReflexesMs));
    kg.setProperty(who, "grit_W", std::to_string(kPrinceGritW));

    body_plan::create_capability_part(kg, who, "Leg", "left_leg",
                                      kLimbMaxHp, "locomotion", 1.0f, "left");
    body_plan::create_capability_part(kg, who, "Leg", "right_leg",
                                      kLimbMaxHp, "locomotion", 1.0f, "right");
    body_plan::create_capability_part(kg, who, "Arm", "left_arm",
                                      kLimbMaxHp, "manipulation", 1.0f, "left");
    body_plan::create_capability_part(kg, who, "Arm", "right_arm",
                                      kLimbMaxHp, "manipulation", 1.0f, "right");
    body_plan::create_capability_part(kg, who, "Torso", "torso",
                                      kLimbMaxHp * 1.5f, "rotation", 1.0f);
    body_plan::create_capability_part(kg, who, "Head", "head",
                                      kLimbMaxHp, "perception", 1.0f);
}

// The limp: once a leg drops below half health, cap this character's
// speed. This is an engine response rule, evaluated by
// CapabilityProfile::compute_from_kg - no game code checks for it.
void arm_limp_rule(kg::KGModule& kg, kg::EntityID who) {
    for (auto part : kg.getRelated(who, "HAS_PART")) {
        const std::string name = kg.getProperty(part, "body_part_name");
        if (name == "left_leg" || name == "right_leg") {
            kg.setProperty(part, "rule.0.trigger", "health_below:50");
            kg.setProperty(part, "rule.0.effect", "speed_cap:0.6");
        }
    }
}

kg::EntityID find_part(kg::KGModule& kg, kg::EntityID who, const std::string& part_name) {
    for (auto part : kg.getRelated(who, "HAS_PART")) {
        if (kg.getProperty(part, "body_part_name") == part_name) return part;
    }
    return kg::INVALID_ENTITY;
}

// Only tiles the running game must reason about individually become
// entities. Plain floor and empty air stay in the Tier A grid, exactly as
// logotron keeps arena geometry out of the KG.
const char* tile_entity_type(TileKind k) {
    switch (k) {
        case TileKind::Gate:
        case TileKind::GateOpen:      return "Gate";
        case TileKind::PressurePlate: return "PressurePlate";
        case TileKind::LooseFloor:    return "LooseFloor";
        case TileKind::Spikes:        return "Spikes";
        case TileKind::Potion:        return "Potion";
        case TileKind::Exit:          return "Tile";
        default:                      return nullptr;
    }
}

const char* tile_kind_name(TileKind k) {
    switch (k) {
        case TileKind::Empty:         return "EMPTY";
        case TileKind::Floor:         return "FLOOR";
        case TileKind::Wall:          return "WALL";
        case TileKind::Spikes:        return "SPIKES";
        case TileKind::LooseFloor:    return "LOOSE_FLOOR";
        case TileKind::Gate:          return "GATE";
        case TileKind::GateOpen:      return "GATE";
        case TileKind::Exit:          return "EXIT";
        case TileKind::Potion:        return "POTION";
        case TileKind::PressurePlate: return "PRESSURE_PLATE";
    }
    return "EMPTY";
}

} // namespace

World::World() : kg(logosphere::ontology::registry()) {}

std::unique_ptr<World> create_world(const Level& lv, double time_limit_seconds) {
    auto w = std::make_unique<World>();
    w->time_limit_seconds = time_limit_seconds;

    w->kg.setMode(kg::KGMode::MINIMAL);
    w->kg.extendOntology(pop::ontology::registry());
    w->kg.set_event_bus(&w->bus);
    w->damage.set_event_bus(&w->bus);
    w->damage.set_kg(&w->kg);

    // --- the level and its interesting tiles ---
    w->level_e = w->kg.createEntity("Level");
    w->kg.setProperty(w->level_e, "grid_width", std::to_string(lv.w));
    w->kg.setProperty(w->level_e, "grid_height", std::to_string(lv.h));
    w->kg.setProperty(w->level_e, "time_limit_seconds", std::to_string(time_limit_seconds));

    std::vector<kg::EntityID> gates;
    std::vector<kg::EntityID> plates;
    for (int y = 0; y < lv.h; ++y) {
        for (int x = 0; x < lv.w; ++x) {
            const TileKind k = lv.at(x, y);
            const char* type = tile_entity_type(k);
            if (!type) continue;

            const kg::EntityID t = w->kg.createEntity(type);
            w->kg.setProperty(t, "tile_x", std::to_string(x));
            w->kg.setProperty(t, "tile_y", std::to_string(y));
            w->kg.setProperty(t, "tile_kind", tile_kind_name(k));
            w->kg.createRelation(w->level_e, "CONTAINS", t);

            if (k == TileKind::Gate || k == TileKind::GateOpen) {
                w->kg.setProperty(t, "gate_state",
                                  k == TileKind::Gate ? "CLOSED" : "OPEN");
                gates.push_back(t);
            } else if (k == TileKind::PressurePlate) {
                plates.push_back(t);
            } else if (k == TileKind::Potion) {
                w->kg.setProperty(t, "heal_amount", "100.0");
            }
        }
    }
    // Each plate manages every gate, matching open_all_gates() in the
    // pure tier. GAME_DESIGN.md S4 notes the per-gate linkage a fuller
    // game would want here.
    for (auto plate : plates) {
        for (auto gate : gates) {
            w->kg.createRelation(plate, "MANAGES", gate);
        }
    }

    // --- the Prince ---
    w->prince_e = w->kg.createEntity("Prince");
    build_body(w->kg, w->prince_e);
    arm_limp_rule(w->kg, w->prince_e);
    w->damage.register_entity(w->prince_e, 100.0f);

    // The sword hangs off the sword arm. It carries a manipulation
    // capability of its own, and the arm's cascade rule drags it down
    // when the arm is destroyed - the pattern proven by
    // tests/test_capability_system.cpp.
    w->sword_e = w->kg.createEntity("Sword");
    w->kg.setProperty(w->sword_e, "cap_list", "manipulation");
    w->kg.setProperty(w->sword_e, "cap.manipulation.weight", "1.0");
    w->kg.createRelation(w->prince_e, "HAS_PART", w->sword_e);
    if (const kg::EntityID arm = find_part(w->kg, w->prince_e, "right_arm");
        arm != kg::INVALID_ENTITY) {
        w->kg.createRelation(arm, "SUPPORTS", w->sword_e);
        w->kg.setProperty(arm, "rule.0.trigger", "destroyed");
        w->kg.setProperty(arm, "rule.0.cascade", "relation:SUPPORTS:0.5");
    }
    // Two arms plus the blade all feed manipulation.
    w->kg.setProperty(w->prince_e, "cap.manipulation.expected_count", "3");

    // --- the Guard ---
    w->guard_e = w->kg.createEntity("Guard");
    build_body(w->kg, w->guard_e);
    w->kg.setProperty(w->guard_e, "sword_skill", "0.6");
    w->damage.register_entity(w->guard_e, 100.0f);

    w->cached_speed_scale = prince_speed_scale(*w);
    return w;
}

void sync_character(World& w, kg::EntityID who, const Character& c) {
    if (who == kg::INVALID_ENTITY) return;
    w.kg.setProperty(who, "tile_x", std::to_string(c.x));
    w.kg.setProperty(who, "tile_y", std::to_string(c.y));
    w.kg.setProperty(who, "facing", std::to_string(c.facing));
    w.kg.setProperty(who, "movement_state", to_string(c.state));
}

float prince_speed_scale(World& w) {
    CapabilityProfile cap = CapabilityProfile::compute_from_kg(
        w.kg, w.prince_e, kPrinceMass, kPrinceLegLength, kPrinceHeight);
    DynamicsParams dyn = DynamicsParams::from_capability(cap);

    const float base = baseline_run_speed();
    if (base <= 0.0f) return 1.0f;
    return std::max(0.05f, std::min(1.0f, dyn.max_run_speed / base));
}

void apply_fall_damage(World& w, int hp_cost) {
    if (hp_cost <= 0) return;
    const float per_leg = static_cast<float>(hp_cost) * 0.5f;
    w.damage.apply_to_body_part(w.prince_e, "left_leg", per_leg, DamageType::Blunt);
    w.damage.apply_to_body_part(w.prince_e, "right_leg", per_leg, DamageType::Blunt);
    w.cached_speed_scale = prince_speed_scale(w);
}

void apply_sword_damage(World& w, kg::EntityID who, int hp_cost) {
    if (hp_cost <= 0 || who == kg::INVALID_ENTITY) return;
    w.damage.apply_to_body_part(who, "torso", static_cast<float>(hp_cost), DamageType::Slash);
    if (who == w.prince_e) {
        w.cached_speed_scale = prince_speed_scale(w);
    }
}

void heal_prince_fully(World& w) {
    w.damage.heal(w.prince_e, 1000.0f);
    for (auto part : w.kg.getRelated(w.prince_e, "HAS_PART")) {
        const std::string max_health = w.kg.getProperty(part, "max_health");
        if (!max_health.empty()) {
            w.kg.setProperty(part, "health", max_health);
        }
    }
    w.cached_speed_scale = prince_speed_scale(w);
}

void advance_clock(double dt_seconds) {
    GameTime::advance(dt_seconds);
}

double seconds_remaining(const World& w) {
    return std::max(0.0, w.time_limit_seconds - GameTime::get_current_time());
}

bool time_expired(const World& w) {
    return seconds_remaining(w) <= 0.0;
}

} // namespace pop
