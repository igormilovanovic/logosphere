# Prince of Persia on Logosphere

A single palace level, side-on: run and jump past a spiked pit and a floor
that gives way, open a gate, beat a guard with a sword, and reach the door
before the sand runs out.

It is a headless console game. It builds under every `LOGOSPHERE_PROFILE`
(`core`, `physics`, `full`) — including plain Linux, where the full engine
(software rasterizer, Metal lighting, GLFW) does not build at all.

![Terminal recording of Prince of Persia on Logosphere](demo.svg)

## Build and play

```bash
cmake -S . -B build -G Ninja -DLOGOSPHERE_PROFILE=core   # or physics, or full on macOS
cmake --build build --target pop
./build/pop/pop
```

Commands take an optional repeat count, so `d 6` steps right six ticks:

| Key | Action | Key | Action |
| --- | --- | --- | --- |
| `a` | step left | `d` | step right |
| `w` | climb up / catch a ledge | `s` | climb down / crouch |
| `j` | jump | `.` | wait |
| `k` | strike | `p` | parry |
| `q` | quit | `?` | help |

A run that wins:

```
d 4   j   d 18   d 6   d 16   p   k 2   p   k 2   p   k 2   p   k 2   d 10
```

Approach the gap at a run before jumping. In the duel, mixing parries with
short ripostes wins; spamming strikes gets you killed, and so does pure
defence, because you cannot walk away from a fight.

Run the tests:

```bash
cmake --build build --target test_pop_level test_pop_movement test_pop_combat test_pop_world
./build/test_pop_level && ./build/test_pop_movement && \
./build/test_pop_combat && ./build/test_pop_world
```

## How it maps onto the engine

Logosphere's most portable layer is the knowledge graph and the systems
built on it — the layer every example game uses, and the one that needs no
rendering, physics or platform code. This example is built on exactly
that, and leans on it for things a game would normally hand-code:

- **The Prince limps because of an engine rule, not an `if`.** Each leg
  carries `rule.0.trigger = "health_below:50"` and `rule.0.effect =
  "speed_cap:0.6"`. No game code reads it.
  `CapabilityProfile::compute_from_kg` evaluates the rule,
  `DynamicsParams::from_capability` turns the result into `max_run_speed`,
  and the movement code divides that into its per-tile tick cost. Wound a
  leg and the Prince is measurably slower — slow enough that a loose floor
  tile he crossed safely at full health will now drop him.
- **The sword degrades with the arm holding it.** The blade is a
  `HAS_PART` of the Prince with its own manipulation capability, and the
  sword arm `SUPPORTS` it with `rule.0.cascade = "relation:SUPPORTS:0.5"`
  — the cascade pattern from `tests/test_capability_system.cpp`.
- **`DamageSystem`** runs the duel (`DamageType::Slash`) and falls
  (`Blunt`, split across both legs), writing health into the graph and
  emitting on `bus.damage()` and `bus.deaths()`.
- **`body_plan::declare_biped`** gives both fighters a real body — legs,
  arms, torso, head — each carrying health and a capability weight.
- **`GameTime`** is the countdown.
- **The event bus** carries everything: once `kg.set_event_bus(&bus)` is
  called, every property and relation write emits without the game asking.
- **The ontology** (`schema/pop.yaml`) declares `Level`, `Tile`, `Gate`,
  `PressurePlate`, `LooseFloor`, `Spikes`, `Potion`, `Prince`, `Guard` and
  `Sword` on top of the engine's base types. The registry in
  `src/generated/` is produced by the repo's own
  `scripts/generate_registry.py`.

The example deliberately does **not** use the physics solver. It builds
headless and would run, but the engine's gait system is XY-planar while a
side-view platformer is XZ, and there is no jump, ledge-grab or climb API
in the engine to build on. The original game is itself tile-and-state-
machine based, so a deterministic tile simulation is both more faithful
and fully testable in the cheapest profile. See GAME_DESIGN.md S3.

### A note on relations

`schema/pop.yaml` declares no new relation types, and that is deliberate.
`scripts/generate_registry.py` derives the registry's relation set purely
from the engine's `WorldRelationType` enum, so a game-declared relation
enum generates nothing — `examples/eden/schema/eden.yaml`'s
`EdenRelationType` is decorative. The game therefore reuses the engine's
own relations, as logotron does: `CONTAINS` for the level's tiles,
`HAS_PART` for body parts and the sword, `SUPPORTS` for the cascade, and
`MANAGES` for plate-to-gate. Character position rides on `tile_x` /
`tile_y` properties rather than a relation.

## Code structure

Two tiers, following `examples/logotron/src/cycle.h` and `arena.h`:

| Tier | Files | Depends on |
| --- | --- | --- |
| Pure logic | `level`, `prince`, `combat`, `render_ascii`, `level_one.h` | nothing |
| KG bridge | `world` | `logosphere_core` |

`main.cpp` only orchestrates. Three of the four test suites link no
library at all — the rules of the game are verifiable without an engine.

## Windowed (macOS) version

This example skips `Logosphere::IApplication` and `Engine`, which need the
`full` profile (macOS arm64, GLFW, Metal). To grow it into a windowed
example like `examples/eden/`:

1. Implement `class PopApp : public Logosphere::IApplication`
   (`include/application.h`, `docs/GAME_LAYER.md`). Keep it thin: it
   should call into the existing pure tiers, not reimplement them. Follow
   `examples/logotron/src/logotron_app.h`, and note the rule that keeps
   the headless build working — nothing that includes GLFW or
   `core/engine.h` may be pulled into a `logosphere_core`-only target.
2. Build the level out of particles. `ParticleSystem::create_floor_grid`
   already emits `is_at_rest = true` tiles that wake when disturbed, which
   is the engine's native loose-floor behaviour — flip
   `is_at_rest`/`wake_particle` instead of setting the tile to `Empty`.
   `StrataFloorGenerator::set_tile_skip_mask` will carve the gap from the
   tilemap.
3. Spikes become an interaction profile with a volume trigger, so
   `bus.volume()` reports entry and the game calls `DamageSystem` from the
   subscriber — replacing the arrival check in `resolve_arrival`.
4. Guards can come from `HumanoidGenerator` +
   `HumanoidLocomotion`, which already supply gait, foot planting and
   step-climbing, plus FK punch/guard animation clips that a sword swing
   could be built from (`include/logosphere/dynamics/animation_primitives.h`).
5. Route `handle_key` to the same `Action` enum the console loop uses.
6. Add a `full`-profile target. `examples/pop/CMakeLists.txt` is currently
   reached from the root `CMakeLists.txt` *above* the profile
   short-circuits; add the windowed executable in the same file guarded by
   `if(LOGOSPHERE_FULL)`, linking `logosphere`, `glfw` and the Cocoa /
   Metal / QuartzCore frameworks listed in `examples/eden/CMakeLists.txt`.
   Do not add a second `add_subdirectory`.

Be aware of two real gaps before starting: the engine has **no jump,
ledge-grab or climb API**, and its locomotion drives XY while a side view
needs XZ. Step climbing (`src/animation/humanoid_locomotion.cpp`) applies
a vertical impulse and is the closest thing to a jump to build from.
