#pragma once

#include "sema/Registry.hpp"

namespace engine
{

    // The engine's node vocabulary, in the shape the format library expects.
    //
    // This function returns a reference to a process-lifetime, immutable
    // lucid::sema::Registry. It is built once, on first call, from static
    // const arrays. The reference is valid for the rest of the process.
    //
    // The format library reads this registry:
    //   - at compile time, via lucid::sema::compile(), to check every
    //     node type, every argument, every kind-based rule.
    //   - at load time, via lucid::serialization::deserialize(), to
    //     verify the fingerprint of a .lucgraph file.
    //
    // The engine does not store this registry in engine::ecs::World. World
    // is the live table store; the registry is metadata about node
    // vocabulary. They are unrelated. See docs/Decision.md.
    //
    // Content of the registry:
    //   - 3 phases: update, physics, render
    //   - 1 enum: Key, with members W=0, A=1, S=2, D=3
    //   - 1 handle: BodyRef
    //   - 4 node types:
    //       Float32Node(value: float32) -> float32   (Value, Math)
    //       Int32Node  (value: int32)   -> int32     (Value, Math)
    //       EveryFrame ()                            (Trigger, Flow)
    //       PrintNode  (value: float32)              (Action, Debug, update)
    //
    // Append-only. The index of a node type in nodeTypes is its NodeId
    // in every .lucgraph. Reordering renumbers existing files and breaks
    // the engine's node_runtime_info() table.
    const lucid::sema::Registry &node_registry();

} // namespace engine