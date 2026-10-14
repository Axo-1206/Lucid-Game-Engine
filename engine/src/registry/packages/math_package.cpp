#include "engine/registry/packages/math_package.h"
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
        //
        // Each node type's arguments, positionally. The name is for
        // diagnostics only.

        static const std::array<NodeArgInfo, 2> kFloatBinaryArgs = {{
            NodeArgInfo{"a", TypeId::primitive("float32")},
            NodeArgInfo{"b", TypeId::primitive("float32")},
        }};

        static const std::array<NodeArgInfo, 1> kFloatUnaryArgs = {{
            NodeArgInfo{"a", TypeId::primitive("float32")},
        }};

        static const std::array<NodeArgInfo, 3> kClampArgs = {{
            NodeArgInfo{"v", TypeId::primitive("float32")},
            NodeArgInfo{"lo", TypeId::primitive("float32")},
            NodeArgInfo{"hi", TypeId::primitive("float32")},
        }};

        static const std::array<NodeArgInfo, 2> kIntBinaryArgs = {{
            NodeArgInfo{"a", TypeId::primitive("int32")},
            NodeArgInfo{"b", TypeId::primitive("int32")},
        }};

        static const std::array<NodeArgInfo, 2> kBoolBinaryArgs = {{
            NodeArgInfo{"a", TypeId::primitive("bool")},
            NodeArgInfo{"b", TypeId::primitive("bool")},
        }};

        static const std::array<NodeArgInfo, 1> kBoolUnaryArgs = {{
            NodeArgInfo{"a", TypeId::primitive("bool")},
        }};

        // ---- Node type table ----
        //
        // The order of this array is the order of NodeId values in
        // every .lucgraph compiled against this registry. Append-only.

        static const std::array<NodeTypeInfo, 16> kNodeTypes = {{
            // Float arithmetic.
            NodeTypeInfo{
                "Engine_AddFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kFloatBinaryArgs.data()),
                                       kFloatBinaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_SubFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kFloatBinaryArgs.data()),
                                       kFloatBinaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_MulFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kFloatBinaryArgs.data()),
                                       kFloatBinaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_DivFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kFloatBinaryArgs.data()),
                                       kFloatBinaryArgs.size()),
                TypeId::primitive("float32"),
            },
            // Float unary / clamping.
            NodeTypeInfo{
                "Engine_AbsFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kFloatUnaryArgs.data()),
                                       kFloatUnaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_NegFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kFloatUnaryArgs.data()),
                                       kFloatUnaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_MinFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kFloatBinaryArgs.data()),
                                       kFloatBinaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_MaxFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kFloatBinaryArgs.data()),
                                       kFloatBinaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_ClampFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kClampArgs.data()),
                                       kClampArgs.size()),
                TypeId::primitive("float32"),
            },
            // Int arithmetic.
            NodeTypeInfo{
                "Engine_AddIntNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kIntBinaryArgs.data()),
                                       kIntBinaryArgs.size()),
                TypeId::primitive("int32"),
            },
            NodeTypeInfo{
                "Engine_SubIntNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kIntBinaryArgs.data()),
                                       kIntBinaryArgs.size()),
                TypeId::primitive("int32"),
            },
            NodeTypeInfo{
                "Engine_MulIntNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kIntBinaryArgs.data()),
                                       kIntBinaryArgs.size()),
                TypeId::primitive("int32"),
            },
            // Comparison.
            NodeTypeInfo{
                "Engine_GreaterThanNode",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kFloatBinaryArgs.data()),
                                       kFloatBinaryArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_EqualToNode",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kFloatBinaryArgs.data()),
                                       kFloatBinaryArgs.size()),
                TypeId::primitive("bool"),
            },
            // Boolean logic.
            NodeTypeInfo{
                "Engine_AndNode",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kBoolBinaryArgs.data()),
                                       kBoolBinaryArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_NotNode",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kBoolUnaryArgs.data()),
                                       kBoolUnaryArgs.size()),
                TypeId::primitive("bool"),
            },
        }};

        // ---- Trigger modes ----
        //
        // Every node in this package is a Value node; none fires.

        static const std::array<TriggerMode, 16> kTriggerModes = {{
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
            TriggerMode::NotATrigger,
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

        std::int32_t cell_as_int(const engine::ecs::CellValue &v)
        {
            using Kind = engine::ecs::CellValue::Kind;
            switch (v.kind())
            {
            case Kind::Int:
                return static_cast<std::int32_t>(v.as_int());
            case Kind::Float:
                return static_cast<std::int32_t>(v.as_float());
            case Kind::Bool:
                return v.as_bool() ? 1 : 0;
            default:
                return 0;
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

        void exec_add_float(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const float a = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            const float b = cell_as_float(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_float(a + b);
        }

        void exec_sub_float(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const float a = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            const float b = cell_as_float(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_float(a - b);
        }

        void exec_mul_float(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const float a = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            const float b = cell_as_float(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_float(a * b);
        }

        void exec_div_float(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const float a = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            const float b = cell_as_float(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_float(b != 0.0f ? a / b : 0.0f);
        }

        void exec_abs_float(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const float a = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            ctx.results[i] = engine::ecs::CellValue::from_float(a < 0.0f ? -a : a);
        }

        void exec_neg_float(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const float a = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            ctx.results[i] = engine::ecs::CellValue::from_float(-a);
        }

        void exec_min_float(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const float a = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            const float b = cell_as_float(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_float(a < b ? a : b);
        }

        void exec_max_float(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const float a = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            const float b = cell_as_float(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_float(a > b ? a : b);
        }

        void exec_clamp_float(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const float v = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            const float lo = cell_as_float(ctx.arg_values[node.args_offset + 1]);
            const float hi = cell_as_float(ctx.arg_values[node.args_offset + 2]);
            const float r = v < lo ? lo : (v > hi ? hi : v);
            ctx.results[i] = engine::ecs::CellValue::from_float(r);
        }

        void exec_add_int(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const std::int32_t a = cell_as_int(ctx.arg_values[node.args_offset + 0]);
            const std::int32_t b = cell_as_int(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_int(a + b);
        }

        void exec_sub_int(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const std::int32_t a = cell_as_int(ctx.arg_values[node.args_offset + 0]);
            const std::int32_t b = cell_as_int(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_int(a - b);
        }

        void exec_mul_int(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const std::int32_t a = cell_as_int(ctx.arg_values[node.args_offset + 0]);
            const std::int32_t b = cell_as_int(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_int(a * b);
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
            const bool a = cell_as_bool(ctx.arg_values[node.args_offset + 0]);
            ctx.results[i] = engine::ecs::CellValue::from_bool(!a);
        }

        static const std::array<engine::graph::NodeExecutor, 16> kExecutors = {{
            exec_add_float,
            exec_sub_float,
            exec_mul_float,
            exec_div_float,
            exec_abs_float,
            exec_neg_float,
            exec_min_float,
            exec_max_float,
            exec_clamp_float,
            exec_add_int,
            exec_sub_int,
            exec_mul_int,
            exec_greater_than,
            exec_equal_to,
            exec_and,
            exec_not,
        }};

    } // namespace

    void MathPackage::register_into(PackageBuilder &b)
    {
        for (std::size_t i = 0; i < kNodeTypes.size(); ++i)
        {
            b.add_node_type(kNodeTypes[i]);
            b.add_node_executor(kExecutors[i]);
            b.add_trigger_mode(kTriggerModes[i]);
        }

        // Editor metadata.
        b.add_node_editor_info("Engine_AddFloatNode", NodeEditorInfo{
                                                          "Add (float)", "a + b.", "", "math", "node s = Engine_AddFloatNode(a, b)"});
        b.add_node_editor_info("Engine_SubFloatNode", NodeEditorInfo{
                                                          "Subtract (float)", "a - b.", "", "math", "node d = Engine_SubFloatNode(a, b)"});
        b.add_node_editor_info("Engine_MulFloatNode", NodeEditorInfo{
                                                          "Multiply (float)", "a * b.", "", "math", "node p = Engine_MulFloatNode(a, b)"});
        b.add_node_editor_info("Engine_DivFloatNode", NodeEditorInfo{
                                                          "Divide (float)", "a / b. Returns 0 if b is 0.", "", "math", "node q = Engine_DivFloatNode(a, b)"});
        b.add_node_editor_info("Engine_AbsFloatNode", NodeEditorInfo{
                                                          "Abs (float)", "|a|.", "", "math", "node r = Engine_AbsFloatNode(a)"});
        b.add_node_editor_info("Engine_NegFloatNode", NodeEditorInfo{
                                                          "Negate (float)", "-a.", "", "math", "node n = Engine_NegFloatNode(a)"});
        b.add_node_editor_info("Engine_MinFloatNode", NodeEditorInfo{
                                                          "Min (float)", "min(a, b).", "", "math", "node m = Engine_MinFloatNode(a, b)"});
        b.add_node_editor_info("Engine_MaxFloatNode", NodeEditorInfo{
                                                          "Max (float)", "max(a, b).", "", "math", "node m = Engine_MaxFloatNode(a, b)"});
        b.add_node_editor_info("Engine_ClampFloatNode", NodeEditorInfo{
                                                            "Clamp (float)", "Clamp v to [lo, hi].", "", "math", "node c = Engine_ClampFloatNode(v, lo, hi)"});
        b.add_node_editor_info("Engine_AddIntNode", NodeEditorInfo{
                                                        "Add (int)", "a + b.", "", "math", "node s = Engine_AddIntNode(a, b)"});
        b.add_node_editor_info("Engine_SubIntNode", NodeEditorInfo{
                                                        "Subtract (int)", "a - b.", "", "math", "node d = Engine_SubIntNode(a, b)"});
        b.add_node_editor_info("Engine_MulIntNode", NodeEditorInfo{
                                                        "Multiply (int)", "a * b.", "", "math", "node p = Engine_MulIntNode(a, b)"});
        b.add_node_editor_info("Engine_GreaterThanNode", NodeEditorInfo{
                                                             "Greater Than", "True if a > b.", "", "less", "node g = Engine_GreaterThanNode(a, b)"});
        b.add_node_editor_info("Engine_EqualToNode", NodeEditorInfo{
                                                         "Equal To", "True if a == b.", "", "equal", "node e = Engine_EqualToNode(a, b)"});
        b.add_node_editor_info("Engine_AndNode", NodeEditorInfo{
                                                     "And", "True if both a and b.", "", "and", "node c = Engine_AndNode(a, b)"});
        b.add_node_editor_info("Engine_NotNode", NodeEditorInfo{
                                                     "Not", "True if a is false.", "", "not", "node n = Engine_NotNode(a)"});
    }

} // namespace engine