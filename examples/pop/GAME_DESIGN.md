# Prince of Persia — Game Design

Working design doc for the `examples/pop/` example. Source comments cite
these sections by number (`level.h` → S2, `prince.h` → S3, and so on), so
keep the numbering stable.

Status markers: ✅ shipped · 🔜 next · 📋 considered, not planned

---

## S1. Pitch ✅

A single palace level, viewed from the side. Run, jump and climb past a
spiked pit, a floor that gives way, and a locked gate; beat one guard in a
sword fight; reach the door before the sand runs out.

The point is not to reproduce Jordan Mechner's game. It is to show what a
Logosphere game looks like when it needs none of the engine's rendering:
the knowledge graph holds the world, the capability system decides how
fast the Prince moves, the damage system runs the duel, and `GameTime`
counts down — all in the headless `core` profile, on any C++17 toolchain.

## S2. Tiles and level geometry ✅

`src/level.h`. One flat grid, side-on, y growing downward. Cells have two
disjoint roles:

- **passable** — a character's body occupies the cell (`Empty`, `Exit`,
  `GateOpen`)
- **support** — floor-level material a character stands *on top of*
  (`Floor`, `LooseFloor`, `Spikes`, `Potion`, `PressurePlate`, `Wall`)

A character at `(x, y)` is grounded when `(x, y+1)` is a support tile. No
tile is ever both, which is asserted in `tests/test_pop_level.cpp`. This
split is what makes "walk along the floor and hit the spikes set into it"
fall out without a special case: the spikes are the floor.

Out-of-bounds reads return `Wall`, so the level is implicitly sealed and
no caller needs its own bounds checks.

Levels are authored as ASCII (legend in `level.h`), which keeps them
readable in source and trivial to fixture in tests.

**📋 Not planned:** multiple rooms as separate data structures. The
original's 10×3 room grid was a memory constraint; one flat grid is
simpler and the renderer shows the whole level anyway.

## S3. Movement ✅

`src/prince.h`. A tick-based state machine, not a physics simulation —
which is also how the original works. Every transition is a pure function
of `(character, level, action, speed_scale)`.

States: `Standing`, `Running`, `Crouching`, `StandJumping`, `RunJumping`,
`Climbing`, `Hanging`, `Falling`, `Fighting`, `Dead`, `Escaped`.

Rules:

| Rule | Value | Why |
| --- | --- | --- |
| Ticks per tile | 2 at full speed | Slow enough that `speed_scale` is visible in play |
| Standing jump | 2 tiles | Clears a one-tile gap |
| Running jump | 3 tiles | Rewards approaching at a run |
| Safe fall | 1 storey | Short drops are free |
| Fall damage | 34/storey beyond that | Three wounds kill, as in the original |
| Fatal fall | 3 storeys | |
| Loose floor | collapses 3 ticks after being stepped on | See below |

`speed_scale` is the one number the engine feeds in. It arrives from the
knowledge graph by way of `CapabilityProfile` → `DynamicsParams`
(S6), and it divides into the per-tile tick cost, so a wounded Prince is
genuinely slower.

**The loose floor is where that becomes a mechanic rather than a
statistic.** Three ticks is long enough to cross at full speed and too
short when limping. Wound a leg and a tile you crossed safely five
seconds ago will drop you.

**🔜 Crouching** is modelled and reachable (`s` on solid ground) but not
yet load-bearing — nothing in the level requires it. A fuller game would
use it for half-height gaps under gates.

## S4. Traps, gates and pickups ✅

- **Spikes** — fatal on arrival, no exceptions.
- **Loose floor** — arms on arrival, collapses to `Empty` when its timer
  expires. Whoever armed it owns the timer, so it collapses behind them.
- **Pressure plate** — raises every gate in the level.
- **Potion** — restores the Prince fully, including limb health, which
  clears any limp (S6). Consumed, leaving plain floor.
- **Exit** — reaching the cell ends the level.

**🔜 Per-gate linkage.** A plate currently opens *all* gates, in both the
pure tier (`open_all_gates`) and the graph (each plate `MANAGES` every
gate). One plate and one gate ship, so the distinction is invisible; a
second gate would need the relation consulted rather than the blanket
call.

## S5. Sword fighting ✅

`src/combat.h`. Both duellists act each tick.

- A **strike** lands unless the opponent is parrying, and costs 34 —
  three clean hits kill.
- A **parry** blocks completely and leaves the attacker open: being
  parried costs 2 ticks of recovery against 1 for a landed blow.
- Mutual strikes trade blows.
- Health lives on `Character`, not `Fighter`, so movement and combat
  share one source of truth.

The guard parries roughly `skill × 50%` of the time and swings otherwise,
so a skilled guard is defensive but never inert. Its RNG state is owned by
the caller, so a duel replays exactly from a seed — which is what lets
`tests/test_pop_combat.cpp` assert on outcomes.

The resulting fight has real technique. Verified by playing it:
spamming attacks loses (the guard parries and counters), pure defence
also loses (you cannot disengage — the duel holds you until someone
falls), and mixing parries with two-hit ripostes wins with about a third
of your health left.

**📋 Not planned:** advance/retreat as separate actions, guard variety,
disarming. One guard is the vertical slice.

## S6. The engine doing the work ✅

`src/world.h`. This is why the example exists. Everything below is engine
machinery, not game code:

- **Body plans** — `body_plan::declare_biped` plus six
  `create_capability_part` calls give the Prince and the guard legs
  (locomotion), arms (manipulation), a torso and a head.
- **The limp is a response rule, not an `if`.** Each leg carries
  `rule.0.trigger = "health_below:50"` and `rule.0.effect =
  "speed_cap:0.6"`. Nothing in the game reads it;
  `CapabilityProfile::compute_from_kg` evaluates it, `DynamicsParams`
  turns the result into `max_run_speed`, and `world.cpp` normalises that
  against an unhurt baseline into `speed_scale`.
- **The sword degrades with the arm that holds it.** The blade is a
  `HAS_PART` of the Prince carrying its own manipulation capability, and
  the sword arm `SUPPORTS` it with `rule.0.cascade =
  "relation:SUPPORTS:0.5"`. Destroy the arm and the blade is halved. This
  is the pattern from `tests/test_capability_system.cpp`.
- **Damage** goes through `DamageSystem` — `Slash` for blades, `Blunt`
  split across both legs for falls — which writes the graph and emits on
  `bus.damage()` and `bus.deaths()`.
- **The countdown** is `GameTime`, advanced one second per tick.

Constants: 68 kg, 0.92 m legs, 1.78 m tall, 210 ms reflexes, 520 W grit —
a lighter, quicker build than the engine's 75 kg / 250 ms reference human.

**Caching matters here.** `compute_from_kg` re-fires any `emit_event`
rules on every call and prints a `[CAP]` line, so `world.cpp` recomputes
`speed_scale` only when health actually changes, never per tick.

**Damage contract:** the pure tiers are authoritative for hp within a
tick; every point is mirrored into `DamageSystem`, which owns the graph
side. Game code never writes health into the graph directly.

## S7. The shipped level ✅

`src/level_one.h`, 32×4. The Prince walks row 1 and stands on row 2.

| Column | Feature |
| --- | --- |
| 1 | start |
| 4 | gap, spikes below — must be jumped |
| 9 | loose tile |
| 15 | potion |
| 18 | pressure plate |
| 21 | gate, closed until the plate is stepped on |
| 27 | guard |
| 30 | exit |

`tests/test_pop_movement.cpp` walks this route and asserts it is
winnable, and separately asserts that walking into the gap instead of
jumping is fatal — so the level cannot silently rot.

## S8. What is not here ⛔

No AI beyond the guard's parry roll. No save/load. No audio. No score. No
second level. No LLM director — unlike logotron, this example is
deliberately deterministic, because determinism is what makes the whole
game testable in CI.

## S9. Testing ✅

Four suites, 238 assertions, all headless, all in the PR-gating CI job:

| Suite | Links | Covers |
| --- | --- | --- |
| `test_pop_level` | nothing | grid, legend, bounds, tile roles, shipped-level shape |
| `test_pop_movement` | nothing | every transition, plus the shipped route end to end |
| `test_pop_combat` | nothing | strikes, parries, recovery, determinism |
| `test_pop_world` | `logosphere_core` | ontology, body plans, capability→speed, damage, events, clock |

Three of the four link no library at all, which is the point of the tier
split (S10): the rules of the game are testable without an engine.

## S10. Code structure ✅

Two tiers, following `examples/logotron/src/cycle.h` (pure) and `arena.h`
(KG bridge):

- **Pure** — `level`, `prince`, `combat`, `render_ascii`. No engine, no
  knowledge graph, no I/O. Each header says so at the top.
- **KG bridge** — `world`. Owns every engine call.

`main.cpp` only orchestrates. Nothing in the example includes GLFW, Metal
or `core/engine.h`, which is what lets the whole thing build in the
`core` profile — see POP.md for the windowed path that would change that.

## Change log

- **v0.1** — vertical slice: one level, movement, traps, gate, one duel,
  countdown, four test suites, animated terminal demo.
