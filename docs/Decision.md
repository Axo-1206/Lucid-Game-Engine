# Design Decisions — Lucid Engine Rework

Decisions are recorded in the order they were made, one per phase.
Each decision answers a question that was genuinely open at the time;
the answer is the thing that changed, not a restatement of the status
quo.

Cross-references: `docs/FileStructure.md` for the repository layout,
`docs/notes/SceneFormat.md` for the scene format spec, and
`docs/notes/TriggerModel.md` for how a graph expresses control flow.

---

## Format library freeze

During the engine rework (Phases 0–8), the format library
(`lucid::core`, `lucid::parser`, `lucid::formatter`, `lucid::sema`,
`lucid::serialization`, the `lucid` CLI) is frozen. No files under
those namespaces change. The engine includes `sema/Registry.hpp` and
`sema/Graph.hpp` and builds a `Registry` struct from its own node
definitions; it does not modify the library.

The boundary:

```
lucid::*           the format library
engine::*          the engine
engine::ecs::*     the engine's storage layer (World, Table, Column, …)
```

Vocabulary, one word one meaning:

```
Registry  →  lucid::sema::Registry
             The format library's node vocabulary struct.
             The engine fills it in; the format library reads it.

World     →  engine::ecs::World
             The engine's live table store.

Graph     →  lucid::sema::Graph
             The format library's compiled output of a .lucid file.

Scene     →  engine::Scene
             The description of entities and cameras read from a
             .lscene file. Schema type; not runtime state.
```

---

## Decision 1 — What `World` is

**What is `engine::ecs::World`?** The engine's live table store —
the container of all entity, component, and camera rows for the
currently loaded game state. One per engine instance.

- **Does it know about scenes?** No. `World` has no concept of a
  scene. It has tables.
- **Does it own the tables?** Yes. It is the sole owner. A `Scene`
  does not reference it.
- **What is its namespace?** `engine::ecs`. Not `lucid`. Not `engine`
  alone.
- **What is its relationship to `lucid::sema::Registry`?** None at
  runtime. The engine builds a `lucid::sema::Registry` from its own
  node definitions; that struct is separate from `World` and is never
  stored in it.
- **Is `World` copyable?** No. It is an owning, mutable store. Copy
  is deleted; move is allowed.
- **Does `World` clear itself?** Yes. `World::clear()` clears all
  tables. `World::clear_table(name)` clears one.

Migration from old design:

> Old: `lucid::Runtime` was the table store. `Scene` held a
> `Runtime&` and mutated it. `Scene::clear()` cleared the runtime's
> tables.
>
> New: `engine::ecs::World` is the table store. `Scene` does not hold
> a `World&`. `World::clear()` clears the world.

---

## Decision 2 — What `Scene` is

**What is a `Scene`?** A description — the entities, components,
cameras, and metadata read from a `.lscene` file. It does not hold a
`World&`. It is a value that can be applied to a world.

- **How many scenes can a world have?** At most one active at a time
  (Model A). Model B (additive partitioning) and Model C (one world
  per scene) were rejected for now: Model B requires partition
  identity to be tracked throughout the ECS, which adds complexity
  before it is needed; Model C duplicates renderer and system state
  per scene.
- **Does `Scene` have `apply_to` / `extract_from`?**
  - `Scene::apply_to(World&, ApplyMode)` where `ApplyMode` is
    `Clear` (default) or `Additive`.
  - `Scene::extract_from(const World&)` — returns current world
    state as a `Scene`.
- **Does `Scene` have `clear()`?** No. Clearing is a `World`
  operation. To unload the current scene: `world.clear()`.
- **What identity does `Scene` hold?**
  - A `name` — a display name, not a unique key, not the file path.
  - The active camera by name, not by `RowRef` (see Decision 3).
- **Is `Scene` copyable?** Yes. It is a description.
- **File and type:** `engine/include/engine/scene/scene.h`,
  type `engine::Scene`.

Migration from old design:

> Old: `Scene(lucid::Runtime& rt)`, `scene.load_from_file(path)`
> cleared `rt` and loaded into it, `scene.clear()` cleared `rt`.
>
> New: `Scene{}` is default-constructible. `SceneLoader::load(path)`
> returns a `Scene`. `scene.apply_to(world, ApplyMode::Clear)`.
> `world.clear()` is the only clear.

---

## Decision 3 — Where cameras live

- **Are cameras entities?** No.
- **Where do they live?** In the `World`'s `Camera` table, keyed by
  `name` (primary column). A scene description carries a
  `std::vector<CameraDesc>`.
- **Is `CameraTag` kept?** No. The comment `// not an entity
  component` was an apology; this decision removes the need for it.
  If anything needs the table name as a constant, it is
  `constexpr const char* kCameraTable = "Camera"` in the camera
  header — not a component tag.
- **How is the active camera identified?** In the `Scene`, by name.
  In the `World`, cached as a `RowRef` for speed. On `apply_to`, the
  world looks up the name in its `Camera` table and caches the ref.
  On `extract_from`, the world reads the ref's name back into the
  description. The name is canonical; the `RowRef` is a cache.
- **Three camera types and their roles:**
  - `CameraDesc` — the schema type. What is serialized. (Renamed
    from `CameraData`.)
  - The `Camera` table row — runtime state. Columns mirror
    `CameraDesc`.
  - `Camera2D` — the renderer's view struct. Produced by
    `to_camera2d(const CameraDesc&, int w, int h)` or by reading a
    camera row.
- **Who sets the renderer's camera each frame?** The engine loop. It
  looks up the world's cached active camera `RowRef`, reads the row
  into a `CameraDesc`, converts to `Camera2D`, calls
  `renderer.set_camera_2d(cam2d)`. The renderer never reads the
  `World`. The `World` never talks to the renderer.

---

## Decision 4 — Schema type names

Two options were considered:

- **Option A — `Desc` suffix on schema types:** `SceneDesc`,
  `EntityDesc`, `CameraDesc`. Runtime types keep their names: `World`,
  `Table`, `Camera2D`.
- **Option B — no suffix:** `Scene`, `Entity`, `Camera`. These
  collide with table names and runtime types.

**Chosen: Option A.** The `Desc` suffix makes it unambiguous at every
use site that you are looking at the description, not the runtime
state. `SceneDesc::apply_to(world)` reads unambiguously;
`Scene::apply_to(world)` invites the question of whether `Scene` is
runtime state.

In practice the top-level type exposed from `scene.h` remains
`engine::Scene` for the scene-as-a-whole (the loaded description), and
internal schema sub-types use `Desc`: `EntityDesc`, `CameraDesc`,
`ComponentData`. This is a pragmatic split: `Scene` has no same-level
name collision; `Entity` and `Camera` do.

---

## Decision 5 — What `.lscene` is *(superseded by Decision 8)*

Decision 5 deferred the `.lscene` format choice. Decision 8 resolves
it: `.lscene` is JSON, its schema is engine-owned and versioned
independently, and its only interface to the format library is a
`Script` component holding a `.lucgraph` path.

See Decision 8 for the full rationale.

---

## Decision 6 — Entity ID stability across load

- **Are entity IDs serialized?** Yes. `EntityDesc` carries the `id`.
  Parent references would break across load otherwise.
- **Are IDs stable across save/load?** Yes. A scene saved with entity
  id 1 loads with entity id 1.
- **What happens to the world's `next_entity_id` after load?** It
  advances past the max loaded ID. The loader does this; the caller
  does not need to remember.

The exact rule:

> After `Scene::apply_to(world, ...)`, the world's entity-id
> allocator is set to `max(loaded_id) + 1`. The loader enforces
> this.

Covered by `tests/engine/test_scene.cpp`: load a scene with entities
id 1, 2, 3; create a new entity; assert its id is 4, not 1.

Bug resolved by this decision:

> Old: no test asserted this. `allocate_entity_id` could collide
> after load.

---

## Decision 7 — `_EngineState`

- **What is `_EngineState`?** A core table holding engine-global
  counters and flags. Current known contents: `next_entity_id`.
- **Is it serialized per scene?** No. It is engine-lifetime state,
  not scene data. The `.lscene` format carries an `"engine_state"`
  block only so the loader can restore the id allocator after load
  (see Decision 6). The table is not round-tripped as part of a scene
  description.
- **Is it cleared by `World::clear()`?** The counter is preserved;
  the table content is cleared. `clear()` removes entities and
  components but the id allocator keeps counting. The next entity
  after a clear has id N+1, not 1. This is safer (ids never reuse
  within a session) and matches common engine behavior.
- **What else lives in it?** Only `next_entity_id` currently. Adding
  fields is allowed; removing them is a breaking change.

---

## Decision 8 — The `.lscene` format *(resolves Decision 5)*

`.lscene` files are JSON. The engine's scene loader and saver read
and write JSON via nlohmann/json. The scene format is an independent,
engine-owned schema with its own version (`kSceneSchemaVersion`), its
own schema header (`engine/include/engine/scene/scene_schema.h`), its
own spec (`docs/notes/SceneFormat.md`), and its own evolution schedule.

`.lucgraph` files are the Lucid format library's binary format for
behavior graphs. They are read and written only by
`lucid::serialization`.

The two are different things. A scene is data — entities, components,
cameras. A graph is behavior — nodes, subscriptions, phases. Their
shapes are different, their lifetimes are different, their versioning
is different. The only interface between them is a `Script` component
that holds a `.lucgraph` file path. The scene neither compiles nor
loads the graph; the runtime does, passing the same node registry that
was used at compile time.

Rationale:

- Data and behavior are different categories. Expressing one in the
  other's format bends the format.
- The format library's `Registry`, `Graph`, and `NodeInstance` are
  designed for graphs, not entity lists. Coupling them to the ECS is
  a category error.
- The registry fingerprint is meaningful for graphs (node vocabularies
  must match) and meaningless for scenes (a scene's compatibility is
  about table schemas).
- The scene schema evolves frequently — new components, new columns —
  and must not invalidate compiled graphs. The behavior format evolves
  rarely and must not be invalidated by scene changes.
- Independent evolution is what makes each layer stable.

See `docs/notes/SceneFormat.md` for the full spec and the schema's
evolution rules.

---

## Gap table

The fourteen gaps identified in Phase 0, with resolution status.

| #   | Gap                                    | Status                                                                                                                    |
| --- | -------------------------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| 1   | Scene ownership                        | Resolved — Decision 2. `Scene` is a value; `World` owns tables.                                                           |
| 2   | Load additive vs. clear                | Resolved — Decision 2. `ApplyMode::Clear` is default; `Additive` is a future extension.                                   |
| 3   | Active camera identity                 | Resolved — Decision 3. Name is canonical; `RowRef` is a cache.                                                            |
| 4   | Cameras as entities vs. table          | Resolved — Decision 3. Camera table, keyed by name. Not entities.                                                         |
| 5   | Entity ID stability                    | Resolved — Decision 6. IDs are stable; allocator advances past max on load.                                               |
| 6   | Parent references across load          | Resolved — scene loader does a two-pass load; child may reference a parent later in the file. Tested in `test_scene.cpp`. |
| 7   | `.lscene` format                       | Resolved — Decision 8. JSON, engine-owned schema, versioned independently.                                                |
| 8   | `Scene::clear()` semantics             | Resolved — Decision 1. `World::clear()` is the only clear; `Scene` has no `clear()`.                                      |
| 9   | Scene name vs. file name               | Resolved — Decision 2. `name` is a display string; the file path is identity.                                             |
| 10  | `_EngineState`                         | Resolved — Decision 7. Core table; counter preserved across `clear()`; not scene data.                                    |
| 11  | `CameraData` / `Camera2D` / camera row | Resolved — Decision 3. Three types, three roles: `CameraDesc`, camera row, `Camera2D`.                                    |
| 12  | Renderer camera source                 | Resolved — Decision 3. Engine loop bridges: reads camera row, converts to `Camera2D`, sets renderer.                      |
| 13  | `SceneLoader` / `SceneSaver` friends   | Resolved — `Scene` is an aggregate; loader and saver operate on public fields. No friends.                                |
| 14  | `Scene` vs. `SceneDesc` naming         | Resolved — Decision 4. `Desc` suffix on sub-types; top-level scene type stays `engine::Scene`.                            |
