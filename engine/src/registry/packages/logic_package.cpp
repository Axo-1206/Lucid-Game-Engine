#include "engine/registry/packages/logic_package.h"
#include "engine/registry/package_builder.h"
#include "engine/ecs/cell_value.h"
#include "engine/graph/tick_context.h"
#include "engine/registry/default_registry.h"

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

        // ---- Argument lists ----

        static std::array<NodeArgInfo, 1> kBoolArgs = {{
            NodeArgInfo{"value", TypeId::primitive("bool")},
        }};

        static std::array<NodeArgInfo, 1> kBoolUnaryArgs = {{
            NodeArgInfo{"a", TypeId::primitive("bool")},
        }};

        static std::array<NodeArgInfo, 2> kBoolBinaryArgs = {{
            NodeArgInfo{"a", TypeId::primitive("bool")},
            NodeArgInfo{"b", TypeId::primitive("bool")},
        }};

        static std::array<NodeArgInfo, 2> kFloatCompareArgs = {{
            NodeArgInfo{"a", TypeId::primitive("float32")},
            NodeArgInfo{"b", TypeId::primitive("float32")},
        }};

        static std::array<NodeArgInfo, 1> kIfArgs = {{
            NodeArgInfo{"cond", TypeId::primitive("bool")},
        }};

        // ---- Node type table ----
        //
        // The order of this array is the order of NodeId values in
        // every .lucgraph compiled against this registry. Append-only.
        //
        // 7 nodes:
        //   0  Bool
        //   1  LessThan
        //   2  GreaterThan
        //   3  EqualTo
        //   4  And
        //   5  Not
        //   6  If
        //
        // The comparison nodes take float32 arguments and produce
        // bool. The boolean nodes take and produce bool. If is the
        // gating trigger.

        static const std::array<NodeTypeInfo, 7> kNodeTypes = {{
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
                ArenaSpan<NodeArgInfo>(kFloatCompareArgs.data(),
                                       kFloatCompareArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_GreaterThanNode",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kFloatCompareArgs.data(),
                                       kFloatCompareArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_EqualToNode",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kFloatCompareArgs.data(),
                                       kFloatCompareArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_AndNode",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kBoolBinaryArgs.data(),
                                       kBoolBinaryArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_NotNode",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kBoolUnaryArgs.data(),
                                       kBoolUnaryArgs.size()),
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

        static_assert(kNodeTypes.size() == 7,
                      "kNodeTypes must have exactly 7 entries");

        static const std::array<TriggerMode, 7> kTriggerModes = {{
            TriggerMode::NotATrigger, // Bool
            TriggerMode::NotATrigger, // LessThan
            TriggerMode::NotATrigger, // GreaterThan
            TriggerMode::NotATrigger, // EqualTo
            TriggerMode::NotATrigger, // And
            TriggerMode::NotATrigger, // Not
            TriggerMode::Gating,      // If
        }};

        // ---- CellValue helpers ----

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

        bool cell_as_bool(const engine::ecs::CellValue &v)
        {
            using Kind = engine::ecs::CellValue::Kind;
            switch (v.kind())
            {
            case Kind::Bool:
                return v.as_bool();
            case Kind::Int:
                return v.as_int() != 0;
            case Kind::Float:
                return v.as_float() != 0.0;
            default:
                return false;
            }
        }

        // ---- Executors ----

        void exec_bool(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            ctx.results[i] = ctx.arg_values[node.args_offset];
        }

        void exec_less_than(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const float a = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            const float b = cell_as_float(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_bool(a < b);
        }

        void exec_greater_than(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const float a = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            const float b = cell_as_float(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_bool(a > b);
        }

        void exec_equal_to(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const float a = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            const float b = cell_as_float(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_bool(a == b);
        }

        void exec_and(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const bool a = cell_as_bool(ctx.arg_values[node.args_offset + 0]);
            const bool b = cell_as_bool(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_bool(a && b);
        }

        void exec_not(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const bool a = cell_as_bool(ctx.arg_values[node.args_offset]);
            ctx.results[i] = engine::ecs::CellValue::from_bool(!a);
        }

        void exec_if(engine::graph::TickContext &, std::uint32_t) {}

        static const std::array<engine::graph::NodeExecutor, 7> kExecutors = {{
            exec_bool,
            exec_less_than,
            exec_greater_than,
            exec_equal_to,
            exec_and,
            exec_not,
            exec_if,
        }};

        static_assert(kExecutors.size() == kNodeTypes.size(),
                      "kExecutors must be parallel to kNodeTypes");
        static_assert(kTriggerModes.size() == kNodeTypes.size(),
                      "kTriggerModes must be parallel to kNodeTypes");

    } // namespace

    void LogicPackage::register_into(PackageBuilder &b)
    {
        for (std::size_t i = 0; i < kNodeTypes.size(); ++i)
        {
            b.add_node_type(kNodeTypes[i]);
            b.add_node_executor(kExecutors[i]);
            b.add_trigger_mode(kTriggerModes[i]);
        }

        b.add_node_editor_info("Engine_BoolNode", NodeEditorInfo{
                                                      "Bool", "A boolean constant.", "", "bool",
                                                      "node c = Engine_BoolNode(true)"});
        b.add_node_editor_info("Engine_LessThanNode", NodeEditorInfo{
                                                          "Less Than", "True if a < b.", "", "less",
                                                          "node c = Engine_LessThanNode(a, b)"});
        b.add_node_editor_info("Engine_GreaterThanNode", NodeEditorInfo{
                                                             "Greater Than", "True if a > b.", "", "greater",
                                                             "node c = Engine_GreaterThanNode(a, b)"});
        b.add_node_editor_info("Engine_EqualToNode", NodeEditorInfo{
                                                         "Equal To", "True if a == b.", "", "equal",
                                                         "node c = Engine_EqualToNode(a, b)"});
        b.add_node_editor_info("Engine_AndNode", NodeEditorInfo{
                                                     "And", "True if both a and b.", "", "and",
                                                     "node c = Engine_AndNode(a, b)"});
        b.add_node_editor_info("Engine_NotNode", NodeEditorInfo{
                                                     "Not", "True if a is false.", "", "not",
                                                     "node c = Engine_NotNode(a)"});
        b.add_node_editor_info("Engine_IfNode", NodeEditorInfo{
                                                    "If", "Fires its subscribers when the condition is true.", "", "if",
                                                    "node gate = Engine_IfNode(cond) on parent"});
    }

} // namespace engine