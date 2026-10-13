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

    // The engine's node registry, produced by the process-lifetime
    // PackageManager (see default_registry.cpp).
    //
    // Constructed once, on first call, from CorePackage and
    // LogicPackage. Returns a reference valid for the process's
    // lifetime.
    //
    // This function is the compatibility shim for existing call sites.
    // New code uses a PackageManager directly, so that custom packages
    // can be added.
    //
    // Node types registered (in this order):
    //   0: Engine_Float32Node    (engine.core)
    //   1: Engine_Int32Node      (engine.core)
    //   2: Engine_EveryFrame     (engine.core)
    //   3: Engine_PrintNode      (engine.core)
    //   4: Engine_BoolNode       (engine.logic)
    //   5: Engine_LessThanNode   (engine.logic)
    //   6: Engine_IfNode         (engine.logic)
    //
    // Append-only. The index of a node type in nodeTypes is its NodeId
    // in every .lucgraph.
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