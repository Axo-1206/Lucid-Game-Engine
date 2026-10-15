# Editor metadata

The engine exposes metadata about its vocabulary so an editor
can build a package list, a node palette, and a table
inspector. This note describes the metadata types, the operation
table, and how a package provides them.

See also `docs/notes/PackageNaming.md` for the naming
convention, `docs/notes/PackageComposition.md` for how a package
registers content, and `docs/notes/RenderPasses.md` for what the
renderer's passes provide the editor.

---

## What the editor reads

- **Packages.** `PackageManager::package_infos()` returns one
  `PackageInfo` per loaded package: name, prefix, version,
  description, author, and counts of the node types, tables,
  enums, and handles the package registered.
- **Node display info.** `PackageManager::node_editor_info(type_name)`
  returns a `NodeEditorInfo` with the display name, a short
  description, a longer description, an icon id, an example, and
  the node's operation.
- **Table display info.** `PackageManager::table_editor_info(table_name)`
  returns a `TableEditorInfo` with a display name and a short
  description.
- **Categories.** `NodeTypeInfo::category` from the registry
  gives the group a node belongs to.
- **Operations.** The operation a node performs, and the visual
  group it belongs to, come from `NodeEditorInfo::operation`
  resolved through the engine's operation table (see below).

The editor reads `NodeTypeInfo` from the registry for a node's shape
(kind, argument types, result type, category) and `NodeEditorInfo` from
the package for its display (display name, documentation, icon,
operation, operation group). It does not parse the node type name, so a
package whose names do not follow the naming convention still gets a
correct palette entry.

The registry itself (the `lucid::sema::Registry` the runtime
uses) carries the format's vocabulary: type names, kinds,
arguments. It does not carry display metadata; that is what
these structs are for.

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

### `NodeEditorInfo`

    struct NodeEditorInfo {
        std::string display_name;  // "Print"
        std::string doc_summary;   // "Print a float to the engine log."
        std::string doc_long;      // optional
        std::string icon_id;       // "print"
        std::string example;       // optional .lucid snippet

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

### `TableEditorInfo`

    struct TableEditorInfo {
        std::string display_name;
        std::string doc_summary;
        std::string icon_id;
    };

Registered via `PackageBuilder::add_table_editor_info(...)`.

---

## The operation table

`engine/registry/operation.h` declares an `Operation` enum and a
table that maps each well-known value to a canonical operation
name and a visual group. A package sets `operation` in
`add_node_editor_info`; it does not carry the operation's name or
group inline. Changing a table entry updates every node that uses
the operation.

The enum has three categories of value:

- `None`: the node has no operation. Primitive constant nodes such as
  `Engine_Float_Node` use it.
- `Custom`: the operation is not in the table. The package
  supplies its name and, optionally, its group.
- The named operations: `Add`, `LessThan`, `If`, `Print`, and so
  on.

The editor resolves the operation name and group from the table
for named operations, and from `custom_operation` and
`operation_group_override` for `Custom`. It never parses the node
type name.

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
                               "node t = Alice_Teleport_Entity_Vector2_Node(e, pos)",
                               Operation::Custom,
                               /* custom_operation */ "Teleport",
                               /* operation_group_override */ "AliceMovement",
                           });

The editor shows the node in the group `"AliceMovement"` with the
operation name `"Teleport"`. It uses the color the user has chosen
for that group, or a light-gray default.

---

## Defaults

A node type without explicit editor info still gets a palette
entry. `node_editor_info()` returns a default:

- `display_name` = the base name (type name with the package
  prefix stripped). `Engine_Print_Float_Node` → `Print_Float_Node`.
- `doc_summary`, `doc_long`, `icon_id`, `example` are empty.
- `operation` is `Operation::None`; `custom_operation` and
  `operation_group_override` are empty.

A table without explicit editor info gets a default too:
`display_name` is the base name, everything else empty.

A package author who doesn't provide editor info gets a usable
palette. A package author who does gets a polished one.

`node_editor_info()` returns `nullptr` if the type name is not
in the registry.

---

## What a package author writes

For a package with a few node types:

    void MyPackage::register_into(PackageBuilder& b) {
        // ... add_node_type, add_node_executor, add_trigger_mode ...

        b.add_node_editor_info("Alice_Add_Vector3_Vector3_Node", NodeEditorInfo{
            "Vector3 Add",
            "Add two Vector3 values.",
            "",
            "add",
            "node s = Alice_Add_Vector3_Vector3_Node(a, b)",
            Operation::Add,
        });

        // ... more nodes ...
    }

Editor info is optional. It improves the palette; it does not
affect the runtime.

---

## What this does not cover

- **Icons.** The engine stores an `icon_id` string. The editor
  maps it to an image. The engine does not ship icons.
- **Localization.** Display names and descriptions are English
  strings. A future phase could add a localization table keyed
  by type name.
- **Node ports.** The format library's `NodeTypeInfo::args` and
  `resultType` describe the node's arguments and result. The
  editor reads those for the palette; `NodeEditorInfo` is
  display metadata only.
- **Group colors.** The operation table is engine data. The color
  table is editor data. A settings file that persists the user's
  colors for custom groups is future work.