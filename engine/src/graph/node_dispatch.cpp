#include "engine/graph/node_dispatch.h"
#include "engine/core/log.h"
#include "engine/nodes/node_registry.h"

#include <cassert>

namespace engine::graph
{

    namespace
    {

        // -----------------------------------------------------------------------------
        // Helpers
        // -----------------------------------------------------------------------------
        //
        // The graph's arguments are stored flat, and each node slices its own
        // with (offset, count). These helpers read the common shapes.

        const lucid::sema::Arg *args_of(const lucid::sema::Graph &g,
                                        std::uint32_t node_index)
        {
            const auto &node = g.nodes[node_index];
            return g.args.data() + node.args_offset;
        }

        std::uint32_t arg_count_of(const lucid::sema::Graph &g,
                                   std::uint32_t node_index)
        {
            return g.nodes[node_index].args_count;
        }

        // Read the float value of an argument. If the arg is a literal,
        // use its literal value. If it's a node reference, read the
        // referenced node's result from `results`. If it's a resource
        // reference, this Phase 9 runtime does not support it; log and
        // return 0.
        float arg_as_float(const lucid::sema::Graph &g,
                           const lucid::sema::Arg &arg,
                           const std::vector<float> &results)
        {
            using Arg = lucid::sema::Arg;
            switch (arg.kind)
            {
            case Arg::Kind::Literal:
                // Literal carries either a float or an int; the
                // executor for the node knows which. Use the float
                // field for both for now.
                return arg.literal.f;
            case Arg::Kind::NodeRef:
                return results[arg.node_ref];
            case Arg::Kind::ResourceRef:
                // Not supported in Phase 9. A future phase adds a
                // resource-to-value conversion path.
                return 0.0f;
            }
            return 0.0f;
        }

        // Read the int value of an argument. See arg_as_float.
        std::int64_t arg_as_int(const lucid::sema::Graph &g,
                                const lucid::sema::Arg &arg,
                                const std::vector<float> &results)
        {
            using Arg = lucid::sema::Arg;
            switch (arg.kind)
            {
            case Arg::Kind::Literal:
                return arg.literal.i;
            case Arg::Kind::NodeRef:
                return static_cast<std::int64_t>(results[arg.node_ref]);
            case Arg::Kind::ResourceRef:
                return 0;
            }
            return 0;
        }

        // -----------------------------------------------------------------------------
        // Executors
        // -----------------------------------------------------------------------------

        // Float32Node(value: float32) -> float32
        void exec_float32(engine::ecs::World &,
                          const lucid::sema::Graph &g,
                          std::uint32_t node_index,
                          std::vector<float> &results,
                          float)
        {
            const auto *args = args_of(g, node_index);
            results[node_index] = args[0].literal.f;
        }

        // Int32Node(value: int32) -> int32
        void exec_int32(engine::ecs::World &,
                        const lucid::sema::Graph &g,
                        std::uint32_t node_index,
                        std::vector<float> &results,
                        float)
        {
            const auto *args = args_of(g, node_index);
            results[node_index] = static_cast<float>(args[0].literal.i);
        }

        // EveryFrame() — source trigger. The runtime fires it directly;
        // the executor does nothing. Subscribers are dispatched by the
        // runtime's trigger logic, which Phase 9 does not yet implement.
        void exec_every_frame(engine::ecs::World &,
                              const lucid::sema::Graph &,
                              std::uint32_t,
                              std::vector<float> &,
                              float)
        {
            // No-op. See the note in graph_runtime.cpp about trigger
            // dispatch being a Phase 9.5 addition.
        }

        // PrintNode(value: float32) — action.
        void exec_print(engine::ecs::World &,
                        const lucid::sema::Graph &g,
                        std::uint32_t node_index,
                        std::vector<float> &results,
                        float)
        {
            const auto *args = args_of(g, node_index);
            const float v = arg_as_float(g, args[0], results);
            engine::log_info("[graph] PrintNode: %f", static_cast<double>(v));
        }

        // BoolNode(value: bool) -> bool (stored as 0.0 or 1.0)
        void exec_bool(engine::ecs::World &,
                       const lucid::sema::Graph &g,
                       std::uint32_t node_index,
                       std::vector<float> &results,
                       float)
        {
            const auto *args = args_of(g, node_index);
            // Literal::Kind::Bool uses .b (or .i != 0). Checking .b != false or .i != 0.
            results[node_index] = (args[0].literal.kind == lucid::sema::Literal::Kind::Bool
                                       ? args[0].literal.b
                                       : args[0].literal.i != 0)
                                      ? 1.0f
                                      : 0.0f;
        }

        // LessThanNode(a: float32, b: float32) -> bool
        void exec_less_than(engine::ecs::World &,
                            const lucid::sema::Graph &g,
                            std::uint32_t node_index,
                            std::vector<float> &results,
                            float)
        {
            const auto *args = args_of(g, node_index);
            const float a = arg_as_float(g, args[0], results);
            const float b = arg_as_float(g, args[1], results);
            results[node_index] = (a < b) ? 1.0f : 0.0f;
        }

        // IfNode(cond: bool) — gating trigger. Handled in fire_node.
        void exec_if(engine::ecs::World &,
                     const lucid::sema::Graph &,
                     std::uint32_t,
                     std::vector<float> &,
                     float)
        {
            // Gating triggers do not run. Firing is handled by fire_node,
            // which evaluates the condition (this node's first argument)
            // before propagating. See graph_runtime.cpp.
        }

        // -----------------------------------------------------------------------------
        // The table
        // -----------------------------------------------------------------------------
        //
        // One executor per node type in node_registry().nodeTypes, in the
        // same order. If you add a node type, add its executor here, at the
        // same index.

        constexpr NodeExecutor kExecutors[] = {
            exec_float32,     // 0: Float32Node
            exec_int32,       // 1: Int32Node
            exec_every_frame, // 2: EveryFrame
            exec_print,       // 3: PrintNode
            exec_bool,        // 4: BoolNode
            exec_less_than,   // 5: LessThanNode
            exec_if,          // 6: IfNode
        };

    } // namespace

    const NodeExecutor *node_executors()
    {
        return kExecutors;
    }

    std::uint32_t node_executor_count()
    {
        return static_cast<std::uint32_t>(sizeof(kExecutors) / sizeof(kExecutors[0]));
    }

} // namespace engine::graph