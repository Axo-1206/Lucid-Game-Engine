#pragma once

#include "engine/ecs/cell_value.h"
#include "engine/ecs/world.h"
#include "sema/Graph.hpp"

#include <cstdint>
#include <vector>

namespace engine::graph
{

    // The per-graph, per-tick context an executor runs against.
    //
    // - `world`      the live table store.
    // - `graph`      the compiled graph being ticked.
    // - `results`    one CellValue per node; a Value node writes its
    //                result here, others read from here.
    // - `arg_values` resolved argument values, one per graph.args
    //                entry. Filled by the runtime before an executor
    //                runs; an executor reads its own slice as
    //                arg_values[node.args_offset .. +node.args_count).
    // - `dt`         seconds since the previous tick.
    struct TickContext
    {
        engine::ecs::World &world;
        const lucid::sema::Graph &graph;
        std::vector<engine::ecs::CellValue> &results;
        const std::vector<engine::ecs::CellValue> &arg_values;
        float dt;
    };

    // One executor per node type, indexed by NodeId. A NodeId is the
    // index of a node type in node_registry().nodeTypes.
    //
    // The executor reads its arguments from `ctx.arg_values`, does the
    // work against `ctx.world`, and — for a Value node — stores the
    // result in `ctx.results[node_index]`.
    //
    // The executor must not throw. A malformed argument is logged and
    // skipped.
    using NodeExecutor = void (*)(TickContext &ctx, std::uint32_t node_index);

    // The executor table. One entry per node type in
    // node_registry().nodeTypes, in the same order. A debug assertion
    // in GraphRuntime's constructor enforces the size match at startup.
    const NodeExecutor *node_executors();
    std::uint32_t node_executor_count();

} // namespace engine::graph