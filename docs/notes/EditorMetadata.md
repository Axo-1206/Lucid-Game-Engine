# Editor metadata

The engine exposes metadata about its vocabulary so an editor
can build a package list, a node palette, and a table
inspector. This note describes the metadata types, the operation
table, and how a package provides them.

See also `docs/notes/PackageNaming.md` for the naming
convention, `docs/notes/PackageComposition.md` for how a package
registers content, `docs/notes/ExecutionModel.md` for the walker
and phases the editor's palette reflects, and
`docs/notes/RenderPasses.md` for what the renderer's passes
provide the editor.

---

## What the editor reads

- **Packages.** `PackageManager::package_infos()` returns one
  `PackageInfo` per loaded package: name, prefix, version,
  description, author, and counts of the node types, tables,
  enums, and handles the package registered.
- **Node display info.** `PackageManager::node_editor_info(type_name)`
  returns a `NodeEditorInfo` with the display name, a short
  description, a longer description, an icon id, and the node's
  operation.
- **Table display info.** `PackageManager::table_editor_info(table_name)`
  returns a `TableEditorInfo` with a display name and a short
  description.
- **Categories.** `NodeTypeInfo::category` from the node registry
  gives the group a node belongs to.
- **Operations.** The operation a node performs, and the visual
  group it belongs to, come from `NodeEditorInfo::operation`
  resolved through the engine's operation table (see below).
- **Argument kinds and phase hint.** `NodeTypeInfo::args` carries
  a `NodeArgKind` per argument (`Value` or `Successor`), and
  `NodeTypeInfo::phase_hint` carries an optional phase id. The
  editor uses the first to decide how to draw a port and the
  second to warn when a node is placed in a sub-graph of a
  different phase (see `ExecutionModel.md` §3).

The editor reads `NodeTypeInfo` from the node registry for a
node's shape (kind, argument types, argument kinds, result type,
category, phase hint) and `NodeEditorInfo` from the package for
its display (display name, documentation, icon, operation,
operation group). It does not parse the node type name, so a
package whose names do not follow the naming convention still
gets a correct palette entry.

The node registry (the `engine::NodeRegistry` the runtime uses)
carries the vocabulary: type names, kinds, arguments, phase
hints. It does not carry display metadata; that is what these
structs are for.

> [!NOTE]
> The old §"What the editor reads" described the registry as
> "the `lucid::sema::Registry` the runtime uses" and the node
> kinds as "Value, Action, Trigger." There is no format library
> and no `lucid::sema::Registry`. The registry is the engine's
> `NodeRegistry`. The node kinds are `Action` on the user
> surface (see `PackageComposition.md` §5.2); the editor still
> reads `NodeTypeInfo::kind`, but it is `Action` for every
> user-facing node.
>
> The old list included `example` as a field the editor reads
> from `NodeEditorInfo`. The `example` field is removed (see
> below). The list now includes `NodeArgKind` and `phase_hint`,
> which are new to `NodeTypeInfo`.

---

## The types

### `PackageInfo`

    struct PackageInfo {
        std::string name;         // "engine.core"
        std::string prefix;       // "Engine"
        std::string version;      // "0.1.0"
        std::string description;
        std::string author;
        std::size_t node_type_count = 0;
        std::size_t table_count = 0;
        std::size_t enum_count = 0;
        std::size_t handle_count = 0;
    };

Populated by `PackageManager::load_package()`. The counts are
filled during `apply()`.

> Kept as-is. The counts and fields are unchanged.

### `NodeEditorInfo`

    struct NodeEditorInfo {
        std::string display_name;  // "Log"
        std::string doc_summary;   // "Log a value to the engine log."
        std::string doc_long;      // optional
        std::string icon_id;       // "log"

        // What operation the node performs. See Operation in
        // engine/registry/operation.h. Operation::None means the node
        // has no operation; Operation::Custom means the package
        // supplies `custom_operation` and `operation_group_override`;
        // any other value is a well-known operation whose name and
        // group come from the engine's operation table.
        Operation operation = Operation::None;

        // The operation name, when `operation == Operation::Custom`.
        // Ignored otherwise.
        std::string custom_operation;

        // The group, when `operation == Operation::Custom`, or an
        // override of the group for a well-known operation. Empty
        // means "no override".
        std::string operation_group_override;
    };

Registered by a package via
`PackageBuilder::add_node_editor_info(type_name, info)`.

> [!NOTE]
> The old `NodeEditorInfo` had an `example` field holding a
> `.lucid` snippet (`"node p = Engine_Print_Float_Node(v) on frame"`).
> There is no `.lucid` source text and no compiler; the `example`
> field is removed. The editor's palette uses `doc_summary`,
> `doc_long`, and `icon_id` for its display. A package that wants
> to ship an example can attach it to a sub-graph template (see
> `PackageComposition.md` §11), which the editor can render as a
> small preview. The field is not part of `NodeEditorInfo`.

### `TableEditorInfo`

    struct TableEditorInfo {
        std::string display_name;
        std::string doc_summary;
        std::string icon_id;
    };

Registered via `PackageBuilder::add_table_editor_info(...)`.

> Kept as-is.

### `NodeTypeInfo`

The editor reads `NodeTypeInfo` from the node registry. It is not
an editor type; it is the node vocabulary's record. The editor
reads:

    struct NodeTypeInfo {
        std::string_view name;
        NodeKind kind;                          // Action
        std::string_view category;              // palette grouping
        std::uint32_t phase_hint = kNoPhase;    // advisory
        ArenaSpan<NodeArgInfo> args;            // each has NodeArgKind
        // Value nodes' resultType is unused on the user surface;
        // Action nodes have no result.
    };

    struct NodeArgInfo {
        std::string_view name;
        TypeId type;
        NodeArgKind kind = NodeArgKind::Value;  // Value | Successor
    };

`phase_hint` is the phase the node is typically used in
(`pre_physics`, `physics`, ..., or `kNoPhase` for "any"). It is
**advisory**: the editor warns (`Warn_NodeInWrongPhase`) when a
node is placed in a sub-graph of a different phase, but the
scheduler does not read it. See `ExecutionModel.md` §3.

`NodeArgInfo::kind` is `Value` for a data input (a literal,
resource field, path into the World or the scratchpad, or a
handle) and `Successor` for a control-flow target (a node index).
The editor draws the port differently and enforces the target
kind when the user wires it. See `ExecutionModel.md` §1.2.

> [!NOTE]
> The old doc mentioned `NodeTypeInfo::args` and
> `NodeTypeInfo::resultType` only in passing. The new shape
> includes `NodeArgKind` per argument and a `phase_hint` on the
> node type. Both are new under the pivot. `resultType` survives
> on `NodeTypeInfo` for the engine's internal Value nodes, but
> user-facing Actions have no result; the editor shows Actions
> with output ports only if they write to a named cell (the
> editor reads the cell from the node's `Successor`/`Value`
> arguments, not from `resultType`).

---

## The operation table

`engine/registry/operation.h` declares an `Operation` enum and a
table that maps each well-known value to a canonical operation
name and a visual group. A package sets `operation` in
`add_node_editor_info`; it does not carry the operation's name or
group inline. Changing a table entry updates every node that uses
the operation.

The enum has three categories of value:

- `None`: the node has no operation. Engine control-flow nodes
  such as `Engine_Entry_Node` and `Engine_Yield_Node` use it.
- `Custom`: the operation is not in the table. The package
  supplies its name and, optionally, its group.
- The named operations: `Add`, `LessThan`, `Branch`, `Jump`, and
  so on.

The editor resolves the operation name and group from the table
for named operations, and from `custom_operation` and
`operation_group_override` for `Custom`. It never parses the node
type name.

> [!NOTE]
> The old examples listed `If` and `Print` as named operations.
> `If` is now `Branch`, and `Print` is `Log` (or similar). The
> set of named operations is the engine's; the doc names two as
> illustrations.

### Recommended operation groups

The engine's table maps each named operation to a group. The
groups are:

- `Arithmetic`
- `Comparison`
- `Logic`
- `Conversion`
- `Vector`
- `Flow`
- `Debug`
- `Package`, used for a group the editor does not know

The editor maps a group to a color. The set is not closed: a
package that uses a group the editor does not know gets the
editor's fallback color.

The editor's color table belongs to the editor, not the engine.
The engine documents the recommended groups; the editor ships the
colors.

### A package's custom operation

A package that wants an operation the enum does not name uses
`Operation::Custom`:

    b.add_node_editor_info("Alice_Teleport_Entity_Vector2_Node",
                           NodeEditorInfo{
                               "Teleport",
                               "Teleport Alice's avatar.",
                               "",
                               "teleport",
                               Operation::Custom,
                               /* custom_operation */ "Teleport",
                               /* operation_group_override */ "AliceMovement",
                           });

The editor shows the node in the group `"AliceMovement"` with the
operation name `"Teleport"`. It uses the color the user has chosen
for that group, or a light-gray default.

> [!NOTE]
> The old example's `NodeEditorInfo` had an `example` string
> between `icon_id` and `operation`. The struct has no `example`
> field; the example here omits it.

---

## Defaults

A node type without explicit editor info still gets a palette
entry. `node_editor_info()` returns a default:

- `display_name` = the base name (type name with the package
  prefix stripped). `Engine_Log_Node` → `Log_Node`.
- `doc_summary`, `doc_long`, `icon_id` are empty.
- `operation` is `Operation::None`; `custom_operation` and
  `operation_group_override` are empty.

A table without explicit editor info gets a default too:
`display_name` is the base name, everything else empty.

A package author who doesn't provide editor info gets a usable
palette. A package author who does gets a polished one.

`node_editor_info()` returns `nullptr` if the type name is not
in the registry.

> [!NOTE]
> The old example used `Engine_Print_Float_Node` (which no
> longer exists) and listed `example` among the empty defaults.
> The new example uses `Engine_Log_Node`, and `example` is not
> among the fields.

---

## What a package author writes

For a package with a few node types:

    void MyPackage::register_into(PackageBuilder& b) {
        // ... add_action_node, add_node_editor_info ...

        b.add_node_editor_info("Alice_Add_Vector3_Vector3_Node", NodeEditorInfo{
            "Vector3 Add",
            "Add two Vector3 values and write the result to a scratch cell.",
            "",
            "add",
            Operation::Add,
        });

        // ... more nodes ...
    }

Editor info is optional. It improves the palette; it does not
affect the runtime.

The node's arguments, argument kinds, and phase hint come from
the `NodeTypeInfo` the package registered with `add_action_node`,
not from `NodeEditorInfo`. The editor reads both: `NodeTypeInfo`
for shape, `NodeEditorInfo` for display.

> [!NOTE]
> The old example used `add_node_type` + `add_node_executor` +
> `add_trigger_mode`. Those three are replaced by `add_action_node`
> (see `PackageComposition.md` §5). The old example passed an
> `example` string to `NodeEditorInfo`; the new one omits it.

---

## What this does not cover

- **Icons.** The engine stores an `icon_id` string. The editor
  maps it to an image. The engine does not ship icons.
- **Localization.** Display names and descriptions are English
  strings. A future phase could add a localization table keyed
  by type name.
- **Node ports.** `NodeTypeInfo::args` (with `NodeArgKind`) and
  `NodeTypeInfo::resultType` describe the node's arguments and
  result. The editor reads those for the palette;
  `NodeEditorInfo` is display metadata only.
- **Group colors.** The operation table is engine data. The color
  table is editor data. A settings file that persists the user's
  colors for custom groups is future work.
- **Sub-graph templates.** A package can ship sub-graph templates
  that the editor offers as starting points. Their display
  metadata (name, category, description) is separate from
  `NodeEditorInfo`; see `PackageComposition.md` §11.

> [!NOTE]
> The "Node ports" bullet was updated to mention `NodeArgKind`.
> The "Sub-graph templates" bullet is new: templates are a new
> editor-facing concept under the pivot.

---

## Cross-references

- `docs/notes/PackageNaming.md` — the naming convention.
- `docs/notes/PackageComposition.md` — how a package registers
  content, including `add_action_node` and sub-graph templates.
- `docs/notes/ExecutionModel.md` — the walker, the phases, the
  `Successor`/`Value` distinction, and the `phase_hint`'s
  advisory role.
- `docs/notes/RenderPasses.md` — what the renderer's passes
  provide the editor.
