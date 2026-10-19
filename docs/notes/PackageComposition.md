# Package composition

What a package contributes, how it declares it, how one package
uses another's content, and how a package owns its tables. This
note is the policy layer above `PackageNaming.md` (which covers
names) and `PackageTables.md` (which covers table ownership at
runtime). It assumes `Dataflow.md` (the model) and
`ExecutionModel.md` (the walker, phases, sub-graphs, scratchpad).

See also:
- `docs/notes/PackageNaming.md` — the prefix convention and the
  registry.
- `docs/notes/PackageTables.md` — the package-owned-table
  convention and the cross-package boundary.
- `docs/notes/Dataflow.md` — nodes, systems, tables, services.
- `docs/notes/ExecutionModel.md` — the walker, phases,
  sub-graphs, scratchpad.
- `docs/notes/EditorMetadata.md` — the editor metadata a package
  registers, including node operations.
- `docs/notes/PhysicsPlan.md` — a worked package
  (`engine.physics_2d`) that uses every mechanism this note
  describes.
- `docs/Decision.md` — the decisions behind the package system.

---

## 1. What a package is

A package is a C++ object that registers content with the engine
at startup. It is a `Package` subclass. Its
`register_into(PackageBuilder&)` is called exactly once, in load
order, before the project file is loaded.

A package contributes six kinds of content:

- **Node types** — user-facing Actions, with a signature and a
  C++ executor. See §5.
- **Executors** — the C++ function that runs when the walker
  reaches the node. See §5.
- **Types** — enums, handles, and structs that the project and
  the editor can name. See §4.
- **Tables and component codecs** — the package's tables, their
  scene serializers where applicable, and their schemas. This is
  the package's **data**, and under the pivot it is the primary
  thing a package owns. See §3 and `PackageTables.md`.
- **Scratchpad cell types** — struct types a package declares
  for its sub-graphs' scratchpads. See §4.2 and
  `ExecutionModel.md` §3.
- **A dependency declaration** — the packages this package
  requires, checked at load time. See §8.1.

> [!NOTE]
> The old package contributed `.lucid` helper nodes (`.lucid`
> files the package shipped, whose exported nodes entered the
> registry at compile time). That kind is gone: there is no
> `.lucid` file. The old package also registered three node
> kinds (Value, Action, Trigger) with a shared `void` executor
> and a `TriggerMode`; the new package registers Actions with a
> `uint32_t` executor. See §5.

A package owns all six. It is the unit of registration, of
naming, and of versioning. It is not a runtime object; there is
one instance per package, created by the game's `main.cpp` and
handed to the `PackageManager` before `apply()` or
`apply_registration_only()`.

A package has two halves, and they live in two places. The
`Package` subclass — the object the `PackageManager` loads, the
thing that calls `PackageBuilder::add_*` — lives in
`engine/include/engine/registry/packages/`. The package's public
C++ types — the structs and classes other code includes, and the
tables its nodes read and write — live in the layer the package
serves: `engine/math/` for `engine.math`, `engine/render/` for
`engine.render`. A package can have one, the other, or both.

`engine.core` and `engine.logic` have the Package subclass only;
they ship no public C++ types beyond the registry. `engine.math`
ships `Vector2` under `engine/math/`. `engine.render` ships
`Color` under `engine/render/`. The `Package` subclass for each
lives under `registry/packages/`.

A user's package follows the same convention, in the user's tree.
The engine cannot name the user's paths; it names the interface
(`Package`, `PackageBuilder`). A user who wants a package that
ships a public type puts the type wherever the user's code
expects it and subclasses `Package` in the user's own source
tree.

---

## 2. What a package is not

A package is not:

- **A plugin.** There is no dynamic loading, no ABI contract, no
  version negotiation. A package is linked into the executable.
  Its `Package` subclass is constructed by the game's
  `main.cpp`.
- **A runtime object.** The `Package` instance exists only during
  registration. After `PackageManager::apply()` or
  `apply_registration_only()`, the package's contributions live
  in the node registry and the component registry. The `Package`
  object is not reachable.
- **A file format.** A package may ship C++ headers, but those
  are the package's source, not a packaged artifact. There is no
  `.lucidpkg` file.
- **A service.** A package may offer a service (see §7), but a
  package is not itself a service. The two are separate concepts.

> [!NOTE]
> The old doc listed "`.lucid` helper files" as a package's
> source. That kind is gone. The old doc also said a package
> "may ship `.lucid` helper files and C++ headers"; only the C++
> headers remain.

---

## 3. Registering a package

A package's `register_into` calls the `PackageBuilder`'s `add_*`
methods. Each call declares one piece of content. The builder
checks the name against the package's prefix and against every
name already registered.

```cpp
void AliceMathPackage::register_into(PackageBuilder& b)
{
    b.add_handle(HandleTypeInfo{"Alice_Vector3"});

    // A table that is not an entity component: no entity column, no codec.
    // Under the pivot, this is where a package's data lives.
    b.add_table("Alice_Vector3Table",
        [](engine::ecs::World& world) {
            auto* t = world.create_table("Alice_Vector3Table");
            t->add_column("id", ColumnType::Uint64, /*primary=*/true);
            t->add_column("x",  ColumnType::Float32);
            t->add_column("y",  ColumnType::Float32);
            t->add_column("z",  ColumnType::Float32);
        });

    // A component table: an entity column, serialized with the scene.
    b.add_component("Alice_Health",
        [](engine::ecs::World& world) {
            auto* t = world.create_table("Alice_Health");
            t->add_column("entity", ColumnType::Uint64, /*primary=*/true);
            t->add_column("hp",     ColumnType::Float32);
            t->add_column("max_hp", ColumnType::Float32);
        },
        ComponentCodec{
            .load = load_health,
            .save = save_health,
        });

    // Under the pivot, the "Vector3 add" node is an Action: it reads
    // its arguments from the World and the scratchpad, and writes its
    // result to the scratchpad. Its executor returns the next PC.
    static std::array<NodeArgInfo, 3> add_args = {{
        {"a", TypeId::handle("Alice_Vector3"), NodeArgKind::Value},
        {"b", TypeId::handle("Alice_Vector3"), NodeArgKind::Value},
        {"out", TypeId::primitive("string"), NodeArgKind::Value},  // scratch cell path
    }};
    b.add_action_node(NodeTypeInfo{
        "Alice_Add_Vector3_Vector3_Node",
        NodeKind::Action,
        "Math",
        ArenaSpan(add_args.data(), add_args.size()),
        },
        exec_vector3_add);

    // ... more nodes ...

    b.add_node_editor_info("Alice_Add_Vector3_Vector3_Node", NodeEditorInfo{
        "Vector3 Add",
        "Add two Vector3 values and write the result to a scratch cell.",
        "",
        "add",
        Operation::Add,
    });
}
```

Every name starts with the package's prefix. The builder enforces
this. A registration that violates the prefix, or that collides
with an existing name, is recorded as a conflict;
`PackageManager::apply` refuses to run if any conflict exists.

`add_component` registers a table and its codec in one call. The
table creator runs at apply time; the codec is stored in the
`ComponentRegistry` for the scene loader and saver. A package
that wants a component to be serializable registers it this way.
`add_table` registers a table without a codec: the scene loader
does not know how to move the table's rows to or from JSON, and
the saver skips the table. This is the right call for a
package's *internal* table (a runtime cache, an index, a
scratchpad backing store) and it is what the engine itself does
for `Engine_Camera` and `Engine_BodyRuntime`.

A package that registers a struct type uses `add_struct`. The
struct's fields are declared in the registry; the C++ struct is
the package's own type in a header the package ships. The
package author keeps the two in sync. See §4.2.

A component table's name must start with the package's prefix,
like every other registered name. A codec's table name must be
unique across all packages: two packages that register a codec
for the same table name is a conflict caught at registration
time.

> [!NOTE]
> The old example registered `add_node_type` +
> `add_node_executor` + `add_trigger_mode`. That trio is replaced
> by `add_action_node` (which takes the info and the executor in
> one call). `TriggerMode` is gone. The old `Alice_Add_Vector3_Vector3_Node`
> was a `NodeKind::Value`; it is now `NodeKind::Action`, writes
> its result to a scratch cell, and its executor returns the
> next PC.
>
> The `NodeArgInfo` struct now has a `NodeArgKind` field
> (`Value` or `Successor`), so a node's arguments can be
> distinguished as data vs. control-flow targets. See
> `ExecutionModel.md` §2.

### 3.1 Editor metadata

A package registers editor metadata for each node type with
`add_node_editor_info`. The metadata includes an `Operation`
value that names the operation (`Operation::Add`,
`Operation::LessThan`) and, through the engine's operation table
(`engine/registry/operation.h`), a visual group. A package that
uses an operation the enum does not have sets `Operation::Custom`
and supplies `custom_operation` and `operation_group_override`.
See `EditorMetadata.md`.

> [!NOTE]
> The old `NodeEditorInfo` carried an `example` field holding a
> `.lucid` snippet. `.lucid` is gone, so the field is removed.
> See `EditorMetadata.md`.

---

## 4. Types

The project and the editor can name four kinds of type:

- **Primitives.** `bool`, `char`, `string`, and the sized integer
  and float types (`int8`..`int64`, `uint8`..`uint64`, `float32`,
  `float64`). These are declared by the engine. A package cannot
  add a primitive.
- **Enums.** A named set of members, registered by a package. An
  enum member is a value. The project names the enum type and
  its members (`Engine_Key`, `Engine_Key.W`).
- **Handles.** An opaque reference, registered by a package.
  Every handle has `nil` as a valid value. Two handles to the
  same resource compare equal; two handles to different
  resources compare unequal. The project names the handle type
  (`Engine_BodyRef`, `Alice_Vector3`).
- **Structs.** A named record of fields, registered by a package
  with `add_struct`. A struct value is passed between nodes as a
  unit (`Engine_Vector2`).

A package that wants a compound type (a vector, a matrix, a
quaternion, a transform) has two options: a handle plus a table,
or a struct.

### 4.1 A handle plus a table

The package registers a handle (`Alice_Vector3`) and a table
(`Alice_Vector3Table`). The handle is an opaque id; the value
lives in the table. Operations are nodes.

```
handle:  Alice_Vector3
table:   Alice_Vector3Table  (id: uint64 primary, x: float32,
                              y: float32, z: float32)
nodes:   Alice_New_Float_Float_Float_Node(x, y, z, out)
         Alice_GetX_Vector3_Node(v, out)
         Alice_GetY_Vector3_Node(v, out)
         Alice_GetZ_Vector3_Node(v, out)
         Alice_Add_Vector3_Vector3_Node(a, b, out)
         Alice_Scale_Vector3_Float_Node(v, s, out)
         Alice_Length_Vector3_Node(v, out)
```

This is the ECS-idiomatic choice. It matches `Engine_BodyRef` in
`PhysicsPlan.md`. It keeps the type system simple: the only new
thing the project sees is a handle. The `out` argument is a
scratchpad path; the node writes its result there. (The old
model's "returns a value" is gone; a node's output is a write to
the scratchpad or the World.)

A `bob.game` package that wants to use `Alice_Vector3`:

- Its sub-graphs declare the handle type in their node arguments.
- It composes `alice.math`'s nodes to compute with vectors.

`bob.game` cannot compute on a `Vector3` without going through
`alice.math`'s nodes. That is the package boundary.

The `Alice_Vector3Table` is not an entity component: its rows are
not attached to entities, and the table has no `entity` column.
It is registered with `add_table`, not `add_component`, and it has
no codec. Vectors created by `Alice_New_Float_Float_Float_Node`
live for the session; a scene that wants to persist a vector
carries the entity that owns it and the component that stores the
handle, not the vector table's rows.

If a package wants a vector to be part of an entity — a velocity,
a spawn offset — it registers a *component* table with
`add_component`, whose columns include the vector's fields as
primitives (`float32` `x`, `float32` `y`, `float32` `z`) and whose
codec reads and writes them. The distinction is: the handle
pattern is for shared values referenced from many places; the
component pattern is for values attached to an entity and
serialized with it.

### 4.2 A struct

The package registers a struct with `add_struct`. The struct's
fields are declared in the registry, and the project sees a value
of that struct type. The package ships the C++ struct in a header;
the two are kept in sync by the package author.

```cpp
// alice/math/vector3.h
namespace alice::math {
    struct Vector3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; };
}
```

Nodes take and return the struct as one value. Under the pivot, a
struct-typed argument is a `NodeArgKind::Value` argument, and the
value is *the handle to the struct's row in a table* or *a
scratchpad struct cell*. There is no per-node `struct_results`
buffer on the user surface; a struct value lives in a table row
or a scratchpad cell.

> [!NOTE]
> The old model passed a struct value by bytes in
> `ctx.struct_results[node_index]`, with consumers reading the
> producer's buffer via `arg_node_index`. That buffer is gone on
> the user surface. Under the pivot, a struct value is stored in
> a table row (the handle pattern) or a scratchpad struct cell
> (a `Struct` cell type), and the node's argument is the handle
> or the path. The package author chooses. See
> `ExecutionModel.md` §3 and `PackageTables.md`.

A struct keeps the type system small and avoids a table for a
value that fits in a few fields. It is copied, not referenced: two
nodes holding the same struct hold two independent values.

### 4.3 The rule

**A compound type is either a handle plus a table, or a struct
registered with `add_struct`.** A package cannot add a new
primitive to the engine's type system.

The choice is the package author's. Handles are the right choice
when the value is large, shared, or referenced from many places
(a physics body, a texture, a mesh). Structs are the right choice
when the value is small and copied (a vector, a color, a
quaternion).

### 4.4 Scratchpad cell types

A package that wants its sub-graphs to hold a temporary of a
particular type declares a **scratchpad cell type**. A
scratchpad cell type is a `StructTypeInfo` (or a primitive, or an
enum, or a handle) that the package's sub-graphs declare in their
`scratch { ... }` block. The editor and the engine resolve
scratchpad paths against the sub-graph's declared cells. See
`ExecutionModel.md` §3.

> [!NOTE]
> Scratchpad cell types are new in the pivot. They are the
> mechanism by which a package's nodes exchange temporaries
> without touching the World. A package that ships a
> `CollisionPair` struct declares it with `add_struct` and its
> sub-graphs declare `pairs: CollisionPair` in their scratchpad.
> A node writes `pairs`, the next node reads `pairs`. The
> scratchpad is per-sub-graph; it does not leak across frames or
> calls.

---

## 5. Nodes and executors

A node type is a project-visible name with:

- A kind (`Action` on the user surface). The engine still has
  `Value` internally for its own math, but user-facing packages
  should register `Action` nodes only.
- A list of argument names, kinds (`Value` or `Successor`), and
  types.
- A category (for the editor's palette).

The executor is a C++ function pointer of type
`engine::graph::ActionExecutor`:

```cpp
std::uint32_t (*)(engine::graph::TickContext& ctx, std::uint32_t node_index)
```

A node type and its executor are registered together, in the same
`register_into`. The builder keeps them parallel: the Nth executor
is the executor for the Nth node type. `PackageManager::apply`
verifies the counts match.

An executor does four things:

- **Reads its arguments** from the current frame's scratchpad and
  the world's tables. A `Value` argument is a literal, a resource
  field, a path (into the World or the scratchpad), or a handle;
  a `Successor` argument is a node index. The `TickContext`
  provides typed accessors for each.
- **Writes its output** to the World or the scratchpad.
- **Returns the next node's index**, or `HALT_FOR_TICK`, or
  `HALT`.
- **Reads and writes tables** in `ctx.world`.
- **Calls services** through `ctx.services`.

An executor does not call another node's executor. It does not read
another node's internal state. It does not choose its own execution
order. See `Dataflow.md` §8 for the full list.

> [!NOTE]
> The old `NodeExecutor` was `void (TickContext&, uint32_t)`.
> The new `ActionExecutor` returns `uint32_t`, the next PC. This
> is the pivot's central mechanical change: every executor's
> `return` value is a node index, and the walker uses it to pick
> the next node. A node that wants fall-through returns
> `node_index + 1`; a branch returns one of its successors; a
> call pushes a frame and returns the callee's entry; a return
> pops a frame and returns the caller's saved PC; a yield
> returns `HALT_FOR_TICK`. See `ExecutionModel.md` §2.

### 5.1 Where the work happens

The work a node performs lives in one of three places:

- **In the executor itself.** A pure computation reads its
  arguments and writes a result. No table access. No service
  call. The executor is the whole implementation.
- **In a table the executor reads or writes.**
  `Engine_CreateBody_Entity_Node` writes a row in
  `Engine_BodyRuntime`. `Alice_New_Float_Float_Float_Node` writes
  a row in `Alice_Vector3Table`. The table is the package's; the
  executor touches only its own package's tables (see §8).
- **In a service the executor calls.**
  `Engine_ApplyForce_BodyRef_Float_Float_Node` calls
  `ctx.services.physics->apply_force(...)`. The service is the
  backend; the executor is the glue. `Engine_Log_Node` calls
  `engine::log_info`. `Render_*` nodes write to a table that a
  render system reads (see §6).

The pattern is: **the executor is the boundary between the graph
and the thing the graph is asking for.** The thing itself is a
table, a service, or an inline computation.

### 5.2 Action nodes and the walker

Under the pivot, every user-facing node is an `Action`. The
walker walks the project's action graph; each Action's executor
runs and returns the next PC. There is no separate "value pass."

> [!NOTE]
> The old model's three kinds — `Value`, `Action`, `Trigger` —
> are reduced to `Action` on the user surface. The old "Value
> nodes are evaluated on demand, in `value_order`" is gone; there
> is no `value_order`. The old "Trigger nodes fire" is gone;
> there are no triggers. `Engine_EveryFrame_Node` is now an
> Action whose executor returns its successor; the walker reaches
> it once per tick because it's on the tick's entry path. See
> `ExecutionModel.md` §2 and §4.

A package's node types should be `Action` only. The engine's own
math package may ship `Value` nodes for its internal use, but
user-facing packages should not.

### 5.3 The `TickContext` an executor sees

An executor's `TickContext` carries `world`, `frame` (the current
sub-graph's frame, holding its scratchpad and per-node per-tick
scratch), `services`, `dt`, `phase` (the current phase id), and
`graph` (the loaded project). See `Dataflow.md` §4 for the field
list and `ExecutionModel.md` §5 for the frame model.

> [!NOTE]
> The old `TickContext` carried `graph` (the compiled `.lucgraph`),
> `results` (one `CellValue` per node), `arg_values` (resolved
> arguments), and `struct_results` (per-node struct buffers). The
> pivot's `TickContext` carries `world`, `frame` (with its
> scratchpad and per-node scratch), `services`, `dt`, `phase`,
> and `graph` (the loaded project, for introspection). The
> per-node `results` and `arg_values` buffers are gone; a node's
> data is in the World or the scratchpad.

### 5.4 `.lucid` helper nodes

> [!CAUTION]
> The old §5.3 described `.lucid` helper nodes: `.lucid` files a
> package shipped, whose exported nodes entered the registry at
> compile time. There is no `.lucid`, no compiler, and no
> "compile time" registry. The section is deleted. A package
> that wants a composed node writes it in C++ (an `Action` whose
> executor calls the other nodes' logic) or registers a
> sub-graph template (see §10).

---

## 6. Rendering

Rendering is a system, not a node. The engine loop runs the graph
tick first, then the systems. The graph writes tables; the systems
read them and draw.

A package that wants to contribute renderable content does one of
two things:

- **Registers a component table** that a render system reads. The
  engine's core does this: `Engine_Sprite` is a component table,
  and `render_system` reads it. A package that wants a new
  renderable component registers its own table (for example,
  `Alice_Particle`) and either ships its own render system or
  registers a component that the existing render system knows about.
- **Registers nodes that write to a render table.** A
  `Render_DrawSpriteNode` would be an Action node that writes a row
  to `Engine_Sprite`. The render system reads it in the same frame.
  The node does not call the renderer directly.

The render system draws once per `World2D` view, and the UI system
once per `UI` view; see `RenderPasses.md`.

A node does not call the renderer. The renderer is not in
`TickContext`. This is deliberate: rendering happens in the system
phase, and a node that drew immediately would violate the
table-mediated model in `Dataflow.md` §6.

A future `Render_*` package that wants to draw custom primitives
registers a `Render_*` table and a system that reads it. The system
calls the renderer. The package's nodes write the table.

---

## 7. Services

A service is a C++ object that lives across the frame and is
reachable from node executors and from systems. The engine exposes
services through a typed `Services` struct:

```cpp
struct Services {
    physics::Backend* physics = nullptr;
    // future: audio::Backend* audio; render::DebugRenderer* debug;
};
```

A node executor reads `ctx.services.physics`; a system reads
`ctx.services.physics`. If the field is null, the service is not
available, and the caller checks and returns early.

### 7.1 Who may add a service

A service is added by the engine, not by a package. The engine
owns `Services`. A package that wants a service the engine does
not have has two options:

- **Ask the engine to add one.** The engine adds a typed field to
  `Services`. Every consumer updates in the same commit. This is
  the shape `Services::physics` took, and the shape a future
  `Services::audio` would take.
- **Keep the service internal to the package.** The package owns
  the object; its executors reach it through a package-level
  accessor. The package does not expose it on `Services`. Other
  packages cannot use it.

The first is for services the engine itself uses. The second is for
services that are the package's own business. A package that wants
to offer a service to *other* packages is asking for the first;
there is no third mechanism.

A package has two ways to affect what a node executor can reach
without adding a field to `Services`. The first is to keep the
object inside the package: the package's `register_into` stores a
pointer in a package-level accessor, and the package's executors
call that accessor. This is a service in everything but name; it
is not on `Services` because no other package can use it. The
second is to write to a table instead of calling a service: a
package that wants to affect rendering writes a row in a
`Render_*` table and lets a system read it. `Dataflow.md` §6 and
§9 describe why this is the right shape when the operation is
per-frame infrastructure rather than a discrete request. Between
them, "keep it internal" and "write a table" cover every case a
package will hit before it genuinely needs a new `Services` field.

### 7.2 Why a typed struct and not a service locator

`Services` is a struct with typed fields. It is not
`world.get_service("physics")`. A typed struct fails at compile
time if a field is misspelled; a string-keyed locator fails at
runtime. A typed struct is checked by the compiler; a locator is
checked by the caller.

Adding a service is a deliberate change: a new field on `Services`,
and every consumer updates. That is the cost. It is the right cost
because services are few and long-lived.

> [!NOTE]
> The old `Services` struct was the same shape. The change is not
> to `Services` itself but to how a package reaches it: the old
> `NodeExecutor` read `ctx.services.physics` in the same way; the
> new `ActionExecutor` does too. No structural change here.

---

## 8. Cross-package use

A package may use another package's content. There are three
channels under the pivot:

- **The other package's node types.** `bob.game`'s project (via
  the editor) uses `Alice_*` nodes the same way it uses
  `Engine_*` nodes. The `PackageManager`'s registry is the
  source of truth; the editor presents the union.
- **C++ includes.** `bob.game`'s C++ includes
  `alice/math/vector3.h` if `alice.math` ships a `Vector3` struct.
  This is normal C++ usage; the engine does not mediate it.
- **Services.** If `alice.math` exposes a service on `Services`,
  `bob.game`'s executors read it. If it does not, they do not.

> [!NOTE]
> The old §8 first channel was "`.lucid` imports": `bob.game`'s
> `.lucid` file writes `import alice.math` and names the package's
> nodes. There is no `.lucid` file and no import syntax. The
> channel is now "the package's node types are in the registry;
> the editor presents them; the project uses them."

**A package does not reach into another package's tables
directly.** It reads and writes `Alice_Vector3Table` only through
`alice.math`'s nodes. This is a policy, not an enforcement; the
engine cannot stop `bob.game` from calling
`world.get_table("Alice_Vector3Table")`. The policy is what makes
the package boundary meaningful. See `PackageTables.md`.

The reason for the policy: a package's tables are its schema. A
package that reads another package's table has coupled to that
schema, and a schema change in `alice.math` breaks `bob.game`
without warning. Going through nodes means `alice.math` can change
its table layout as long as its nodes' interfaces stay the same.

A package that does need another package's table data registers a
component that references it, or asks `alice.math` to expose a
node. There is no third option.

> [!NOTE]
> The policy is unchanged; its enforcement surface changed. The
> old model could say "the format library enforces this at
> compile time" because `.lucid` imports were compile-time
> checked. The pivot has no compiler; the policy is enforced by
> **code review** and by the package's dependency declaration
> (§8.1). The engine cannot prevent a package from calling
> `world.get_table("Alice_Vector3Table")`; a code reviewer can.

### 8.1 Dependencies

A package declares its dependencies:

```cpp
std::vector<std::string_view> AliceGamePackage::dependencies() const {
    return {"engine.core", "engine.logic", "alice.math"};
}
```

The `PackageManager` validates the dependency graph at load time:

- A package's dependencies must be loaded **before** it, in the
  order the game's `main.cpp` constructs them.
- A missing dependency is `Error_MissingPackage` at load time.
- A cycle is `Error_PackageCycle` at load time.

The engine does not do version negotiation, conflict resolution,
or runtime dependency checking. A package that is loaded is
loaded; a package that is not is a link error, not a runtime
error. The dependency declaration exists to give the
`PackageManager` the information to order the load and report
mistakes early.

> [!NOTE]
> The old §8.1 said "The engine does not track package
> dependencies at runtime. A package that is loaded is loaded; a
> package that is not is not. There is no dependency graph, no
> version negotiation, no conflict resolution." That was true of
> the old model, but it left a hole: a package that *used*
> another package's C++ API had no way to say so, and the
> `PackageManager` had no way to order the loads. Under the
> pivot, packages declare dependencies and the manager validates
> them. Version negotiation and conflict resolution are still
> out of scope.

---

## 9. A worked example

`engine.physics_2d`, described in `PhysicsPlan.md`, uses every
mechanism in this note.

- **Types.** It registers `Engine_BodyRef` as a handle.
- **Tables.** It registers `Engine_Body` (the serializable
  description) and `Engine_BodyRuntime` (the backend's state).
  `Engine_Body` is a component: it has an `entity` column and is
  registered with `add_component`, so it has a codec.
  `Engine_BodyRuntime` has an `entity` column but is registered
  with `add_table`, so it has no codec and is never serialized.
- **Nodes.** It registers `Engine_CreateBody_Entity_Node` (an
  Action: creates a body, writes the `Engine_BodyRef` to a
  scratch cell or a World row), `Engine_BodyPosition_BodyRef_Node`
  (an Action: reads the body's position, writes it to a scratch
  cell), `Engine_BodyVelocity_BodyRef_Node` (similar),
  `Engine_MoveBody_BodyRef_Float_Float_Node`,
  `Engine_ApplyForce_BodyRef_Float_Float_Node`, and
  `Engine_ApplyImpulse_BodyRef_Float_Float_Node` (Actions).
- **Executors.** The action nodes call `ctx.services.physics`.
  The value reads are Actions that write scratch cells; a
  `Branch` reads the cell.
- **Service.** The physics backend is a `physics::Backend` on
  `Services::physics`. The game's `main.cpp` constructs a
  `Box2DBackend`, puts it in `Services`, and loads the package.
- **Dependencies.** `engine.physics_2d` declares
  `{"engine.core"}`.

`bob.game` uses `Engine_BodyRef` and the physics nodes. It does
not read `Engine_BodyRuntime` directly; it uses
`Engine_BodyPosition_BodyRef_Node` and reads the scratch cell.
If the engine changes `Engine_BodyRuntime`'s columns, `bob.game`
is unaffected.

> [!NOTE]
> The old §9 said `Engine_BodyPosition_BodyRef_Node` was a
> Value node. It is an Action now: it reads the body's position
> and writes a scratch cell. `bob.game`'s branch reads the
> cell. The dependency declaration is new.

---

## 10. A second example: `alice.math`

`alice.math` offers a vector type. There are two ways to build it.

**As a handle.** The package registers `Alice_Vector3` as a
handle, `Alice_Vector3Table` as a plain table (no codec), and a
set of Action nodes: `Alice_New_Float_Float_Float_Node(x, y, z,
out)`, `Alice_GetX_Vector3_Node(v, out)`,
`Alice_Add_Vector3_Vector3_Node(a, b, out)`, and so on. Each
node's executor reads or writes `Alice_Vector3Table` and writes
its output to a scratch cell. The package ships no C++ header for
a `Vector3` struct; the type is the handle.

**As a struct.** The package registers `Alice_Vector3` with
`add_struct` and ships `alice/math/vector3.h`. The same nodes
take and return the struct by value, through the scratchpad (a
`Struct` cell type). `bob.game`'s C++ can include the header to
compute on vectors inline; its sub-graphs still compose
`alice.math`'s nodes to keep the policy. The engine's own math
package uses this form: `Engine_Vector2` is a struct.

Neither is wrong. The handle version keeps the ECS model intact:
a value is a row in a table, operations are nodes, other packages
use the handle. The struct version passes the value through the
scratchpad and lets a C++ consumer use a normal type. The choice
is the package author's, and §4.3 states the rule: a compound
type is a handle plus a table, or a struct registered with
`add_struct`. It is never a new primitive in the engine's type
system.

> [!NOTE]
> The old example used Value nodes returning the struct by bytes
> in `ctx.struct_results`. The pivot's version writes the struct
> to a scratchpad `Struct` cell. The handle-vs-struct trade is
> unchanged; the mechanism is.

---

## 11. Sub-graph templates

A package may ship **sub-graph templates** for phases it
participates in. A template is a named sub-graph (a set of nodes
and edges) that the editor offers in a "templates" panel; the
user drops it into a phase's sub-graph slot.

```cpp
b.add_subgraph_template(SubgraphTemplateInfo{
    "DefaultPhysics",
    "Physics",
    "A default physics phase: step the backend, sync transforms.",
    /* nodes and edges */,
});
```

A template is a convenience: it saves the user from wiring the
phase's sub-graph by hand. It is not a node type; it is a
sub-graph the editor instantiates. The engine's `PhaseScheduler`
does not know about templates; it runs the sub-graph the project
registered on the phase, whether the user built it by hand or
instantiated it from a template.

> [!NOTE]
> Sub-graph templates are new in the pivot. They are the
> ergonomic answer to "a package participates in a phase": the
> package ships a default sub-graph for that phase, the user
> drops it in. See `ExecutionModel.md` §4.

---

## 12. Summary in one line

**A package registers node types (Actions), executors (returning
the next PC), types, tables and component codecs, scratchpad cell
types, and its dependencies with the engine at startup; its node
executors read and write its own tables and the sub-graph's
scratchpad and call services; other packages use it through its
node types, C++ includes, and services; a package does not reach
into another package's tables.**

---

## 13. What this note does not cover

This note covers what a package contributes and how packages
compose. It does not cover:

- **Naming.** The prefix convention and its enforcement are in
  `PackageNaming.md`.
- **The node vocabulary's shape.** `NodeTypeInfo`,
  `EnumTypeInfo`, `HandleTypeInfo`, and the node registry are
  the engine's types. See `ExecutionModel.md` §2 and
  `EditorMetadata.md`.
- **How a graph runs.** The walker, the phases, the sub-graphs,
  the scratchpad are in `ExecutionModel.md`. The dataflow
  model — tables as the interface — is in `Dataflow.md`.
- **Table ownership.** The package-owned-table convention and
  the cross-package boundary are in `PackageTables.md`.
- **The project file.** What the editor writes and the engine
  loads is in `ProjectFormat.md`.
- **The editor's view of a package.** `PackageInfo`,
  `NodeEditorInfo` (including the `Operation` metadata), and
  `TableEditorInfo` are in `EditorMetadata.md`.
- **A specific package's design.** `PhysicsPlan.md` is the
  worked case for `engine.physics_2d`.
