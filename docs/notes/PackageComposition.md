# Package composition

What a package contributes, how it declares it, and how one package
uses another's content. This note is the policy layer above
`PackageNaming.md` (which covers names) and `Dataflow.md` (which covers
what nodes and systems do at runtime).

See also:
- `docs/notes/PackageNaming.md` — the prefix convention and the
  registry.
- `docs/notes/Dataflow.md` — nodes, systems, tables, services.
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

A package contributes four kinds of content:

- **Node types** — `.lucid`-visible names with a signature, a kind
  (Value, Action, Trigger), and an executor.
- **Executors** — the C++ function that runs when a node fires.
- **Types** — enums and handles that `.lucid` files can name.
- **Tables and component codecs** — `.lscene`-visible table schemas
  and their JSON loaders and savers.

A package owns all four. It is the unit of registration, of naming,
and of versioning. It is not a runtime object; there is one instance
per package, created by the game's `main.cpp` and handed to the
`PackageManager` before `apply()`.

---

## 2. What a package is not

A package is not:

- **A plugin.** There is no dynamic loading, no ABI contract, no
  version negotiation. A package is linked into the executable. Its
  `Package` subclass is constructed by the game's `main.cpp`.
- **A runtime object.** The `Package` instance exists only during
  registration. After `PackageManager::apply()`, the package's
  contributions live in the registry and in the world's tables.
- **A file format.** A package may ship `.lucid` helper files and
  C++ headers, but those are the package's source, not a packaged
  artifact. There is no `.lucidpkg` file.
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

    b.add_component("Alice_Vector3Table",
        [](engine::ecs::World& world) {
            auto* t = world.create_table("Alice_Vector3Table");
            t->add_column("id", ColumnType::Uint64, /*primary=*/true);
            t->add_column("x",  ColumnType::Float32);
            t->add_column("y",  ColumnType::Float32);
            t->add_column("z",  ColumnType::Float32);
        },
        ComponentCodec{
            .load = load_vector3_table,
            .save = save_vector3_table,
        });

    b.add_node_type(NodeTypeInfo{
        "Alice_Vector3NewNode",
        NodeKind::Value,
        "Math",
        0,
        /*args=*/...,
        /*result=*/TypeId::handle("Alice_Vector3"),
    });
    b.add_node_executor(exec_vector3_new);
    b.add_trigger_mode(TriggerMode::NotATrigger);

    // ... more nodes ...

    b.add_node_editor_info("Alice_Vector3NewNode", NodeEditorInfo{
        "Vector3 New",
        "Create a Vector3 from three floats.",
        "",
        "vector3",
        "node v = Alice_Vector3NewNode(1.0, 2.0, 3.0)",
    });
}
```

Every name starts with the package's prefix. The builder enforces this.
A registration that violates the prefix, or that collides with an
existing name, is recorded as a conflict; `PackageManager::apply`
refuses to run if any conflict exists.

---

## 4. Types

`.lucid` files can name three kinds of type:

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

A package that wants a compound type (a vector, a matrix, a
quaternion, a transform) has two options.

### 4.1 A handle plus a table

The package registers a handle (`Alice_Vector3`) and a table
(`Alice_Vector3Table`). The handle is an opaque id; the value lives
in the table. Operations are nodes.

```
handle:  Alice_Vector3
table:   Alice_Vector3Table  (id: uint64 primary, x: float32,
                              y: float32, z: float32)
nodes:   Alice_Vector3NewNode(x, y, z) -> Alice_Vector3
         Alice_Vector3XNode(v) -> float32
         Alice_Vector3YNode(v) -> float32
         Alice_Vector3ZNode(v) -> float32
         Alice_Vector3AddNode(a, b) -> Alice_Vector3
         Alice_Vector3ScaleNode(v, s) -> Alice_Vector3
         Alice_Vector3LengthNode(v) -> float32
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

### 4.2 A C++ struct with primitive arguments

The package ships a C++ header with the struct. `.lucid` sees the
component fields as primitives. The node's C++ executor uses the
struct internally.

```cpp
// alice/math/vector3.h
namespace alice::math {
    struct Vector3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; };
}
```

`.lucid` file:

```lucid
node v = Alice_Vector3NewNode(1.0, 2.0, 3.0)
```

The node's `NodeTypeInfo` declares three `float32` arguments, not
a `Vector3` argument. The executor takes the three floats, constructs
a `Vector3`, and does the work.

This is the pragmatic choice. It keeps the type system simple and
avoids a handle and a table for a value that fits in three floats.
The cost is that `.lucid` files cannot pass a `Vector3` as one value;
they pass three floats, in order.

### 4.3 The rule

**A compound type is either a handle plus a table, or a C++ struct
whose fields are declared to `.lucid` as primitives.** A package
cannot add a new primitive to the format library's type system.

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
  `ctx.graph.nodes[node_index].args_offset`.
- **Reads and writes tables** in `ctx.world`.
- **Calls services** through `ctx.services`.

An executor does not call another node's executor. It does not read
another node's internal state. It does not choose its own execution
order. See `Dataflow.md` §8 for the full list.

### 5.1 Where the work happens

The work a node performs lives in one of three places:

- **In the executor itself.** A pure computation (`Alice_Vector3AddNode`)
  reads its arguments and writes a result. No table access. No
  service call. The executor is the whole implementation.
- **In a table the executor reads or writes.** `Engine_BodyNode`
  writes a row in `Engine_BodyRuntime`. `Alice_Vector3NewNode`
  writes a row in `Alice_Vector3Table`. The table is the package's;
  the executor touches only its own package's tables (see §8).
- **In a service the executor calls.** `Engine_ApplyForceNode`
  calls `ctx.services.physics->apply_force(...)`. The service is the
  backend; the executor is the glue. `Engine_PrintNode` calls
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
node Alice_ApplyFriction(body, amount) =
    Alice_MultiplyNode(amount, Alice_BodyFrictionNode(body))
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
  `Alice_Vector3AddNode`. The format library's import model binds
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

---

## 9. A worked example

`engine.physics_2d`, described in `PhysicsPlan.md`, uses every
mechanism in this note.

- **Types.** It registers `Engine_BodyRef` as a handle.
- **Tables.** It registers `Engine_Body` (the serializable
  description) and `Engine_BodyRuntime` (the backend's state).
  `Engine_Body` is a component: it has an `entity` column and a
  component codec. `Engine_BodyRuntime` is not a component; it has
  an `entity` column but is not serialized.
- **Nodes.** It registers `Engine_BodyNode` (creates a body),
  `Engine_BodyPositionNode` and `Engine_BodyVelocityNode` (read
  state), `Engine_MoveBodyNode`, `Engine_ApplyForceNode`, and
  `Engine_ApplyImpulseNode` (act on the body). It registers
  `Engine_OnCollision` as a trigger (firing is deferred).
- **Executors.** The action nodes call `ctx.services.physics`.
  The value nodes read `Engine_BodyRuntime`.
- **Service.** The physics backend is a `physics::Backend` on
  `Services::physics`. The game's `main.cpp` constructs a
  `Box2DBackend`, puts it in `Services`, and loads the package.

`bob.game` imports `engine.physics_2d` and uses `Engine_BodyRef`
and the physics nodes. It does not read `Engine_BodyRuntime`
directly; it uses `Engine_BodyPositionNode`. If the engine changes
`Engine_BodyRuntime`'s columns, `bob.game` is unaffected.

---

## 10. Summary in one line

**A package registers node types, executors, types, and tables with
the engine at startup; its node executors read and write its own
tables and call services; other packages use it through `.lucid`
imports and C++ includes; and a package does not reach into another
package's tables.**
