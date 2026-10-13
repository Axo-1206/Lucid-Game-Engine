# `docs/Decision.md`

The decisions are recorded in the order they were made. Each answers a question that was genuinely open at the time. Where an earlier decision's text is now inaccurate, the file says so rather than rewriting history.

Cross-references: `docs/FileStructure.md` for the repository layout, `docs/notes/SceneFormat.md` for the scene format spec, `docs/notes/TriggerModel.md` for how a graph expresses control flow, `docs/notes/WorldLifecycle.md` for the world's empty operations, and `docs/notes/PackageComposition.md` for the package policy.

---

## Format library freeze

During the engine rework (Phases 0–8), the format library (`lucid::core`, `lucid::parser`, `lucid::formatter`, `lucid::sema`, `lucid::serialization`, the `lucid` CLI) is frozen. No files under those namespaces change. The engine includes `sema/Registry.hpp` and `sema/Graph.hpp` and builds a `Registry` struct from its own node definitions; it does not modify the library.

The boundary:

```
lucid::*           the format library
engine::*          the engine
engine::ecs::*     the engine's storage layer (World, Table, Column, …)
engine::graph::*   the graph runtime (walks a compiled Graph against a World)
```

Vocabulary, one word one meaning:

```
Registry          →  lucid::sema::Registry
                     The format library's node vocabulary struct.
                     The engine fills it in; the format library reads it.

ComponentRegistry →  engine::ComponentRegistry
                     The scene layer's component codec table. Maps a
                     table name to a load/save pair. Populated by the
                     engine's core components and by the packages.
                     Not the format library's Registry.

World             →  engine::ecs::World
                     The engine's live table store.

Graph             →  lucid::sema::Graph
                     The format library's compiled output of a .lucid file.

Scene             →  engine::Scene
                     The description of entities and cameras read from
                     a .lscene file. Schema type; not runtime state.
```

The distinction between `Registry` and `ComponentRegistry` is load-bearing. They have similar names and unrelated jobs. See `docs/notes/WorldLifecycle.md` and `docs/notes/PackageComposition.md`.

---

## Decision 1 — What `World` is

**What is `engine::ecs::World`?** The engine's live table store — the container of all entity, component, and camera rows for the currently loaded game state. One per engine instance.

- **Does it know about scenes?** No. `World` has no concept of a scene. It has tables.
- **Does it own the tables?** Yes. It is the sole owner. A `Scene` does not reference it.
- **What is its namespace?** `engine::ecs`. Not `lucid`. Not `engine` alone.
- **What is its relationship to `lucid::sema::Registry`?** None at runtime. The engine builds a `lucid::sema::Registry` from its own node definitions; that struct is separate from `World` and is never stored in it.
- **Is `World` copyable?** No. It is an owning, mutable store. Copy is deleted; move is allowed.
- **Does `World` clear itself?** Yes, in two ways. `World::clear()` removes every non-preserved table and keeps every preserved table with its rows. `World::clear_rows()` empties every table's rows and keeps every schema. See Decision 9.

Migration from old design:

> Old: `lucid::Runtime` was the table store. `Scene` held a `Runtime&` and mutated it. `Scene::clear()` cleared the runtime's tables.
>
> New: `engine::ecs::World` is the table store. `Scene` does not hold a `World&`. `World::clear()` and `World::clear_rows()` are the two resets.

---

## Decision 2 — What `Scene` is

**What is a `Scene`?** A description — the entities, components, cameras, and metadata read from a `.lscene` file. It does not hold a `World&`. It is a value that can be applied to a world.

- **How many scenes can a world have?** At most one active at a time (Model A). Model B (additive partitioning) and Model C (one world per scene) were rejected for now: Model B requires partition identity to be tracked throughout the ECS, which adds complexity before it is needed; Model C duplicates renderer and system state per scene.
- **Does `Scene` have `apply_to` / `extract_from`?**
  - `Scene::apply_to(World&, const ComponentRegistry&, ApplyMode)` where `ApplyMode` is `Clear` (default) or `Additive`.
  - `Scene::extract_from(const World&, const ComponentRegistry&)` — returns current world state as a `Scene`.
- **Does `Scene` have `clear()`?** No. Clearing is a `World` operation. To unload the current scene: `world.clear_rows()`.
- **What identity does `Scene` hold?**
  - A `name` — a display name, not a unique key, not the file path.
  - The active camera by name, not by `RowRef` (see Decision 3).
  - The entity-id allocator value to restore after `apply_to`, or 0 to compute it from the loaded entities.
- **Is `Scene` copyable?** Yes. It is a description.
- **File and type:** `engine/include/engine/scene/scene.h`, type `engine::Scene`.

**What does a `Scene` hold internally?** The file's JSON text, not a typed model of its entities and cameras. `apply_to` parses the text. This keeps `scene.h` free of `nlohmann/json`, keeps `EntityDesc` from being a half-typed mirror of the file, and defers component parsing to `apply_to` where the `ComponentRegistry` owns it. A `Scene` built by `SceneLoader_load` holds the file's text; a `Scene` built by `Scene::extract_from` holds JSON text produced by `SceneSaver_from_world`. The two are symmetric.

Migration from old design:

> Old: `Scene(lucid::Runtime& rt)`, `scene.load_from_file(path)` cleared `rt` and loaded into it, `scene.clear()` cleared `rt`.
>
> New: `Scene{}` is default-constructible. `SceneLoader_load(path)` returns a `Scene`. `SceneLoader_load_into(world, components, path)` loads and applies in one call. `scene.apply_to(world, components, ApplyMode::Clear)`. `world.clear_rows()` is the only clear.

---

## Decision 3 — Where cameras live

- **Are cameras entities?** No.
- **Where do they live?** In the `World`'s `Engine_Camera` table, keyed by `name` (primary column). A scene description carries its cameras as JSON; `load_camera` writes them into the table.
- **Is `CameraTag` kept?** No. The comment `// not an entity component` was an apology; this decision removes the need for it. The camera table's name is `engine::kCameraTableName`, declared in `engine/ecs/core/component_tags.h` next to the entity-component tags, with a comment explaining why it is a constant and not a tag. There is no `add_component<CameraTag>` instantiation.
- **How is the active camera identified?** In the `Scene`, by name. The engine loop looks up the name in the `Engine_Camera` table when it needs the camera. The name is canonical; a `RowRef` is a short-lived lookup result, not stored.
- **Three camera types and their roles:**
  - `CameraDesc` — the schema type. What is serialized, what a camera table row mirrors. Lives in `engine/include/engine/render/camera.h`.
  - The `Camera` table row — runtime state. Columns mirror `CameraDesc`.
  - `Camera2D` — the renderer's view struct. Produced by `to_camera2d(const CameraDesc&, int w, int h)` or by reading a camera row and converting.
- **Who sets the renderer's camera each frame?** The engine loop. It looks up the world's camera row by the scene's active camera name, reads the row into a `CameraDesc`, converts to `Camera2D`, calls `renderer.set_camera_2d(cam2d)`. The renderer never reads the `World`. The `World` never talks to the renderer. The loop is the bridge.
- **Camera helpers:** `find_camera(world, name)`, `read_camera_row(world, ref)`, `write_camera_row(world, desc)`, `to_camera2d(desc, w, h)` — all in `engine/render/camera.h`.

---

## Decision 4 — Schema type names

Two options were considered:

- **Option A — `Desc` suffix on schema types:** `CameraDesc`. Runtime types keep their names: `World`, `Table`, `Camera2D`.
- **Option B — no suffix:** `Camera`. Collides with the table name and the runtime type.

**Chosen: Option A.** The `Desc` suffix makes it unambiguous that a name is the description, not the runtime state. `CameraDesc::name` reads unambiguously; `Camera::name` invites the question of whether `Camera` is the schema or the row.

In practice, the top-level type exposed from `scene.h` remains `engine::Scene` for the scene-as-a-whole. The camera schema type is `CameraDesc` in `camera.h`. There is no `EntityDesc` in the current code: entity data is parsed from the scene's JSON text in `apply_to`, not modeled as a typed struct. If a typed entity description is introduced later, it takes the `Desc` suffix.

---

## Decision 5 — What `.lscene` is *(superseded by Decision 8)*

Decision 5 deferred the `.lscene` format choice. Decision 8 resolves it: `.lscene` is JSON, its schema is engine-owned and versioned independently, and its only interface to the format library is a `Script` component holding a `.lucgraph` path.

See Decision 8 for the full rationale.

---

## Decision 6 — Entity ID stability across load

- **Are entity IDs serialized?** Yes. Each entity object in the scene file carries its `id`. Parent references would break across load otherwise.
- **Are IDs stable across save/load?** Yes. A scene saved with entity id 1 loads with entity id 1.
- **What happens to the world's `next_entity_id` after load?** It advances past the max loaded ID. `apply_json_to_world` reads `engine_state.next_entity_id` from the scene if the scene carries one; otherwise it computes `max(loaded_id) + 1`. It writes the value into `Engine_EngineState`. The caller does not need to remember.

The exact rule:

> After `Scene::apply_to(world, ...)`, the world's entity-id allocator is set to the scene's `engine_state.next_entity_id` if present, or to `max(entity.id) + 1` computed from the loaded entities otherwise.

Covered by `tests/engine/test_entity.cpp`: the `id allocator after a scene apply` and `scene's next_entity_id is used after apply` tests.

Bug resolved by this decision:

> Old: no test asserted this. `allocate_entity_id` could collide after load.

---

## Decision 7 — `Engine_EngineState`

- **What is `Engine_EngineState`?** A core table holding engine-lifetime state. Current known contents: a `next_entity_id` row. It is engine bookkeeping, not a package's domain.
- **Is it serialized per scene?** No. It is engine-lifetime state, not scene data. The `.lscene` format carries an `"engine_state"` block so the loader can restore the id allocator after load (see Decision 6). The table is not round-tripped as part of a scene description.
- **Is it cleared by `World::clear()`?** The table is preserved; its rows survive. `clear()` removes non-preserved tables and keeps preserved tables with their rows. The id allocator keeps counting. The next entity after a `clear()` has id N+1, not 1. This is safer (ids never reuse within a session) and matches common engine behavior.
- **Is it cleared by `World::clear_rows()`?** The table is preserved and its rows are cleared, because `clear_rows()` empties every table. `apply_to(Clear)` uses `clear_rows()` and re-seeds the `next_entity_id` row from the scene. A caller who uses `clear_rows()` directly must re-seed the row if it wants `allocate_entity_id` to work.
- **What else lives in it?** Only `next_entity_id` currently. Adding fields is allowed; removing them is a breaking change.
- **What is its exact name?** `Engine_EngineState` — one underscore after the prefix, no leading underscore. The `Engine_` prefix already marks it internal.

See `docs/notes/WorldLifecycle.md` for the full note on the three empty operations.

---

## Decision 8 — The `.lscene` format *(resolves Decision 5)*

`.lscene` files are JSON. The engine's scene loader and saver read and write JSON via nlohmann/json. The scene format is an independent, engine-owned schema with its own version (`kSceneSchemaVersion`), its own schema header (`engine/include/engine/scene/scene_schema.h`), its own spec (`docs/notes/SceneFormat.md`), and its own evolution schedule.

`.lucgraph` files are the Lucid format library's binary format for behavior graphs. They are read and written only by `lucid::serialization`.

The two are different things. A scene is data — entities, components, cameras. A graph is behavior — nodes, subscriptions, phases. Their shapes are different, their lifetimes are different, their versioning is different. The only interface between them is a `Script` component that holds a `.lucgraph` file path. The scene neither compiles nor loads the graph; the runtime does, passing the same node registry that was used at compile time.

Rationale:

- Data and behavior are different categories. Expressing one in the other's format bends the format.
- The format library's `Registry`, `Graph`, and `NodeInstance` are designed for graphs, not entity lists. Coupling them to the ECS is a category error.
- The registry fingerprint is meaningful for graphs (node vocabularies must match) and meaningless for scenes (a scene's compatibility is about table schemas).
- The scene schema evolves frequently — new components, new columns — and must not invalidate compiled graphs. The behavior format evolves rarely and must not be invalidated by scene changes.
- Independent evolution is what makes each layer stable.

See `docs/notes/SceneFormat.md` for the full spec and the schema's evolution rules. See Decision 11 for when the runtime loads a graph.

---

## Decision 9 — Three operations for emptying a world

`World::clear()` removes non-preserved tables and keeps preserved tables with their rows. `World::clear_rows()` empties every table and keeps every schema. `Scene::apply_to(world, ApplyMode::Clear)` calls `clear_rows()` and re-seeds `Engine_EngineState.next_entity_id` from the scene.

The three exist because three different things are needed:

- A session-level reset that keeps the engine's counters (`clear()`). The entity-id allocator must not repeat an id within a session.
- A scene-level reset that keeps the engine's schema so a new scene can populate it (`clear_rows()`).
- A scene load that does the scene-level reset and then writes the scene's entities and cameras (`apply_to(Clear)`).

`clear()` is not a synonym for `clear_rows()`. Calling `clear()` before populating a scene removes `Engine_Entity` and the next `create_entity` throws. Calling `clear_rows()` and then `allocate_entity_id` throws unless the state row is re-seeded.

Preservation is a per-table flag, set by `World::mark_preserved(name)`. The flag is keyed by name and is order-independent: it may be set before or after the table is created. `Engine_EngineState` is marked preserved in `setup_core_tables`.

The arrangement holds while there is exactly one engine-lifetime table and one engine-lifetime row. If a second appears, the right shape is a `World::clear_for_scene(callback)` that re-seeds engine-lifetime state through a callback. See `docs/notes/WorldLifecycle.md` for the full note.

Resolved by this decision: the gap where `apply_to(Clear)` called `world.clear()` and removed the core schema, causing "Entity table not set up" on the next `create_entity`. The fix moved `apply_to(Clear)` to `clear_rows()` and re-seeded the state row.

---

## Decision 10 — Package composition

A package contributes node types, executors, types (enums and handles), and tables (with component codecs). It registers them through `PackageBuilder` at startup. It is a C++ object, not a plugin and not a runtime object.

A compound type (a vector, a matrix) is either a handle plus a table, with operations as nodes, or a C++ struct whose fields are declared to `.lucid` as primitives. A package cannot add a new primitive to the format library's type system.

A component table is registered with `add_component(table_name, table_creator, codec)`: one call registers the table and its codec. A table that is not an entity component (the camera table, a package's internal table) uses `add_table(name, table_creator)`, which registers no codec; the table's rows are not serialized by the scene loader and saver.

A node's executor is a C++ function. It reads and writes its own package's tables, and it calls services. The service is either the engine's (`Services::physics`) or the package's own internal object. A package does not call another package's executor or read another package's table directly; it uses the other package's nodes.

See `docs/notes/PackageComposition.md` for the full policy.

---

## Decision 11 — Graph load lifecycle

`GraphRuntime::load()` walks the world's `Engine_Script` table and loads each `.lucgraph` path. It is idempotent: it clears `graphs_` first, then reloads. It does not poll; it does not read `Engine_Script` on tick.

The engine loop calls `runtime.load()` after `scene.apply_to` and before the first `runtime.tick()` for that scene. On a scene reload, the caller applies the scene and then calls `load()` again. A hot-reload key calls `load()` after writing the new Script row.

A failed load logs and returns false. The runtime keeps the graphs it did load; the caller decides whether to abort or continue.

See `docs/notes/Dataflow.md` §7 for the loop's step order.

Rationale for the explicit call: a lazy load (the runtime polls `Engine_Script` on first tick) hides a load failure inside a tick. The explicit call makes the load failure reportable at a known point: after scene apply, before the first tick. The caller logs it and decides.

---

## Decision 12 — Two ways to apply a package manager

`PackageManager::apply(World&)` creates the packages' tables and builds the registries. `PackageManager::apply_registration_only()` builds the registries without a world. The two are mutually exclusive; a manager is applied once, by one or the other. `applied()` is true after either.

`apply(World&)` is what the game and the editor call: they want the tables. `apply_registration_only()` is for callers that only need the registry: the `default_registry()` shim, a test that checks node vocabulary, a tool that validates a package's registrations without creating a world.

A manager does not create a scratch `World` to satisfy its own `apply` contract. The caller says which kind of apply it wants. A second call to either method logs a warning and returns `!has_conflicts()`; the manager's state is not corrupted.

Rationale: the scratch `World` — a `World` constructed inside `apply` purely to satisfy the parameter, then discarded — hides ownership and invites the reader to wonder who owns the tables. A named method that says "no world" is clearer.

---

## Decision 13 — No friends; `Scene` is an aggregate

`Scene` is an aggregate. The loader and saver are free functions declared in `scene.h` (`SceneLoader_load`, `SceneLoader_load_into`, `SceneSaver_save`, `SceneSaver_from_world`). They operate on `Scene`'s public fields. There is no `friend class SceneLoader`.

Rationale: friendship exists to give a helper access to private state. If the helper can work through the public surface, it does not need friendship. `Scene`'s public surface is its fields; the loader reads them and constructs a `Scene`; the saver reads them. No helper needs private access.

---

## Decision 14 — `ComponentRegistry` lives in `scene/`, not `registry/`

`ComponentRegistry` (the codec dispatch table) lives in `engine/include/engine/scene/component_registry.h`. It is not under `registry/`.

The reason: `registry/` is the node-vocabulary layer, the boundary with the format library. `ComponentRegistry` is a scene-layer concern; its only consumer is `Scene::apply_to` and `SceneSaver_from_world`. A package feeds the registry (via `PackageBuilder::add_component`), but the registry itself belongs where its reader is.

`PackageBuilder` has two jobs: node-vocabulary registration and component-codec registration. The first produces `lucid::sema::Registry`; the second produces `engine::ComponentRegistry`. Two registries, two consumers, two names. Putting them both under `registry/` would suggest they are the same kind of thing. They are not.

---

## Decision 15 — `entity.h` and `component_tags.h` live in `engine::ecs::core`

`create_entity`, `destroy_entity`, `is_alive`, the `get_*` and `set_*` accessors, `add_component<Tag>`, `setup_core_tables`, and `allocate_entity_id` live in `engine/ecs/include/engine/ecs/core/entity.h`. The entity-component tags (`EntityTag`, `TransformTag`, `WorldTransformTag`, `VelocityTag`, `SpriteTag`, `ScriptTag`) and the camera-table constant (`kCameraTableName`) live in `engine/ecs/include/engine/ecs/core/component_tags.h`.

Rationale: the `Entity` table is the world's identity table. It is not scene-specific. A system can create an entity without a scene; a node can create an entity at runtime; a test can create an entity with no scene at all. The direction of dependency is `engine::scene` → `engine::ecs::core`, not the reverse.

`lucid_ecs` does not depend on `engine/core/log.h`. The one place that wanted `log_fatal` (in `allocate_entity_id`, for an exhausted counter) throws instead. Storage does not log; the caller decides what a broken invariant means.

---

## Gap table

The fourteen gaps identified in Phase 0, with resolution status.

| #   | Gap                                    | Status                                                                                                                    |
| --- | -------------------------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| 1   | Scene ownership                        | Resolved — Decision 2. `Scene` is a value; `World` owns tables.                                                           |
| 2   | Load additive vs. clear                | Resolved — Decision 2. `ApplyMode::Clear` is default; `Additive` is implemented (does not clear first).                   |
| 3   | Active camera identity                 | Resolved — Decision 3. Name is canonical; `RowRef` is a transient lookup result.                                          |
| 4   | Cameras as entities vs. table          | Resolved — Decision 3. Camera table, keyed by name. Not entities. `CameraTag` deleted.                                    |
| 5   | Entity ID stability                    | Resolved — Decision 6. IDs are stable; allocator set from the scene or `max + 1`.                                         |
| 6   | Parent references across load          | Resolved — scene loader does a two-pass load; child may reference a parent later in the file. Tested in `test_scene.cpp`. |
| 7   | `.lscene` format                       | Resolved — Decision 8. JSON, engine-owned schema, versioned independently.                                                |
| 8   | `Scene::clear()` semantics             | Resolved — Decision 9. `World::clear()` and `World::clear_rows()` are the two clears; `Scene` has no `clear()`.           |
| 9   | Scene name vs. file name               | Resolved — Decision 2. `name` is a display string; the file path is identity.                                             |
| 10  | `_EngineState`                         | Resolved — Decision 7. Core table `Engine_EngineState`; preserved across `clear()`; not scene data.                       |
| 11  | `CameraData` / `Camera2D` / camera row | Resolved — Decision 3. Three types, three roles: `CameraDesc`, camera row, `Camera2D`.                                    |
| 12  | Renderer camera source                 | Resolved — Decision 3. Engine loop bridges: reads camera row, converts to `Camera2D`, sets renderer.                      |
| 13  | `SceneLoader` / `SceneSaver` friends   | Resolved — Decision 13. `Scene` is an aggregate; loader and saver are free functions. No friends.                         |
| 14  | `Scene` vs. `SceneDesc` naming         | Resolved — Decision 4. `Desc` suffix on schema sub-types; top-level scene type stays `engine::Scene`.                     |

Gaps discovered and resolved after Phase 0:

| #   | Gap                                         | Status                                                                                                            |
| --- | ------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| 15  | `World::clear()` removes the core schema    | Resolved — Decision 9. `clear()` preserves marked tables; `apply_to(Clear)` uses `clear_rows()`.                  |
| 16  | `Table::add_partial` inherits stale values  | Resolved — a reused slot resets every column to default before setting the named one. Tested in `test_table.cpp`. |
| 17  | Five-place component ritual                 | Resolved — Decision 14. `ComponentRegistry` is the list; `add_component` registers table and codec together.      |
| 18  | Scratch `World` in `default_registry`       | Resolved — Decision 12. `apply_registration_only()`.                                                              |
| 19  | Graph load lifecycle unspecified            | Resolved — Decision 11. `runtime.load()` after `scene.apply_to`.                                                  |
| 20  | `lucid_ecs` depended on `engine/core/log.h` | Resolved — Decision 15. The one log call became a throw; `lucid_ecs` is free of engine dependencies.              |
| 21  | `CameraTag` was a misnamed constant         | Resolved — Decision 3. `kCameraTableName` in `component_tags.h`; not a tag.                                       |

---

## What this file is not

- It is not a changelog. Each decision answers a question that was open. The commit history is the changelog.
- It is not a design note. Where a topic needs more than a paragraph — the world's empty operations, the package policy, the trigger model, the dataflow model — the note exists and this file points at it.
- It is not a plan. Decisions 1–15 are realized in the code. Where a decision describes a future extension (a second engine-lifetime row, a service on `Services`), it says so and points at the note.