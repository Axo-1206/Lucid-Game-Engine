## Decision 1 — What `World` is

Write this section first. It is the foundation.

**Answer these in writing:**

- **What is `engine::ecs::World`?** One sentence. My recommendation: *the engine's live table store — the container of all entity, component, and camera rows for the currently loaded game state. One per engine instance.*
- **Does it know about scenes?** No. `World` has no concept of a scene. It has tables.
- **Does it own the tables?** Yes. It is the sole owner. A `Scene` does not reference it.
- **What is its namespace?** `engine::ecs`. Not `lucid`. Not `engine` alone.
- **What is its relationship to `lucid::sema::Registry`?** None at runtime. The engine builds a `lucid::sema::Registry` from its own node definitions (in Phase 6); that struct is separate from `World` and is never stored in it.
- **Is `World` copyable?** No. It's an owning, mutable store. Delete copy, allow move if useful.
- **Does `World` clear itself?** Yes — `World::clear()` clears all tables. `World::clear_table(name)` clears one. This replaces `Scene::clear()` from the old design.

Then state the *old* behavior you are replacing, so future readers can see the diff:

> Old: `lucid::Runtime` was the table store, and `Scene` held a `Runtime&` and mutated it. `Scene::clear()` cleared the runtime's tables.
> New: `engine::ecs::World` is the table store. `Scene` does not hold a `World&`. `World::clear()` clears the world.

**Do not write code in this section.** Write the contract. Code comes in Phase 1.

---

## Decision 2 — What `Scene` is

This is the decision that fixes the "does a world have multiple scenes" question.

**Answer these in writing:**

- **What is a `Scene`?** One sentence. My recommendation: *a description — the entities, components, cameras, and metadata read from a `.lscene` file. It does not hold a `World&`. It is a value that can be applied to a world.*
- **How many scenes can a world have?** At most one active at a time (Model A). State this explicitly and state the two models you rejected (B: additive partitioning, C: one world per scene) and *why* you rejected them *for now*.
- **Does `Scene` have `apply_to` / `extract_from`?** Yes. Name the signatures in prose:
  - `Scene::apply_to(World&, ApplyMode)` where `ApplyMode` is `Clear` or `Additive`. Default `Clear`.
  - `Scene::extract_from(const World&)` returns the current world state as a `Scene`.
- **Does `Scene` have `clear()`?** No. Clearing is a `World` operation. If you want "unload the current scene," that's `world.clear()`.
- **What identity does `Scene` hold?**
  - A `name` — a display name, not a unique key, not the file path. Say this explicitly so nobody later assumes it's unique.
  - The active camera **by name**, not by `RowRef` (see Decision 3).
- **Is `Scene` copyable?** Yes. It's a description.
- **What is the file called?** `engine/scene/scene.h`. What is the type called? `engine::Scene`. (Or `SceneDesc` if you prefer the `Desc` suffix everywhere — see Decision 4.)

**Also state the migration shape**, so Phase 2 has a target:

> Old: `Scene(lucid::Runtime& rt)`, `scene.load_from_file(path)` clears `rt` and loads into it, `scene.clear()` clears `rt`.
> New: `Scene{}` is default-constructible. `SceneLoader::load(path) → Scene`. `scene.apply_to(world, ApplyMode::Clear)`. `world.clear()` is the only clear.

---

## Decision 3 — Where cameras live

Answer these in writing:

- **Are cameras entities?** No. State this in one line.
- **Where do they live?** In the `World`'s `Camera` table, keyed by `name` (primary column). A scene description has a `std::vector<CameraDesc>`.
- **Is `CameraTag` kept?** No. Delete it. The comment `// not an entity component` was an apology; the decision removes the need for it. If anything needs the table name as a constant, add `constexpr const char* kCameraTable = "Camera";` in the camera header, not a component tag.
- **How is the active camera identified?** In the `Scene`, **by name**. In the `World`, cached as a `RowRef` for speed. On `apply_to`, the world looks up the name in its `Camera` table and caches the ref. On `extract_from`, the world reads the ref's name back into the description. The name is canonical; the `RowRef` is a cache.
- **What are the three camera types and their roles?** Name them and give each one line:
  - `CameraDesc` — the schema type. What's serialized. Rename `CameraData` → `CameraDesc`.
  - The `Camera` table row — runtime. Columns mirror `CameraDesc`.
  - `Camera2D` — the renderer's view struct. Produced by `to_camera2d(const CameraDesc&, int w, int h)` or by reading a camera row.
- **Who sets the renderer's camera each frame?** The engine loop. It looks up the world's cached active camera `RowRef`, reads the row into a `CameraDesc`, converts to `Camera2D`, calls `renderer.set_camera_2d(cam2d)`. The renderer never reads the `World`. The `World` never talks to the renderer.

---

## Decision 4 — Schema type names

Pick one convention and apply it everywhere. Two options:

**Option A — `Desc` suffix on schema types:**
- `SceneDesc`, `EntityDesc`, `CameraDesc`
- Runtime types keep their names: `World`, `Table`, `Camera2D`, `EntityRef`

**Option B — no suffix, schema types get the plain names:**
- `Scene`, `Entity`, `Camera`
- Then `Entity` collides with `EntityRef` and with the entity concept in the world; `Camera` collides with the `Camera` table and `Camera2D`; `Scene` is fine.

**My recommendation: Option A.** The `Desc` suffix makes it obvious at every use site that you're looking at the description, not the runtime state. `SceneDesc::apply_to(world)` reads unambiguously. `Scene::apply_to(world)` makes you wonder if `Scene` is runtime state.

Write the decision down. Name every schema type. Name every runtime type. Say which is which.

---

## Decision 5 — What `.lscene` is (superseded by Decision 8)

Decision 5 deferred the .lscene format choice. Decision 8 records
the resolution: .lscene is JSON, its schema is engine-owned and
versioned separately, and its only interface to the format library
is a Script component holding a .lucgraph path.

---

## Decision 6 — Entity ID stability across load

Answer these in writing:

- **Are entity IDs serialized?** Yes. `EntityDesc` carries the `id`. Otherwise parent references break across load.
- **Are IDs stable across save/load?** Yes. The test in `test_scene.cpp` assumes entity A is id 1 after reload; that's intended behavior.
- **What happens to the world's `next_entity_id` after load?** It must advance past the max loaded ID, or the next `create_entity` will collide.

Write down the exact rule:

> After `SceneDesc::apply_to(world, ...)`, the world's entity-id allocator is set to `max(loaded_id) + 1`. The loader does this; the caller does not need to remember.

Then note the test you'll add in Phase 2 (or 6):

> Test: load a scene with entities id 1, 2, 3. Create a new entity. Assert its id is 4, not 1.

And note the current bug:

> Old: no test asserts this. `allocate_entity_id` may collide after load. Resolved by Decision 6 and the test above.

---

## Decision 7 — `_EngineState`

Answer these in writing:

- **What is `_EngineState`?** A core table holding engine-global counters and flags. Current known contents: `next_entity_id`.
- **Is it serialized per scene?** No. It's engine-lifetime state, not scene data.
- **Is it cleared by `World::clear()`?** Two options:
  - **Clear it and reset counters.** `clear()` is a hard reset. Next entity created after a clear has id 1.
  - **Preserve it.** `clear()` removes entities and components but the id allocator keeps counting. Next entity has id N+1.
  
  The second is safer (ids never reuse within a session) and matches what most engines do. Recommend: **preserve the counter, clear the table content.** Document it.
- **What else lives in it?** Name anything you know now. Leave it open to grow. State that adding fields is allowed; removing them is a breaking change.

## Decision 8: The .lscene format

`.lscene` files are JSON. The engine's scene loader and saver read
and write JSON via nlohmann/json. The scene format is an
independent, engine-owned schema, with its own version
(kSceneSchemaVersion), its own schema header (scene_schema.h), its
own spec (docs/notes/SceneFormat.md), and its own evolution
schedule.

`.lucgraph` files are the Lucid format library's binary format for
behavior graphs. They are read and written only by
`lucid::serialization`.

The two are different things. A scene is data — entities,
components, cameras. A graph is behavior — nodes, subscriptions,
phases. Their shapes are different, their lifetimes are different,
their versioning is different. The only interface between them is
a `Script` component that holds a `.lucgraph` file path. The
scene neither compiles nor loads the graph; the runtime does, and
passes the same node registry that was used at compile time.

Rationale:
- Data and behavior are different categories. Expressing one in
  the other's format bends the format.
- The format library's Registry, Graph, and NodeInstance are
  designed for graphs, not for entity lists. Coupling them to
  the ECS is a category error.
- The registry fingerprint is meaningful for graphs (node
  vocabularies must match) and meaningless for scenes (a scene's
  compatibility is about table schemas).
- The scene schema evolves frequently — new components, new
  columns — and must not invalidate compiled graphs. The behavior
  format evolves rarely and must not be invalidated by scene
  changes.
- Independent evolution is what makes each layer stable.

See docs/notes/SceneFormat.md for the full spec and the schema's
evolution rules.

---

## The gap list

Copy the fourteen gaps from my previous message into `Decision.md`. For each, mark one of:

- **Resolved** — with a one-line answer pointing at a decision above.
- **Deferred** — with a one-line reason and a note on when it'll be revisited.

Suggested resolutions:

| #   | Gap                                    | Status                                                                                 |
| --- | -------------------------------------- | -------------------------------------------------------------------------------------- |
| 1   | Scene ownership                        | Resolved by Decision 2.                                                                |
| 2   | Load additive vs. clear                | Resolved by Decision 2 (`ApplyMode`). Default `Clear`; additive is a future extension. |
| 3   | Active camera identity                 | Resolved by Decision 3. Name canonical; `RowRef` is a cache.                           |
| 4   | Cameras as entities vs. table          | Resolved by Decision 3. Table, keyed by name.                                          |
| 5   | Entity ID stability                    | Resolved by Decision 6.                                                                |
| 6   | Parent references across load          | Deferred to Phase 2 implementation. Note: loader must do a two-pass load; add a test.  |
| 7   | `.lscene` format                       | Resolved by Decision 5.                                                                |
| 8   | `Scene::clear()` semantics             | Resolved by Decision 1 (`World::clear`).                                               |
| 9   | Scene name vs. file name               | Resolved by Decision 2. Name is display; path is identity.                             |
| 10  | `_EngineState`                         | Resolved by Decision 7.                                                                |
| 11  | `CameraData` / `Camera2D` / camera row | Resolved by Decision 3. Three types, three roles.                                      |
| 12  | Renderer camera source                 | Resolved by Decision 3. Engine loop bridges.                                           |
| 13  | `SceneLoader` / `SceneSaver` friends   | Deferred to Phase 2. Delete friends when `Scene` becomes an aggregate.                 |
| 14  | `Scene` vs. `SceneDesc` naming         | Resolved by Decision 4.                                                                |

Mark 6 and 13 as deferred with a note. Everything else should resolve now.

---

## The freeze statement

Add one section at the top of `Decision.md`:

```
## Format library freeze

During the engine rework, the format library (lucid::core, lucid::parser,
lucid::formatter, lucid::sema, lucid::serialization, the `lucid` CLI)
is frozen. No files under those namespaces change. The engine includes
sema/Registry.hpp and sema/Graph.hpp and builds a Registry struct from
its own node definitions; it does not modify the library.

The boundary:
  lucid::*           the format library
  engine::*          the engine
  engine::ecs::*     the engine's storage layer (World, Table, Column, ...)

Vocabulary, one word one meaning:
  Registry  →  lucid::sema::Registry, the format library's struct.
               The engine fills it in; the format library reads it.
  World     →  engine::ecs::World, the engine's live table store.
  Graph     →  lucid::sema::Graph, the format library's compiled output.
  Scene     →  engine::Scene, the description of entities and cameras
               read from a .lscene file.
  SceneDesc →  if Decision 4 chooses the Desc suffix, the schema type;
               otherwise the same as Scene.
```

Adjust the last line to match Decision 4.

---
