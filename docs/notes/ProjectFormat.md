# The `.lucproj` project format

A `.lucproj` file describes a game's behavior: its sub-graphs, the
phases they belong to, the nodes they contain, the arguments the
nodes take, the scratchpad cells their sub-graphs declare, and the
resources they reference. It is JSON. It is written by the editor
and read by the engine's runtime at startup.

A `.lucproj` file is **not** a `.lscene`. A `.lscene` describes a
scene's *data* (entities, components, cameras); a project describes
the game's *behavior*. The two are separate, and the only interface
between them is the `Engine_Script` component: a scene row whose
`graph` column names a sub-graph in the project. See
`SceneFormat.md` and `docs/Decision.md` Decision 8.

A `.lucproj` file is **not** a source file. There is no text language
and no compiler. The editor composes nodes; the project file is the
composition as data; the runtime walks it.

See also:
- `docs/notes/ExecutionModel.md` — the walker, phases, sub-graphs,
  scratchpad. The project is the data the walker walks.
- `docs/notes/Dataflow.md` — the model the runtime obeys.
- `docs/notes/PackageTables.md` — the package-owned-table
  convention the project's node arguments point into.
- `docs/notes/SceneFormat.md` — the scene format, and the
  `Engine_Script` seam.
- `docs/notes/PackageComposition.md` — how the packages the project
  references register their vocabulary.

---

## Version

The file carries `"version": N`. The current version is
`kProjectFormatVersion` (see `engine/graph/project_format.h`), which
is 1.

The version is the **project format's version**. It is independent of
the scene format's version (see `SceneFormat.md`) and of the engine's
overall version.

The loader:

- accepts the current version,
- accepts older versions with a log line, on the assumption that
  compatible changes don't break old projects,
- refuses newer versions. A project from a newer engine may have
  fields this engine doesn't know; refusing is safer than guessing.

---

## Component recognition

The loader does not hardcode the set of node types, argument kinds, or
scratchpad cell types. It consults:

- **The node registry** — the engine's `NodeRegistry`, populated from
  the loaded packages. A node type name the registry does not know is
  a load error (`Error_UnknownNodeType`).
- **The argument kinds** — each node type's `NodeArgInfo::kind`
  (`Value` or `Successor`) tells the validator which arguments are
  paths, literals, handles, or successor indices. A project that puts
  a path where a `Successor` is expected is
  `Error_ArgumentKindMismatch`.
- **The scratchpad declarations** — each sub-graph's `scratch` block
  declares the cell types. A path into a cell the sub-graph did not
  declare is `Error_UnknownScratchCell`.
- **The resources** — the project's `resources` block declares them.
  A path or literal that names a resource the project did not declare
  is `Error_UnknownResource`.

This is the project format's equivalent of `SceneFormat.md`'s
component recognition: unknown content is caught at load time, not at
runtime, and the error names the offending node and the rule it
violated.

---

## Top-level shape

```json
{
  "version": 1,
  "name": "my_game",
  "resources": { ... },
  "phases": { ... },
  "subgraphs": [ ... ],
  "engine_state": { "registry_fingerprint": "..." }
}
```

- `version`: integer. Required for new files; defaults to 1 if absent.
- `name`: string. A display name, not a unique key. Empty string if
  absent.
- `resources`: object. Compile-time constants the project's nodes
  reference. May be empty.
- `phases`: object. Maps each engine phase id to the sub-graph that
  runs in that phase. May be empty (a project with no phase entries
  does nothing).
- `subgraphs`: array of sub-graph objects. May be empty.
- `engine_state`: object. Currently carries `registry_fingerprint`, a
  hash of the loaded packages' vocabulary that the project was
  authored against. The loader compares it against the runtime's
  registry; a mismatch is `Error_RegistryMismatch` at load time.
  `registry_fingerprint` is optional for hand-written projects (a
  project that skips it is loaded without the check).

> [!NOTE]
> `engine_state.registry_fingerprint` exists so a project authored
> against one package set is not silently loaded against a
> different one. It is not required (the editor writes it; a hand
> project can omit it), but a project that carries it is protected
> against the case where a package's vocabulary changed under it.
> The mismatch is an *error*, not a warning: the project's node
> arguments reference column types and node types that may no
> longer exist.

---

## Resources

A resource is a compile-time constant the project's nodes can
reference. It is the project's equivalent of the scene format's
`engine_state`: a small, project-wide declaration of named values.

```json
"resources": {
  "PlayerConfig": {
    "max_hp": { "type": "int32", "value": 100 },
    "speed":  { "type": "float32", "value": 200.0 },
    "name":   { "type": "string", "value": "hero" }
  }
}
```

- The resource's name is a string key. It is a project-local name;
  two projects can each declare a resource named `PlayerConfig`
  without colliding.
- Each field has a `type` (a primitive, enum, handle, or struct name
  the registry declares) and a `value`. The value's JSON shape
  follows the type: a JSON number for a numeric primitive, a JSON
  boolean for `bool`, a JSON string for `string`, a JSON object for
  a struct, a JSON `null` for a handle's nil value.
- A field's value is a compile-time constant. A resource field
  cannot reference a node or a scratchpad cell; the value is
  resolved at project load time.

A node's `Value` argument may be a `ResourceRef` (`{ "kind":
"resource", "resource": "PlayerConfig", "field": "max_hp" }`) or a
literal (`{ "kind": "literal", "type": "int32", "value": 5 }`).
Resource references are resolved at load time; the value is a
literal in the loaded project. See §"Node argument" below.

> [!NOTE]
> Resources are the only "compile-time constant" mechanism in the
> project format. There is no `.lucid` resource declaration, no
> attribute, no source. A package that wants to ship a default
> resource value (say, `PlayerConfig.max_hp = 100`) does so by
> shipping a *sub-graph template* that includes the resource
> declaration; the user drops the template into their project and
> gets the resource. See `PackageComposition.md` §11.

---

## Phases

A phase is the engine's scheduling unit. The engine declares an
ordered list of phases (`pre_physics`, `physics`, `post_physics`,
`pre_render`, `render`, `post_render`, `ui`; see
`ExecutionModel.md` §3). The project's `phases` block maps each
phase id to the sub-graph that runs in it.

```json
"phases": {
  "pre_physics": "MyInputUpdate",
  "physics":     "DefaultPhysics",
  "render":      "MyRenderPass",
  "ui":          "MyHud"
}
```

- The key is an engine phase id. The value is a sub-graph name. A
  phase the project does not list is not populated; the engine
  runs no sub-graph in it.
- A sub-graph named in `phases` must exist in `subgraphs`
  (`Error_UnknownPhaseGraph`).
- A sub-graph may be registered on at most one phase. Registering
  the same sub-graph on two phases is `Error_DuplicatePhaseGraph`.
- A sub-graph may be registered on **no** phase. It is then a
  *function*: callable by a `Call` from any phase's sub-graph, not
  run by the scheduler. See `ExecutionModel.md` §4.

**A phase's sub-graph may `Call` only: a sub-graph in the same
phase, or a function.** A `Call` to another phase's sub-graph is
`Error_CrossPhaseCall`. This is the scheduler's cross-phase
guarantee: `physics` always runs before `render`, and `physics`
cannot invoke `render`'s sub-graph.

---

## Sub-graphs

A sub-graph is an ordered list of nodes with an entry, an optional
input list, an optional output list, and a scratchpad declaration.

```json
{
  "name": "MyInputUpdate",
  "inputs": [],
  "outputs": [],
  "scratch": {
    "hp":     "float32",
    "max_hp": "float32",
    "low":    "bool"
  },
  "nodes": [
    { "id": 0, "type": "Engine_Entry_Node",
      "args": { "successor": 1 } },
    { "id": 1, "type": "Game_ReadHp_Node",
      "args": { "entity": { "kind": "path", "path": "world.Player.self" },
                "out":    { "kind": "path", "path": "scratch.hp" },
                "successor": 2 } },
    ...
  ]
}
```

### Fields

- `name`: string. Unique within the project. A project with two
  sub-graphs named `DefaultPhysics` is `Error_DuplicateSubgraph`.
  The name is what `phases` and `Call` nodes reference.
- `inputs`: array of `{ "name": string, "type": type_name }`. The
  sub-graph's input signature. A `Call` to this sub-graph must
  supply one argument per input, of matching type, at load time
  (`Error_CallSignature`). An empty `inputs` array means the
  sub-graph takes no arguments.
- `outputs`: array of `{ "name": string, "type": type_name }`.
  The sub-graph's output signature. On `Return`, the runtime
  copies the values the caller's `Call` node declared as outputs
  from the callee's scratchpad to the caller's scratchpad. An
  empty `outputs` array means the sub-graph returns nothing.
- `scratch`: object. Maps a scratchpad cell name to its type. The
  type is a primitive, enum, handle, or struct the registry
  declares. A path `"scratch.<cell>"` names a cell in this block;
  a path that names a cell not in this block is
  `Error_UnknownScratchCell`.
- `nodes`: array. The sub-graph's instruction stream. Order is
  the array's order. The first node must be an `Engine_Entry_Node`
  (`Error_MissingEntry`); its `successor` names the first node to
  run. See §"Nodes" below.

### Sub-graph inputs and outputs

A `Call` node's `args` (see §"Call" below) bind to the callee's
inputs in order. A `Call` to a sub-graph whose `inputs` has three
entries with four arguments is `Error_CallSignature`. The types
must match (the callee's `inputs[i].type` against the caller's
argument type) at load time. The editor validates the call at
wiring time; the loader re-validates.

A `Return` node in the callee's stream ends the callee's walk and
returns to the caller's `Call` node's `successor`. The callee's
`outputs` list says which scratchpad cells the caller should copy
out. The caller's `Call` node has a `outputs` argument (a list of
`{ "name": string, "path": string }`) that names the caller's
scratchpad cells to write into. The names must match the callee's
output names (`Error_ReturnSignature`).

### The entry node

The first node in `nodes` must be an `Engine_Entry_Node`. Its
`successor` is a `Successor` argument naming the next node's
`id`. A sub-graph with no `Entry` is `Error_MissingEntry` at load
time.

---

## Nodes

A node is one entry in a sub-graph's `nodes` array.

```json
{
  "id": 0,
  "type": "Engine_Entry_Node",
  "args": {
    "successor": 1
  }
}
```

- `id`: integer. Unique within the sub-graph. The walker uses
  `id` as the program counter; `successor` arguments reference
  `id`s. The first node's `id` must be 0; subsequent ids must be
  unique (`Error_DuplicateNodeId`). Ids do not need to be
  contiguous, but the editor writes them contiguous starting at 0.
- `type`: string. The node type's full name
  (`Engine_Branch_Bool_Node`, `Alice_Add_Vector3_Vector3_Node`).
  The node registry must know it (`Error_UnknownNodeType`).
- `args`: object. Maps each argument's name to its value. The
  argument set must match the node type's `NodeArgInfo` list:
  - Every required argument must be present.
  - No unknown argument is allowed.
  - Each argument's value must match its `NodeArgKind`
    (`Value` or `Successor`); see §"Node argument" below.

A node with no arguments has an empty `args` object. A node with
only a `successor` has `"args": { "successor": <id> }`.

### Node argument

A node's argument is one of four JSON shapes, distinguished by the
`kind` field:

**Literal:**

```json
{ "kind": "literal", "type": "int32", "value": 5 }
```

`type` is the argument's declared type. `value`'s JSON shape
follows the type: a JSON number for a numeric primitive, a
JSON boolean for `bool`, a JSON string for `string` or `char`, a
JSON object for a struct, a JSON `null` for a handle's nil. The
literal's type must match the node argument's declared type
(`Error_ArgumentTypeMismatch`).

**Resource reference:**

```json
{ "kind": "resource", "resource": "PlayerConfig", "field": "max_hp" }
```

The resource must be declared in `resources`
(`Error_UnknownResource`); the field must exist in the resource
(`Error_UnknownResourceField`); the field's type must match the
node argument's declared type. The loader resolves the reference
to a literal at load time; the loaded project carries the value,
not the reference.

**Path:**

```json
{ "kind": "path", "path": "scratch.hp" }
```

A path names a cell in the current sub-graph's scratchpad
(`scratch.<cell>`) or a column in the World (`world.<table>.<column>`).
The cell or column's type must match the node argument's declared
type (`Error_ArgumentTypeMismatch` for the scratchpad,
`Error_UnknownWorldColumn` for the World). A path is resolved at
load time to a `(region, index)` pair (scratchpad column index,
or `(table_index, column_index)`); the runtime reads and writes
the cell directly.

The `path` string is human-readable; the loaded project does not
carry it. A path that is not a `scratch.` or `world.` prefix is
`Error_UnknownPathRegion`.

**Successor:**

```json
{ "kind": "successor", "successor": 3 }
```

`successor` is a node `id` in the same sub-graph. The node type's
argument must be declared as `NodeArgKind::Successor`
(`Error_ArgumentKindMismatch` otherwise). The target must be a
node in the same sub-graph (`Error_UnknownSuccessor`). The
successor's type must be an Action node; a successor cannot point
to a Value node (there are none on the user surface, but the
loader checks anyway) or to a node in another sub-graph
(`Error_CrossSubgraphSuccessor`).

**A successor is only for control-flow arguments.** An
`Engine_Entry_Node`'s `successor` names the next node. A
`Branch`'s `on_true` and `on_false` are successors. A `Jump`'s
`target` is a successor. A `Call`'s `successor` is the return
point; its `callee` is a sub-graph name (not a successor) and its
`args` are value arguments.

### The engine's control-flow nodes

The engine registers the following node types; the editor's
palette presents them; the loader validates them; the walker
executes them. Their argument shapes:

| Node                      | Args                                                                                                                                                 |
| ------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Engine_Entry_Node`       | `successor: Successor`                                                                                                                               |
| `Engine_Jump_Node`        | `target: Successor`                                                                                                                                  |
| `Engine_Branch_Bool_Node` | `selector: Value(path to bool)`, `on_true: Successor`, `on_false: Successor`                                                                         |
| `Engine_Switch_Int_Node`  | `selector: Value(path to int32)`, `cases: Value(JSON array of {value: int32, successor: Successor})`, `default: Successor`                           |
| `Engine_Call_Node`        | `callee: Value(string sub-graph name)`, `args: Value(JSON array of arguments)`, `outputs: Value(JSON array of {name, path})`, `successor: Successor` |
| `Engine_Return_Node`      | `successor: Successor` (the caller's saved PC; the loader resolves it from the caller's `Call`)                                                      |
| `Engine_Yield_Node`       | (no args)                                                                                                                                            |

A sub-graph that uses one of these must declare it as a node in
`nodes`; the loader resolves its `successor` args as described
above.

> [!NOTE]
> `Engine_Return_Node`'s `successor` is written by the editor as
> the caller's `Call`'s `successor`. It is the same node id in
> the caller's sub-graph, but the editor writes it into the
> callee's node's `args` so the loader can validate the
> sub-graph's return path statically. This is a serialization
> convenience; the runtime does not read it. See
> `ExecutionModel.md` §1.

---

## Engine state

```json
"engine_state": {
  "registry_fingerprint": "sha256:abc123..."
}
```

- `registry_fingerprint`: string. A hash of the loaded packages'
  vocabulary (node types, their args, their kinds; table names;
  enum names and members; handle names; struct names and fields).
  The engine computes it after package loading and compares it
  against the project's value. A mismatch is
  `Error_RegistryMismatch` at load time. The fingerprint is
  optional; a hand-written project that omits it is loaded
  without the check.

The fingerprint protects against loading a project against a
package set it was not authored against. It is the project
format's equivalent of the `.lucgraph` fingerprint; see
`Decision.md` Decision 11.

> [!NOTE]
> The fingerprint's *value* is implementation-defined. Two
> engines with the same package set produce the same fingerprint;
> two engines with different package sets produce different
> fingerprints. The format defines what is hashed (the package
> vocabulary), not the hash algorithm. The editor writes it; the
> runtime checks it; a hand project that skips it is accepted
> without the check. This is deliberate: the fingerprint is a
> safety net, not a contract.

---

## Saving

The editor writes the project file. `ProjectSaver_save(project, path)`
in the editor's project module writes the project's JSON to disk.
The project is the editor's in-memory model; the file is its
serialization. There is no "save from a running game" operation;
the runtime is read-only against the project.

`ProjectSaver_from_editor(editor_state)` produces a `Project`
whose JSON reflects the editor's current composition. The editor
calls it when the user saves; the file is written from the
returned `Project`.

The saver does not validate the project. The editor's live
validator catches most issues as the user composes (unknown node
types are impossible; the palette only offers registered nodes;
argument kinds are enforced by the wiring UI; scratchpad paths
are resolved against the sub-graph's `scratch` block). The loader
re-validates on load; a project the editor accepted and the
loader rejected is an editor bug.

---

## Evolution rules

The schema changes over time. The rules:

**Compatible changes** — old projects load correctly, no
migration, no version bump required (the team may bump for
bookkeeping):

- Adding an optional field to a top-level object (`engine_state`
  gaining a field; a sub-graph gaining a `description` field).
- Adding a JSON key to a node's `args` for a node type the
  project does not use. The loader ignores unknown args on
  unused node types (it only validates a node's args against its
  type's `NodeArgInfo`).
- Widening a field's range or precision. The new type accepts
  the old values.
- Adding a new control-flow node type. Old projects that don't
  use it are unaffected.

**Breaking changes** — old projects need migration or must be
refused:

- Removing a top-level key. Old projects that use it lose data.
- Removing a node type, argument, or kind. Old projects that use
  it are rejected.
- Narrowing a field's type in a lossy way. Old values may not
  parse.
- Repurposing a field (keeping the name, changing the meaning).
  Old projects load but behave differently. **Avoid.** This is
  the worst kind of breaking change because it fails silently.
- Changing the walker's semantics (e.g., a node's `successor`
  meaning changing from "next node" to "next node + 1"). This is
  a *runtime* change, not a format change, but it makes old
  projects behave differently without a format change. Bump the
  format version to force a reload; the load will fail with
  `Error_BadVersion` for the old version.

**Rule of thumb:** if a project written for version N loads in
version N+1's engine without loss and without surprise, the
change was compatible.

**Version bump policy:** bump `kProjectFormatVersion` on every
schema change. The loader accepts older versions with a log
line, so bumping on compatible changes gives a version history
without noise. Breaking changes must bump and must either
migrate the old project or refuse it with a clear message.

> [!NOTE]
> The pivot itself is a breaking change for the project format
> if `.lucgraph` was ever a project's format. It was not: the
> `.lucgraph` was a compiler output, not a save format. The
> project format is new; `kProjectFormatVersion` starts at 1.

---

## What the project is not

- **It is not a `.lucgraph`.** The `.lucgraph` was a compiled
  output of the format library. There is no format library, no
  compiler, and no `.lucgraph`. The project is the composition,
  not a compilation of it.
- **It is not a `.lscene`.** The scene is data; the project is
  behavior. The two are separate, joined only by
  `Engine_Script`.
- **It is not a source file.** There is no text language. The
  editor is the surface; the project is the data behind it.
- **It is not human-authorable in practice.** A hand-written
  project is technically valid (the loader accepts it; the
  fingerprint is optional), but it is verbose and the editor
  exists for a reason. The format is documented so a tool can
  read or write it; a human uses the editor.

---

## The `.lucproj` and the `.lscene` together

A game consists of:

- One or more `.lscene` files — the scenes (levels, menus).
- One `.lucproj` file — the project's behavior (sub-graphs,
  phases, resources).

At startup, the game's `main.cpp`:

1. Loads packages (`PackageManager::load_package`, then `apply`).
2. Loads the project (`runtime.load(path)`), which validates the
   project against the loaded packages.
3. Loads a scene (`SceneLoader_load_into(world, components,
   path)`), which populates the world's tables.
4. Attaches the scene's `Engine_Script` rows to the project's
   sub-graphs (`runtime.attach(world)`).

Then the tick loop runs (`Dataflow.md` §7). A scene reload
re-runs steps 3–4; a project reload re-runs step 2.

The project and the scene are independent files. A project
shipped without its scenes is a behavior library with nothing to
run; a scene shipped without its project is data with no
behavior. The two together are the game.

---

## What this note does not cover

- **The editor's in-memory model of the project.** The editor's
  `Project` type is the editor's; its fields mirror the JSON
  format, but the doc describes the JSON, not the editor's
  structs. See `apps/editor/include/editor/project.h`.
- **The node registry.** The project's node types are the
  registry's; the registry is populated by the packages. See
  `PackageComposition.md` and `EditorMetadata.md`.
- **The walker's execution.** See `ExecutionModel.md`.
- **The package-owned-table boundary.** See
  `PackageTables.md`.
- **The scene's `Engine_Script` seam.** See `SceneFormat.md`.
- **The runtime's `attach` step.** How the scene's
  `Engine_Script` rows become per-entity sub-graph instances is
  the runtime's; the project format's job is to carry the
  sub-graph names.

---

## Cross-references

- `docs/notes/ExecutionModel.md` — the walker, phases,
  sub-graphs, scratchpad.
- `docs/notes/Dataflow.md` — the runtime model.
- `docs/notes/PackageTables.md` — the package-owned-table
  convention.
- `docs/notes/SceneFormat.md` — the scene format and the
  `Engine_Script` seam.
- `docs/notes/PackageComposition.md` — how packages register the
  node types the project references.
- `docs/notes/EditorMetadata.md` — how the editor reads the
  registry to build the palette.
- `docs/Decision.md` — the design decisions behind the format.