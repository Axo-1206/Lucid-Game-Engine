# Lucid Engine

A game engine with an embedded data-oriented scripting language.

## Status

Phase 8 complete — engine rework done. The engine compiles, opens a
window, runs the ECS systems, loads and saves scenes, and the format
library boundary is tested. See `docs/Decision.md` for the full record
of design decisions across Phases 0–8.

---

## Vocabulary

Four terms are used precisely in this codebase. They are easy to
confuse and doing so causes real bugs.

- **Registry** — `lucid::sema::Registry`. The format library's
  declaration of node types, enums, handles, and phases. Read-only,
  process-lifetime. The engine *constructs* one from its own node
  definitions and passes it to `lucid::sema::compile()` and
  `lucid::serialization::deserialize()`. It is metadata about a
  node vocabulary; it holds no rows.

- **World** — `engine::ecs::World`. The engine's live table store.
  Every table of rows the engine reads and writes at runtime lives
  here: Entity, Transform, Camera, Player, and so on. It is mutated
  every frame by systems. It holds no metadata about node types.

- **Graph** — `lucid::sema::Graph`. The compiled output of a
  `.lucid` file. A flat, resolved, acyclic structure the engine
  loads and walks. Produced by `compile()`, serialized to a
  `.lucgraph` binary, deserialized at load time.

- **Scene** — `engine::Scene`. The description of a `.lscene` file:
  entities, their components, cameras, and metadata. Loaded from
  and saved to JSON. Applied to a `World` via `apply_to()`. A
  scene is data; a graph is behavior. They are separate.

A common mistake is to conflate `Registry` and `World`: the first is
metadata about what nodes exist; the second is the rows those nodes
operate on. A system touches the `World`; it never touches the
`Registry`.

See `docs/Decision.md` for the design decisions, `docs/notes/SceneFormat.md`
for the scene format spec, and `docs/notes/TriggerModel.md` for how a
Lucid graph expresses control flow.

---

## Building

### Prerequisites

- CMake 3.20+
- A C++17 compiler (MSVC 2019+, Clang 12+, GCC 10+)
- Git (for fetched dependencies)

### First-time setup

    cmake -S . -B build

CMake fetches GLFW, GLM, and the Lucid format library automatically
on the first configure. No submodule init is needed.

### Build

    cmake --build build --config Debug

### Run

    ./build/bin/lucid_game.exe
    ./build/bin/lucid_editor.exe

Press Escape to close the window.

### Tests

    cmake --build build --config Debug
    ctest --test-dir build -C Debug --output-on-failure

### Build options

- `-DLUCID_BUILD_EDITOR=OFF` — skip the editor (for shipping)
- `-DLUCID_BUILD_GAME=OFF` — skip the game executable
- `-DLUCID_BUILD_TESTS=ON` — build the test suite

---

## Structure

- `ecs/` — the engine's table store (`engine::ecs::World`, `Table`, `Column`, …)
- `engine/` — the engine: `core/`, `platform/`, `render/`, `scene/`, `systems/`, `nodes/`
- `apps/` — executables: `game/`, `editor/`, `editor_main/`
- `tests/` — unit and integration tests (Catch2)
- `externals/` — vendored dependencies (GLAD, stb, nlohmann/json)
- `assets/` — test and demo assets (scenes, graphs)
- `docs/` — design documents; start with `docs/README.md`

For the full directory listing with file-level detail, see
`docs/FileStructure.md`.

---

## License

TBD
