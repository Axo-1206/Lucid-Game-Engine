# Package naming and table naming convention

This note defines how the engine names its content: node types,
enum types, handle types, struct types, and table names. It
explains the prefix convention, why it exists, how the engine
enforces it, the node type name convention, and what a package
author and a script author need to know.

See also:
- `docs/notes/TriggerModel.md` — how a graph expresses control flow.
- `docs/notes/PackageComposition.md` — what a package contributes
  and how packages compose.
- `docs/notes/EditorMetadata.md` — the editor metadata a package
  registers for its nodes.
- `docs/notes/RegistryContractNote.md` (in the format library's
  docs) — the boundary between the engine and the format library.
- `docs/Decision.md` — the design decisions that led to this
  convention (Decision 16 for node type names).

---

## 1. The problem

The format library's `NodeTypeInfo` carries **one name**. Two
packages that both declare a node type called `GravityNode` produce
two entries in the merged registry with the same `name`. Sema
cannot tell them apart when a `.lucid` file writes
`GravityNode(...)`.

The same problem applies to enum types, handle types, struct types,
and table names. The engine's registry is a flat namespace; two
declarations with the same name collide.

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
type, enum type, handle type, and struct type has a name of the
form `<Prefix>_<BaseName>`. Table names are also prefixed. The
prefix is the provider's short identifier. The engine enforces the
prefix at package load time.

---

## 2. The naming convention

### 2.1 Node types, enums, handles, structs

A node type, enum type, handle type, or struct type registered by
a package has a name of the form:

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
Engine_Print_Float_Node           a node type from the engine's core package
Engine_EveryFrame_Node            a node type from the engine's core package
Engine_If_Bool_Node               a node type from the engine's logic package
Engine_Key                        an enum type from the engine's core package
Engine_Vector2                    a struct type from the engine's math package
Alice_Gravity_Body_Node           a node type from Alice's physics package
Alice_Body                        a handle type from Alice's physics package
Physics2D_RigidBody               a handle type from a package using the prefix Physics2D
```

### 2.2 Node type names

A node type's `<BaseName>` follows a further convention:

```
Provider_Operation[_ArgType]*_Node
```

- `Provider` is the package's prefix.
- `Operation` is one segment: what the node does (`Print`, `Add`,
  `LessThan`, `If`, `EveryFrame`).
- Each `ArgType` is one segment: the base name of an argument's
  type, in argument order (`Float`, `Int`, `Bool`, `Vector2`,
  `BodyRef`). A node with no arguments has none.
- `_Node` terminates the name.

A node that produces a constant of a primitive type uses the
type-form, `Provider_Type_Node`. It has one type segment and no
operation: `Engine_Float_Node` is a node that produces a float, and
its `Operation` is `Operation::None`. Every other node uses the
operation-form above. A constructor such as
`Engine_Vector2_Float_Float_Node` is operation-form: the type name
is the operation, and the two floats are its arguments.

```
Engine_Float_Node                    a float constant
Engine_Int_Node                      an int constant
Engine_Bool_Node                     a bool constant
Engine_EveryFrame_Node               no arguments
Engine_Print_Float_Node              print a float
Engine_Print_Bool_Node               print a bool
Engine_LessThan_Float_Float_Node     compare two floats
Engine_LessThan_Int_Int_Node         compare two ints
Engine_If_Bool_Node                  gate on a bool
Engine_Add_Vector2_Vector2_Node      add two Vector2 values
Engine_MoveBody_BodyRef_Float_Float_Node
                                     move a body by two floats
```

The same operation over different argument types is a different
node type: `Engine_Print_Float_Node` and `Engine_Print_Bool_Node`
are two entries in the registry. The name says what the node does
and what it takes, so a reader of a `.lucid` file does not have to
look up a signature to know which one is being called.

The prefix rule is the only part of the convention the engine
enforces. The rest is a recommendation that every package in the
engine follows, and that a package author should follow so that
nodes from different packages read alike in a palette and in a
`.lucid` file. A package may register a node whose name does not
follow it.

The engine does not parse node type names. The registry
(`NodeTypeInfo`) supplies a node's shape, and the editor metadata
(`NodeEditorInfo`) supplies its display and its operation. The name
is a convention for human readers. See `Decision.md` Decisions 16
and 17.

### 2.3 Tables

Table names follow the prefix convention:

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
Engine_EngineState        the engine's internal state table
Alice_Player              a table from Alice's player package
Alice_Inventory           a table from Alice's inventory package
```

Table names carry no `_Node` suffix and no argument segments; the
node type convention in §2.2 applies to node types only.

### 2.4 Why the underscore

The underscore separator is a convention, not a syntax rule. The
name is a normal identifier; the engine splits it on the first
underscore when it needs to distinguish prefix from base name.

Why underscore:

- It is a legal identifier character in Lucid.
- It is not a legal `::` or `.`, so it does not conflict with
  the format's qualifier syntax.
- It is the standard convention for module prefixes in languages
  without module-scoped names (C libraries, some C++ libraries).

### 2.5 The reserved prefix

The engine reserves the prefix `Engine`. No third-party package
may declare `Engine` as its prefix. All built-in engine content
uses `Engine_`:

- `Engine_Float_Node`, `Engine_Int_Node`, `Engine_Print_Float_Node`,
  `Engine_Print_Bool_Node`, `Engine_EveryFrame_Node` — the engine's
  core package.
- `Engine_Bool_Node`, `Engine_LessThan_Float_Float_Node`,
  `Engine_LessThan_Int_Int_Node`, `Engine_If_Bool_Node` — the
  engine's logic package.
- `Engine_Vector2`, `Engine_Add_Vector2_Vector2_Node` — the
  engine's math package.
- `Engine_Entity`, `Engine_Transform`, and the other core tables.

A package that tries to register a node type starting with
`Engine_` from a package whose prefix is not `Engine` is rejected
at load time.

### 2.6 The prefix pattern

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
handles, structs, or tables (a logic-only package that only ships
`.lucid` helpers).

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

When a package registers a node type, enum type, handle type,
struct type, or table name, the engine checks:

- The name starts with `<Prefix>_`.
- The name is not already registered by another package.

If either check fails, the registration is rejected with a clear
error. The package load fails; the package's content does not
enter the registry.

The registration calls are:

- `add_node_type`, with `add_node_executor` and `add_trigger_mode`,
  for a node type.
- `add_handle` for a handle type, and the matching `add_*` call for
  an enum type.
- `add_struct` for a struct type.
- `add_component` for a component table: it registers the table
  and its scene codec together.
- `add_table` for a table without a codec.
- `add_node_editor_info` for a node's editor metadata. The
  metadata carries an `Operation` value (see `EditorMetadata.md`).

### 3.5 Error messages

A package registration that violates the convention produces an
error like:

```
Package 'alice.physics' attempted to register node type
'Gravity_Body_Node'. Its prefix is 'Alice', so its node types must
start with 'Alice_'. Rename the node type to
'Alice_Gravity_Body_Node' or change the package's prefix.
```

```
Package 'bob.physics' attempted to register node type
'Alice_Gravity_Body_Node'. That name is already registered by package
'alice.physics'. Rename the node type or change the package's
prefix.
```

```
Package 'bob.physics' attempted to register node type
'Alice_Gravity_Body_Node'. The prefix 'Alice' is already used by
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

4. **Register node types with the prefix.** Name each node type
   `Alice_Operation[_ArgType]*_Node` (§2.2). Every enum type,
   handle type, and struct type is `Alice_<BaseName>`.

5. **Register tables with the prefix.** Every table name is
   `Alice_<BaseName>`.

6. **Register editor metadata.** Call `add_node_editor_info` for
   each node type, with an `Operation` value. See
   `EditorMetadata.md`.

7. **Write `.lucid` helpers with prefixed references.** A helper
   that composes your C++ nodes writes:

   ```lucid
   @export
   node Alice_ApplyFriction_Body_Float_Node(body, amount) =
       Alice_Multiply_Float_Float_Node(amount, Alice_BodyFriction_Body_Node(body))
   ```

   The helper is declared in the package's `.lucid` file, not in
   the C++ registry, and it carries the package's prefix like any
   other exported node. See §6.

8. **Test the package loads.** The engine rejects a package whose
   names don't match the prefix. Test this before shipping.

---

## 5. What a script author does

A script author writes `.lucid` files that use the nodes from the
packages they've imported:

```lucid
import engine.core
import alice.physics

node frame = Engine_EveryFrame_Node()
node body  = Alice_CreateBody_Entity_Node(player)
node grav  = Alice_Gravity_Body_Node(body)
node p     = Engine_Print_Float_Node(grav) on frame
```

Every node type name is written in full, with its prefix. Every
table name a script references (via `resource` or via a node's
argument) is also written in full.

The editor hides the prefix visually. The palette groups nodes by
provider, shows the display name, and displays the prefix as a
badge. Dragging `Gravity` from the `Alice` group writes
`Alice_Gravity_Body_Node` in the underlying `.lucid` file.

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
C++-registered node, and the same node type name convention
(§2.2):

```lucid
@export
node Alice_ApplyFriction_Body_Float_Node(body, amount) =
    Alice_Multiply_Float_Float_Node(amount, Alice_BodyFriction_Body_Node(body))
```

The `@export` attribute makes the node visible to modules that
import the package. Without it, the node is private to the
package's `.lucid` file.

**What this means for the user.** The user sees the package's
exported nodes — both the C++-registered ones and the `.lucid`-defined
ones — through a single import. From the user's perspective, there's
no difference. The prefix convention applies uniformly.

**Why `.lucid`-defined nodes need the prefix too.** They end up in
the same namespace as C++-registered nodes. Any node the package
exports has the package's prefix, so a reader can tell which
package provides it.

**The prefix is explicit in the source.** The compiler does not add
the prefix at export time. A `.lucid` file's content does not
depend on where it is loaded: write `Alice_ApplyFriction_Body_Float_Node`
if the node is the package's export.

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
package-scoped names. `Alice_Gravity_Body_Node` parses as the pair
`(Alice, Gravity_Body_Node)` if the library ever wants to make that
split.

The prefix convention sidesteps both problems without a library
change. It is the standard solution for languages without
module-scoped names.

---

## 8. What is not covered

Three things the convention does not cover.

### 8.1 Function names in C++

The convention applies to *Lucid-visible* names: node types, enum
types, handle types, struct types, tables. It does not apply to C++
function names, class names, or file names. The engine's C++ code
keeps its own conventions; those are separate from the Lucid
namespace.

### 8.2 Resource names

A `.lucid` `resource` declaration's name is a local name within
the file that declares it. Two `.lucid` files can each declare
`resource Config { ... }` without colliding, because resources are
file-local. The convention does not apply.

### 8.3 Node instance names

A `.lucid` `node` declaration's name (e.g. `node p = ...`) is a
local name. It refers to the node instance within the graph, not
to a node type. Two nodes can be named `p` and `q`; nothing
collides. The convention does not apply.

The convention applies only to *type* names and *table* names, not
to *instance* names or *local* names.