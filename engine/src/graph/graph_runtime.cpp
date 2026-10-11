#include "engine/graph/graph_runtime.h"
#include "engine/graph/node_dispatch.h"
#include "engine/core/log.h"
#include "engine/nodes/node_registry.h"
#include "engine/scene/component_tags.h"
#include "engine/core/diagnostic_log.h"

#include "serialization/Deserializer.hpp"

#include <cassert>
#include <fstream>

namespace engine::graph
{

    GraphRuntime::GraphRuntime(engine::ecs::World &world,
                               const lucid::sema::Registry &registry)
        : world_(world), registry_(registry), asset_root_(std::filesystem::current_path())
    {
        // The executor table and the node registry must have the same
        // length and the same order. This is the engine's private
        // contract with itself; a mismatch is a programming error.
        assert(node_executor_count() == registry_.nodeTypes.size() && "node_dispatch table size must equal node registry size");
    }

    void GraphRuntime::set_asset_root(const std::filesystem::path &root)
    {
        asset_root_ = root;
    }

    namespace
    {

        std::vector<std::uint8_t> read_file(const std::filesystem::path &path,
                                            bool &ok)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in.is_open())
            {
                engine::log_error("GraphRuntime: cannot open '%s'", path.string().c_str());
                ok = false;
                return {};
            }
            in.seekg(0, std::ios::end);
            const auto size = in.tellg();
            in.seekg(0, std::ios::beg);
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
            in.read(reinterpret_cast<char *>(bytes.data()), size);
            ok = true;
            return bytes;
        }

    } // namespace

    bool GraphRuntime::load()
    {
        clear();

        auto *script_tbl = world_.get_table(ScriptTag::name);
        if (!script_tbl)
        {
            // No Script table means the scene loader hasn't populated
            // the world, or the world is empty. Either way, nothing to
            // load. Not an error.
            engine::log_info("GraphRuntime: no Script table; no graphs to load.");
            return true;
        }

        bool all_ok = true;

        script_tbl->each([&](engine::ecs::RowRef script_ref)
                         {
        const std::string rel = script_tbl->get_string(script_ref, "graph");
        if (rel.empty()) return;

        const std::filesystem::path full = asset_root_ / rel;

        bool read_ok = false;
        std::vector<std::uint8_t> bytes = read_file(full, read_ok);
        if (!read_ok) {
            all_ok = false;
            return;
        }

        auto dr = lucid::serialization::deserialize(
            lucid::serialization::ByteSpan(bytes), registry_);
        if (!dr.ok)
        {
            engine::log_error("GraphRuntime: failed to deserialize '%s'",
                              full.string().c_str());
            engine::log_diagnostics(dr.diagnostics);
            all_ok = false;
            return;
        }

        LoadedGraph lg;
        lg.source_path = full.string();
        lg.graph = std::move(*dr.graph);
        lg.results.assign(lg.graph.nodes.size(), 0.0f);
        graphs_.push_back(std::move(lg));

        engine::log_info("GraphRuntime: loaded '%s' (%zu nodes)",
                         full.string().c_str(),
                         graphs_.back().graph.nodes.size()); });

        engine::log_info("GraphRuntime: %zu graph(s) loaded.", graphs_.size());
        return all_ok;
    }

    void GraphRuntime::tick(float dt)
    {
        const NodeExecutor *executors = node_executors();
        const std::uint32_t executor_count = node_executor_count();

        for (auto &lg : graphs_)
        {
            for (lucid::sema::NodeIndex node_idx : lg.graph.phase_order)
            {
                const auto &node = lg.graph.nodes[node_idx];
                const std::uint32_t type_id = node.type_id;

                if (type_id >= executor_count)
                {
                    engine::log_error(
                        "GraphRuntime: node %u has type_id %u out of range "
                        "(executor table has %u entries)",
                        static_cast<unsigned>(node_idx),
                        static_cast<unsigned>(type_id),
                        static_cast<unsigned>(executor_count));
                    continue;
                }

                executors[type_id](world_, lg.graph,
                                   static_cast<std::uint32_t>(node_idx),
                                   lg.results, dt);
            }
        }
    }

    void GraphRuntime::clear()
    {
        graphs_.clear();
    }

} // namespace engine::graph