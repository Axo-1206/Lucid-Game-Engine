# The `.lscene` format

A `.lscene` file describes a scene: a list of entities, the components
they carry, and one or more cameras. It is JSON. It is read and written
by `engine/src/scene/scene_loader.cpp` and `scene_saver.cpp`.

A `.lscene` file is **not** a `.lucgraph`. A `.lucgraph` describes
behavior — a graph of nodes and subscriptions — and is read and written
by the format library (`lucid::serialization`). A scene is data; a graph
is behavior. They are separate, and the only interface between them is a
`Engine_Script` component that holds a path to a `.lucgraph` file.

See `docs/Decision.md` Decision 8 for why the two are separate.

---

## Version

The file carries `"version": N`. The current version is
`kSceneSchemaVersion` (see `engine/scene/scene_schema.h`), which is 1.

The version is the **scene schema's version**. It is independent of the
format library's registry fingerprint and of the engine's overall
version. Adding a component or a field bumps this version when the team
chooses to; removing or repurposing a field must bump it.

The loader:

- accepts the current version,
- accepts older versions with a log line, on the assumption that
  compatible changes don't break old scenes,
- refuses newer versions. A scene from a newer engine may have fields
  this engine doesn't know; refusing is safer than guessing.

---

## Component recognition

The scene loader and saver do not hardcode the set of component types.
They consult the engine's `ComponentRegistry`, a codec table populated
at startup: the engine's five core components (`Engine_Transform`,
`Engine_WorldTransform`, `Engine_Velocity`, `Engine_Sprite`,
`Engine_Script`) are registered by `register_core_components`; each
loaded package registers its own component tables via
`PackageBuilder::add_component`. A `components` key the registry does
not know is logged as a warning and ignored; the entity does not get
that component. The same registry drives the saver: a component table
with no codec is not serialized. This is what makes a scene portable
across engines with different package sets, and what makes adding a
package's component a one-line change in the package's `register_into`.

---

## Top-level shape

```json
{
  "version": 1,
  "name": "level1",
  "cameras": [ ... ],
  "entities": [ ... ],
  "engine_state": { "next_entity_id": 42 }
}
```

- `version`: integer. Required for new files; defaults to 1 if absent.
- `name`: string. A display name, not a unique key. Empty string if absent.
- `cameras`: array of camera objects. May be empty.
- `entities`: array of entity objects. May be empty.
- `engine_state`: object. Currently carries `next_entity_id`, the value
  the engine's id allocator starts at after loading. If absent, the
  loader computes it as `max(entity.id) + 1`.

---

## Camera

```json
{
  "name": "main",
  "pos": [0.0, 0.0, 10.0],
  "target": [0.0, 0.0, 0.0],
  "fov": 60.0,
  "near": 0.1,
  "far": 1000.0,
  "is_orthographic": true,
  "ortho_size": 5.0,
  "clear_color": [0.1, 0.1, 0.15, 1.0]
}
```

- `name`: string. Unique within a scene. The active camera is identified
  by name in the engine.
- `pos`: `[x, y, z]`. Default `[0, 0, 0]`.
- `target`: `[x, y, z]`. Default `[0, 0, 0]`. Ignored when
  `is_orthographic` is true.
- `fov`: degrees. Default `60.0`. Ignored when `is_orthographic` is true.
- `near`, `far`: clip planes. Defaults `0.1`, `1000.0`.
- `is_orthographic`: boolean. Default `true`. True for 2D, false for
  perspective. There is no scene-level 2D/3D mode; the camera's mode is
  per-camera.
- `ortho_size`: half-height of the orthographic view in world units.
  Default `1.0`. Ignored when `is_orthographic` is false.
- `clear_color`: `[r, g, b, a]` in `[0, 1]`. Default `[0, 0, 0, 1]`.

---

## Entity

```json
{
  "id": 1,
  "name": "player",
  "parent": 0,
  "components": { ... }
}
```

- `id`: unsigned integer. Nonzero. Unique within the scene. Referenced by
  `parent` fields and by any component that references an entity.
- `name`: string. A display name, not a unique key. Empty string if absent.
- `parent`: unsigned integer. The `id` of the parent entity, or `0` for
  no parent. The loader resolves parent references in a second pass, so a
  child may reference a parent that appears later in the file.
- `components`: object. Keys are component type names (see below). A
  component the engine doesn't recognize is logged as a warning and
  ignored.

---

## Components

A component's fields are all optional. A missing field keeps the table's
default value (0 for numbers, `""` for strings, `false` for booleans).
A missing component means the entity doesn't have it.

### `Engine_Transform`

A 2D transform. 3D will add columns or a sibling component; see
"Evolution" below.

```json
{ "x": 0.0, "y": 0.0, "z": 0.0, "rot_z": 0.0 }
```

- `x`, `y`, `z`: floats, default `0.0`.
- `rot_z`: float, radians, default `0.0`.

### `Engine_WorldTransform`

The computed world-space transform. Written by `hierarchy_system`. Not
usually authored in a scene; the loader accepts it for round-tripping.

```json
{ "x": 0.0, "y": 0.0, "z": 0.0, "rot_z": 0.0 }
```

### `Engine_Velocity`

```json
{ "dx": 0.0, "dy": 0.0, "dz": 0.0 }
```

- `dx`, `dy`, `dz`: floats, default `0.0`. Units per second.

### `Engine_Sprite`

A 2D sprite. 3D will add a sibling `Mesh` component; `Engine_Sprite` stays as
it is.

```json
{
  "texture_path": "textures/player.png",
  "w": 32.0,
  "h": 32.0,
  "tint_r": 1.0,
  "tint_g": 1.0,
  "tint_b": 1.0,
  "tint_a": 1.0,
  "layer": 0
}
```

- `texture_path`: string. An asset path (see "Asset paths" below).
- `w`, `h`: floats. Size in world units. Default `0.0`.
- `tint_r`, `tint_g`, `tint_b`, `tint_a`: floats in `[0, 1]`. Default `1.0`.
- `layer`: int32. Draw order. Higher layers draw later. Default `0`.

### `Engine_Script`

A reference to a compiled behavior graph.

```json
{ "graph": "scripts/player.lucgraph" }
```

- `graph`: string. An asset path (see below) to a `.lucgraph` file.

The scene loader stores the path. The runtime loads the graph separately,
at scene-runtime setup time, and passes the engine's node registry to
`lucid::serialization::deserialize`. If the graph was compiled against a
different node registry, the load fails loudly.

The scene neither compiles nor loads the graph. The path is a reference,
not a dependency.

The runtime reads the row after `Scene::apply_to` returns, not during
the tick. The engine loop calls `scene.apply_to(world, components,
ApplyMode::Clear)` and then `runtime.load()`; `load()` walks the
`Engine_Script` table, loads each path, and is idempotent. A scene
reload calls `apply_to` and then `load()` again. Nothing else in the
loop reads `Engine_Script`; the runtime does not poll. See
`docs/notes/Dataflow.md` §7 and `docs/Decision.md` Decision 11.

---

## Asset paths

Asset references — `texture_path`, `graph`, future `mesh_path`,
`material_path` — are strings. They are relative to the project's asset
root, which is the folder that contains `assets/`. They use `/` as the
separator regardless of platform.

The scene loader stores asset paths verbatim. Resolving them to
platform-specific paths, loading the referenced assets, and handling
missing files is the graphics (or audio, or physics) layer's job, not
the scene loader's.

Example: a project whose asset root is `assets/` and whose scene refers
to `textures/player.png` expects the file at `assets/textures/player.png`.

---

## Unknown components

A component key the engine doesn't recognize is logged as a warning and
ignored. The entity simply doesn't get that component. This makes a
scene portable across engines with different package sets: a scene can
carry components for a package the current engine doesn't have, and the
scene still loads.

The loader does not refuse to load a scene with unknown components. It
refuses only if the file's version is newer than the engine's.

"Unknown" means "not in the `ComponentRegistry`." The registry is built
once, after all packages are loaded, before any scene is applied. A
component that a package registers is known; a component that no loaded
package registers is unknown. The loader does not consult a global name
list; it asks the registry that the game's `main.cpp` built.

---

## Saving

`SceneSaver_from_world(world, components)` produces a `Scene` whose JSON
reflects the world's current rows. For each entity in `Engine_Entity`,
the saver walks every table with an `entity` column (except
`Engine_Entity` itself), looks up the table's codec in the same
`ComponentRegistry` the loader uses, and calls the codec's `save`
function. A table with no codec is skipped: the saver writes nothing for
it, and the JSON for that entity has no key for the table. This is the
mechanism by which `Engine_BodyRuntime` (a runtime table with no codec)
is never serialized while `Engine_Body` (a component with a codec) is.

The saver does not read the file. It writes a fresh JSON document from
the world's rows. `SceneSaver_save(scene, path)` writes the `Scene`'s
JSON text to disk. The two are separate: a caller who wants to save the
world writes `SceneSaver_save(SceneSaver_from_world(world, components),
path)`.

---

## Evolution rules

The schema changes over time. The rules:

**Compatible changes** — old scenes load correctly, no migration, no
version bump required (the team may bump for bookkeeping):

- Adding a component type. Old scenes lack it on every entity.
- Adding a column to an existing component. Old scenes leave it at its
  default.
- Adding a camera field. Same.
- Adding a top-level key that old scenes omit. Same.
- Widening a field's range or precision (`int32` → `int64`, `float` →
  `double`). The new type accepts the old values.

**Breaking changes** — old scenes need migration or must be refused:

- Removing a component type. Old scenes that use it lose data.
- Removing a column. Same.
- Narrowing a field's type in a lossy way (`float` → `int`). Old values
  may not parse.
- Repurposing a field (keeping the name, changing the meaning). Old
  scenes load but behave differently. This is the worst kind of breaking
  change because it fails silently; avoid it.
- Restructuring an entity's children (flattening a hierarchy, moving
  component data into a different shape). Old scenes need rewriting.

**Rule of thumb:** if a scene written for version N loads in version N+1's
engine without loss and without surprise, the change was compatible.

**Version bump policy:** bump `kSceneSchemaVersion` on every schema
change. The loader accepts older versions with a log line, so bumping on
compatible changes gives a version history without noise. Breaking
changes must bump and must either migrate the old scene or refuse it
with a clear message.

A scene's compatibility with an engine is a question of the engine's
`ComponentRegistry`, not of the scene format version. Adding a package's
component is compatible: old scenes lack it on every entity, new scenes
may carry it, and the format version does not need to bump. Removing a
package from the engine makes the components it registered unknown; old
scenes that carry those components load with warnings and lose the data.
The format version tracks the shape of the envelope — the top-level
keys, the entity object, the camera object — not the set of components a
given engine knows.

---

## Future: 3D

There is no scene-level 2D/3D mode. The camera's `is_orthographic` field
chooses the projection. Whether an entity is 2D or 3D is determined by
which components it has: `Engine_Sprite` is 2D, a future `Mesh` is 3D.

3D support will be additive:

- Rotation: `Engine_Transform` is 2D-only (`rot_z`). 3D will either add
  `rot_x`/`rot_y` columns to `Engine_Transform`, or add a `Rot3D` component with
  a quaternion. Existing 2D scenes are unaffected either way.
- Rendering: a `Mesh` component alongside `Engine_Sprite`. A 3D entity has
  `Mesh`; a 2D entity has `Engine_Sprite`; an entity with both is unusual but
  not forbidden.
- Materials, lights, skeletons: new components, added as needed.

Existing 2D scenes load unchanged when 3D support is added, because every
3D feature is a new component or a new column with a default.

---

## Future: alternate backends

The scene schema is backend-agnostic. Swapping the physics or graphics
backend does not change the schema.

- Components describe properties in engine-agnostic terms. A `Body`
  component (future) describes mass, shape, type; a backend interprets
  it. Backend-specific runtime data — a Jolt body id, a GPU handle —
  is stored in runtime tables that are never serialized.
- Assets are referenced by path. A texture is a `.png`; a future mesh
  might be a `.gltf`. The scene stores the path; the backend loads it in
  whatever format it uses.

The rule: **a component's fields describe the entity's properties in
terms any backend can interpret. Runtime artifacts are not serialized.
Assets are referenced by path.**

---

## Tools

The scene format is documented JSON. Any JSON tool can inspect, edit, or
generate a `.lscene` file. Scene-specific tools (a scene inspector, a
version-migration tool, a procedural-level generator) will live under
`tools/scene/`. These tools read and write `.lscene` files directly; they
do not go through the format library.

The format library is used only for behavior graphs (`.lucid` source and
`.lucgraph` binaries). It is never invoked by scene tools.

---

## Related

A scene's `Engine_Script` component references a `.lucgraph` file. For what a
graph does when it runs — how triggers fire, how conditions gate actions,
how the engine scheduler walks the graph — see `docs/notes/TriggerModel.md`.