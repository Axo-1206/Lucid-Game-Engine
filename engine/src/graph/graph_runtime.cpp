#include "engine/graph/graph_runtime.h"
#include "engine/graph/node_dispatch.h"
#include "engine/core/log.h"
#include "engine/nodes/node_registry.h"
#include "engine/nodes/node_runtime.h"
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

        // Evaluate a node's argument as a bool, for gating conditions.
        // The argument must be a NodeRef to a Value node whose result was
        // computed in the value-order pass, or a Literal.
        bool arg_as_bool(const lucid::sema::Graph &g,
                         const lucid::sema::Arg &arg,
                         const std::vector<float> &results)
        {
            using Arg = lucid::sema::Arg;
            switch (arg.kind)
            {
            case Arg::Kind::Literal:
                return (arg.literal.kind == lucid::sema::Literal::Kind::Bool)
                           ? arg.literal.b
                           : (arg.literal.i != 0);
            case Arg::Kind::NodeRef:
                return results[arg.node_ref] != 0.0f;
            case Arg::Kind::ResourceRef:
                // Not supported in Phase 9.5.
                return false;
            }
            return false;
        }

        void fire_node(LoadedGraph &lg, std::uint32_t node_index)
        {
            // A node fires at most once per tick. See TriggerModel.md §6.
            if (lg.fired[node_index])
                return;
            lg.fired[node_index] = true;

            const auto &node = lg.graph.nodes[node_index];
            const auto &info = engine::node_runtime_info(node.type_id);

            // Gating triggers check their condition before propagating.
            if (info.mode == engine::TriggerMode::Gating)
            {
                if (node.args_count == 0)
                {
                    engine::log_error(
                        "GraphRuntime: gating trigger node %u has no condition argument",
                        static_cast<unsigned>(node_index));
                    return;
                }
                const auto *args = lg.graph.args.data() + node.args_offset;
                if (!arg_as_bool(lg.graph, args[0], lg.results))
                    return;
            }

            // Propagate to subscribers.
            const auto *subs = lg.graph.subscribers.data() + node.subscribers_offset;
            for (std::uint32_t i = 0; i < node.subscribers_count; ++i)
            {
                fire_node(lg, subs[i]);
            }
        }

        void tick_one_graph(engine::ecs::World &world,
                            LoadedGraph &lg,
                            float dt)
        {
            // 1. Reset the per-tick fired flags.
            std::fill(lg.fired.begin(), lg.fired.end(), false);

            // 2. Evaluate Value nodes, in value_order.
            //
            //    Value nodes don't have side effects; they write their
            //    result to lg.results[node_index]. After this pass, every
            //    Value node's result is available for the firing pass.
            const NodeExecutor *executors = node_executors();
            const std::uint32_t executor_count = node_executor_count();

            for (lucid::sema::NodeIndex node_idx : lg.graph.value_order)
            {
                const auto &node = lg.graph.nodes[node_idx];
                if (node.type_id >= executor_count)
                {
                    engine::log_error(
                        "GraphRuntime: node %u has type_id %u out of range",
                        static_cast<unsigned>(node_idx),
                        static_cast<unsigned>(node.type_id));
                    continue;
                }
                executors[node.type_id](world, lg.graph,
                                        static_cast<std::uint32_t>(node_idx),
                                        lg.results, dt);
            }

            // 3. Fire source triggers.
            //
            //    A source trigger is any node whose runtime info says
            //    TriggerMode::Source. Firing it propagates through its
            //    subscribers, marking fired nodes and stopping at gates
            //    whose condition is false.
            for (std::uint32_t node_idx = 0;
                 node_idx < lg.graph.nodes.size();
                 ++node_idx)
            {
                const auto &node = lg.graph.nodes[node_idx];
                const auto &info = engine::node_runtime_info(node.type_id);
                if (info.mode == engine::TriggerMode::Source)
                {
                    fire_node(lg, node_idx);
                }
            }

            // 4. Run fired Actions in phase order.
            //
            //    Actions that were not fired this tick do not run. The
            //    phase_order list contains only Action nodes, sorted by
            //    phase; we filter it by the fired set.
            for (lucid::sema::NodeIndex node_idx : lg.graph.phase_order)
            {
                if (!lg.fired[node_idx])
                    continue;

                const auto &node = lg.graph.nodes[node_idx];
                if (node.type_id >= executor_count)
                {
                    continue; // already logged in the value pass
                }
                executors[node.type_id](world, lg.graph,
                                        static_cast<std::uint32_t>(node_idx),
                                        lg.results, dt);
            }
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
        lg.fired.assign(lg.graph.nodes.size(), false);
        graphs_.push_back(std::move(lg));

        engine::log_info("GraphRuntime: loaded '%s' (%zu nodes)",
                         full.string().c_str(),
                         graphs_.back().graph.nodes.size()); });

        engine::log_info("GraphRuntime: %zu graph(s) loaded.", graphs_.size());
        return all_ok;
    }

    void GraphRuntime::tick(float dt)
    {
        for (auto &lg : graphs_)
        {
            tick_one_graph(world_, lg, dt);
        }
    }

    void GraphRuntime::clear()
    {
        graphs_.clear();
    }

} // namespace engine::graph