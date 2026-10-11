#include "engine/graph/graph_runtime.h"
#include "engine/graph/node_dispatch.h"
#include "engine/core/diagnostic_log.h"
#include "engine/core/log.h"
#include "engine/nodes/node_registry.h"
#include "engine/nodes/node_runtime.h"
#include "engine/scene/component_tags.h"

#include "sema/Literal.hpp"
#include "serialization/Deserializer.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>

namespace engine::graph
{

    // =============================================================================
    // Construction
    // =============================================================================

    GraphRuntime::GraphRuntime(engine::ecs::World &world,
                               const lucid::sema::Registry &registry)
        : world_(world), registry_(registry), asset_root_(std::filesystem::current_path())
    {
        assert(node_executor_count() == registry_.nodeTypes.size() && "node_dispatch table size must equal node registry size");
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

        // Convert a format-library Literal to an engine CellValue.
        //
        // The two type systems have different granularity. Literal has 14
        // kinds (sized integers and floats, char, string). CellValue has 7
        // (Bool, Int, Float, String, Nil, RowRef, Callback). The widths
        // collapse: Int8..Int64 all become CellValue::Int; Float32 and
        // Float64 both become CellValue::Float. The value survives; the
        // specific width does not. That is fine, because the executor for
        // a node type is written against the type the registry declares
        // for that argument, and reads the CellValue accordingly.
        //
        // A String literal stores (offset, length) into the graph's string
        // pool. Reconstruct the bytes; bounds-check against the pool.
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
                // The union stores unsigned in `u`. CellValue stores
                // integers as int64_t. A uint64 above 2^63 wraps; for
                // values below 2^63 this is exact.
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
                        "GraphRuntime: string literal (off=%u len=%u) "
                        "out of pool bounds (%zu)",
                        static_cast<unsigned>(off),
                        static_cast<unsigned>(len),
                        g.string_pool.size());
                    return CV::nil();
                }
                return CV::from_string(std::string(
                    g.string_pool.data() + off, len));
            }
            }
            return CV::nil();
        }

        // =============================================================================
        // Argument resolution
        // =============================================================================

        // Read a resource field's value.
        //
        // Resources are compile-time constants. The library's
        // zeroLiteralOf resolves a field whose source omitted `= value`
        // to the type's zero; the graph carries only values, no
        // hasDefault flag. There is no branch here.
        engine::ecs::CellValue resolve_resource_arg(
            const lucid::sema::Graph &g,
            const lucid::sema::Arg &arg)
        {

            const auto &ref = arg.resource_ref;

            if (ref.resource_index >= g.resources.size())
            {
                engine::log_error(
                    "GraphRuntime: resource index %u out of range",
                    static_cast<unsigned>(ref.resource_index));
                return engine::ecs::CellValue::nil();
            }

            const auto &res = g.resources[ref.resource_index];

            if (ref.field_index >= res.fields_count)
            {
                engine::log_error(
                    "GraphRuntime: field index %u out of range for resource %u",
                    static_cast<unsigned>(ref.field_index),
                    static_cast<unsigned>(ref.resource_index));
                return engine::ecs::CellValue::nil();
            }

            const auto &field =
                g.resource_fields[res.fields_offset + ref.field_index];

            return literal_to_cell_value(field.value, g);
        }

        // Resolve one argument to a concrete CellValue.
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
                    engine::log_error(
                        "GraphRuntime: NodeRef %u out of range",
                        static_cast<unsigned>(idx));
                    return engine::ecs::CellValue::nil();
                }
                return results[idx];
            }

            case Arg::Kind::ResourceRef:
                return resolve_resource_arg(g, arg);
            }

            return engine::ecs::CellValue::nil();
        }

        // Fill ctx.arg_values for one node's arguments.
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
    // Firing
    // =============================================================================

    namespace
    {

        // Fire a node: mark it fired, and if it is a gating trigger with a
        // true condition, propagate to its subscribers.
        void fire_node(LoadedGraph &lg,
                       TickContext &ctx,
                       std::uint32_t node_index)
        {

            // A node fires at most once per tick. TriggerModel.md §6.
            if (lg.fired[node_index])
                return;
            lg.fired[node_index] = true;

            const auto &node = lg.graph.nodes[node_index];
            const auto &info = engine::node_runtime_info(node.type_id);

            if (info.mode == engine::TriggerMode::Gating)
            {
                if (node.args_count == 0)
                {
                    engine::log_error(
                        "GraphRuntime: gating trigger node %u has no condition argument",
                        static_cast<unsigned>(node_index));
                    return;
                }

                // Resolve the gate's arguments; the first is the condition.
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
                {
                    return; // gate did not pass; do not propagate
                }
            }

            // Propagate to subscribers.
            const auto *subs = lg.graph.subscribers.data() + node.subscribers_offset;
            for (std::uint32_t i = 0; i < node.subscribers_count; ++i)
            {
                fire_node(lg, ctx, subs[i]);
            }
        }

        // Tick one graph: reset state, evaluate values, fire triggers,
        // run fired actions in phase order.
        void tick_one_graph(engine::ecs::World &world,
                            LoadedGraph &lg,
                            float dt)
        {

            // 1. Reset per-tick state.
            std::fill(lg.fired.begin(), lg.fired.end(), false);
            std::fill(lg.results.begin(), lg.results.end(),
                      engine::ecs::CellValue::nil());

            TickContext ctx{world, lg.graph, lg.results, lg.arg_values, dt};

            const NodeExecutor *executors = node_executors();
            const std::uint32_t executor_count = node_executor_count();

            // 2. Evaluate Value nodes in value_order.
            //
            //    value_order is a topological order: a Value node's
            //    NodeRef arguments reference earlier Value nodes (or
            //    literals or resources). Evaluating in this order means
            //    each node's arguments are already resolved.
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
                resolve_args_of(static_cast<std::uint32_t>(node_idx), lg, ctx);
                executors[node.type_id](ctx, static_cast<std::uint32_t>(node_idx));
            }

            // 3. Fire source triggers.
            for (std::uint32_t node_idx = 0;
                 node_idx < lg.graph.nodes.size();
                 ++node_idx)
            {
                const auto &node = lg.graph.nodes[node_idx];
                const auto &info = engine::node_runtime_info(node.type_id);
                if (info.mode == engine::TriggerMode::Source)
                {
                    fire_node(lg, ctx, node_idx);
                }
            }

            // 4. Run fired Actions in phase order.
            for (lucid::sema::NodeIndex node_idx : lg.graph.phase_order)
            {
                if (!lg.fired[node_idx])
                    continue;

                const auto &node = lg.graph.nodes[node_idx];
                if (node.type_id >= executor_count)
                    continue;

                resolve_args_of(static_cast<std::uint32_t>(node_idx), lg, ctx);
                executors[node.type_id](ctx, static_cast<std::uint32_t>(node_idx));
            }
        }

    } // namespace

    // =============================================================================
    // File I/O
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

    // =============================================================================
    // Public API
    // =============================================================================

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
        if (!read_ok) {
            all_ok = false;
            return;
        }

        auto dr = lucid::serialization::deserialize(
            lucid::serialization::ByteSpan(bytes), registry_);
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