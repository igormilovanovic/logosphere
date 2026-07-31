# Changelog

All notable changes to Logosphere are documented here. The format
follows [Keep a Changelog](https://keepachangelog.com/); versions
follow [Semantic Versioning](https://semver.org) on a 0.x line
(minor versions may break public API until 1.0).

## [Unreleased]

- **Fix: `IApplication::render_game()` was never called, and
  `Engine::get_draw_surface()` pointed at the wrong buffer.** Building
  the Prince of Persia GUI (below) surfaced two dead-on-arrival pieces of
  the app/HUD-overlay contract: `render_game()` — documented as "called
  after engine rendering, before display" — had no call site anywhere in
  `Engine`, and `get_draw_surface()` returned `draw_surface_` (the 3D
  scene buffer), not `overlay_surface_` (the `ui_buffer_` plane
  `draw_ui_overlays()` actually composites onto the presented frame) —
  despite its own doc comment describing it as the surface for "UI
  widgets, HUDs, and debug overlays." Any app drawing through
  `get_draw_surface()` rendered into a buffer nothing read back from
  under GPU rasterization, and `render_game()` overrides were silently
  never invoked. Fixed in `src/core/engine.h`/`engine.cpp`: retargeted
  `get_draw_surface()` to `overlay_surface_`, and added the missing
  `application_->render_game()` call in `draw_ui_overlays()`, after the
  frame's dirty-region clear and before `ui_system_->render()`.
- **Prince of Persia windowed GUI.** `examples/pop/` gained a `full`-
  profile macOS frontend (`pop_gui`) alongside the console one, drawing
  the identical, unmodified tile simulation through the engine's existing
  `IDrawSurface` interface rather than rebuilding the game on the physics
  engine. A second `IDrawSurface` implementation (`SvgDrawSurface`) lets a
  new tool, `pop_record_demo`, replay a scripted winning route through the
  same drawing code to record `demo_gui.svg` — so the recording is
  pixel-for-pixel what the live window draws. The game loop (`Game`,
  `tick()`) moved out of `main.cpp` into `game.h`/`game.cpp` so the
  console, GUI and demo recorder all drive one simulation.
- **Prince of Persia example.** A headless/console platformer
  (`examples/pop/`) built on the knowledge graph: a tile level with
  spikes, a collapsing floor, a pressure-plate gate and a sword-fighting
  guard, against a sixty-minute countdown. Notably it lets the engine do
  the work rather than hand-coding it — the Prince limps because a
  `health_below:50` response rule on his legs feeds
  `CapabilityProfile` → `DynamicsParams` → movement speed, and his sword
  degrades with the arm that holds it through a `SUPPORTS` cascade.
  Splits into a pure-logic tier that links no library at all and a
  knowledge-graph tier, so three of its four test suites need no engine.
- **Tic-Tac-Toe example.** A headless/console example game
  (`examples/tictactoe/`) driven entirely through `kg::KGModule` and the
  ontology-extension/event-bus APIs, with no rendering or `IApplication`
  dependency, so it builds under every `LOGOSPHERE_PROFILE` (`core`,
  `physics`, `full`) instead of `full`-only like the other examples.

## [0.2.0] - 2026-07-30

First public release. Everything below describes the engine as it
ships today.

### The engine

- **Particle-first world model.** Walls, creatures, trees, terrain,
  fire: all particles with mass, friction, contacts, and
  constraints. Every particle is a node in a queryable knowledge
  graph. The world turtle (an absolute floor at z = 0) is the only
  immovable thing.
- **Software rasterization + Metal compute.** No OpenGL, Vulkan, or
  DirectX: a software rasterizer writes a direct framebuffer, with
  Metal compute shaders for shadow rays, soft shadows, SSAO, SSGI,
  and DDGI probes.
- **Physics V4.** Sequential-impulse solver with SAT face-clipping
  manifolds, speculative contacts, momentum-based sleep/wake, a
  speed-capped Baumgarte push-out, gluon constraint family
  (nail / organic / angular drives) with cluster-aware structural
  damping, and an absolute turtle boundary.
- **Knowledge graph + ontology.** LinkML-defined type system with
  generated C++ registries, runtime extension, schema-validated
  KG operations (the LLM-facing creation grammar), typed event
  journal with reader cursors, and a query algebra with prompt-ready
  renderers.
- **Humanoid locomotion.** Kinematic-root gait (stance foot pinned,
  hips derived), two-bone IK in the committed yaw frame, an
  eyes-head-torso-hips yaw cascade with per-segment time constants,
  twist-step replanting, and a permanent particle write-tracer for
  causal debugging.
- **Worldgen.** Space-colonization trees with species presets and
  growth time-lapse, grass with painterly clustered distribution,
  rocks (scenery and gluon-bonded physics boulders), layered
  strata ground with a settle-based earth preset, streamed chunked
  terrain, butterflies, and full humanoid rigs.
- **Celestial system.** Sun, moons, and stars as real orbiting
  particles far enough that only their light enters the frame;
  color and emission curves keyed to the day fraction; time
  acceleration with exact-hour arrival.
- **LLM integration.** Engine-side HTTP client (Anthropic, OpenAI,
  local servers) with prompt caching support, plus the KG-ops
  grammar that lets a model create and mutate world state under
  schema validation.
- **Three example games.** Logogenesis (conversational world
  creation), Eden (knowledge-garden tableau), Logotron (light-cycle
  arena with an LLM director).

### Known issues

- Small blockers close to the ground produce a sub-pixel penumbra
  kernel, so their shadow edges render hard instead of soft
  (`test_shadow_penumbra_softness` documents the collapse and
  ratchets it from regressing further).
- No CMake install/export surface yet: Logosphere builds in-tree and
  games live in `examples/`; `find_package(logosphere)` consumption
  is the first post-release packaging milestone.

- A heavy boulder impact on layered ground can ripple outward into
  an oversized explosion of tiles. Physics work in progress; the
  crater contract (local splash, far field still, bedrock intact)
  is enforced by test at moderate energies.
- GPU frame stalls during BVH rebuilds on chunk streaming under
  very high particle counts.
- The ontology regeneration toolchain (LinkML C++ generator) is not
  yet published; generated sources are committed, so regeneration
  is only needed when editing schemas.
- Windows builds of the headless core are structurally supported
  but untested.
