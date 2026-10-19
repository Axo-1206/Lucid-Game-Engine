# Package naming and table naming convention

This note defines how the engine names its content: node types,
enum types, handle types, struct types, and table names. It
explains the prefix convention, why it exists, how the engine
enforces it, and what a package author needs to know.

See also:
- `docs/notes/PackageComposition.md` — what a package
  contributes and how packages compose.
- `docs/notes/PackageTables.md` — the package-owned-table
  convention.
- `docs/notes/ExecutionModel.md` — the walker, the phases, the
  sub-graphs.
- `docs/notes/EditorMetadata.md` — the editor metadata a package
  registers for its nodes.
- `docs/Decision.md` — the design decisions that led to this
  convention (Decision 16 for node type names).

---

## 1. The problem

The engine's node registry is a flat namespace. Every node type,
enum type, handle type, struct type, and table has one name. Two
packages that both register a node type called `Gravity_Node`
produce two entries in the registry with the same name. The
editor cannot tell them apart in the palette; the project cannot
tell them apart when it references a node type by name.

The engine has three options:

1. Refuse to load conflicting packages.
2. Change the registry to support package-scoped names.
3. Enforce a naming convention at the package level.

The engine chooses **option 3**: a prefix convention. Every node
type, enum type, handle type, and struct type has a name of the
form `<Prefix>_<BaseName>`. Table names are also prefixed. The
prefix is the provider's short identifier. The engine enforces
the prefix at package load time.

> [!NOTE]
> The old §1 also explained the format library's `::` qualifier,
> the one-level qualifier depth limit, and why the qualifier
> approach was rejected. That entire discussion is gone: there
> is no format library, no `.lucid` text, no `::` qualifier. The
> prefix convention is the *only* namespacing mechanism, and the
> doc's explanation of why is now one paragraph instead of three
> sections.

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
Engine_Log_Node                   a node type from the engine's core package
Engine_Branch_Bool_Node           an engine control-flow node
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
- `Operation` is one segment: what the node does (`Log`, `Add`,
  `LessThan`, `Branch`, `Jump`).
- Each `ArgType` is one segment: the base name of an argument's
  type, in argument order (`Float`, `Int`, `Bool`, `Vector2`,
  `BodyRef`). A node with no arguments has none.
- `_Node` terminates the name.

A node that produces a constant of a primitive type uses the
type-form, `Provider_Type_Node`. It has one type segment and no
operation.

```
Engine_Log_Node                    log a value
Engine_Branch_Bool_Node            branch on a bool
Engine_Jump_Node                   unconditional jump
Engine_LessThan_Float_Float_Node   compare two floats
Engine_Add_Vector2_Vector2_Node    add two Vector2 values
Engine_MoveBody_BodyRef_Float_Float_Node
                                   move a body by two floats
```

The same operation over different argument types is a different
node type: `Engine_Log_Float_Node` and `Engine_Log_Bool_Node`
are two entries in the registry. The name says what the node
does and what it takes.

The prefix rule is the only part of the convention the engine
enforces. The rest is a recommendation that every package in the
engine follows, and that a package author should follow so that
nodes from different packages read alike in a palette. A package
may register a node whose name does not follow it.

The engine does not parse node type names. The registry
(`NodeTypeInfo`) supplies a node's shape, and the editor metadata
(`NodeEditorInfo`) supplies its display and its operation. The
name is a convention for human readers. See `Decision.md`
Decisions 16 and 17.

> [!NOTE]
> The old §2.2 example list included `Engine_Float_Node`,
> `Engine_Int_Node`, `Engine_Bool_Node` (constant nodes),
> `Engine_EveryFrame_Node` (a source trigger), and
> `Engine_If_Bool_Node` (a gating trigger). Constant nodes are
> gone (a literal is an argument, not a node). Triggers are gone
> (control flow is `Branch`/`Jump`). The replacement examples are
> the engine's actual control-flow nodes and the package actions
> that survive.

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
Engine_Camera             the camera table
Engine_EngineState        the engine's internal state table
Engine_Body               a physics component table
Engine_BodyRuntime        a physics runtime table (no codec)
Alice_Player              a table from Alice's player package
Alice_Inventory           a table from Alice's inventory package
Alice_Vector3Table        a plain table from Alice's math package
```

Table names carry no `_Node` suffix and no argument segments; the
node type convention in §2.2 applies to node types only.

> [!NOTE]
> The old table list included `Engine_Script` (a reference to a
> `.lucgraph` file). Under the pivot, `Engine_Script` is
> repurposed: its column holds the name of a sub-graph in the
> project, not a file path. See `SceneFormat.md`. The name is
> unchanged; its meaning is not.

### 2.4 Why the underscore

The underscore separator is a convention, not a syntax rule. The
name is a normal identifier; the engine splits it on the first
underscore when it needs to distinguish prefix from base name.

Why underscore:

- It is a legal identifier character.
- It is the standard convention for module prefixes in languages
  without module-scoped names (C libraries, some C++ libraries).

> [!NOTE]
> The old §2.4 justified the underscore by "It is not a legal
> `::` or `.`, so it does not conflict with the format's
> qualifier syntax." There is no qualifier syntax; that
> justification is gone. The underscore is still the convention
> because it is portable and unambiguous.

### 2.5 The reserved prefix

The engine reserves the prefix `Engine`. No third-party package
may declare `Engine` as its prefix. All built-in engine content
uses `Engine_`:

- `Engine_Log_Node`, `Engine_Branch_Bool_Node`, `Engine_Jump_Node`,
  `Engine_Entry_Node`, `Engine_Call_Node`, `Engine_Return_Node`,
  `Engine_Yield_Node` — the engine's core control-flow and utility
  nodes.
- `Engine_Key` — the engine's core enum.
- `Engine_Vector2` — the engine's math struct.
- `Engine_Entity`, `Engine_Transform`, and the other core tables.

A package that tries to register a node type starting with
`Engine_` from a package whose prefix is not `Engine` is rejected
at load time.

> [!NOTE]
> The old §2.5 list included `Engine_Float_Node`,
> `Engine_Int_Node`, `Engine_Print_Float_Node`,
> `Engine_Print_Bool_Node`, `Engine_EveryFrame_Node`, and
> `Engine_If_Bool_Node`. Those node types do not exist under the
> pivot. The replacement list is the actual engine vocabulary.

### 2.6 The prefix pattern

A prefix must match:

```
[A-Za-z][A-Za-z0-9]*
```

That is: a letter, followed by any number of letters or digits. No
underscores, no dots, no other punctuation.

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

A package declares its name, prefix, and dependencies in its
metadata. The format is a `package.json`-like file at the
package's root:

```json
{
    "name": "alice.physics",
    "prefix": "Alice",
    "version": "1.0.0",
    "description": "2D physics for Lucid",
    "author": "Alice",
    "dependencies": ["engine.core"]
}
```

The engine reads `name`, `prefix`, and `dependencies`. A package
that contributes anything to the merged registry — node types,
enums, handles, structs, tables, or components — must declare a
prefix. The prefix is what makes the package's content unique in
the shared namespace.

A package with no `prefix` may still load if it registers nothing:
no node types, enums, handles, structs, tables, or components.
Such a package contributes nothing to the registry; the engine
logs it and registers no content.

> [!NOTE]
> The old §3.1 said "A `.lucid` file whose nodes are private (not
> marked `@export`) does not require a prefix." There is no
> `.lucid` file. The exception is now narrower: a package with
> no prefix may load if it registers *nothing*. The dependency
> list is new; see `PackageComposition.md` §8.1.

### 3.2 Package name rules

- The package name is unique across all loaded packages.
- The package name is a dotted path: `<provider>.<thing>` or
  `<provider>.<thing>.<sub>`.

> [!NOTE]
> The old §3.2 added "The last segment of the package name is
> used by the format library's import model as the module
> qualifier. It must be unique across loaded packages, or two
> packages cannot both be imported in the same `.lucid` file."
> There is no import model and no `.lucid` file. The
> last-segment-uniqueness rule is gone.

### 3.3 Prefix rules

- The prefix is unique across loaded packages, except that two
  packages from the same provider may share a prefix. The
  provider is the first segment of the package name.
  `alice.physics` and `alice.render` share the provider `alice`
  and may both use the prefix `Alice`. `alice.physics` and
  `bob.physics` have different providers and must have different
  prefixes.
- The prefix is `Engine` only for the engine's built-in packages.
- The prefix matches `[A-Za-z][A-Za-z0-9]*`.

Unchanged from the old model. The rule is the same.

### 3.4 Registration rules

When a package registers a node type, enum type, handle type,
struct type, or table name, the engine checks:

- The name starts with `<Prefix>_`.
- The name is not already registered by another package.

If either check fails, the registration is rejected with a clear
error. The package load fails; the package's content does not
enter the registry.

The registration calls are:

- `add_action_node`, with `add_node_editor_info` for the editor
  metadata.
- `add_handle` for a handle type, and the matching `add_*` call for
  an enum type.
- `add_struct` for a struct type.
- `add_component` for a component table: it registers the table
  and its scene codec together.
- `add_table` for a table without a codec.
- `add_subgraph_template` for a sub-graph template.

> [!NOTE]
> The old §3.4 listed `add_node_type`, `add_node_executor`, and
> `add_trigger_mode` for a node type. Those three are replaced by
> `add_action_node` (which takes the info and the executor in one
> call). `add_subgraph_template` is new; see
> `PackageComposition.md` §11.

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

3. **Declare name, prefix, and dependencies in `package.json`.**
   The engine reads them.

4. **Register node types with the prefix.** Name each node type
   `Alice_Operation[_ArgType]*_Node` (§2.2). Every enum type,
   handle type, and struct type is `Alice_<BaseName>`.

5. **Register tables with the prefix.** Every table name is
   `Alice_<BaseName>`.

6. **Register editor metadata.** Call `add_node_editor_info` for
   each node type, with an `Operation` value. See
   `EditorMetadata.md`.

7. **Register sub-graph templates.** If your package participates
   in a phase, ship a default sub-graph for that phase. See
   `PackageComposition.md` §11.

8. **Test the package loads.** The engine rejects a package whose
   names don't match the prefix. Test this before shipping.

> [!NOTE]
> The old §4 had a step 7 ("Write `.lucid` helpers with the
> package's prefix"). There is no `.lucid`. The new step 7 is
> sub-graph templates, which is the pivot's replacement for
> "shipping a composed behavior."

---

## 5. What the editor shows the user

The editor hides the prefix visually. The palette groups nodes by
provider, shows the display name, and displays the prefix as a
badge. Dragging `Gravity` from the `Alice` group creates a node
whose type is `Alice_Gravity_Body_Node` in the project file.

The project file stores the full name. The editor's
presentation is a convenience layered on top.

> [!NOTE]
> The old §5 was "What a script author does" and showed `.lucid`
> source. There is no `.lucid` source and no script author. The
> section is now about the editor's display, which is the
> surface the user actually sees.

---

## 6. What is not covered

### 6.1 Function names in C++

The convention applies to *engine-visible* names: node types,
enum types, handle types, struct types, tables. It does not
apply to C++ function names, class names, or file names. The
engine's C++ code keeps its own conventions; those are separate
from the engine's namespace.

### 6.2 Resource names

A project's `resource` declarations are local names within the
project. Two projects can each declare `resource Config { ... }`
without colliding. The convention does not apply.

### 6.3 Node instance names

A project's node *instance* names (the names the editor gives a
placed node for its own bookkeeping) are local to the project.
The convention does not apply.

### 6.4 Scratchpad cell names

A sub-graph's scratchpad cell names are local to the sub-graph.
Two sub-graphs can each declare a cell named `hp` without
colliding. The convention does not apply.

> [!NOTE]
> The old §6 was "`.lucid`-defined nodes" and explained the
> prefix rule for `.lucid` helper nodes. There is no `.lucid`.
> The section is replaced with the four "what the convention
> does not apply to" cases, which are the local-name categories
> that survive.

---

## 7. Summary in one line

**Every engine-visible name is `<Prefix>_<BaseName>`; the prefix
is the package's, unique per provider, enforced at load time;
node types follow `<Prefix>_<Operation>[_<ArgType>]*_Node` by
convention; tables follow `<Prefix>_<BaseName>`; local names
(resources, node instances, scratchpad cells) do not follow the
convention.**
