# Package tables

A package owns its tables. Its nodes are the only sanctioned way
to read and write them. This note is the spec for that
convention: what "owns" means, how packages depend on each other,
what the runtime can and cannot enforce, and what a package
author does.

This note is the policy layer. It assumes `Dataflow.md` (the
table-mediated model) and `ExecutionModel.md` (the walker, the
phases, the scratchpad). It is cross-referenced from
`PackageComposition.md` §8 and `PhysicsPlan.md` §4.

See also:
- `docs/notes/Dataflow.md` — nodes, systems, tables, services.
- `docs/notes/ExecutionModel.md` — the walker, phases,
  sub-graphs, scratchpad.
- `docs/notes/PackageComposition.md` — what a package registers,
  including its tables.
- `docs/notes/PackageNaming.md` — the table naming convention.
- `docs/notes/PhysicsPlan.md` — a worked package
  (`engine.physics_2d`) that owns two tables.

---

## 1. What "a package owns a table" means

A package **owns** a table if the package's `register_into` calls
`PackageBuilder::add_table` or `add_component` for it. Ownership
is a naming and registration fact; the table's creator function
runs at apply time, and the table name starts with the package's
prefix (see `PackageNaming.md`).

The engine owns a small set of tables too. These are the **core
tables**: `Engine_Entity`, `Engine_Transform`,
`Engine_WorldTransform`, `Engine_Velocity`, `Engine_Sprite`,
`Engine_Script`, `Engine_Camera`, `Engine_EngineState`. They are
registered by `register_core_components` and the engine's own
setup. The engine's core tables are engine-owned; the convention
below does not apply to them, because they are the shared state
the engine's systems read and write.

The line is: **engine-owned tables are shared; package-owned
tables are the package's.** A package's nodes read and write the
engine's core tables freely (the physics package writes
`Engine_Transform` via `physics_sync_system`, which is a system,
not a node; a node reads `Engine_Transform.x` and writes a scratch
cell). A package's nodes read and write the package's own tables
freely. A package's nodes do not read or write another package's
tables.

> [!NOTE]
> "A node reads and writes the engine's core tables freely" is
> the one exception to the ownership rule, and it exists because
> the core tables *are* the engine. A physics node reading
> `Engine_Transform.x` is not violating a boundary; it is reading
> the shared world. The boundary is between packages, not between
> a package and the engine.

---

## 2. Why the boundary

The boundary exists so a package can change its table layout
without breaking its consumers. If `alice.math` owns
`Alice_Vector3Table` and `bob.game` calls
`world.get_table("Alice_Vector3Table")` directly, then a schema
change in `alice.math` (renaming a column, splitting a table,
adding a row shape) breaks `bob.game` at runtime, with no warning
and no compile-time check.

If `bob.game` uses `alice.math`'s nodes instead, the nodes'
interfaces (`Alice_Add_Vector3_Vector3_Node(a, b, out)`) are the
contract, and `alice.math` can change its table layout as long as
its nodes' interfaces stay the same.

This is the same reason a C library exposes functions instead of
header-inlined globals: the function is a contract; the global
is not. The node is the function; the table is the global.

**The boundary is a policy, not an enforcement.** The engine's
`World` exposes `get_table(name)`. Any code can read any table.
The engine cannot stop `bob.game` from calling
`world.get_table("Alice_Vector3Table")`. The convention is
enforced by code review and by the dependency declaration (§3).
The engine's tests do not check the boundary. This note gives the
reviewer something to point at.

> [!NOTE]
> If a future engine iteration wants to enforce the boundary at
> runtime, the mechanism exists: the `World` could expose a
> per-package view (`world.package_view("alice.math")`) that
> returns only the package's own tables, and the engine could
> pass that view in `TickContext` instead of the full `World`.
> This is not the current design; it is a future option. It
> would change `TickContext`'s `world` field's type and every
> executor's `ctx.world.get_table(...)` call. See §6.

---

## 3. Dependencies

A package declares its dependencies. The declaration has two
purposes: load ordering, and a written record of the cross-package
use.

```cpp
std::vector<std::string_view> AliceGamePackage::dependencies() const {
    return {"engine.core", "engine.logic", "alice.math"};
}
```

The `PackageManager` validates the dependency graph at load time:

- **Load ordering.** A package's dependencies must be loaded
  before it. If `alice.game` declares `alice.math`, then
  `alice.math` must be loaded first. The game's `main.cpp`
  controls the order; the `PackageManager` reports
  `Error_DependencyLoadOrder` if a package is loaded before one
  of its dependencies.
- **Missing dependency.** A dependency that is not loaded at all
  is `Error_MissingDependency`, reported when the package is
  loaded. The package is rejected.
- **Cycle.** A cycle in the dependency graph is
  `Error_PackageCycle`, reported at load time. The load is
  aborted.

The engine does not do version negotiation, conflict resolution,
or runtime dependency checks. A package that is loaded is
loaded; a package that is not is a link error, not a runtime
error. The declaration exists to give the `PackageManager` the
information to order the load and report mistakes early.

**A dependency declaration is a claim, not an enforcement.** A
package that declares `alice.math` and then reads
`Alice_Vector3Table` directly has not *violated* the engine; the
engine has no way to know. The declaration is a signal that
"this package may use `alice.math`'s nodes"; the reviewer uses
the declaration as the starting point for "does this package
read another package's tables?" and the answer should be no.

---

## 4. The three sanctioned ways to use another package

A package may use another package's content through three
channels:

1. **Its node types.** `bob.game`'s sub-graphs use
   `Alice_*` nodes the same way they use `Engine_*` nodes. The
   `PackageManager`'s registry is the source of truth; the
   editor presents the union of all loaded packages' nodes.
2. **Its C++ headers.** `bob.game`'s C++ includes
   `alice/math/vector3.h` if `alice.math` ships a `Vector3`
   struct. This is normal C++ usage; the engine does not
   mediate it. **Including a header is not the same as reading a
   table.** The struct is a value type; the table is the
   package's storage. A package that includes another's header
   and calls the other's nodes is fine. A package that includes
   another's header and reads the other's tables is not.
3. **Its services.** If `alice.math` exposes a service on
   `Services`, `bob.game`'s executors read it. If it does not,
   they do not. Services are the engine's escape hatch for
   cross-package access that is genuinely a service (a
   long-lived backend, a shared cache); they are not for
   "I want to read your table."

**None of the three is "read the other package's tables."** That
is the boundary.

---

## 5. The cross-package cases and what to do

The cases that come up in practice, with the recommended
resolution for each.

### 5.1 "I need a value from `alice.math`'s table"

Use `alice.math`'s read node. If `alice.math` does not have a
read node for the value you need, ask `alice.math` to expose one.
That is the boundary: the package's vocabulary is its public
surface.

If `alice.math` ships no read node and cannot add one, the
correct resolution is a *design* change: either `alice.math`
adds the node (and `bob.game` declares the dependency and uses
it), or the value moves to a table both packages can share. The
second option is the "shared table" question in §5.2.

### 5.2 "Both packages need a table"

A table that two packages need is one of three things:

- **A core table.** The engine owns it; both packages read and
  write it. This is the right answer when the table is genuinely
  shared state (the entity's transform, the entity's velocity,
  the entity's parent). The engine's core tables are exactly
  this: the shared world.
- **A package table with a public read/write node.** One package
  owns the table; the other package uses the owner's nodes.
  `bob.game` reads the `Alice_Vector3` values it needs through
  `alice.math`'s nodes. `alice.math` owns the storage; `bob.game`
  uses the vocabulary.
- **A component table with a codec.** The table is a component
  (it has an `entity` column), the package that owns it registers
  it with `add_component`, and the scene format serializes it.
  A package that needs the component reads it through the
  owner's nodes or (if it's a core component) directly. A package
  that wants to add a component to an entity uses the owner's
  node (`Engine_AddComponent_*_Node` or similar); it does not
  add a row to the owner's table directly.

The recommendation: **prefer core tables for shared state, a
public read/write node for package-internal state, a component
table for state that belongs to an entity and is serialized.**
Do not add a shared table that neither package owns; that is a
table with no contract, and it will drift.

### 5.3 "I want to write another package's table directly"

Don't. If the write is genuinely something the owner should
support, ask the owner to expose a node. If the owner refuses,
the write is not sanctioned, and the design is wrong: the state
does not belong to the writer.

The engine cannot enforce this. The reviewer can.

### 5.4 "I want to read the engine's core tables"

The engine's core tables are shared. Read `Engine_Entity`,
`Engine_Transform`, `Engine_WorldTransform`, `Engine_Velocity`,
`Engine_Sprite`, `Engine_Script`, `Engine_Camera`,
`Engine_EngineState` freely from a node. The physics package does
this (its executors read `Engine_Transform`). The render package
does this (its executors read `Engine_Sprite`). This is the
shared world.

A package **writes** the core tables through the engine's
sanctioned paths: `physics_sync_system` writes
`Engine_Transform`; `hierarchy_system` writes
`Engine_WorldTransform`; the scene loader writes all of them at
load time. A node that writes `Engine_Transform` directly (via a
package's node that does so) is moving against the physics
package's authority ("physics owns the position"; see
`PhysicsPlan.md` §7) and will be overwritten on the next sync.
The node is not *forbidden*; it is *misguided*, and the reviewer
should point at `PhysicsPlan.md` §7.

### 5.5 "I want to define a new core table"

Don't. Core tables are the engine's. If you need a shared table
that is genuinely a shared world concept, ask the engine to add
it. If the table is the package's, it stays the package's.

A package that wants a table to be shared with another package
uses the §5.2 patterns: a core table (ask the engine), a
package-owned table with a public read/write node (ask the
owner), or a component table (register it as the package's
component).

---

## 6. What the runtime can and cannot enforce

The runtime **cannot** enforce the boundary. `World::get_table`
takes a name; `Table` is a public type; a node's executor can
call `ctx.world.get_table("Alice_Vector3Table")`. The engine's
type system does not track which package a node belongs to; a
node's package is a registration-time fact, not a runtime one.

The runtime **can** support boundary-respecting designs:

- `TickContext::world` is a `World&`. Every executor has the full
  `World`. This is the current design.
- `PackageBuilder::add_action_node` records the node type's
  package prefix. The registry could expose it
  (`NodeTypeInfo::package`). The editor could use it to warn
  when a node's executor touches another package's table (it
  can't see the executor's code; it can only see the node type's
  declared *arguments*, which are paths into the World). This is
  a future tooling direction, not a current enforcement.
- A future engine could give `TickContext` a `package_view`
  instead of the full `World`. The `package_view` would expose
  only the package's own tables and the engine's core tables.
  This is the §2 note's "if a future engine iteration wants to
  enforce." It is not the current design.

**The current enforcement is code review.** This note exists so
the reviewer has something to point at. `PackageComposition.md`
§8 says the same thing; this note is the detail.

---

## 7. What a package author does

A package author follows these steps for the tables their package
registers:

1. **Name each table `<Prefix>_<BaseName>`.** See
   `PackageNaming.md` §2.3.
2. **Register each table with `add_table` or `add_component`.**
   `add_component` if the table is a component (has an `entity`
   column, has a codec for the scene format). `add_table` if the
   table is the package's internal storage (no codec, not
   serialized).
3. **Expose the table's reads and writes through nodes.** A
   consumer should not call `world.get_table("Alice_...")`;
   they should use `Alice_*_Node`. Every column the package
   wants external packages to read needs a read node; every
   column external packages may write needs a write node.
4. **Declare the package's dependencies.** `dependencies()`
   returns the packages the package's nodes use. See §3.
5. **Do not read or write another package's tables directly.**
   Use the owner's nodes. See §5.4 for the engine's core tables.
6. **Reviewers check the boundary.** The engine's tests do not.
   The reviewer reads the dependency declaration and the node
   executors' `ctx.world.get_table` calls. A `get_table` call to
   a table whose prefix is not the package's own (and is not a
   core table) is a review finding.

---

## 8. A worked example: `engine.physics_2d`

`engine.physics_2d` owns two tables:

- **`Engine_Body`** — a component table. Has an `entity` column,
  registered with `add_component`, has a codec. The scene loader
  and saver serialize it. See `PhysicsPlan.md` §4 and §9.
- **`Engine_BodyRuntime`** — a plain table. Has an `entity`
  column, registered with `add_table`, no codec. The scene
  loader skips it; the saver skips it. It is the backend's
  state, populated by `physics_step_system` and read by the
  package's nodes. See `PhysicsPlan.md` §4 and §8.

`engine.physics_2d` declares `{"engine.core"}` as a dependency.
`engine.core` provides the engine's core tables (`Engine_Entity`,
`Engine_Transform`, etc.), which `engine.physics_2d`'s
`physics_sync_system` writes and its nodes read.

`bob.game` wants to know the player's Y position. It composes
`Engine_BodyPosition_BodyRef_Node(body, "scratch.x", "scratch.y")`,
which is an `engine.physics_2d` node. The node reads
`Engine_BodyRuntime` (the package's own table) and writes the
result to `bob.game`'s scratch cell. `bob.game` does **not** call
`world.get_table("Engine_BodyRuntime")`; it uses the physics
package's node. The boundary holds.

If `bob.game` needed a value the physics package does not expose,
it would ask `engine.physics_2d` to add a read node. It would
not read the table directly. This is the §5.1 case.

---

## 9. Summary in one line

**A package owns its tables; its nodes are the only sanctioned
way to read or write them; the engine's core tables are shared;
a package declares its dependencies so the `PackageManager` can
order the load; and the boundary is enforced by code review, not
by the runtime.**

---

## 10. What this note does not cover

- **The naming convention.** See `PackageNaming.md`.
- **How a package registers a table.** See
  `PackageComposition.md` §3.
- **The `ComponentRegistry`.** See `SceneFormat.md` §"Component
  recognition."
- **The engine's own table registration.** The core tables are
  registered by `register_core_components` and the engine's
  setup; see `engine/src/scene/component_registry.cpp` and the
  engine's `setup_core_tables`.
- **Runtime enforcement.** The §2 and §6 notes describe the
  current (review-based) and future (package-view) options.
- **The dependency graph's serialization.** Dependencies are
  declared in `package.json` and read at load time; the doc
  describes the rules, not the file format. See
  `PackageNaming.md` §3.1 for `package.json`'s shape.
