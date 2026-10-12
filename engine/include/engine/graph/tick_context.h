#pragma once

#include "engine/ecs/cell_value.h"
#include "engine/ecs/world.h"
#include "engine/services.h"
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
    //                runs.
    // - `services`   the engine-wide services. May be null in a
    //                context that has no services (a unit test, a
    //                tool). An executor that needs a service checks
    //                for null.
    // - `dt`         seconds since the previous tick.
    struct TickContext
    {
        engine::ecs::World &world;
        const lucid::sema::Graph &graph;
        std::vector<engine::ecs::CellValue> &results;
        const std::vector<engine::ecs::CellValue> &arg_values;
        Services *services;
        float dt;
    };

    // One executor per node type, indexed by NodeId.
    using NodeExecutor = void (*)(TickContext &ctx, std::uint32_t node_index);

} // namespace engine::graph