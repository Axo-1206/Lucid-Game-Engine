# World lifecycle

How a `World` is emptied, what survives, and which operation is correct
in which situation. Three operations look like they do the same thing.
They do not, and picking the wrong one is the difference between a
clean scene load and a crash.

See also:
- `docs/Decision.md` — Decision 1 (`World` is the table store),
  Decision 2 (`Scene` is a value), Decision 7 (`Engine_EngineState`),
  Decision 9 (the three operations).
- `docs/notes/SceneFormat.md` — the `.lscene` schema and its
  `engine_state` block.
- `docs/notes/Dataflow.md` — the engine loop's phase order.

---

## 1. The three operations

| Operation                                   | Non-preserved tables | Preserved tables                            | Use when                                                       |
| ------------------------------------------- | -------------------- | ------------------------------------------- | -------------------------------------------------------------- |
| `World::clear()`                            | removed              | kept, **rows kept**                         | Reset the world to "engine state only." Rare. Tests, teardown. |
| `World::clear_rows()`                       | kept, rows cleared   | kept, rows cleared                          | Reset the world to "empty but with the schema." Scene load.    |
| `Scene::apply_to(world, components, Clear)` | kept, rows cleared   | kept, rows cleared, **state row re-seeded** | Load a scene. The common case.                                 |

A table is **preserved** if it was marked with `World::mark_preserved()`
before `clear()` was called. `setup_core_tables` marks
`Engine_EngineState` preserved. Nothing else is preserved today.

The distinction exists because three different things are needed:

- A **session-level reset** that keeps the engine's counters.
  `World::clear()`.
- A **scene-level reset** that keeps the engine's schema (so a new
  scene can populate it) but empties every row.
  `World::clear_rows()`.
- A **scene load** that does the scene-level reset and then writes the
  scene's entities and cameras into the empty schema.
  `Scene::apply_to(world, components, ApplyMode::Clear)`.

---

## 2. Why `clear()` removes tables at all

A `World` accumulates tables: the engine's core set, plus every table a
loaded package registered. If a scene is unloaded and another is loaded,
the first scene's tables may not be wanted by the second. Removing tables
lets the world's schema shrink.

If `clear()` only emptied rows, every table ever created would linger
with a stale schema. A game that loads a physics scene and then a
non-physics scene would carry `Engine_Body` and `Engine_BodyRuntime`
forever.

Removing tables is the right operation for "the schema should reflect
what is loaded."

---

## 3. Why `clear()` keeps preserved rows

`Engine_EngineState.next_entity_id` is a counter. Entity ids never repeat
within a session. If a `clear()` reset the counter to 1, the next entity
created after the clear would collide with an id that was already
handed out. Stale `RowRef`s, serialized ids, and any code holding an id
would break.

`clear()` is the session-level reset. It keeps the counter counting.

`clear_rows()` is the "throw the session away" operation. It clears the
counter's row. After `clear_rows()`, `allocate_entity_id` will throw
"Engine_EngineState key not found" until the row is re-created.

---

## 4. Why `apply_to(Clear)` uses `clear_rows()` and not `clear()`

`Scene::apply_to(engine::ecs::World&, const ComponentRegistry&, ApplyMode)`
calls `world.clear_rows()` for `ApplyMode::Clear` and then populates the
world's rows from the scene's JSON. The `ComponentRegistry` is the
dispatch table the apply uses to load each component. The state row
is re-seeded from the scene's `engine_state.next_entity_id`, or
computed from the loaded entity ids.

`apply_to` is going to populate `Engine_Entity`, `Engine_Transform`,
`Engine_Camera`, and the rest of the core schema. If it called `clear()`,
it would remove those tables and immediately need them again. `clear()`
is the wrong operation.

`clear_rows()` keeps the schema and empties the rows. `apply_to` then
populates the rows from the scene. The counter is cleared along with
every other row, and `apply_to` re-seeds it from the scene's
`engine_state.next_entity_id`, or computes `max(loaded_id) + 1` if the
scene does not carry one.

The scene is authoritative about where the allocator restarts. The
counter's survival across `clear()` is for the case where no scene is
involved.

---

## 5. The invariant

> After `Scene::apply_to(world, components, ApplyMode::Clear)`:
>
> - The world has the engine's core schema: `Engine_Entity`,
>   `Engine_Transform`, `Engine_WorldTransform`, `Engine_Velocity`,
>   `Engine_Sprite`, `Engine_Script`, `Engine_Camera`, and
>   `Engine_EngineState`.
> - The world has the scene's entities, components, and cameras.
> - `Engine_EngineState` has a `next_entity_id` row whose value is
>   the scene's `engine_state.next_entity_id` if the scene carries
>   one, or `max(entity.id) + 1` computed from the loaded entities
>   otherwise.
>
> No other rows exist.

That is the sentence to remember. Every operation in §1 is a way to
reach this state or to leave it.

---

## 6. The failure modes

Each of these is an easy mistake to make with the three operations.

- **Calling `clear()` before populating a scene.** `clear()` removes
  `Engine_Entity` and the rest of the core schema. The next
  `create_entity` throws "Entity table not set up." Use
  `clear_rows()` instead.

- **Calling `clear_rows()` and then `allocate_entity_id` without
  re-seeding.** `clear_rows()` empties `Engine_EngineState`. The
  next `allocate_entity_id` throws "Engine_EngineState key not
  found." `apply_to` re-seeds the row; a caller who uses
  `clear_rows()` directly must do so too.

- **Calling `clear()` and expecting a fresh allocator.** The counter
  keeps counting. The next entity id is not 1; it is the previous
  value plus one. This is the intended behavior. A caller who wants
  a fresh allocator must use `clear_rows()` and re-seed with 1.

- **Calling `clear_rows()` and then assuming the schema shrank.**
  `clear_rows()` keeps every table. A game that loaded a physics
  scene and then a non-physics scene via `clear_rows()` would carry
  `Engine_Body` with zero rows. Removing the table requires
  `clear()`.

---

## 7. The extension point

The current arrangement holds because there is exactly one
engine-lifetime table (`Engine_EngineState`) and exactly one
engine-lifetime row inside it (`next_entity_id`). `apply_to` knows
both, and re-seeds the row after `clear_rows()`.

This breaks if:

- A second engine-lifetime table is added. `clear()` must preserve it
  (`mark_preserved`), and `apply_to` must know whether `clear_rows()`
  should clear its rows and whether to re-seed them.
- A second engine-lifetime row is added inside `Engine_EngineState`.
  `apply_to` must re-seed it too.
- `apply_to` stops knowing `Engine_EngineState`'s schema.

At that point the right shape is a `World::clear_for_scene(callback)`
that empties every table's rows, keeps every schema, and calls the
callback to re-seed engine-lifetime state. The callback is the one
place that knows what "engine-lifetime" means for this engine.

This is a known limit of the current design. Revisit it when a
second engine-lifetime table or row is added.

---

## 8. Summary in one line

**`clear()` removes non-preserved tables and keeps the engine's
counters; `clear_rows()` empties everything and keeps every schema;
`apply_to(Clear)` uses `clear_rows()` and re-seeds the counter from
the scene.**