# The execution model

This note explains how the engine runs a project: the walker, the
phases, the sub-graphs, the scratchpad, and how a project expresses
control flow. It replaces the old `TriggerModel.md`. The old model
had three node kinds (Value, Action, Trigger) and `on`
subscriptions; the new model has one user-facing kind (Action), a
per-node `next()` that returns the index of the next node to run,
and phases that the engine owns and the project populates.

The short version: **the graph is an instruction stream. Each
node's executor returns the index of the next node. Control flow
is the executor's return value. Phases are engine-owned
scheduling boundaries. Sub-graphs are functions with their own
frames and scratchpads.**

See also:
- `docs/notes/Dataflow.md` — the model this note assumes (nodes,
  systems, tables, services).
- `docs/notes/ProjectFormat.md` — the project file the engine
  loads.
- `docs/notes/PackageComposition.md` — how a package registers
  the Actions, tables, and types this note uses.
- `docs/notes/PackageTables.md` — the package-owned-table
  convention.
- `docs/notes/PhysicsPlan.md` — a worked phase.

---

## 0. What changed since `TriggerModel.md`

The old doc described three node kinds and `on` subscriptions. The
new model has none of that. If you remember the old doc, here is
the mapping:

| Old                                                          | New                                                                                                                         |
| ------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------- |
| Value node (produces a value, no side effect)                | Action that writes its result to a scratch cell or a World cell.                                                            |
| Action node (performs a side effect)                         | Action. Unchanged in spirit.                                                                                                |
| Trigger node (fires; subscribers run)                        | Action whose executor returns a successor. There is no separate kind.                                                       |
| `A on B` (A runs when B fires)                               | `A <- B` (B's successor is A). A is a `Successor` argument of B.                                                            |
| `Engine_EveryFrame_Node` (source trigger)                    | An Action whose executor returns its successor. The walker reaches it once per tick because it is on the tick's entry path. |
| `Engine_If_Bool_Node(cond) on parent` (gating trigger)       | `Branch(selector_path, [true -> A, false -> B])`. Reads a cell; returns one of two successors.                              |
| `value_order` (Value nodes evaluated in dependency order)    | No value order. A node writes its result to a cell; the next node reads the cell. The order is the walker's order.          |
| `phase_order` (Actions sorted by phase)                      | Phases are engine-owned, project-populated entry sub-graphs, run in order. See §3.                                          |
| Trigger firing propagation (subscribers fired within a tick) | The walker just goes to the next node. There is no propagation.                                                             |

Read the rest of this note as the "new" column, in detail.

---

## 1. The walker

A **sub-graph** is an ordered list of nodes. A **node** is an
entry in that list. The **walker** is a loop:

```
pc = sub_graph.entry
while pc != HALT and pc != HALT_FOR_TICK:
    pc = executor[sub_graph.nodes[pc].type_id](ctx, pc)
```

That is the entire runtime. `pc` is a program counter — an index
into the sub-graph's node list. `HALT` ends the whole tick; `HALT_FOR_TICK`
ends the current sub-graph and returns control to the scheduler.

**Each node's executor returns the next `pc`.** A node with a
single successor returns the successor's index. A node with no
successor (the sub-graph's terminator) returns `HALT` or
`HALT_FOR_TICK`. A node with more than one successor (`Branch`,
`Switch`) picks one based on its inputs and returns it. A `Call`
node pushes a frame and returns the callee's entry. A `Return`
node pops a frame and returns the caller's saved `pc`.

**The executor's return value is the control flow.** There is no
"the engine decides what runs next" step. The node decides what
runs next; the engine obeys. This is the fundamental shift from
the old model, where the engine's scheduler decided firing order.

### 1.1 The node types

The engine registers the following built-in control-flow Actions.
A package may not register its own `Branch`, `Jump`, etc.; these
are the engine's. A package registers *ordinary* Actions (see
§5), and the engine's built-ins provide the control flow.

| Node                      | Inputs                                                                         | Behavior                                                                                                       | Returns              |
| ------------------------- | ------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------- | -------------------- |
| `Engine_Entry_Node`       | (none)                                                                         | Marks the start of a sub-graph.                                                                                | its single successor |
| `Engine_Jump_Node`        | `target: Successor`                                                            | Unconditional jump.                                                                                            | `target`             |
| `Engine_Branch_Bool_Node` | `selector: path to bool`, `on_true: Successor`, `on_false: Successor`          | Reads `selector`; returns `on_true` or `on_false`.                                                             | one of the two       |
| `Engine_Switch_Int_Node`  | `selector: path to int32`, `cases: [(int32, Successor)]`, `default: Successor` | Reads `selector`; returns the matching case's target or `default`.                                             | one successor        |
| `Engine_Call_Node`        | `callee: SubGraphId`, `args: [...]`                                            | Pushes a frame; binds `args` to the callee's inputs; jumps to the callee's entry.                              | callee's entry `pc`  |
| `Engine_Return_Node`      | (none)                                                                         | Pops a frame; returns to the caller's saved `pc`.                                                              | caller's saved `pc`  |
| `Engine_Yield_Node`       | (none)                                                                         | Ends the current tick. The scheduler resumes at the next phase (or the next tick, if this was the last phase). | `HALT_FOR_TICK`      |

That is the entire control-flow vocabulary. Everything else is an
ordinary Action.

> [!NOTE]
> `Engine_Entry_Node` is the `main` node of the old model. It
> has a single successor, the first node to run in the
> sub-graph. A sub-graph with no `Entry` is a load-time error
> (`Error_MissingEntry`).

### 1.2 Successors vs. arguments

A node's inputs come in two flavors:

- **Value inputs.** Literals, resource fields, paths into the
  World or the scratchpad, or handles. The executor reads them
  via typed `TickContext` accessors.
- **Successor inputs.** Node indices. Only the engine's
  control-flow nodes have these. The executor reads them via
  `ctx.successor(node_index, arg_i)`.

The distinction is carried by `NodeArgKind` (`Value` or
`Successor`) in the node type's `NodeArgInfo` (see
`PackageComposition.md` §5). The editor enforces it at wiring
time; the project validator enforces it at load time.

---

## 2. A conditional, read two ways

Consider the old `TriggerModel.md` example: regenerate health when
the player's HP is below max.

**The old form:**

```lucid
node every_frame  = Engine_EveryFrame_Node()
node hp           = Game_PlayerHp_Entity_Node(player)
node max_hp       = Game_PlayerMaxHp_Entity_Node(player)
node below_max    = Engine_LessThan_Float_Float_Node(hp, max_hp)
node if_below_max = Engine_If_Bool_Node(below_max) on every_frame
node regen        = Game_Regen_Entity_Float_Node(player, 5.0) on if_below_max
```

**The new form:**

```
sub-graph Gameplay {
    scratch {
        hp:      float32,
        max_hp:  float32,
        below:   bool,
    }

    nodes:
        entry   : Engine_Entry_Node                 successor: read_hp
        read_hp : Game_PlayerHp_Entity_Node         args: [player, "scratch.hp"]   successor: read_max
        read_max: Game_PlayerMaxHp_Entity_Node      args: [player, "scratch.max_hp"] successor: cmp
        cmp     : Game_LessThan_Float_Float_Node    args: ["scratch.hp", "scratch.max_hp", "scratch.below"] successor: gate
        gate    : Engine_Branch_Bool_Node           args: ["scratch.below", true -> regen, false -> done]
        regen   : Game_Regen_Entity_Float_Node      args: [player, 5.0]              successor: done
        done    : Engine_Yield_Node
}
```

**The old mechanic, in one sentence:** every frame, if hp <
max_hp, regenerate 5.

**The new mechanic, in one sentence:** every tick, the walker
visits `entry`, `read_hp`, `read_max`, `cmp`, `gate`; if `below`
is true it visits `regen`, else it goes to `done`; `done` yields.

The sentence is the same. The mechanic is a walk, not a firing
tree. **`Branch` is the conditional; `scratch` cells are the
intermediate values; the walker's `pc` is the program order.**

---

## 3. Phases

The engine defines an ordered list of phases. The project
populates each phase with a sub-graph. The scheduler runs each
phase in order, and within each phase, walks its sub-graph.

**The engine's phases (illustrative):**

```
pre_physics
physics
post_physics
pre_render
render
post_render
ui
```

The list is engine-owned. A package does not add a phase; the
engine does. (A future engine could let a package add a phase, but
that is out of scope today.) The names and order are in
`engine/graph/phases.h`.

**The project's phase registrations:**

```
phases:
    pre_physics:  MyInputUpdate
    physics:      DefaultPhysics
    render:       MyRenderPass
    ui:           MyHud
```

Each value names a sub-graph in the project. The engine's
`PhaseScheduler` walks that sub-graph during that phase.

**A phase's sub-graph must not call another phase's sub-graph.**
A `Call` may target a sub-graph in the same phase, or a sub-graph
that is *not* registered on any phase (a "function"). Anything
else is `Error_CrossPhaseCall` at load time. This preserves the
scheduler's guarantee: `physics` always runs before `render`.

> [!NOTE]
> A "function" is a sub-graph with no phase registration. Any
> phase's sub-graph may `Call` a function. A function's nodes
> run in the caller's phase. This is how a package ships a
> reusable sub-graph (see `PackageComposition.md` §11).

**What "phase is on the registration, not the node" means:** a
node type does not have a phase. `Engine_MoveBody_Node` is not a
"physics-phase node"; it is a node that the user can place in any
phase's sub-graph. Whether it runs in `pre_physics` or `physics`
is the *project's* choice, not the *node type's*. The editor
groups nodes by `NodeTypeInfo::category` for the palette, but
`category` is a display hint, not a phase.

**Warn_NodeInWrongPhase.** If a package sets `phase_hint` on a
node type (a display hint: "this node is typically used in the
`physics` phase"), the editor warns when the user places the node
in a sub-graph of a different phase. It is a warning, not an
error; the user may override. The `phase_hint` is advisory.

---

## 4. Sub-graphs

A **sub-graph** is a named list of nodes with an entry, an
optional input list, an optional output list, and a scratchpad
declaration. There are three roles a sub-graph can play:

- **Phase entry.** Registered on a phase. The scheduler runs it.
- **Function.** Not registered on any phase. Called by a `Call`.
- **Template.** A package-shipped sub-graph the editor offers as
  a starting point. It is not run directly; the user instantiates
  it into a phase's sub-graph slot.

**A `Call` binds inputs and outputs.** The caller's `Call` node
has a `callee: SubGraphId` and a list of arguments (values from
the caller's scratchpad, the World, or literals). The callee's
inputs are declared in its sub-graph declaration. On `Call`, the
runtime pushes a frame; the frame holds a fresh scratchpad sized
by the callee's scratch declaration, and the callee's input
values. On `Return`, the runtime copies the callee's declared
outputs back to the caller's scratchpad or World cells (as the
caller's `Call` node's output paths) and pops the frame.

**A sub-graph's scratchpad is frame-local.** A cell written by a
node in `MyPhysics` is not visible to a node in `MyRender`; the
two sub-graphs have separate scratchpads. This is the frame
isolation rule: a sub-graph's locals die on `Return`.

**The sub-graph's input and output lists are the frame's
contract.** A `Call` to a sub-graph whose input list does not
match the caller's argument count and types is `Error_CallSignature`
at load time. The editor validates this when the user wires the
call.

**Loops are sub-graphs with a back-edge.** A `Jump` to an earlier
node in the same sub-graph is a within-tick loop. The loop must
terminate; the editor warns (`Warn_PotentialInfiniteLoop`) when a
back-edge exists and no node in the loop writes a cell that a
`Branch` in the loop reads. This is a heuristic, not a proof; the
user is responsible for termination.

**Across-tick loops are `Yield` + the scheduler.** A `Yield` ends
the current sub-graph's walk for the tick; the scheduler moves to
the next phase, or to the next tick if this was the last phase.
An "every frame" behavior is a sub-graph registered on the phase
whose actions run every tick; a "once per second" behavior is a
sub-graph whose first node reads `dt` and branches on a
counter. The engine provides no built-in timer node; packages do
(`Engine_OnceEvery_Seconds_Node`, etc.).

---

## 5. The scratchpad

The scratchpad is a per-frame, per-sub-graph typed cell set. A
sub-graph declares its cells in a `scratch { ... }` block; the
editor resolves paths against them; the runtime sizes the
scratchpad from the declaration.

**A cell's type is one of:** `bool`, `char`, `string`, `int8`..
`int64`, `uint8`..`uint64`, `float32`, `float64`, an enum, a
handle, or a struct. The engine's scratchpad cell types are the
same as its table column types plus the types its packages
register (`add_struct`, `add_enum`, `add_handle`).

**A path is a string.** `"scratch.hp"` names the `hp` cell in the
current sub-graph's scratchpad. `"world.Player.hp"` names the
`hp` column of the entity's `Player` row in the World. The
distinction is the prefix: `scratch.` is the scratchpad,
`world.` is the World. The editor and the validator resolve the
path at load time to a typed cell; the runtime does an indexed
read, not a string lookup.

**A struct-typed cell** is the way a package exchanges compound
values between its nodes without going through the World. A
package registers a struct (`add_struct`), a sub-graph declares a
cell of that type (`pairs: CollisionPair`), one node writes
`pairs`, the next reads `pairs`. The scratchpad is the package's
private exchange; the cell does not leak to other packages, other
sub-graphs, or other frames.

**Scratchpad cells are not tables.** They live in the sub-graph's
frame, not in `World`. They are not observable from systems; a
system sees only the World. The scratchpad is a sub-graph's
private working memory.

**The scratchpad is reset at `Yield` and at `Return`.** A cell's
value does not survive a `Yield`. A cell's value does survive
within a straight-line region (a node writes, the next node
reads). If a package needs state across `Yield`, it uses a
World table. If it needs state across `Call` boundaries, it
passes the state as an argument and reads it as an output.

---

## 6. Why this is not "if inside a loop"

The old `TriggerModel.md` had a section under this heading. The
content still applies, with one change: the graph is no longer a
subscription tree; it is an instruction stream.

In an imperative language:

```
loop {
    if condition {
        action
    }
}
```

The loop body runs top to bottom. The `if` selects whether
`action` runs. `condition` is evaluated once per iteration, at
the point the `if` appears in the body.

In the engine:

- The "loop" is a `Branch` with a back-edge: `gate: Branch(cond,
  [true -> body, false -> done])`, `body: ...`, `loop: Jump(gate)`.
  The walker visits `gate`, then `body` or `done`, then `loop` if
  it went `body`, then `gate` again.
- The "if" is the `Branch`. Its `cond` is a scratchpad path. The
  branch reads the path each time the walker reaches it.
- There is no body list to run "top to bottom"; the walker's
  order is whatever the nodes' successors say.

The result is the same. The mechanism is a program counter, not
a subscription tree. The mental model: **the walker's `pc` is the
program order. The graph is the program.**

---

## 7. Ordering within a tick

The order of node execution within a tick is the walker's `pc`
order, subject to `Branch` decisions, `Jump` targets, and `Call`
boundaries.

**Across phases, the order is fixed by the engine.** `pre_physics`
before `physics` before `post_physics` before `pre_render` before
`render` before `post_render` before `ui`, or whatever order
`engine/graph/phases.h` declares. The project does not choose the
order.

**Within a phase, the order is the sub-graph's.** The entry's
successor, then that node's successor, and so on. A `Branch`'s
decision picks the successor. A `Jump`'s target is a fixed
successor. A `Call`'s callee runs to `Return`.

**A node that wants to see another node's output** must be after
the other node in `pc` order. If two nodes write the same cell,
the second one's write wins; the editor warns
(`Warn_DoubleWrite`) when two nodes in a sub-graph write the same
cell without an intervening read, but this is a heuristic, not
an enforcement. The order is the program order; the user is
responsible for it.

**A node cannot see the future.** A node's `pc` is a node index;
the executor cannot read a cell another node will write later in
the same tick. This is the same rule as any imperative program.

---

## 8. What "control flow" means now

The old `TriggerModel.md` said "the graph is the control flow"
and meant "the subscription structure is the control flow." The
new model says "the graph is the control flow" and means "the
successor edges are the control flow." The difference:

- **Subscription:** a node `A` writes `A on B`; when `B` fires,
  `A` is asked to fire. `A` and `B` are peers; the relationship
  is one-way (from `B` to `A`), but neither is "before" or
  "after" the other in a linear sense; the engine's scheduler
  decides the order.
- **Successor:** a node `A` has `successor: B`; when `A` runs,
  the walker goes to `B`. `A` and `B` are consecutive in the
  program order. The relationship is "then."

A graph has both kinds of edges in the old model (subscriptions
and argument references). The new model has only successors. The
argument references that survive are `Value` arguments: literals,
resource fields, paths. Those are inputs, not edges; the editor
draws them as data wires, but they don't determine execution
order. The execution order is the successor edges.

This is the mental model change the pivot asks of a reader who
knows the old doc: **stop thinking "what fires what" and start
thinking "what comes after what."**

---

## 9. Who sets the order

The graph's successors set the order of nodes within a sub-graph.
Other orders are set elsewhere:

| Scope                                  | Set by                            | Documented in                         |
| -------------------------------------- | --------------------------------- | ------------------------------------- |
| Frame (input, graph, systems, render)  | Engine loop                       | `Dataflow.md` §7                      |
| System order                           | Engine loop                       | `Dataflow.md` §7, `PhysicsPlan.md` §6 |
| Phase order                            | Engine (`engine/graph/phases.h`)  | This note, §3                         |
| Sub-graph order (which node runs when) | The sub-graph's successors        | This note, §1                         |
| Argument resolution                    | The walker (reads cells)          | This note, §5                         |
| Package registration order             | `main.cpp`'s `load_package` calls | `PackageNaming.md`                    |

The walker is a program counter. The sub-graph's successors are
the program. The phase order is the engine's. The project
populates the phases; the packages provide the nodes; the editor
composes them.

---

## 10. The `Engine_EveryFrame_Node` question

The old model had `Engine_EveryFrame_Node` as a source trigger
that fires every tick. The new model has no triggers. What
replaces it?

**Nothing specific.** The "every frame" behavior is a sub-graph
registered on a phase. The phase runs every tick; the sub-graph
runs every tick. The user does not need an `EveryFrame` node; the
phase's registration is the "every frame" declaration.

If a package wants to expose "run this sub-graph once per second"
or "run this on a key press", it ships a node that branches on
its condition. `Engine_OnceEvery_Seconds_Node(seconds)` reads a
per-node counter from the World or the scratchpad and returns a
successor on the tick the counter hits zero. The user composes
`Engine_OnceEvery_Seconds_Node(1.0)` with a `Branch` on its
result. The `EveryFrame` behavior is free; more specialized
cadences are package nodes.

> [!NOTE]
> This is a philosophical shift: the old model gave every
> "trigger" behavior a dedicated node type. The new model gives
> the walker one primitive (the phase's entry) and expects
> packages to compose cadences as ordinary nodes. The vocabulary
> is smaller; the package is bigger. The trade is deliberate:
> one model, one primitive, packages compose.

---

## 11. Summary

| Concept                                   | What it is                                                                         |
| ----------------------------------------- | ---------------------------------------------------------------------------------- |
| Walker                                    | The loop: `pc = executor[...](ctx, pc)` until `HALT` or `HALT_FOR_TICK`.           |
| Sub-graph                                 | An ordered list of nodes with an entry, optional inputs/outputs, and a scratchpad. |
| Action                                    | A node whose executor returns the next `pc`. All user-facing nodes are Actions.    |
| `Successor` argument                      | A node index. Only engine control-flow nodes have these.                           |
| `Value` argument                          | A literal, resource field, path into the World or the scratchpad, or handle.       |
| Scratchpad                                | Per-sub-graph, per-frame typed cell set. Path prefix `scratch.`.                   |
| Phase                                     | Engine-owned scheduling boundary. Project-populated with a sub-graph.              |
| `Engine_Branch_*`                         | The conditional. Reads a cell, returns one of two successors.                      |
| `Engine_Jump_Node`                        | An unconditional successor. A loop when the target is earlier.                     |
| `Engine_Call_Node` / `Engine_Return_Node` | Sub-graph call. Pushes/pops a frame.                                               |
| `Engine_Yield_Node`                       | Ends the tick. The scheduler moves to the next phase.                              |
| `Error_CrossPhaseCall`                    | A `Call` from one phase's sub-graph to another phase's sub-graph.                  |
| `Warn_NodeInWrongPhase`                   | A node placed in a sub-graph whose phase differs from its `phase_hint`. Advisory.  |
| `Warn_PotentialInfiniteLoop`              | A back-edge with no write the loop's branch reads. Advisory.                       |

The mental model: **the graph is the program. The walker is the
program counter. The scratchpad is the stack frame. The phases
are the scheduler. Sub-graphs are functions.** Nothing fires;
everything runs in order.

---

## 12. Cross-references

- `docs/notes/Dataflow.md` — the model this note assumes.
- `docs/notes/ProjectFormat.md` — the project file the engine
  loads.
- `docs/notes/PackageComposition.md` — how packages register the
  nodes this note composes.
- `docs/notes/PackageTables.md` — the package-owned-table
  convention.
- `docs/notes/SceneFormat.md` — the `Engine_Script` component
  references a named sub-graph in the project.
- `docs/notes/PhysicsPlan.md` — a worked phase.
- `docs/Decision.md` — the design decisions behind the walker.