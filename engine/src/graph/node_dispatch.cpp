#include "engine/graph/node_dispatch.h"
#include "engine/core/log.h"

namespace engine::graph
{

    namespace
    {

        // -----------------------------------------------------------------------------
        // Numeric coercion
        // -----------------------------------------------------------------------------
        //
        // A node type declares its argument types via the registry. The
        // executor for that node reads its arguments as the declared type.
        // In practice the only widths that matter at the argument level are
        // "float-like" and "int-like"; bools coerce to 0.0f / 1.0f.
        //
        // cell_as_float covers the cases an executor needs for a numeric
        // read. A String, RowRef, or Callback argument read as a float
        // yields 0.0f; that is a Sema bug if it happens, not a runtime one.

        float cell_as_float(const engine::ecs::CellValue &v)
        {
            using Kind = engine::ecs::CellValue::Kind;
            switch (v.kind())
            {
            case Kind::Float:
                return static_cast<float>(v.as_float());
            case Kind::Int:
                return static_cast<float>(v.as_int());
            case Kind::Bool:
                return v.as_bool() ? 1.0f : 0.0f;
            case Kind::Nil:
                return 0.0f;
            default:
                return 0.0f;
            }
        }

        // -----------------------------------------------------------------------------
        // Executors
        // -----------------------------------------------------------------------------
        //
        // A Value node reads its arguments from ctx.arg_values and writes
        // its result to ctx.results[node_index].
        //
        // A Trigger never reaches its executor: firing is handled by
        // fire_node in graph_runtime.cpp. Its table entry is a no-op.
        //
        // An Action reads its arguments and performs a side effect.

        // 0: Engine_Float32Node(value: float32) -> float32
        void exec_float32(TickContext &ctx, std::uint32_t node_index)
        {
            const auto &node = ctx.graph.nodes[node_index];
            ctx.results[node_index] = ctx.arg_values[node.args_offset];
        }

        // 1: Engine_Int32Node(value: int32) -> int32
        void exec_int32(TickContext &ctx, std::uint32_t node_index)
        {
            const auto &node = ctx.graph.nodes[node_index];
            ctx.results[node_index] = ctx.arg_values[node.args_offset];
        }

        // 2: Engine_EveryFrame() — source trigger. Never dispatched.
        void exec_every_frame(TickContext &, std::uint32_t) {}

        // 3: Engine_PrintNode(value: float32) — action.
        void exec_print(TickContext &ctx, std::uint32_t node_index)
        {
            const auto &node = ctx.graph.nodes[node_index];
            const float v = cell_as_float(ctx.arg_values[node.args_offset]);
            engine::log_info("[graph] Engine_PrintNode: %f", static_cast<double>(v));
        }

        // 4: Engine_BoolNode(value: bool) -> bool
        void exec_bool(TickContext &ctx, std::uint32_t node_index)
        {
            const auto &node = ctx.graph.nodes[node_index];
            ctx.results[node_index] = ctx.arg_values[node.args_offset];
        }

        // 5: Engine_LessThanNode(a: f32, b: f32) -> bool
        void exec_less_than(TickContext &ctx, std::uint32_t node_index)
        {
            const auto &node = ctx.graph.nodes[node_index];
            const float a = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            const float b = cell_as_float(ctx.arg_values[node.args_offset + 1]);
            ctx.results[node_index] = engine::ecs::CellValue::from_bool(a < b);
        }

        // 6: Engine_IfNode(cond: bool) — gating trigger. Never dispatched.
        void exec_if(TickContext &, std::uint32_t) {}

        // -----------------------------------------------------------------------------
        // The table
        // -----------------------------------------------------------------------------
        //
        // One executor per node type in node_registry().nodeTypes, in the
        // same order. If you add a node type, add its executor here, at the
        // same index.

        constexpr NodeExecutor kExecutors[] = {
            exec_float32,     // 0: Engine_Float32Node
            exec_int32,       // 1: Engine_Int32Node
            exec_every_frame, // 2: Engine_EveryFrame
            exec_print,       // 3: Engine_PrintNode
            exec_bool,        // 4: Engine_BoolNode
            exec_less_than,   // 5: Engine_LessThanNode
            exec_if,          // 6: Engine_IfNode
        };

    } // namespace

    const NodeExecutor *node_executors()
    {
        return kExecutors;
    }

    std::uint32_t node_executor_count()
    {
        return static_cast<std::uint32_t>(
            sizeof(kExecutors) / sizeof(kExecutors[0]));
    }

} // namespace engine::graph