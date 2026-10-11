#pragma once

#include "engine/ecs/world.h"
#include "sema/Graph.hpp"
#include "sema/Registry.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace engine::graph
{

    // A compiled graph loaded from a .lucgraph file, plus the
    // per-tick scratch space its executors use.
    struct LoadedGraph
    {
        // Where the graph came from, for diagnostics.
        std::string source_path;

        // The deserialized graph. `lucid::sema::Graph` is movable, so
        // LoadedGraph is movable.
        lucid::sema::Graph graph;

        // Per-node result scratch. Sized to graph.nodes.size() at load
        // time, reused every tick. Value nodes write here; Action and
        // Trigger nodes read from here when their arguments reference
        // other nodes.
        std::vector<float> results;
    };

    // The graph runtime: loads .lucgraph files referenced by the
    // world's Script table, and ticks them against the world.
    //
    // Lifetime:
    //   - Construct after the world has core tables. GraphRuntime
    //     does not create tables.
    //   - Call load(world) after the scene has been applied to the
    //     world. load() reads the world's Script table; if the scene
    //     has not been applied yet, the Script table is empty.
    //   - Call tick(dt) once per frame, before the systems.
    //   - Call clear() before destroying the world, if you want to
    //     release the graphs eagerly.
    //
    // GraphRuntime does not own the world; it holds a reference.
    // GraphRuntime does not own the registry; node_registry() returns
    // a process-lifetime reference.
    class GraphRuntime
    {
    public:
        GraphRuntime(engine::ecs::World &world,
                     const lucid::sema::Registry &registry);

        // Non-copyable (holds a reference to the world); movable.
        GraphRuntime(const GraphRuntime &) = delete;
        GraphRuntime &operator=(const GraphRuntime &) = delete;
        GraphRuntime(GraphRuntime &&) noexcept = default;
        GraphRuntime &operator=(GraphRuntime &&) noexcept = default;

        // Walk the world's Script table and load each referenced
        // .lucgraph file. Returns false if any load failed; a failed
        // graph is skipped and the others are loaded. Logs each error.
        bool load();

        // Tick every loaded graph. For each graph, walk
        // graph.phase_order and dispatch each node's executor.
        void tick(float dt);

        // Release all loaded graphs and their scratch space.
        void clear();

        std::size_t graph_count() const { return graphs_.size(); }
        const std::vector<LoadedGraph> &graphs() const { return graphs_; }

        // The asset root used to resolve Script.graph paths. Defaults
        // to the process's current working directory. Set before
        // calling load().
        void set_asset_root(const std::filesystem::path &root);
        const std::filesystem::path &asset_root() const { return asset_root_; }

    private:
        engine::ecs::World &world_;
        const lucid::sema::Registry &registry_;
        std::filesystem::path asset_root_;
        std::vector<LoadedGraph> graphs_;
    };

} // namespace engine::graph