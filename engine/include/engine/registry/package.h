#pragma once

namespace engine
{

    class PackageBuilder;

    // A package registers its content into the engine. It is a C++
    // object; the engine calls register_into() once, at startup,
    // before any .lucid file is compiled.
    //
    // A package provides:
    //   - Tables it owns (Engine_Entity, Alice_Player, ...).
    //   - Node types (Engine_PrintNode, Alice_GravityNode, ...).
    //   - Node executors (the C++ function for each node type).
    //   - Enum types and handle types it declares.
    //
    // Naming: every node type, enum type, handle type, and table name
    // the package registers starts with `<prefix>_`. The manager
    // enforces this. See docs/notes/PackageNaming.md.
    //
    // Two packages from the same provider may share a prefix
    // (alice.physics and alice.render both use `Alice`). Two packages
    // from different providers must have different prefixes.
    class Package
    {
    public:
        virtual ~Package() = default;

        // The package's full name: `<provider>.<thing>`.
        // E.g. "engine.core", "alice.physics".
        virtual const char *name() const = 0;

        // The package's prefix. Matches `[A-Za-z][A-Za-z0-9]*`.
        // E.g. "Engine", "Alice", "Physics2D".
        // The engine reserves "Engine" for its own packages.
        virtual const char *prefix() const = 0;

        // Register this package's content. Called exactly once, in
        // package load order, before any compile() call.
        virtual void register_into(PackageBuilder &b) = 0;
    };

} // namespace engine