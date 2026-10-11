#pragma once

#include "engine/ecs/world.h"
#include "sema/Graph.hpp"

#include <cstdint>

namespace engine::graph
{

    // One executor per node type, indexed by NodeId. A NodeId is the
    // index of a node type in node_registry().nodeTypes.
    //
    // An executor reads its node's arguments from the graph, does the
    // work against the world, and — for a Value node — stores the
    // result in `results[node_index]`.
    //
    // The `results` vector is per-graph scratch space, sized to
    // graph.nodes.size(), that the runtime passes in on each tick. A
    // Value node writes its result there; an Action or Trigger reads
    // from it (for its arguments that reference other nodes).
    //
    // `dt` is the tick's delta time in seconds.
    //
    // The executor must not throw. If a node's arguments are
    // malformed at runtime (which Sema should have caught at compile
    // time), the executor logs and returns; it does not abort the
    // tick.
    using NodeExecutor = void (*)(engine::ecs::World &world,
                                  const lucid::sema::Graph &graph,
                                  std::uint32_t node_index,
                                  std::vector<float> &results,
                                  float dt);

    // The executor table. One entry per node type in
    // node_registry().nodeTypes, in the same order.
    //
    // The table's size must equal node_registry().nodeTypes.size().
    // A debug assertion enforces this at startup. A NodeId outside
    // the table's range is a programming error.
    const NodeExecutor *node_executors();
    std::uint32_t node_executor_count();

} // namespace engine::graph