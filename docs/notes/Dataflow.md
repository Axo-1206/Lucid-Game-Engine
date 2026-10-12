# The dataflow model

This note states the model the engine runs on. It is the
foundational document; every other note assumes it. If you read
one thing about how Lucid works, read this.

See also:
- `docs/notes/TriggerModel.md` — how a Lucid graph expresses
  control flow, given this model.
- `docs/notes/SceneFormat.md` — how a scene file describes the
  initial state of the tables.
- `docs/notes/PackageNaming.md` — how packages name their
  content.
- `docs/notes/PhysicsPlan.md` — how physics applies this model.

---

## 1. The model in one paragraph

The engine is a **table-mediated dataflow**. Entities have
components. Components are rows in tables. Nodes and systems both
read and write tables. The engine loop runs the graph (nodes),
then the systems, then the renderer. Each phase reads the state
the previous phase left in the tables. Nodes do not reach into
other nodes; they receive other nodes' results as arguments.
Systems do not call nodes; they read and write tables. The tables
are the interface. That is the whole model.

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
and handles are registered together. See `PackageNaming.md`.

---

## 4. Nodes

A **node** is a unit of behavior in a graph. Nodes come in three
kinds (see `TriggerModel.md` and the format library's
`NodeKind`):

- **Value nodes** produce a value. No side effects.
- **Action nodes** perform a side effect. They read and write
  tables.
- **Trigger nodes** fire. They ask their subscribers to fire.

**A node's executor is a C++ function.** It receives:

- `TickContext`, which has:
  - `world` — the table store. The executor can read and write
    any table.
  - `graph` — the compiled graph. The executor can read its own
    node's arguments and subscribers if it needs to, but this is
    rare.
  - `results` — the per-node result scratch. A Value node writes
    its result here.
  - `arg_values` — the resolved values of this node's arguments.
  - `dt` — seconds since the last tick.
  - `services` — engine-wide services (physics backend, and
    future audio and rendering services).

**A node's arguments are its inputs.** An argument is one of:

- A literal.
- A reference to another node's result.
- A resource field (a compile-time constant).

The runtime resolves arguments before the executor runs. The
executor sees values, not node references.

**A node's result is its output.** A Value node writes a value to
`results[node_index]`. An Action node has no result. A Trigger
node has no result.

**Nodes do not reach into other nodes.** A node cannot call
another node's executor. A node cannot read another node's
internal state. If node A needs a value node B produces, A
declares B's result as an argument; the runtime resolves it.
This is the only channel between nodes.

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

The tables are the shared state. Every writer writes to a table;
every reader reads from a table. There is no in-memory channel
between phases; there is only the table store.

**Why this is the model:**

- **The format can be statically checked.** Node arguments are
  types known at compile time; the format library's Sema checks
  them. If nodes could reach into other nodes at runtime, the
  compiler could not check anything.
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

---

## 7. The engine loop

The engine loop runs in a fixed order each frame:

```
1. Input is polled. The InputState is updated.
2. runtime.tick(dt) is called. The graph's nodes execute:
   Value nodes evaluate; source triggers fire; gated triggers
   fire conditionally; fired actions run. Nodes read and write
   tables and call into services.
3. Systems run, in a defined order:
   - physics_step_system (future)
   - physics_sync_system (future)
   - hierarchy_system
   - movement_system (if it still exists)
   - hierarchy_system (again, after movement)
4. The renderer begins a frame.
5. render_system builds a RenderList and submits it.
6. The renderer ends the frame and presents.
```

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

**A node can:**

- Read any table in `world`.
- Write any table in `world` (if it is an Action node; Value
  nodes are supposed to be side-effect-free).
- Read its own arguments.
- Write its own result (Value nodes only).
- Read `TickContext::services` and call into a service.
- Read `TickContext::graph` to inspect the graph structure.

**A node cannot:**

- Call another node's executor.
- Read another node's internal state.
- Read another node's result unless it is declared as an
  argument.
- Modify another node's arguments.
- See the future. A node cannot read a value another node will
  write later in the same tick.

**A node should not:**

- Read `TickContext::graph` to reach across the graph, except in
  specific patterns (a trigger that inspects its subscribers, an
  action that needs to know its own node's structure).
- Write tables that are not part of its package's domain. A
  package's nodes own a package's tables.
- Read tables that are not part of its package's domain, unless
  the package declares a dependency.

**The boundary between packages:** a package's nodes read and
write the package's tables, plus the engine's core tables
(`Engine_Entity`, `Engine_Transform`, `Engine_WorldTransform`).
A package that needs another package's data is a design question,
not a runtime one. See `PackageNaming.md` and the eventual
package dependency note.

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
4. The node produces a value or performs an action.

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

**It is not a scripting language.** The graph is statically
checked; there is no reflection, no dynamic lookup, no evaluation.
A node's arguments are fixed at compile time.

**It is not an event bus.** Nodes fire subscriptions, but there
is no global event queue; the fires propagate synchronously within
the tick.

**It is not a functional program.** The graph has side effects
(Action nodes write tables). The systems have side effects (they
mutate the world). Purity is a property of individual Value
nodes, not of the engine.

---

## 12. Why this model

The model is a specific choice among alternatives:

- **Alternative A — behavior on components (Unity).** A
  component's class has `update()`; the world iterates
  components and calls their methods. Rejected because it
  couples behavior to data and prevents the format from
  statically checking anything.

- **Alternative B — a scripting language.** A node is a bytecode
  instruction; the graph is a program. Rejected because it
  requires a VM, a type system, and an evaluation model, and
  the graph format is designed to be simpler than a language.

- **Alternative C — pure ECS with systems only.** Systems do
  everything; there are no graphs. Rejected because per-frame
  systems cannot express game-specific behavior (only universal
  behavior), and a game needs both.

- **Alternative D — the table-mediated dataflow.** Chosen.
  Nodes express game behavior; systems express per-frame
  infrastructure; both share the tables.

The model is simple, statically checkable, and separates the
concerns (data, behavior, infrastructure). It scales from a
small 2D game to a large multi-package game without changing its
shape.

---

## 13. The model in one line

**The tables are the interface. Nodes write them; systems write
them; the renderer reads them through the render list. Nothing
reaches across the graph; nothing reaches across the systems.
The engine loop orders the phases.**

---

## 14. Cross-references

- `docs/notes/TriggerModel.md` — how the graph's nodes express
  control flow in this model.
- `docs/notes/SceneFormat.md` — how a scene file populates the
  tables initially.
- `docs/notes/PackageNaming.md` — how packages name their
  content (node types, tables, enums, handles).
- `docs/notes/EditorMetadata.md` — how the editor reads the
  registry and the metadata.
- `docs/notes/RenderPasses.md` — how the renderer draws, given
  the tables the systems have produced.
- `docs/notes/PhysicsPlan.md` — how physics applies this model.
- `docs/Decision.md` — the design decisions that led to this
  model.
```
