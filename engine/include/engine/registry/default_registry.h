#pragma once

#include "sema/Registry.hpp"

#include <cstdint>

namespace engine
{

    // The runtime role of a node type. A Value or Action node is
    // NotATrigger. A Trigger node is either Source (the engine fires
    // it directly) or Gating (it fires when a parent fires and its
    // condition holds). See docs/notes/TriggerModel.md.
    enum class TriggerMode : std::uint8_t
    {
        NotATrigger,
        Source,
        Gating,
    };

    struct NodeTypeRuntimeInfo
    {
        TriggerMode mode = TriggerMode::NotATrigger;
    };

    // The engine's built-in node registry, produced by the
    // process-lifetime PackageManager built from the engine's own
    // packages (CorePackage, LogicPackage, MathPackage).
    //
    // Constructed once, on first call. Returns a reference valid for
    // the process's lifetime.
    //
    // This is a convenience, not the engine's only registry. A
    // caller that wants the built-in vocabulary without constructing
    // its own PackageManager calls this. A caller that wants a
    // custom vocabulary — a game that loads its own packages, a test
    // that uses a test package — builds a PackageManager directly.
    //
    // The node types the registry contains are the ones the
    // built-in packages register; see default_registry.cpp for the
    // list of packages and the order they are loaded in. The
    // registry is append-only: adding a package at the end of the
    // load list leaves the existing node ids stable. Inserting one
    // in the middle renumbers every node after it, and every
    // .lucgraph compiled against the old order must be regenerated.
    // See docs/notes/PackageNaming.md and docs/Decision.md.
    const lucid::sema::Registry &default_registry();

    // The runtime info for the node type at index `id` in
    // default_registry().nodeTypes. `id` is a NodeId.
    //
    // Out-of-range ids return a default-constructed
    // NodeTypeRuntimeInfo (NotATrigger).
    const NodeTypeRuntimeInfo &node_runtime_info(std::uint32_t id);

    // The number of node types in the default registry. Equal to
    // default_registry().nodeTypes.size().
    std::uint32_t node_runtime_info_count();

} // namespace engine