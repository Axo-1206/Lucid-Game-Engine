# Editor metadata

The engine exposes metadata about its vocabulary so an editor
can build a package list, a node palette, and a table
inspector. This note describes the metadata types and how a
package provides them.

See also `docs/notes/PackageNaming.md` for the naming
convention, and `docs/notes/RenderPasses.md` for what the
renderer's passes provide the editor.

---

## What the editor reads

- **Packages.** `PackageManager::package_infos()` returns one
  `PackageInfo` per loaded package: name, prefix, version,
  description, author, and counts of the node types, tables,
  enums, and handles the package registered.
- **Node display info.** `PackageManager::node_editor_info(type_name)`
  returns a `NodeEditorInfo` with the display name, a short
  description, a longer description, an icon id, and an example.
- **Table display info.** `PackageManager::table_editor_info(table_name)`
  returns a `TableEditorInfo` with a display name and a short
  description.
- **Categories.** `NodeTypeInfo::category` from the registry
  gives the group a node belongs to.

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
        std::string doc_summary;   // "Print a value to the engine log."
        std::string doc_long;      // optional
        std::string icon_id;       // "print"
        std::string example;       // optional .lucid snippet
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

## Defaults

A node type without explicit editor info still gets a palette
entry. `node_editor_info()` returns a default:

- `display_name` = the base name (type name with the package
  prefix stripped). `Engine_PrintNode` → `PrintNode`.
- `doc_summary`, `doc_long`, `icon_id`, `example` are empty.

A table without explicit editor info gets a default too:
`display_name` is the base name, everything else empty.

A package author who doesn't provide editor info gets a usable
palette. A package author who does gets a polished one.

`node_editor_info()` returns `nullptr` if the type name is not
in the registry.

---

## What a package author writes

For a package with four node types:

    void MyPackage::register_into(PackageBuilder& b) {
        // ... add_node_type, add_node_executor, add_trigger_mode ...

        b.add_node_editor_info("Alice_GravityNode", NodeEditorInfo{
            "Gravity",
            "Apply gravity to a body for one tick.",
            "",
            "gravity",
            "node g = Alice_GravityNode(body)",
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