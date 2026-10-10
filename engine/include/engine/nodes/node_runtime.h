#pragma once

#include <cstdint>

namespace engine
{

    // The engine's runtime role for a node type.
    //
    // The format library's Registry says *what* a node type is (its kind,
    // its arguments). It does not say how a trigger participates in the
    // event graph. That is the engine's concern, and it lives here.
    //
    // Indexed by lucid::sema::NodeId (the index of the node type in
    // node_registry().nodeTypes).
    enum class TriggerMode : std::uint8_t
    {
        NotATrigger, // a Value or Action node; fires nothing
        Source,      // a Trigger the engine fires directly each frame
        Gating,      // a Trigger that fires when a parent fires and its
                     // condition argument holds
    };

    struct NodeTypeRuntimeInfo
    {
        TriggerMode mode = TriggerMode::NotATrigger;
    };

    // Look up the runtime info for a node type. `id` must be a valid
    // index into node_registry().nodeTypes. A Value or Action node
    // returns {TriggerMode::NotATrigger}. Out-of-range ids are a
    // programming error and abort in debug builds; in release they
    // return a default-constructed NodeTypeRuntimeInfo.
    const NodeTypeRuntimeInfo &node_runtime_info(std::uint32_t id);

    // The number of entries in the runtime metadata table. Always equals
    // node_registry().nodeTypes.size().
    std::uint32_t node_runtime_info_count();

} // namespace engine