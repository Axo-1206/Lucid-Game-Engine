# The dataflow model

This note states the model the engine runs on. It is the
foundational document; every other note assumes it. If you read
one thing about how Lucid works, read this.

See also:
- `docs/notes/ExecutionModel.md` — the walker, the phases, the
  sub-graphs, the scratchpad. This note assumes you have read it.
- `docs/notes/SceneFormat.md` — how a scene file describes the
  initial state of the tables.
- `docs/notes/PackageNaming.md` — how packages name their
  content.
- `docs/notes/PackageComposition.md` — what a package contributes
  and how packages compose.
- `docs/notes/PackageTables.md` — the package-owned-table
  convention.
- `docs/notes/WorldLifecycle.md` — how a world is emptied and
  what survives.
- `docs/notes/PhysicsPlan.md` — how physics applies this model.

---

## 1. The model in one paragraph

The engine is a **table-mediated dataflow**. Entities have
components. Components are rows in tables. Nodes and systems both
read and write tables. The engine loop runs the graph (nodes),
then the systems, then the renderer. Each step reads the state
the previous step left in the tables. Nodes do not reach into
other nodes; a node reads the tables and a per-sub-graph
scratchpad, and writes the tables and the scratchpad. Systems do
not call nodes; they read and write tables. The tables are the
interface. That is the whole model.

> [!NOTE]
> The old model also let a node read another node's *result* via
> an `Arg::Kind::NodeRef` argument, and had a Value/Action/Trigger
> distinction. The new model has neither. Every user-facing node
> is an Action; its inputs are literals, resource fields, and
> paths into the World or the scratchpad; its output is a
> write to the World or the scratchpad. See `ExecutionModel.md`.

---

## 2. Entities

An **entity** is a unit of identity. It is a row in the
`Engine_Entity` table. It has:

- An `id` (a stable integer, unique within a scene).
- A `name` (a display string, not a unique key).
- A `parent` (a `RowRef` to another entity, or nil).

An entity by itself has no data beyond its identity. What makes
it interesting is the **components** attached to it.

**Not everything is an entity.** Cameras are rows in the
`Engine_Camera` table, keyed by name. They have identity and
data, but they are not entities. This is a deliberate decision
(see `docs/Decision.md` Decision 3): cameras are scene-level, not
entity-level.

**Not everything is a component.** Tables hold data, but not
every table is an entity's component. Some tables (`Engine_Camera`,
`Engine_EngineState`) are engine-managed and hold no `entity`
column. A component table is a table with an `entity` column that
references `Engine_Entity`.

---

## 3. Components

A **component** is a row in a table, keyed by the entity it
belongs to. The table's `entity` column is its primary key: it
references a row in `Engine_Entity`.

Example: `Engine_Transform`.

```
Engine_Transform
├── entity: RowRef  (primary)
├── x: float32
├── y: float32
├── z: float32
└── rot_z: float32
```

An entity that has a transform has a row in this table. An entity
that doesn't, doesn't. **Presence is the fact of the row.**
There's no `has_component` flag; the row either exists or it
doesn't.

Components are **data only**. They have no methods, no behavior,
no references to nodes or systems. A component is a schema, and a
row is a value.

The set of component tables a package registers is part of the
package's vocabulary. A package's components, node types, enums,
and handles are registered together. A package registers a
component table with `add_component`, which registers the table and
its scene codec in one call. See `PackageNaming.md` and
`PackageComposition.md`.

---

## 4. Nodes

A **node** is a unit of behavior in a graph.

> [!NOTE]
> The old model had three node kinds (Value, Action, Trigger), a
> `NodeExecutor` signature of `void (TickContext&, uint32_t)`,
> `arg_values`, `struct_results`, and `Arg::Kind::NodeRef`
> arguments. The new model has **one user-facing kind — Action** —
> and a `NodeExecutor` signature of
> `uint32_t (TickContext&, uint32_t)` returning the next node's
> index (or `HALT_FOR_TICK`, or `HALT`). The full node-kind
> system, the value-node result buffer, and node references have
> been removed from the user surface. What follows describes the
> new shape.

**A node's executor is a C++ function.** It receives:

- `TickContext`, which has:
  - `world` — the table store. The executor can read and write
    any table its package owns, and read the engine's core
    tables. See §8.
  - `frame` — the current sub-graph frame. Holds the scratchpad
    cells this sub-graph declared, and a `results` buffer that
    actions use for their per-node per-tick scratch (a branch's
    decision, a computed temporary). See `ExecutionModel.md`.
  - `services` — engine-wide services (physics backend, and
    future audio and rendering services).
  - `dt` — seconds since the last tick.
  - `phase` — the id of the phase whose sub-graph is running.
    See §7.

**A node's arguments are its inputs.** An argument is one of:

- A **literal.** A compile-time constant.
- A **resource field.** A compile-time constant, declared by a
  resource in the project. See §7's "Resources" note.
- A **path.** A string that names a cell in the World or the
  scratchpad. `"world.Player.hp"` names the `hp` column of the
  entity's `Player` row; `"scratch.low"` names the `low` cell in
  the sub-graph's scratchpad. The path is resolved at project
  load time into a `(table, column, key)` triple or a
  `(scratch_column)` index, so the executor reads a typed cell,
  not a string lookup. See `ExecutionModel.md` §3.
- A **successor.** A node index. Only control-flow nodes
  (`Branch`, `Jump`, `Call`, `Return`) have successor arguments.
  See `ExecutionModel.md` §2.

**A node's output is its write to the World or the scratchpad.**
There is no "node result" the way the old model had one. If a
node computes a value, it writes the value to a World column or
a scratchpad cell. The next node reads it from there.

**Nodes do not reach into other nodes.** A node cannot call
another node's executor. A node cannot read another node's
internal state. A node cannot read another node's *result*
because there is no such thing — a node writes a table cell or a
scratchpad cell, and the next node reads the cell. This is the
only channel between nodes.

> [!NOTE]
> The old model's struct argument (`struct_results`) does not
> exist on the user surface. A package that wants to pass a
> struct between its own nodes uses the scratchpad: a `struct`
> cell type declared in the sub-graph's scratchpad, written by
> one node and read by the next. This is a package-internal
> convention; it is not user-visible.

### 4.1 The node's place in the walk

A node runs when the walker's program counter reaches it. See
`ExecutionModel.md` for the walker. An action returns the index
of the next node to run; a control-flow node chooses between
successors. The walker's loop is:

```
pc = entry
while pc != HALT and pc != HALT_FOR_TICK:
    pc = executor[node.type_id](ctx, pc)
```

The node's executor is a single function; the walker calls it
once per visit. If a node needs to loop internally — a physics
solver, a particle system — it loops in C++ inside its executor
and returns the next node's index. The graph has no loop; the
executor has a C++ loop. This is the same as any bytecode VM's
opcode.

---

## 5. Systems

A **system** is a C++ function that runs every frame, outside the
graph. It receives `SystemContext`, which has:

- `world` — the table store.
- `renderer` — the renderer interface.
- `input` — the input state.
- `clock` — the frame clock.
- `render_list` — a per-frame list of draw items.
- `services` — engine-wide services.

Systems are the engine's per-frame infrastructure. They are not
part of any graph. They read and write tables the same way nodes
do.

Examples of systems:

- `hierarchy_system` — computes `Engine_WorldTransform` from
  `Engine_Transform` and the entity hierarchy.
- `movement_system` — integrates `Engine_Velocity` into
  `Engine_Transform`. (May be replaced by a node in a future
  phase.)
- `render_system` — builds a `RenderList` from the world's
  sprites and transforms.
- `ui_system` — draws the UI pass.
- `physics_step_system` (future) — steps the physics backend.
- `physics_sync_system` (future) — copies body state to
  `Engine_Transform` and `Engine_Velocity`.

**Systems do not call nodes.** Systems do not know a graph
exists. They read and write tables, and the graph reads and
writes tables, and the two communicate through the tables.

**Systems do not call each other.** Systems are called by the
engine loop, in a defined order. Each reads the state the
previous one left.

---

## 6. Tables as the interface

The tables are the interface between everything:

- Between the graph and the systems.
- Between the systems themselves.
- Between the systems and the renderer.
- Between a package's nodes and a package's other nodes.
- Between the packages themselves. See `PackageTables.md`.

The tables are the shared state. Every writer writes to a table;
every reader reads from a table. There is no in-memory channel
between phases; there is only the table store and, within one
sub-graph, its scratchpad.

**Why this is the model:**

- **Systems and nodes are independent.** A system does not know
  a node exists; a node does not know a system exists. They can
  be developed, tested, and replaced separately.
- **The scene format is the schema.** A scene file describes the
  initial rows of the tables. Loading a scene is populating
  tables. Saving a scene is reading tables. The scene and the
  runtime share the same schema.
- **Debugging is possible.** The tables are the state. To see
  what the engine is doing, look at the tables. To reproduce a
  bug, replay the writes.
- **The package boundary is a table boundary.** A package owns
  its tables; its nodes are the sanctioned way to read and write
  them. See `PackageTables.md`.

> [!NOTE]
> The old model also justified "the format can be statically
> checked" by node argument types being known at compile time.
> That justification survives in spirit — the project file's
> node arguments are validated at load time against the package
> vocabulary — but it is no longer a compiler's job. The editor
> validates; the engine validates; there is no separate
> compiler. The justification is stated here in the terms of the
> new model.

---

## 7. The engine loop

The engine loop runs in a fixed order each frame:

```
0. Scene setup. If a scene was loaded this frame,
   scene.apply_to(world, components, ApplyMode::Clear) runs and
   runtime.load() follows. load() reads the project file (see
   ProjectFormat.md) and validates its sub-graphs and node
   arguments against the loaded packages. Nothing else in the
   loop reads the project file.
1. Input is polled. The InputState is updated.
2. runtime.tick(dt) is called. The phase scheduler runs each
   engine-defined phase in order. Within each phase, the phase's
   entry sub-graph is walked. Nodes read and write tables and
   the per-sub-graph scratchpad, and call into services.
3. Systems that produce world state run once per frame, in a
   defined order:
   - physics_step_system (future)
   - physics_sync_system (future)
   - hierarchy_system
   - movement_system (if it still exists)
   - hierarchy_system (again, after movement)
4. renderer.begin_frame(clear_color).
5. For each View in the view list:
   - set_viewport(view.x, view.y, view.w, view.h).
   - If World2D: set_camera_2d(to_camera2d(view.camera, view.w, view.h));
     begin_world_2d(); render_system(ctx, dt); end_world_2d().
   - If UI: begin_ui(); ui_system(ctx, dt); end_ui().
   - set_full_viewport().
6. renderer.end_frame(); renderer.present().
```

> [!NOTE]
> Step 0's old wording ("load() walks the world's
> Engine_Script table and loads each graph") is replaced. The
> project file is loaded once, not per entity. The
> `Engine_Script` component is repurposed (see `SceneFormat.md`)
> to reference a *named sub-graph* in the project, not a
> `.lucgraph` path. This is the one place where the scene and
> the project meet.
>
> Step 2's old wording ("Value nodes evaluate; source triggers
> fire; gated triggers fire conditionally; fired actions run") is
> replaced by the phase-scheduler description. See
> `ExecutionModel.md` §4 for the phase model.

**Phases.** The engine defines an ordered list of phases:
`pre_physics`, `physics`, `post_physics`, `pre_render`,
`render`, `post_render`, `ui`, or whatever the engine chooses.
The project registers, per phase, which sub-graph is the
phase's entry. The engine's `PhaseScheduler` runs each phase in
order. The user does not wire phase order; the engine guarantees
it. Within a phase, the user wires whatever nodes they want.
See `ExecutionModel.md` §4.

**Views.** A `View` (`engine/render/view.h`) is a scope of draws
that share a world, a camera, a viewport, and a pass. The loop
iterates a `std::vector<View>`. The game builds one `World2D`
view of the scene's active camera, filling the window, and one
`UI` view. An editor adds a scene view and a game view. The
header describes each field; `RenderPasses.md` describes the
passes.

**Every step reads the tables left by the previous step.** A
write by a node is visible to the systems. A write by a system
is visible to the next system and to the next tick's graph, but
not to the current tick's graph (the graph already ran).

**The order is fixed by the engine.** A caller does not choose
the order. The order is documented here and in the code.

**Why fixed order:** it is predictable. A node's write and a
system's read have a known ordering. Debugging is possible; a
reproduction is deterministic (modulo real-time input and
non-deterministic backends).

---

## 8. What nodes can and cannot do

> [!NOTE]
> The old list assumed Value nodes (side-effect-free, write a
> result to a shared buffer) and Action nodes (side-effectful,
> write tables) were both first-class on the user surface. The
> new model has only Action nodes on the surface. The rules
> below are the Action-only rules.

**A node can:**

- Read any table in `world` that its package owns, and read the
  engine's core tables (`Engine_Entity`, `Engine_Transform`,
  `Engine_WorldTransform`).
- Write any table in `world` that its package owns.
- Read and write the current sub-graph's scratchpad cells.
- Read its own arguments (literals, resource fields, paths).
- Read `TickContext::services` and call into a service.
- Read `TickContext::frame` to inspect the current sub-graph's
  scratchpad and per-node per-tick scratch. It cannot modify the
  frame stack; only `Call`/`Return` do that.
- Read `TickContext::graph` to inspect the project's structure
  (rare; for editor-debug and inspection nodes).

**A node cannot:**

- Call another node's executor.
- Read another node's internal state.
- Read another node's "result" — there is no result; a node
  writes a cell, and the cell is read by the next node.
- Modify another node's arguments.
- See the future. A node cannot read a value another node will
  write later in the same tick.
- Read or write another package's tables directly. See
  `PackageTables.md`. This is a policy, not an enforcement; the
  engine cannot stop a package from calling
  `world.get_table("Alice_Player")`. Code review does.

**A node should not:**

- Read `TickContext::graph` to reach across the graph, except in
  specific patterns (an editor-debug node that renders the
  project's structure, a node that needs to know its own node's
  arguments beyond the resolved ones).
- Write tables that are not part of its package's domain.
- Allocate unbounded memory per frame. An executor that loops
  internally is fine; an executor that allocates a fresh
  `std::vector` every tick is a performance bug. Use a
  package-internal `mutable` buffer, or a scratchpad cell, as
  `ExecutionModel.md` §3 describes.

**The boundary between packages:** a package's nodes read and
write the package's tables, plus the engine's core tables. A
package that needs another package's data is a design question,
not a runtime one. See `PackageComposition.md` §8 and
`PackageTables.md`.

> [!NOTE]
> "A node writes a table cell" is the write. There is no
> "write a node result and let the next node read it from a
> buffer" channel. If you want a value to flow from one node to
> the next, write it to a World cell or a scratchpad cell, and
> the next node's path argument names it. This is the same
> discipline as a register machine with a named register file.

---

## 9. What systems can and cannot do

**A system can:**

- Read any table.
- Write any table.
- Call the renderer (only the render system should).
- Read input, clock, services.
- Read and write the render list.

**A system cannot:**

- Call a node.
- Know that a graph exists.
- Choose its own execution order.

**A system should not:**

- Read or write tables outside its domain unless necessary.
- Allocate unbounded memory per frame.
- Do work that a node should do. A system is per-frame
  infrastructure; a node is per-frame behavior. If a system's
  job depends on the game's design (e.g. "when the player presses
  jump"), it belongs in a node. If it's universal (e.g. "compute
  world transforms"), it belongs in a system.

---

## 10. The two paths

Concretely, a write from a node to a table reaches a system like
this:

**Path A — a node writes, a system reads.**

1. A node's executor writes `Engine_Transform.x`.
2. The graph tick ends.
3. `hierarchy_system` reads `Engine_Transform` and writes
   `Engine_WorldTransform`.
4. `render_system` reads `Engine_WorldTransform` and adds a
   render item.

**Path B — a system writes, a node reads.**

1. `physics_step_system` writes `Engine_BodyRuntime.pos_x`.
2. `physics_sync_system` copies it to `Engine_Transform.x`.
3. The next tick, a node's executor reads `Engine_Transform.x`.
4. The node produces a value (writes a cell) or performs an
   action (writes a cell).

Both paths are one-directional per tick. A node writes, systems
read; a system writes, nodes read on the next tick. **There is no
back-and-forth within a tick.**

This is the ordering guarantee that makes the engine predictable.

---

## 11. What this model is not

**It is not ECS with behavior on components.** Components have no
behavior; they are data. Behavior is in nodes and systems.

**It is not Unity's MonoBehaviour.** Nodes are not per-entity;
they are per-graph. One node acts on many entities if its executor
iterates a table.

**It is not a scripting language.** There is no text source; the
project is data; the engine's vocabulary is the node types; the
user composes nodes in the editor. The graph is validated at load
time, not compiled.

**It is not an event bus.** Nodes are walked; there is no queue.
A `Branch` chooses its successor; a `Jump` returns a target; a
`Call` pushes a frame. There is no propagation of a "fire" from a
node to its subscribers — the walker simply goes to the next
node. See `ExecutionModel.md`.

**It is not a functional program.** The graph has side effects
(Action nodes write tables). The systems have side effects (they
mutate the world). There is no Value-node/ Action-node purity
distinction on the user surface; every node is an Action, and
every Action is effectful by definition.

> [!NOTE]
> The old model said "purity is a property of individual Value
> nodes." The new model has no Value nodes on the user surface.
> A package can still write pure helpers internally, but they
> are not the graph's building blocks; the scratchpad cell is.

---

## 12. Why this model

The model is a specific choice among alternatives:

- **Alternative A — behavior on components (Unity).** A
  component's class has `update()`; the world iterates
  components and calls their methods. Rejected because it
  couples behavior to data and prevents the format from
  statically checking anything.

- **Alternative B — a scripting language.** A node is a bytecode
  instruction; the graph is a program. **This is now the model.**
  The old rejection ("it requires a VM, a type system, and an
  evaluation model") is *why the engine builds the walker, the
  packages, and the load-time validation.* The VM is the walker;
  the type system is the node argument validation; the
  evaluation model is the executor contract.

- **Alternative C — pure ECS with systems only.** Systems do
  everything; there are no graphs. Rejected because per-frame
  systems cannot express game-specific behavior (only universal
  behavior), and a game needs both.

- **Alternative D — the table-mediated dataflow.** Chosen.
  Nodes express game behavior; systems express per-frame
  infrastructure; both share the tables. The walker is the
  graph's control flow; the tables are the graph's data flow.

> [!NOTE]
> Alternative B was rejected in the old model; it is the model
> now. The trade is stated plainly: the walker is a bytecode VM,
> the packages are its instruction set, the project is its
> bytecode. This is a deliberate choice; it buys a rich editor
> and a controllable runtime at the cost of no text source.

The model is simple, statically checkable (at project load time,
against the package vocabulary), and separates the concerns (data,
behavior, infrastructure). It scales from a small 2D game to a
large multi-package game without changing its shape.

---

## 13. The model in one line

**The tables are the interface. Nodes write them; systems write
them; the renderer reads them through the render list. Nothing
reaches across the graph; nothing reaches across the systems.
The engine loop orders the phases.**

---

## 14. Cross-references

- `docs/notes/ExecutionModel.md` — the walker, the phases, the
  sub-graphs, the scratchpad.
- `docs/notes/ProjectFormat.md` — the project file the engine
  loads, its sub-graphs, its node arguments, its resources, its
  phase registrations.
- `docs/notes/SceneFormat.md` — how a scene file populates the
  tables initially.
- `docs/notes/PackageNaming.md` — how packages name their
  content (node types, tables, enums, handles).
- `docs/notes/PackageComposition.md` — what a package
  contributes and how packages compose.
- `docs/notes/PackageTables.md` — the package-owned-table
  convention, and the cross-package boundary.
- `docs/notes/EditorMetadata.md` — how the editor reads the
  registry and the metadata.
- `docs/notes/RenderPasses.md` — how the renderer draws, given
  the tables the systems have produced.
- `docs/notes/WorldLifecycle.md` — how a world is emptied and
  what survives.
- `docs/notes/PhysicsPlan.md` — how physics applies this model.
- `docs/Decision.md` — the design decisions that led to this
  model.