# Package naming and table naming convention

This note defines how the engine names its content: node types,
enum types, handle types, and table names. It explains the
prefix convention, why it exists, how the engine enforces it, and
what a package author and a script author need to know.

See also:
- `docs/notes/TriggerModel.md` — how a graph expresses control flow.
- `docs/notes/RegistryContractNote.md` (in the format library's
  docs) — the boundary between the engine and the format library.
- `docs/Decision.md` — the design decisions that led to this
  convention.

---

## 1. The problem

The format library's `NodeTypeInfo` carries **one name**. Two
packages that both declare a node type called `GravityNode` produce
two entries in the merged registry with the same `name`. Sema
cannot tell them apart when a `.lucid` file writes
`GravityNode(...)`.

The same problem applies to enum types, handle types, and table
names. The engine's registry is a flat namespace; two declarations
with the same name collide.

The format library's `LucidGraphFormat.md` §4.2 documents this as
an open gap. The library's import model supports a one-level `::`
qualifier (`module::Name`), but the engine's package names are
dotted paths (`alice.physics`), and the qualifier is the last
segment. Two packages whose last segments collide (`alice.physics`
and `bob.physics`) cannot both be loaded.

The engine has three options:

1. Refuse to load conflicting packages.
2. Change the format library to support package-scoped names.
3. Enforce a naming convention at the package level.

The engine chooses **option 3**: a prefix convention. Every node
type, enum type, and handle type has a name of the form
`<Prefix>_<BaseName>`. Table names are also prefixed. The prefix is
the provider's short identifier. The engine enforces the prefix at
package load time.

---

## 2. The naming convention

### 2.1 Node types, enums, handles

A node type, enum type, or handle type registered by a package has
a name of the form:

```
<Prefix>_<BaseName>
```

Where:

- `<Prefix>` is the package's declared prefix. A short identifier,
  typically the package author's name or the package's short name.
- `<BaseName>` is the descriptive part. What the user sees in the
  editor palette; the prefix is de-emphasized.

Examples:

```
Engine_PrintNode          a node type from the engine's core package
Engine_EveryFrame         a node type from the engine's core package
Engine_IfNode             a node type from the engine's logic package
Engine_Key                an enum type from the engine's core package
Alice_GravityNode         a node type from Alice's physics package
Alice_BodyNode            a node type from Alice's physics package
Physics2D_RigidBody       a node type from a package using the prefix Physics2D
```

### 2.2 Tables

Table names follow the same convention:

```
<Prefix>_<BaseName>
```

Examples:

```
Engine_Entity             the entity table
Engine_Transform          the local transform table
Engine_WorldTransform     the world transform table
Engine_Velocity           the velocity table
Engine_Sprite             the 2D sprite table
Engine_Script             the script table (references a .lucgraph)
Engine_Camera             the camera table
Engine__EngineState       the engine's internal state table
Alice_Player              a table from Alice's player package
Alice_Inventory           a table from Alice's inventory package
```

### 2.3 Why the underscore

The underscore separator is a convention, not a syntax rule. The
name is a normal identifier; the engine splits it on the first
underscore when it needs to distinguish prefix from base name.

Why underscore:

- It is a legal identifier character in Lucid.
- It is not a legal `::` or `.`, so it does not conflict with
  the format's qualifier syntax.
- It is the standard convention for module prefixes in languages
  without module-scoped names (C libraries, some C++ libraries).

### 2.4 The reserved prefix

The engine reserves the prefix `Engine`. No third-party package
may declare `Engine` as its prefix. All built-in engine content
uses `Engine_`:

- `Engine_Engine_Float32Node`, `Engine_Int32Node`, `Engine_PrintNode`,
  `Engine_EveryFrame` — the engine's core package.
- `Engine_BoolNode`, `Engine_LessThanNode`, `Engine_IfNode` — the
  engine's logic package.
- `Engine_Entity`, `Engine_Transform`, and the other core tables.

A package that tries to register a node type starting with
`Engine_` from a package whose prefix is not `Engine` is rejected
at load time.

### 2.5 The prefix pattern

A prefix must match:

```
[A-Za-z][A-Za-z0-9]*
```

That is: a letter, followed by any number of letters or digits. No
underscores, no dots, no other punctuation. This keeps the prefix
identifiable by `name.find('_')` and keeps the base name after the
first underscore.

Examples of valid prefixes: `Engine`, `Alice`, `Bob`, `Physics2D`,
`MyLib`, `X`.

Examples of invalid prefixes: `_Alice` (starts with underscore),
`Alice_2` (contains an underscore), `Alice.Bob` (contains a dot),
`2D` (starts with a digit).

---

## 3. Enforcement

The engine enforces the convention at package load time. A package
that violates the convention is rejected; its content does not
enter the merged registry.

### 3.1 Package metadata

A package declares its name and prefix in its metadata. The format
is a `package.json`-like file at the package's root:

```json
{
    "name": "alice.physics",
    "prefix": "Alice",
    "version": "1.0.0",
    "description": "2D physics for Lucid",
    "author": "Alice"
}
```

The engine reads `name` and `prefix`. If `prefix` is missing, the
package may still load if it registers no node types, enums,
handles, or tables (a logic-only package that only ships `.lucid`
helpers).

### 3.2 Package name rules

- The package name is unique across all loaded packages.
- The package name is a dotted path: `<provider>.<thing>` or
  `<provider>.<thing>.<sub>`.
- The last segment of the package name is used by the format
  library's import model as the module qualifier. It must be
  unique across loaded packages, or two packages cannot both be
  imported in the same `.lucid` file.

### 3.3 Prefix rules

- The prefix is unique across loaded packages, except that two
  packages from the same provider may share a prefix. The
  provider is the first segment of the package name. `alice.physics`
  and `alice.render` share the provider `alice` and may both use
  the prefix `Alice`. `alice.physics` and `bob.physics` have
  different providers and must have different prefixes.
- The prefix is `Engine` only for the engine's built-in packages.
- The prefix matches `[A-Za-z][A-Za-z0-9]*`.

### 3.4 Registration rules

When a package registers a node type, enum type, handle type, or
table name, the engine checks:

- The name starts with `<Prefix>_`.
- The name is not already registered by another package.

If either check fails, the registration is rejected with a clear
error. The package load fails; the package's content does not
enter the registry.

### 3.5 Error messages

A package registration that violates the convention produces an
error like:

```
Package 'alice.physics' attempted to register node type
'GravityNode'. Its prefix is 'Alice', so its node types must
start with 'Alice_'. Rename the node type to 'Alice_GravityNode'
or change the package's prefix.
```

```
Package 'bob.physics' attempted to register node type
'Alice_GravityNode'. That name is already registered by package
'alice.physics'. Rename the node type or change the package's
prefix.
```

```
Package 'bob.physics' attempted to register node type
'Alice_GravityNode'. The prefix 'Alice' is already used by
package 'alice.physics', whose provider differs from
'bob.physics''s provider. Change the package's prefix.
```

The error names the offending registration and the rule it
violates.

---

## 4. What a package author does

A package author follows this workflow:

1. **Pick a package name.** `<provider>.<thing>`: `alice.physics`.
   The last segment (`physics`) should be unique across packages
   you expect to coexist. If you're not sure, add a
   distinguishing word: `alice.physics_2d`.

2. **Pick a prefix.** Typically the provider's name capitalized:
   `Alice`. If the prefix is taken by another package (for a
   different provider), choose a longer prefix: `AlicePhysics`.

3. **Declare both in `package.json`.** The engine reads them.

4. **Register node types with the prefix.** Every node type's name
   is `Alice_<BaseName>`. Every enum type and handle type too.

5. **Register tables with the prefix.** Every table name is
   `Alice_<BaseName>`.

6. **Write `.lucid` helpers with prefixed references.** A helper
   that composes your C++ nodes writes:

   ```lucid
   @export
   node ApplyFriction(body, amount) =
       Alice_MultiplyNode(amount, Alice_BodyFrictionNode(body))
   ```

   The helper's own name (`ApplyFriction`) is unprefixed, because
   it's a node declared *in the package's `.lucid` file*, not in
   the C++ registry. The engine treats `.lucid`-defined nodes as
   the package's exports; whether they need a prefix is a
   convention the package author chooses. (See §6 for the rules on
   `.lucid`-defined nodes.)

7. **Test the package loads.** The engine rejects a package whose
   node names don't match the prefix. Test this before shipping.

---

## 5. What a script author does

A script author writes `.lucid` files that use the nodes from the
packages they've imported:

```lucid
import engine.core
import alice.physics

node frame = Engine_EveryFrame()
node body  = Alice_BodyNode(player)
node grav  = Alice_GravityNode(body)
node p     = Engine_PrintNode(grav) on frame
```

Every node type name is written in full, with its prefix. Every
table name a script references (via `resource` or via a node's
argument) is also written in full.

The editor hides the prefix visually. The palette groups nodes by
provider, shows the base name, and displays the prefix as a badge.
Dragging `Gravity` from the `Alice` group writes `Alice_GravityNode`
in the underlying `.lucid` file.

The `.lucid` text is the source of truth; it is verbose and
explicit. The editor's presentation is a convenience layered on
top.

---

## 6. `.lucid`-defined nodes

A package may ship a `.lucid` file that declares helper nodes. These
nodes are written in the Lucid language and composed from other
nodes.

**Prefix rule for `.lucid`-defined nodes.** A `.lucid`-defined node
declared by a package follows the same prefix convention as a
C++-registered node:

```lucid
@export
node Alice_ApplyFriction(body, amount) =
    Alice_MultiplyNode(amount, Alice_BodyFrictionNode(body))
```

The `@export` attribute makes the node visible to modules that
import the package. Without it, the node is private to the
package's `.lucid` file.

**What this means for the user.** The user sees the package's
exported nodes — both the C++-registered ones and the `.lucid`-defined
ones — through a single import. From the user's perspective, there's
no difference. The prefix convention applies uniformly.

**Why `.lucid`-defined nodes need the prefix too.** They end up in
the same namespace as C++-registered nodes. If a `.lucid`-defined
node is named `GravityNode` and a C++-registered node is named
`Alice_GravityNode`, the two coexist without collision but the
naming is inconsistent. The convention says: any node the package
exports has the package's prefix.

**Alternate: prefix at export time.** A package's `.lucid` file
could declare helper nodes without the prefix and let the compiler
add it at export time:

```lucid
@export
node ApplyFriction(body, amount) =
    Alice_MultiplyNode(amount, Alice_BodyFrictionNode(body))
```

and the compiler rewrites the exported name to `Alice_ApplyFriction`.

This is convenient but it makes the `.lucid` file's content
depend on where it's loaded. The engine's position: the prefix is
explicit in the source. Write `Alice_ApplyFriction` if the node is
the package's export.

---

## 7. Why not qualifiers

An alternative convention would be to use the format's existing
`::` qualifier:

```lucid
import alice.physics

node grav = physics::GravityNode(body)
```

This is what the format library's import model supports. Why not
use it?

**The last-segment collision.** The format's qualifier is the last
segment of the import path. `alice.physics` and `bob.physics` both
bind the qualifier `physics`. A `.lucid` file that imports both
cannot distinguish them. The only fix within the format's current
grammar is to require unique last segments, which is a constraint
on package names, not on node names.

**The dotted-path gap.** The format's type position accepts one
qualifier (`IDENTIFIER '::' IDENTIFIER`). `alice.physics::GravityNode`
is not expressible; you would need `physics::GravityNode` and a
package named `physics` (which collides with any other package
ending in `.physics`).

**Forward-compatibility.** The prefix convention is
forward-compatible with a future library change that supports
package-scoped names. `Alice_GravityNode` parses as the pair
`(Alice, GravityNode)` if the library ever wants to make that
split. No migration is needed; the naming is already there.

The prefix convention sidesteps both problems without a library
change. It is the standard solution for languages without
module-scoped names.

---

## 8. Migration

The convention is applied to the engine's built-in content in
Phase 10. Every existing node type and table name gains the
`Engine_` prefix.

### 8.1 Node type renames

```
Engine_Float32Node     →  Engine_Engine_Float32Node
Int32Node       →  Engine_Int32Node
EveryFrame      →  Engine_EveryFrame
PrintNode       →  Engine_PrintNode
BoolNode        →  Engine_BoolNode
LessThanNode    →  Engine_LessThanNode
IfNode          →  Engine_IfNode
```

### 8.2 Table renames

```
Entity           →  Engine_Entity
Transform        →  Engine_Transform
WorldTransform   →  Engine_WorldTransform
Velocity         →  Engine_Velocity
Sprite           →  Engine_Sprite
Script           →  Engine_Script
Camera           →  Engine_Camera
_EngineState     →  Engine__EngineState
```

The `_EngineState` table becomes `Engine__EngineState` — two
underscores, because the base name starts with an underscore. This
is a legal identifier. If you prefer a single underscore, rename
the base to `EngineState`: `Engine_EngineState`. Either is fine;
pick one.

**Recommendation:** `Engine_EngineState`, no leading underscore.
The underscore in `_EngineState` was a convention marking it as
internal; the `Engine_` prefix already does that.

### 8.3 Component tags

Every `*Tag` struct's `name` member changes:

```cpp
struct EntityTag { static constexpr const char* name = "Engine_Entity"; };
struct TransformTag { static constexpr const char* name = "Engine_Transform"; };
// ...
```

Every C++ file that uses `EntityTag::name`, `TransformTag::name`, etc.
picks up the new name automatically. No call-site changes; the
names are constants.

### 8.4 Scene files

Every `.lscene` file that references a component table by name
changes:

```json
{
  "components": {
    "Engine_Transform": { "x": 0.0, "y": 0.0 },
    "Engine_Sprite": { "texture_path": "..." }
  }
}
```

Every scene file must be updated. The loader can support both
names during a transition period (accept `Transform` and
`Engine_Transform`) if you want backward compatibility. For a
pre-ship engine, just rename; no migration.

### 8.5 `.lucid` sources

Every `.lucid` source and test that names a node type changes:

```lucid
node p = Engine_PrintNode(1.0) on Engine_EveryFrame()
```

Every test that compiles a `.lucid` source updates.

### 8.6 `.lucgraph` files

Existing `.lucgraph` files were compiled against the old registry
(no prefixes). Their fingerprint no longer matches the new
registry. They must be recompiled from their `.lucid` sources.

No `.lucgraph` files have shipped, so this is a regeneration, not
a migration.

### 8.7 `kSceneComponents` in `scene_schema.h`

The array of component names changes:

```cpp
inline constexpr const char* kSceneComponents[] = {
    "Engine_Transform",
    "Engine_WorldTransform",
    "Engine_Velocity",
    "Engine_Sprite",
    "Engine_Script",
};
```

### 8.8 Documentation

Every doc that references a node type or table name by its old
name changes. `docs/notes/TriggerModel.md`,
`docs/notes/SceneFormat.md`, and `docs/FileStructure.md` all
reference node types.

---

## 9. What is not covered

Three things the convention does not cover.

### 9.1 Function names in C++

The convention applies to *Lucid-visible* names: node types, enum
types, handle types, tables. It does not apply to C++ function
names, class names, or file names. The engine's C++ code keeps its
own conventions; those are separate from the Lucid namespace.

### 9.2 Resource names

A `.lucid` `resource` declaration's name is a local name within
the file that declares it. Two `.lucid` files can each declare
`resource Config { ... }` without colliding, because resources are
file-local. The convention does not apply.

### 9.3 Node instance names

A `.lucid` `node` declaration's name (e.g. `node p = ...`) is a
local name. It refers to the node instance within the graph, not
to a node type. Two nodes can be named `p` and `q`; nothing
collides. The convention does not apply.

The convention applies only to *type* names and *table* names, not
to *instance* names or *local* names.

---
