# Package composition

What a package contributes, how it declares it, and how one package
uses another's content. This note is the policy layer above
`PackageNaming.md` (which covers names) and `Dataflow.md` (which covers
what nodes and systems do at runtime).

See also:
- `docs/notes/PackageNaming.md` — the prefix convention and the
  registry.
- `docs/notes/Dataflow.md` — nodes, systems, tables, services.
- `docs/notes/EditorMetadata.md` — the editor metadata a package
  registers, including node operations.
- `docs/notes/TriggerModel.md` — the three node kinds.
- `docs/notes/PhysicsPlan.md` — a worked package (`engine.physics_2d`)
  that uses every mechanism this note describes.
- `docs/Decision.md` — the decisions behind the package system.

---

## 1. What a package is

A package is a C++ object that registers content with the engine at
startup. It is a `Package` subclass. Its `register_into(PackageBuilder&)`
is called exactly once, in load order, before any `.lucid` file is
compiled and before any `.lscene` file is loaded.

A package contributes five kinds of content:

- **Node types** — `.lucid`-visible names with a signature, a kind
  (Value, Action, Trigger), and either a C++ executor or a `.lucid`
  body. See §5.
- **Executors** — the C++ function that runs when a C++-registered
  node fires.
- **Types** — enums, handles, and structs that `.lucid` files can name.
- **Tables and component codecs** — `.lscene`-visible table schemas
  and their JSON loaders and savers.
- **`.lucid` helper nodes** — `.lucid` files the package ships, whose
  exported nodes enter the registry when the file is compiled. See
  §5.3.

A `.lucid` helper is a node type. It has a name, arguments, and a
result type, and it appears in the registry the same way a
C++-registered node does. The difference is how it is registered: a
`.lucid` helper is compiled by the format library, and a C++ node type
is registered with `PackageBuilder::add_node_type`. The registry does
not distinguish them. Both are exported under the package's prefix.

A package owns all five. It is the unit of registration, of naming,
and of versioning. It is not a runtime object; there is one instance
per package, created by the game's `main.cpp` and handed to the
`PackageManager` before `apply()` or `apply_registration_only()`.

A package has two halves, and they live in two places. The `Package` subclass — the object the `PackageManager` loads, the thing that calls `PackageBuilder::add_*` — lives in `engine/include/engine/registry/packages/`. The package's public C++ types — the structs and classes other code includes — live in the layer the package serves: `engine/math/` for `engine.math`, `engine/render/` for `engine.render`. A package can have one, the other, or both.

`engine.core` and `engine.logic` have the Package subclass only; they ship no public C++ types beyond the registry. `engine.math` ships `Vector2` under `engine/math/`. `engine.render` ships `Color` under `engine/render/`. The `Package` subclass for each lives under registry/packages/.

A user's package follows the same convention, in the user's tree. The engine cannot name the user's paths; it names the interface (`Package`, `PackageBuilder`). A user who wants a package that ships a public type puts the type wherever the user's code expects it and subclasses `Package` in the user's own source tree.

---

## 2. What a package is not

A package is not:

- **A plugin.** There is no dynamic loading, no ABI contract, no
  version negotiation. A package is linked into the executable. Its
  `Package` subclass is constructed by the game's `main.cpp`.
- **A runtime object.** The `Package` instance exists only during
  registration. After `PackageManager::apply()` or
  `apply_registration_only()`, the package's contributions live in
  the registry and the component registry. The `Package` object is
  not reachable.
- **A file format.** A package may ship `.lucid` helper files and
  C++ headers, but those are the package's source, not a packaged
  artifact. There is no `.lucidpkg` file. A package's `.lucid` files
  are compiled by the format library, and their exported nodes enter
  the registry the same way C++-registered nodes do. See §5.3.
- **A service.** A package may offer a service (see §7), but a
  package is not itself a service. The two are separate concepts.

---

## 3. Registering a package

A package's `register_into` calls the `PackageBuilder`'s `add_*`
methods. Each call declares one piece of content. The builder checks
the name against the package's prefix and against every name already
registered.

```cpp
void AliceMathPackage::register_into(PackageBuilder& b)
{
    b.add_handle(HandleTypeInfo{"Alice_Vector3"});

    // A table that is not an entity component: no entity column, no codec.
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

    b.add_node_type(NodeTypeInfo{
        "Alice_Add_Vector3_Vector3_Node",
        NodeKind::Value,
        "Math",
        0,
        /*args=*/...,
        /*result=*/TypeId::handle("Alice_Vector3"),
    });
    b.add_node_executor(exec_vector3_add);
    b.add_trigger_mode(TriggerMode::NotATrigger);

    // ... more nodes ...

    b.add_node_editor_info("Alice_Add_Vector3_Vector3_Node", NodeEditorInfo{
        "Vector3 Add",
        "Add two Vector3 values.",
        "",
        "add",
        "node s = Alice_Add_Vector3_Vector3_Node(a, b)",
        Operation::Add,
    });
}
```

Every name starts with the package's prefix. The builder enforces this.
A registration that violates the prefix, or that collides with an
existing name, is recorded as a conflict; `PackageManager::apply`
refuses to run if any conflict exists.

`add_component` registers a table and its codec in one call. The table
creator runs at apply time; the codec is stored in the
`ComponentRegistry` for the scene loader and saver. A package that wants
a component to be serializable registers it this way. `add_table`
registers a table without a codec: the scene loader does not know how to
move the table's rows to or from JSON, and the saver skips the table.
This is the right call for a package's internal table (a runtime cache,
an index) and it is what the engine itself does for `Engine_Camera`
(which is not a component and has its own loader in `scene.cpp`).

A package that registers a struct type uses `add_struct`. The struct's
fields are declared in the registry; the C++ struct is the package's own
type in a header the package ships. The package author keeps the two in
sync. See §4.2.

A component table's name must start with the package's prefix, like
every other registered name. A codec's table name must be unique across
all packages: two packages that register a codec for the same table name
is a conflict caught at registration time.

### 3.1 Editor metadata

A package registers editor metadata for each node type with
`add_node_editor_info`. The metadata includes an `Operation` value
that names the operation (`Operation::Add`, `Operation::LessThan`)
and, through the engine's operation table
(`engine/registry/operation.h`), a visual group. A package that uses an
operation the enum does not have sets `Operation::Custom` and supplies
`custom_operation` and `operation_group_override`. See
`EditorMetadata.md`.

---

## 4. Types

`.lucid` files can name four kinds of type:

- **Primitives.** `bool`, `char`, `string`, and the sized integer
  and float types (`int8`..`int64`, `uint8`..`uint64`, `float32`,
  `float64`). These are declared by the format library. A package
  cannot add a primitive.
- **Enums.** A named set of members, registered by a package. An
  enum member is a value. `.lucid` files name the enum type and its
  members (`Engine_Key`, `Engine_Key.W`).
- **Handles.** An opaque reference, registered by a package. Every
  handle has `nil` as a valid value. Two handles to the same
  resource compare equal; two handles to different resources compare
  unequal. `.lucid` files name the handle type (`Engine_BodyRef`,
  `Alice_Vector3`).
- **Structs.** A named record of fields, registered by a package
  with `add_struct`. A struct value is passed between nodes as a
  unit (`Engine_Vector2`).

A package that wants a compound type (a vector, a matrix, a
quaternion, a transform) has two options: a handle plus a table, or
a struct.

### 4.1 A handle plus a table

The package registers a handle (`Alice_Vector3`) and a table
(`Alice_Vector3Table`). The handle is an opaque id; the value lives
in the table. Operations are nodes.

```
handle:  Alice_Vector3
table:   Alice_Vector3Table  (id: uint64 primary, x: float32,
                              y: float32, z: float32)
nodes:   Alice_New_Float_Float_Float_Node(x, y, z) -> Alice_Vector3
         Alice_GetX_Vector3_Node(v) -> float32
         Alice_GetY_Vector3_Node(v) -> float32
         Alice_GetZ_Vector3_Node(v) -> float32
         Alice_Add_Vector3_Vector3_Node(a, b) -> Alice_Vector3
         Alice_Scale_Vector3_Float_Node(v, s) -> Alice_Vector3
         Alice_Length_Vector3_Node(v) -> float32
```

This is the ECS-idiomatic choice. It matches `Engine_BodyRef` in
`PhysicsPlan.md`. It keeps the format library's type system simple:
the only new thing `.lucid` sees is a handle.

A `bob.game` package that wants to use `Alice_Vector3`:

- Imports `alice.math` in its `.lucid` file.
- Names the handle in node argument types.
- Composes `alice.math`'s nodes to compute with vectors.

`bob.game` cannot compute on a `Vector3` without going through
`alice.math`'s nodes. That is the package boundary.

The `Alice_Vector3Table` is not an entity component: its rows are not
attached to entities, and the table has no `entity` column. It is
registered with `add_table`, not `add_component`, and it has no codec.
Vectors created by `Alice_New_Float_Float_Float_Node` live for the session; a scene
that wants to persist a vector carries the entity that owns it and the
component that stores the handle, not the vector table's rows.

If a package wants a vector to be part of an entity — a velocity, a
spawn offset — it registers a *component* table with `add_component`,
whose columns include the vector's fields as primitives (`float32` `x`,
`float32` `y`, `float32` `z`) and whose codec reads and writes them. The
distinction is: the handle pattern is for shared values referenced from
many places; the component pattern is for values attached to an entity
and serialized with it.

### 4.2 A struct

The package registers a struct with `add_struct`. The struct's fields
are declared in the registry, and `.lucid` sees a value of that struct
type. The package ships the C++ struct in a header; the two are kept in
sync by the package author.

```cpp
// alice/math/vector3.h
namespace alice::math {
    struct Vector3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; };
}
```

Nodes take and return the struct as one value:

```
struct:  Alice_Vector3  (x: float32, y: float32, z: float32)
nodes:   Alice_Add_Vector3_Vector3_Node(a, b) -> Alice_Vector3
         Alice_Length_Vector3_Node(v) -> float32
```

A struct value is not a `CellValue`. It lives as a run of bytes in
`ctx.struct_results[node_index]`. An executor that produces a struct
writes the bytes there; an executor that consumes a struct reads the
producer's buffer through `engine::graph::arg_node_index(ctx.graph, node, i)`.
The graph runtime sizes each buffer from the registry's result type. See
`Dataflow.md` §4.

The engine's own math package uses this form: `engine.math` registers
`Engine_Vector2` as a struct, and nodes such as
`Engine_Add_Vector2_Vector2_Node` pass it by value.

A struct keeps the type system small and avoids a table for a value that
fits in a few fields. It is copied, not referenced: two nodes holding the
same struct hold two independent values.

### 4.3 The rule

**A compound type is either a handle plus a table, or a struct
registered with `add_struct`.** A package cannot add a new primitive to
the format library's type system.

The choice is the package author's. Handles are the right choice when
the value is large, shared, or referenced from many places (a
physics body, a texture, a mesh). Structs are the right choice when
the value is small and copied (a vector, a color, a quaternion).

---

## 5. Nodes and executors

A node type is a `.lucid`-visible name with:

- A kind (`Value`, `Action`, or `Trigger`).
- A list of argument names and types.
- A result type (Value nodes only).
- A category (for the editor's palette).

The executor is a C++ function pointer of type
`engine::graph::NodeExecutor`:

```cpp
void (*)(engine::graph::TickContext& ctx, std::uint32_t node_index)
```

A node type and its executor are registered together, in the same
`register_into`. The builder keeps them parallel: the Nth executor
is the executor for the Nth node type. `PackageManager::apply`
verifies the counts match.

An executor does three things:

- **Reads its arguments** from `ctx.arg_values`, indexed by
  `ctx.graph.nodes[node_index].args_offset`. A struct-typed argument is
  read from the producer's buffer through `arg_node_index`.
- **Writes its result.** A primitive result goes to
  `ctx.results[node_index]`; a struct result goes to
  `ctx.struct_results[node_index]`.
- **Reads and writes tables** in `ctx.world`.
- **Calls services** through `ctx.services`.

An executor does not call another node's executor. It does not read
another node's internal state. It does not choose its own execution
order. See `Dataflow.md` §8 for the full list.

### 5.1 Where the work happens

The work a node performs lives in one of three places:

- **In the executor itself.** A pure computation (`Alice_Add_Vector3_Vector3_Node`)
  reads its arguments and writes a result. No table access. No
  service call. The executor is the whole implementation.
- **In a table the executor reads or writes.** `Engine_CreateBody_Entity_Node`
  writes a row in `Engine_BodyRuntime`. `Alice_New_Float_Float_Float_Node`
  writes a row in `Alice_Vector3Table`. The table is the package's;
  the executor touches only its own package's tables (see §8).
- **In a service the executor calls.** `Engine_ApplyForce_BodyRef_Float_Float_Node`
  calls `ctx.services.physics->apply_force(...)`. The service is the
  backend; the executor is the glue. `Engine_Print_Float_Node` calls
  `engine::log_info`. `Render_*` nodes would write to a table that
  a render system reads (see §6).

The pattern is: **the executor is the boundary between the graph
and the thing the graph is asking for.** The thing itself is a
table, a service, or an inline computation.

### 5.2 The three node kinds

A node's kind determines when its executor runs.

- **Value nodes** produce a value. No side effects. Evaluated on
  demand, in `value_order`. The executor writes its result to
  `ctx.results[node_index]`.
- **Action nodes** perform a side effect. They run when fired.
  They do not produce a value. Their executor reads and writes
  tables and calls services.
- **Trigger nodes** fire. They do not produce a value and do not
  perform a side effect. A source trigger fires unconditionally; a
  gating trigger fires when its parent fires and its condition is
  true.

See `TriggerModel.md` for the firing model, and `Dataflow.md` §4 for
the executor contract.

### 5.3 `.lucid` helper nodes

A package may ship a `.lucid` file that declares helper nodes composed
from other nodes. These are the package's `.lucid`-defined exports.

```lucid
@export
node Alice_ApplyFriction_Body_Float_Node(body, amount) =
    Alice_Multiply_Float_Float_Node(amount, Alice_BodyFriction_Body_Node(body))
```

The helper is compiled by the format library, not registered from
C++. It is visible to any `.lucid` file that imports `alice.math`.
The prefix convention applies (`PackageNaming.md` §6).

A `.lucid` helper has no executor in the C++ sense. It expands into
the nodes it composes. A package that ships only `.lucid` helpers
and no C++ node types still has a prefix and still enforces the
convention on the helpers' names.

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

The render system draws once per `World2D` view, and the UI system once
per `UI` view; see `RenderPasses.md`.

A node does not call the renderer. The renderer is not in
`TickContext`. This is deliberate: rendering happens in the system
phase, and a node that drew immediately would violate the
table-mediated model in `Dataflow.md` §6.

A future `Render_*` package that wants to draw custom primitives
registers a `Render_*` table and a system that reads it. The system
calls the renderer. The package's nodes write the table.

---

## 7. Services

A service is a C++ object that lives across the frame and is reachable
from node executors and from systems. The engine exposes services
through a typed `Services` struct:

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

A service is added by the engine, not by a package. The engine owns
`Services`. A package that wants a service the engine does not
have has two options:

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

A package has two ways to affect what a node executor can reach without
adding a field to `Services`. The first is to keep the object inside the
package: the package's `register_into` stores a pointer in a
package-level accessor, and the package's executors call that accessor.
This is a service in everything but name; it is not on `Services`
because no other package can use it. The second is to write to a table
instead of calling a service: a package that wants to affect rendering
writes a row in a `Render_*` table and lets a system read it.
`Dataflow.md` §6 and §9 describe why this is the right shape when the
operation is per-frame infrastructure rather than a discrete request.
Between them, "keep it internal" and "write a table" cover every case a
package will hit before it genuinely needs a new `Services` field.

### 7.2 Why a typed struct and not a service locator

`Services` is a struct with typed fields. It is not
`world.get_service("physics")`. A typed struct fails at compile time
if a field is misspelled; a string-keyed locator fails at runtime.
A typed struct is checked by the compiler; a locator is checked by
the caller.

Adding a service is a deliberate change: a new field on `Services`,
and every consumer updates. That is the cost. It is the right cost
because services are few and long-lived.

---

## 8. Cross-package use

A package may use another package's content. There are three channels:

- **`.lucid` imports.** `bob.game`'s `.lucid` file writes
  `import alice.math` and names `Alice_Vector3` and
  `Alice_Add_Vector3_Vector3_Node`. The format library's import model binds
  the names. The package's node types and `.lucid` helpers become
  available.
- **C++ includes.** `bob.game`'s C++ includes
  `alice/math/vector3.h` if `alice.math` ships a `Vector3` struct.
  This is normal C++ usage; the engine does not mediate it.
- **Services.** If `alice.math` exposes a service on `Services`,
  `bob.game`'s executors read it. If it does not, they do not.

A package does not reach into another package's tables directly. It
reads and writes `Alice_Vector3Table` only through `alice.math`'s
nodes. This is a policy, not an enforcement; the engine cannot stop
`bob.game` from calling `world.get_table("Alice_Vector3Table")`. The
policy is what makes the package boundary meaningful.

The reason for the policy: a package's tables are its schema. A
package that reads another package's table has coupled to that
schema, and a schema change in `alice.math` breaks `bob.game`
without warning. Going through nodes means `alice.math` can change
its table layout as long as its nodes' interfaces stay the same.

A package that does need another package's table data registers a
component that references it, or asks `alice.math` to expose a node.
There is no third option.

### 8.1 Dependencies

A package that imports another package's `.lucid` content depends on
it. The format library enforces this at compile time: a `.lucid` file
that names a node type from a package it did not import fails to
compile.

A package's C++ code depends on another package's C++ header when it
includes it. The build system enforces this: a target that does not
link the package's library fails to link.

The engine does not track package dependencies at runtime. A package
that is loaded is loaded; a package that is not is not. There is no
dependency graph, no version negotiation, no conflict resolution.
The game's `main.cpp` loads packages in the order it wants, and a
missing package is a link error, not a runtime error.

The boundary in §8 is a policy, not an enforcement. What it buys is that
`alice.math` can change `Alice_Vector3Table`'s columns as long as its
node interfaces stay the same. The engine's tests do not check the
boundary. Code review does. This note gives the reviewer something to
point at.

---

## 9. A worked example

`engine.physics_2d`, described in `PhysicsPlan.md`, uses every
mechanism in this note.

- **Types.** It registers `Engine_BodyRef` as a handle.
- **Tables.** It registers `Engine_Body` (the serializable
  description) and `Engine_BodyRuntime` (the backend's state).
  `Engine_Body` is a component: it has an `entity` column and is
  registered with `add_component`, so it has a codec.
  `Engine_BodyRuntime` has an `entity` column but is registered with
  `add_table`, so it has no codec and is never serialized.
- **Nodes.** It registers `Engine_CreateBody_Entity_Node` (creates a
  body), `Engine_BodyPosition_BodyRef_Node` and
  `Engine_BodyVelocity_BodyRef_Node` (read state),
  `Engine_MoveBody_BodyRef_Float_Float_Node`,
  `Engine_ApplyForce_BodyRef_Float_Float_Node`, and
  `Engine_ApplyImpulse_BodyRef_Float_Float_Node` (act on the body). It
  registers `Engine_OnCollision_BodyRef_String_Node` as a trigger
  (firing is deferred).
- **Executors.** The action nodes call `ctx.services.physics`.
  The value nodes read `Engine_BodyRuntime`.
- **Service.** The physics backend is a `physics::Backend` on
  `Services::physics`. The game's `main.cpp` constructs a
  `Box2DBackend`, puts it in `Services`, and loads the package.

`bob.game` imports `engine.physics_2d` and uses `Engine_BodyRef`
and the physics nodes. It does not read `Engine_BodyRuntime`
directly; it uses `Engine_BodyPosition_BodyRef_Node`. If the engine
changes `Engine_BodyRuntime`'s columns, `bob.game` is unaffected.

---

## 10. A second example: `alice.math`

`alice.math` offers a vector type. There are two ways to build it.

**As a handle.** The package registers `Alice_Vector3` as a handle,
`Alice_Vector3Table` as a plain table (no codec), and a set of value
nodes: `Alice_New_Float_Float_Float_Node(x, y, z)`,
`Alice_GetX_Vector3_Node(v)`, `Alice_Add_Vector3_Vector3_Node(a, b)`,
and so on. Each node's executor reads or writes `Alice_Vector3Table`.
The package ships no C++ header for a `Vector3` struct; the type is the
handle.

**As a struct.** The package registers `Alice_Vector3` with `add_struct`
and ships `alice/math/vector3.h`. The same nodes take and return the
struct by value, through `ctx.struct_results` (§4.2). `bob.game`'s C++
can include the header to compute on vectors inline; its `.lucid` files
still compose `alice.math`'s nodes to keep the policy. The engine's own
math package uses this form: `Engine_Vector2` is a struct.

Neither is wrong. The handle version keeps the ECS model intact: a value
is a row in a table, operations are nodes, other packages use the
handle. The struct version passes the value directly between nodes and
lets a C++ consumer use a normal type. The choice is the package
author's, and §4.3 states the rule: a compound type is a handle plus a
table, or a struct registered with `add_struct`. It is never a new
primitive in the format library.

---

## 11. Summary in one line

**A package registers node types, executors, types, and tables with
the engine at startup; its node executors read and write its own
tables and call services; other packages use it through `.lucid`
imports and C++ includes; and a package does not reach into another
package's tables.**

---

## 12. What this note does not cover

This note covers what a package contributes and how packages compose. It
does not cover:

- **Naming.** The prefix convention and its enforcement are in
  `PackageNaming.md`.
- **The node vocabulary's shape.** `NodeTypeInfo`, `EnumTypeInfo`,
  `HandleTypeInfo`, and the `lucid::sema::Registry` are the format
  library's types. See `LucidGraphFormat.md` and the format library's
  `Architecture.md`.
- **How a graph runs.** The trigger model, the firing rules, and the
  value/action/trigger kinds are in `TriggerModel.md`. The dataflow
  model — tables as the interface — is in `Dataflow.md`.
- **The editor's view of a package.** `PackageInfo`, `NodeEditorInfo`
  (including the `Operation` metadata), and `TableEditorInfo` are in
  `EditorMetadata.md`.
- **A specific package's design.** `PhysicsPlan.md` is the worked case
  for `engine.physics_2d`.