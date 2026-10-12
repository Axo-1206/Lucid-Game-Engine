#pragma once

#include "engine/ecs/cell_value.h"
#include "engine/ecs/world.h"
#include "sema/Graph.hpp"

#include <cstdint>
#include <vector>

namespace engine::graph
{

    // The per-graph, per-tick context an executor runs against.
    struct TickContext
    {
        engine::ecs::World &world;
        const lucid::sema::Graph &graph;
        std::vector<engine::ecs::CellValue> &results;
        const std::vector<engine::ecs::CellValue> &arg_values;
        float dt;
    };

    // One executor per node type, indexed by NodeId. A NodeId is the
    // index of a node type in the registry's nodeTypes span.
    //
    // The executor reads its arguments from `ctx.arg_values`, does the
    // work against `ctx.world`, and — for a Value node — stores the
    // result in `ctx.results[node_index]`.
    //
    // The executor must not throw.
    using NodeExecutor = void (*)(TickContext &ctx, std::uint32_t node_index);

} // namespace engine::graph