#include "engine/package/logic_package.h"
#include "engine/package/package_builder.h"
#include "engine/ecs/cell_value.h"
#include "engine/graph/node_dispatch.h"
#include "engine/nodes/node_runtime.h"

#include "sema/NodeKind.hpp"
#include "sema/TypeId.hpp"
#include "core/memory/ArenaSpan.hpp"

#include <array>
#include <cstdint>

namespace engine
{

    namespace
    {

        using ::ArenaSpan;
        using namespace lucid::sema;

        static const std::array<NodeArgInfo, 1> kBoolArgs = {{
            NodeArgInfo{"value", TypeId::primitive("bool")},
        }};

        static const std::array<NodeArgInfo, 2> kLessThanArgs = {{
            NodeArgInfo{"a", TypeId::primitive("float32")},
            NodeArgInfo{"b", TypeId::primitive("float32")},
        }};

        static const std::array<NodeArgInfo, 1> kIfArgs = {{
            NodeArgInfo{"cond", TypeId::primitive("bool")},
        }};

        static const std::array<NodeTypeInfo, 3> kNodeTypes = {{
            NodeTypeInfo{
                "Engine_BoolNode",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kBoolArgs.data(), kBoolArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_LessThanNode",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kLessThanArgs.data(), kLessThanArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_IfNode",
                NodeKind::Trigger,
                "Flow",
                0,
                ArenaSpan<NodeArgInfo>(kIfArgs.data(), kIfArgs.size()),
                TypeId{},
            },
        }};

        static const std::array<TriggerMode, 3> kTriggerModes = {{
            TriggerMode::NotATrigger, // Engine_BoolNode
            TriggerMode::NotATrigger, // Engine_LessThanNode
            TriggerMode::Gating,      // Engine_IfNode
        }};

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
            default:
                return 0.0f;
            }
        }

        void exec_bool(engine::graph::TickContext &ctx, std::uint32_t node_index)
        {
            const auto &node = ctx.graph.nodes[node_index];
            ctx.results[node_index] = ctx.arg_values[node.args_offset];
        }

        void exec_less_than(engine::graph::TickContext &ctx, std::uint32_t node_index)
        {
            const auto &node = ctx.graph.nodes[node_index];
            const float a = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            const float b = cell_as_float(ctx.arg_values[node.args_offset + 1]);
            ctx.results[node_index] = engine::ecs::CellValue::from_bool(a < b);
        }

        void exec_if(engine::graph::TickContext &, std::uint32_t) {}

        static const std::array<engine::graph::NodeExecutor, 3> kExecutors = {{
            exec_bool,
            exec_less_than,
            exec_if,
        }};

    } // namespace

    void LogicPackage::register_into(PackageBuilder &b)
    {
        for (std::size_t i = 0; i < kNodeTypes.size(); ++i)
        {
            b.add_node_type(kNodeTypes[i]);
            b.add_node_executor(kExecutors[i]);
            b.add_trigger_mode(kTriggerModes[i]);
        }
    }

} // namespace engine