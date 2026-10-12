#pragma once

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
    // empty, category comes from NodeTypeInfo::category.
    struct NodeEditorInfo
    {
        // The base name for display, e.g. "Print" for
        // "Engine_PrintNode". Empty means "use the base name".
        std::string display_name;

        // A one-line summary, e.g. "Print a value to the log."
        std::string doc_summary;

        // A longer description, optional. May be shown in a tooltip
        // or a help panel.
        std::string doc_long;

        // An icon identifier, e.g. "print". The editor maps it to an
        // image. Empty means "no icon".
        std::string icon_id;

        // A sample .lucid snippet, optional. May be shown in a help
        // panel.
        std::string example;
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
    };

} // namespace engine