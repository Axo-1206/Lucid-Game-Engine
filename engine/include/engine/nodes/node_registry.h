#pragma once

#include "sema/Registry.hpp"

#include <cstdint>

namespace engine
{

    // The engine's node registry, produced by the process-lifetime
    // PackageManager (see default_manager.h).
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
    const lucid::sema::Registry &node_registry();

} // namespace engine