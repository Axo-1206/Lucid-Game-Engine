#pragma once

#include "engine/ecs/cell_value.h"
#include "engine/ecs/world.h"
#include "sema/Graph.hpp"
#include "sema/Registry.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace engine::graph
{

    // A compiled graph loaded from a .lucgraph file, plus the
    // per-tick scratch its executors use.
    struct LoadedGraph
    {
        // Where the graph came from, for diagnostics.
        std::string source_path;

        // The deserialized graph. `lucid::sema::Graph` is movable.
        lucid::sema::Graph graph;

        // One CellValue per node. A Value node's executor writes its
        // result here; an Action or Trigger reads from here when its
        // arguments reference other nodes. Reset to nil at the start
        // of each tick.
        std::vector<engine::ecs::CellValue> results;

        // One CellValue per graph.args entry. Filled per tick before a
        // node's executor runs. Indexed by (node.args_offset + i).
        std::vector<engine::ecs::CellValue> arg_values;

        // One bool per node. Set true as firing propagates this tick.
        // Reset at the start of each tick. A node fires at most once
        // per tick; see docs/notes/TriggerModel.md §6.
        std::vector<bool> fired;
    };

    // The graph runtime: loads .lucgraph files referenced by the
    // world's Script table, and ticks them against the world.
    //
    // Lifetime:
    //   - Construct after the world has core tables.
    //   - Call load() after the scene has been applied to the world.
    //     load() reads the world's Script table.
    //   - Call tick(dt) once per frame, before the systems.
    //   - Call clear() before destroying the world.
    //
    // GraphRuntime does not own the world; it holds a reference.
    // GraphRuntime does not own the registry; node_registry() returns
    // a process-lifetime reference.
    class GraphRuntime
    {
    public:
        GraphRuntime(engine::ecs::World &world,
                     const lucid::sema::Registry &registry);

        GraphRuntime(const GraphRuntime &) = delete;
        GraphRuntime &operator=(const GraphRuntime &) = delete;
        GraphRuntime(GraphRuntime &&) noexcept = default;
        GraphRuntime &operator=(GraphRuntime &&) noexcept = default;

        // Walk the world's Script table and load each referenced
        // .lucgraph file. Returns false if any load failed; a failed
        // graph is skipped and the others are loaded. Logs each error.
        bool load();

        // Tick every loaded graph.
        void tick(float dt);

        // Release all loaded graphs and their scratch.
        void clear();

        std::size_t graph_count() const { return graphs_.size(); }
        const std::vector<LoadedGraph> &graphs() const { return graphs_; }

        // The asset root used to resolve Script.graph paths. Defaults
        // to the process's current working directory. Set before load().
        void set_asset_root(const std::filesystem::path &root);
        const std::filesystem::path &asset_root() const { return asset_root_; }

    private:
        engine::ecs::World &world_;
        const lucid::sema::Registry &registry_;
        std::filesystem::path asset_root_;
        std::vector<LoadedGraph> graphs_;
    };

} // namespace engine::graph