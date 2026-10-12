#include "engine/graph/graph_runtime.h"
#include "engine/graph/tick_context.h" // TickContext, NodeExecutor
#include "engine/core/format_diagnostics.h"
#include "engine/core/log.h"
#include "engine/scene/component_tags.h"

#include "sema/Literal.hpp"
#include "serialization/Deserializer.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>

namespace engine::graph
{

    GraphRuntime::GraphRuntime(engine::ecs::World &world,
                               const engine::PackageManager &manager)
        : world_(world), manager_(manager), asset_root_(std::filesystem::current_path())
    {
        assert(manager_.applied() && "GraphRuntime: PackageManager must be applied before use");
        assert(manager_.node_executors().size() == manager_.registry().nodeTypes.size() && "GraphRuntime: manager executor count must match node type count");
        assert(manager_.trigger_modes().size() == manager_.registry().nodeTypes.size() && "GraphRuntime: manager trigger mode count must match node type count");
    }

    void GraphRuntime::set_asset_root(const std::filesystem::path &root)
    {
        asset_root_ = root;
    }

    // =============================================================================
    // Literal -> CellValue
    // =============================================================================

    namespace
    {

        engine::ecs::CellValue literal_to_cell_value(
            const lucid::sema::Literal &lit,
            const lucid::sema::Graph &g)
        {

            using LitKind = lucid::sema::Literal::Kind;
            using CV = engine::ecs::CellValue;

            switch (lit.kind)
            {
            case LitKind::Nil:
                return CV::nil();
            case LitKind::Bool:
                return CV::from_bool(lit.b);
            case LitKind::Char:
                return CV::from_char(lit.c);
            case LitKind::Int8:
            case LitKind::Int16:
            case LitKind::Int32:
            case LitKind::Int64:
                return CV::from_int(lit.i);
            case LitKind::UInt8:
            case LitKind::UInt16:
            case LitKind::UInt32:
            case LitKind::UInt64:
                return CV::from_int(static_cast<std::int64_t>(lit.u));
            case LitKind::Float32:
            case LitKind::Float64:
                return CV::from_float(lit.f);
            case LitKind::String:
            {
                const auto off = lit.string.offset;
                const auto len = lit.string.length;
                if (off + len > g.string_pool.size())
                {
                    engine::log_error(
                        "GraphRuntime: string literal out of pool bounds");
                    return CV::nil();
                }
                return CV::from_string(std::string(
                    g.string_pool.data() + off, len));
            }
            }
            return CV::nil();
        }

        engine::ecs::CellValue resolve_resource_arg(
            const lucid::sema::Graph &g,
            const lucid::sema::Arg &arg)
        {

            const auto &ref = arg.resource_ref;

            if (ref.resource_index >= g.resources.size())
            {
                engine::log_error("GraphRuntime: resource index out of range");
                return engine::ecs::CellValue::nil();
            }
            const auto &res = g.resources[ref.resource_index];
            if (ref.field_index >= res.fields_count)
            {
                engine::log_error("GraphRuntime: field index out of range");
                return engine::ecs::CellValue::nil();
            }
            const auto &field =
                g.resource_fields[res.fields_offset + ref.field_index];
            return literal_to_cell_value(field.value, g);
        }

        engine::ecs::CellValue resolve_arg(
            const lucid::sema::Graph &g,
            const lucid::sema::Arg &arg,
            const std::vector<engine::ecs::CellValue> &results)
        {

            using Arg = lucid::sema::Arg;
            switch (arg.kind)
            {
            case Arg::Kind::Literal:
                return literal_to_cell_value(arg.literal, g);
            case Arg::Kind::NodeRef:
            {
                const auto idx = arg.node_ref;
                if (idx >= results.size())
                {
                    engine::log_error("GraphRuntime: NodeRef out of range");
                    return engine::ecs::CellValue::nil();
                }
                return results[idx];
            }
            case Arg::Kind::ResourceRef:
                return resolve_resource_arg(g, arg);
            }
            return engine::ecs::CellValue::nil();
        }

        void resolve_args_of(std::uint32_t node_index,
                             LoadedGraph &lg,
                             TickContext &ctx)
        {
            const auto &node = lg.graph.nodes[node_index];
            for (std::uint32_t i = 0; i < node.args_count; ++i)
            {
                const auto &arg = lg.graph.args[node.args_offset + i];
                lg.arg_values[node.args_offset + i] =
                    resolve_arg(lg.graph, arg, lg.results);
            }
        }

    } // namespace

    // =============================================================================
    // Firing and ticking
    // =============================================================================

    namespace
    {

        // One runtime's per-tick state. Bundles the manager-derived tables
        // so fire_node and tick_one_graph don't need to look them up on
        // every call.
        struct TickState
        {
            const engine::graph::NodeExecutor *executors = nullptr;
            std::uint32_t executor_count = 0;
            const std::vector<engine::NodeTypeRuntimeInfo> *trigger_modes = nullptr;
        };

        void fire_node(LoadedGraph &lg,
                       TickContext &ctx,
                       const TickState &state,
                       std::uint32_t node_index)
        {

            if (lg.fired[node_index])
                return;
            lg.fired[node_index] = true;

            const auto &node = lg.graph.nodes[node_index];
            const auto &info = (*state.trigger_modes)[node.type_id];

            if (info.mode == engine::TriggerMode::Gating)
            {
                if (node.args_count == 0)
                {
                    engine::log_error(
                        "GraphRuntime: gating trigger node %u has no condition",
                        static_cast<unsigned>(node_index));
                    return;
                }
                resolve_args_of(node_index, lg, ctx);
                const auto &cond = lg.arg_values[node.args_offset];
                if (cond.kind() != engine::ecs::CellValue::Kind::Bool)
                {
                    engine::log_error(
                        "GraphRuntime: gating trigger node %u condition is not bool",
                        static_cast<unsigned>(node_index));
                    return;
                }
                if (!cond.as_bool())
                    return;
            }

            const auto *subs = lg.graph.subscribers.data() + node.subscribers_offset;
            for (std::uint32_t i = 0; i < node.subscribers_count; ++i)
            {
                fire_node(lg, ctx, state, subs[i]);
            }
        }

        void tick_one_graph(engine::ecs::World &world,
                            LoadedGraph &lg,
                            float dt,
                            const TickState &state)
        {

            std::fill(lg.fired.begin(), lg.fired.end(), false);
            std::fill(lg.results.begin(), lg.results.end(),
                      engine::ecs::CellValue::nil());

            TickContext ctx{world, lg.graph, lg.results, lg.arg_values, dt};

            // 1. Evaluate Value nodes in value_order.
            for (lucid::sema::NodeIndex node_idx : lg.graph.value_order)
            {
                const auto &node = lg.graph.nodes[node_idx];
                if (node.type_id >= state.executor_count)
                {
                    engine::log_error(
                        "GraphRuntime: node %u type_id %u out of range",
                        static_cast<unsigned>(node_idx),
                        static_cast<unsigned>(node.type_id));
                    continue;
                }
                resolve_args_of(static_cast<std::uint32_t>(node_idx), lg, ctx);
                state.executors[node.type_id](ctx, static_cast<std::uint32_t>(node_idx));
            }

            // 2. Fire source triggers.
            for (std::uint32_t node_idx = 0;
                 node_idx < lg.graph.nodes.size();
                 ++node_idx)
            {
                const auto &node = lg.graph.nodes[node_idx];
                if ((*state.trigger_modes)[node.type_id].mode == engine::TriggerMode::Source)
                {
                    fire_node(lg, ctx, state, node_idx);
                }
            }

            // 3. Run fired Actions in phase order.
            for (lucid::sema::NodeIndex node_idx : lg.graph.phase_order)
            {
                if (!lg.fired[node_idx])
                    continue;
                const auto &node = lg.graph.nodes[node_idx];
                if (node.type_id >= state.executor_count)
                    continue;
                resolve_args_of(static_cast<std::uint32_t>(node_idx), lg, ctx);
                state.executors[node.type_id](ctx, static_cast<std::uint32_t>(node_idx));
            }
        }

    } // namespace

    // =============================================================================
    // File I/O and public API
    // =============================================================================

    namespace
    {

        std::vector<std::uint8_t> read_file(const std::filesystem::path &path,
                                            bool &ok)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in.is_open())
            {
                engine::log_error("GraphRuntime: cannot open '%s'",
                                  path.string().c_str());
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
        if (!read_ok) { all_ok = false; return; }

        auto dr = lucid::serialization::deserialize(
            lucid::serialization::ByteSpan(bytes), manager_.registry());
        if (!dr.ok) {
            engine::log_error("GraphRuntime: failed to deserialize '%s'",
                              full.string().c_str());
            engine::log_diagnostics(dr.diagnostics);
            all_ok = false;
            return;
        }

        LoadedGraph lg;
        lg.source_path = full.string();
        lg.graph = std::move(*dr.graph);
        lg.results.assign(lg.graph.nodes.size(),
                          engine::ecs::CellValue::nil());
        lg.arg_values.assign(lg.graph.args.size(),
                             engine::ecs::CellValue::nil());
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
        // Build the per-tick state once; every graph uses the same
        // executors and trigger modes.
        TickState state;
        state.executors = manager_.node_executors().data();
        state.executor_count = static_cast<std::uint32_t>(
            manager_.node_executors().size());
        state.trigger_modes = &manager_.trigger_modes();

        for (auto &lg : graphs_)
        {
            tick_one_graph(world_, lg, dt, state);
        }
    }

    void GraphRuntime::clear()
    {
        graphs_.clear();
    }

} // namespace engine::graph