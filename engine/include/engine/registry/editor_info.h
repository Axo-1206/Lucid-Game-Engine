#pragma once

#include "engine/registry/operation.h"

#include <string>

namespace engine
{

    // Display metadata for a node type. Read by the editor to build
    // its node palette. Not used at runtime; the runtime reads
    // NodeTypeInfo from the registry, not this.
    //
    // A node type without explicit editor info gets defaults at
    // lookup time: display_name is the base name (the type name with
    // the package prefix stripped), doc_summary is empty, icon_id is
    // empty, category comes from NodeTypeInfo::category, and the
    // operation is Operation::None.
    struct NodeEditorInfo
    {
        // The base name for display, e.g. "Add" for
        // "Engine_Add_Float_Node". Empty means "use the base name".
        std::string display_name;

        // A one-line summary, e.g. "Add two float32 values."
        std::string doc_summary;

        // A longer description, optional. May be shown in a tooltip
        // or a help panel.
        std::string doc_long;

        // An icon identifier, e.g. "add". The editor maps it to an
        // image. Empty means "no icon".
        std::string icon_id;

        // A sample .lucid snippet, optional. May be shown in a help
        // panel.
        std::string example;

        // What operation the node performs. See Operation in
        // engine/registry/operation.h.
        //
        //   Operation::None   — the node has no operation. A bare
        //                       constant, a passthrough. The editor
        //                       shows no operation name and no
        //                       group.
        //   Operation::Custom — the operation is not in the engine's
        //                       table. The editor reads
        //                       `custom_operation` and
        //                       `operation_group_override`.
        //   Other values      — a well-known operation. The engine's
        //                       operation table supplies the name
        //                       and group; the two strings below
        //                       are ignored for the name and
        //                       optional for the group.
        Operation operation = Operation::None;

        // The operation name, when `operation == Operation::Custom`.
        // Ignored otherwise.
        std::string custom_operation;

        // The group, when `operation == Operation::Custom`, or an
        // override of the group for a well-known operation. Empty
        // means "no override"; the editor falls back to the group
        // from the operation's table entry, or to the fallback
        // color for Operation::None.
        std::string operation_group_override;
    };

    // Display metadata for a table. Read by the editor's entity
    // inspector to present a table's columns.
    struct TableEditorInfo
    {
        // A display name for the table, e.g. "Player" for
        // "Alice_Player". Empty means "use the base name".
        std::string display_name;

        std::string doc_summary;
        std::string icon_id;
    };

    // A package's metadata. Populated by the Package interface and
    // exposed by PackageManager::package_infos().
    struct PackageInfo
    {
        // The package's full name, e.g. "engine.core".
        std::string name;

        // The package's prefix, e.g. "Engine". Names of node types,
        // tables, enums, and handles registered by this package start
        // with this prefix followed by an underscore.
        std::string prefix;

        // Semver-ish version string, e.g. "0.1.0". Informational.
        std::string version;

        // One-line description.
        std::string description;

        // Author or provider.
        std::string author;

        // How many node types, tables, enums, handles this package
        // registered. Useful for a summary view.
        std::size_t node_type_count = 0;
        std::size_t table_count = 0;
        std::size_t enum_count = 0;
        std::size_t handle_count = 0;
        std::size_t struct_count = 0;
    };

} // namespace engine