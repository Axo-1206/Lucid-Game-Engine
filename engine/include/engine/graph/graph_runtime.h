#pragma once

#include "engine/ecs/cell_value.h"
#include "engine/ecs/world.h"
#include "engine/registry/package_manager.h"
#include "engine/services.h"
#include "sema/Graph.hpp"
#include "sema/Registry.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace engine::graph
{

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

        // One byte buffer per node whose result type is a
        // struct. Empty for nodes whose result is a primitive, an
        // enum, a handle, or nothing. A node that produces a struct
        // writes the struct's bytes here; a node that consumes a
        // struct reads from the producing node's buffer.
        std::vector<std::vector<std::byte>> struct_results;

        // The size in bytes of each node's struct result, from
        // the registry's result type. 0 for non-struct results.
        std::vector<std::size_t> struct_result_sizes;
    };

    // The graph runtime: loads .lucgraph files referenced by the
    // world's Engine_Script table, and ticks them against the world.
    //
    // The runtime reads its node vocabulary from a PackageManager.
    // It reads its services (physics backend, etc.) from a Services
    // instance, passed to the constructor. The Services instance may
    // have null fields; an executor that needs a service checks.
    //
    // The manager and the services must outlive the GraphRuntime.
    // The runtime holds references.
    class GraphRuntime
    {
    public:
        GraphRuntime(engine::ecs::World &world,
                     const engine::PackageManager &manager,
                     engine::Services &services);

        GraphRuntime(const GraphRuntime &) = delete;
        GraphRuntime &operator=(const GraphRuntime &) = delete;
        GraphRuntime(GraphRuntime &&) noexcept = default;
        GraphRuntime &operator=(GraphRuntime &&) noexcept = default;

        bool load();
        void tick(float dt);
        void clear();

        std::size_t graph_count() const { return graphs_.size(); }
        const std::vector<LoadedGraph> &graphs() const { return graphs_; }

        void set_asset_root(const std::filesystem::path &root);
        const std::filesystem::path &asset_root() const { return asset_root_; }

    private:
        engine::ecs::World &world_;
        const engine::PackageManager &manager_;
        engine::Services &services_;
        std::filesystem::path asset_root_;
        std::vector<LoadedGraph> graphs_;
    };

} // namespace engine::graph