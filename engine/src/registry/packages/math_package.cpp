#include "engine/registry/packages/math_package.h"
#include "engine/registry/package_builder.h"
#include "engine/ecs/cell_value.h"
#include "engine/graph/tick_context.h"
#include "engine/graph/arg_helpers.h"
#include "engine/registry/default_registry.h"
#include "engine/math/math.h"

#include "sema/NodeKind.hpp"
#include "sema/TypeId.hpp"
#include "core/memory/ArenaSpan.hpp"

#include <array>
#include <cstdint>
#include <cstring>

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
        //
        // These arrays are non-const. The library's ArenaSpan takes a
        // non-const pointer for read-only data; declaring the arrays
        // non-const avoids the const_cast that would otherwise be
        // needed at every call site. Nothing here modifies them.

        static std::array<NodeArgInfo, 2> kFloatBinaryArgs = {{
            NodeArgInfo{"a", TypeId::primitive("float32")},
            NodeArgInfo{"b", TypeId::primitive("float32")},
        }};

        static std::array<NodeArgInfo, 1> kFloatUnaryArgs = {{
            NodeArgInfo{"a", TypeId::primitive("float32")},
        }};

        static std::array<NodeArgInfo, 3> kClampArgs = {{
            NodeArgInfo{"v", TypeId::primitive("float32")},
            NodeArgInfo{"lo", TypeId::primitive("float32")},
            NodeArgInfo{"hi", TypeId::primitive("float32")},
        }};

        static std::array<NodeArgInfo, 2> kIntBinaryArgs = {{
            NodeArgInfo{"a", TypeId::primitive("int32")},
            NodeArgInfo{"b", TypeId::primitive("int32")},
        }};

        static std::array<NodeArgInfo, 1> kIntUnaryArgs = {{
            NodeArgInfo{"a", TypeId::primitive("int32")},
        }};

        static std::array<NodeArgInfo, 1> kIntToFloatArgs = {{
            NodeArgInfo{"a", TypeId::primitive("int32")},
        }};

        static std::array<NodeArgInfo, 1> kFloatToIntArgs = {{
            NodeArgInfo{"a", TypeId::primitive("float32")},
        }};

        // ---- Struct: Engine_Vector2 ----

        static std::array<StructFieldInfo, 2> kVector2Fields = {{
            StructFieldInfo{"x", TypeId::primitive("float32")},
            StructFieldInfo{"y", TypeId::primitive("float32")},
        }};

        static std::array<StructTypeInfo, 1> kStructs = {{
            StructTypeInfo{
                "Engine_Vector2",
                ArenaSpan<StructFieldInfo>(kVector2Fields.data(),
                                           kVector2Fields.size()),
            },
        }};

        // ---- Vector2 argument lists ----

        static std::array<NodeArgInfo, 2> kVec2CtorArgs = {{
            NodeArgInfo{"x", TypeId::primitive("float32")},
            NodeArgInfo{"y", TypeId::primitive("float32")},
        }};

        static std::array<NodeArgInfo, 1> kVec2Arg = {{
            NodeArgInfo{"v", TypeId::structType("Engine_Vector2")},
        }};

        static std::array<NodeArgInfo, 2> kVec2BinaryArgs = {{
            NodeArgInfo{"a", TypeId::structType("Engine_Vector2")},
            NodeArgInfo{"b", TypeId::structType("Engine_Vector2")},
        }};

        static std::array<NodeArgInfo, 2> kVec2ScaleArgs = {{
            NodeArgInfo{"v", TypeId::structType("Engine_Vector2")},
            NodeArgInfo{"s", TypeId::primitive("float32")},
        }};

        // ---- Node type table ----
        //
        // The order of this array is the order of NodeId values in
        // every .lucgraph compiled against this registry. Append-only.
        //
        // 19 nodes:
        //   0.. 8   float arithmetic (add, sub, mul, div, abs, neg,
        //           min, max, clamp)
        //   9..11   int arithmetic (add, sub, mul)
        //   12..13  conversions (int->float, float->int)
        //   14..20  Vector2 nodes
        //
        // No boolean logic here: And, Or, Not, Equal, GreaterThan
        // are logic-package concerns, not math ones.

        static const std::array<NodeTypeInfo, 20> kNodeTypes = {{
            // --- Float arithmetic ---
            NodeTypeInfo{
                "Engine_AddFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kFloatBinaryArgs.data(),
                                       kFloatBinaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_SubFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kFloatBinaryArgs.data(),
                                       kFloatBinaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_MulFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kFloatBinaryArgs.data(),
                                       kFloatBinaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_DivFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kFloatBinaryArgs.data(),
                                       kFloatBinaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_AbsFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kFloatUnaryArgs.data(),
                                       kFloatUnaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_NegFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kFloatUnaryArgs.data(),
                                       kFloatUnaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_MinFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kFloatBinaryArgs.data(),
                                       kFloatBinaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_MaxFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kFloatBinaryArgs.data(),
                                       kFloatBinaryArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_ClampFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kClampArgs.data(),
                                       kClampArgs.size()),
                TypeId::primitive("float32"),
            },

            // --- Int arithmetic ---
            NodeTypeInfo{
                "Engine_AddIntNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kIntBinaryArgs.data(),
                                       kIntBinaryArgs.size()),
                TypeId::primitive("int32"),
            },
            NodeTypeInfo{
                "Engine_SubIntNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kIntBinaryArgs.data(),
                                       kIntBinaryArgs.size()),
                TypeId::primitive("int32"),
            },
            NodeTypeInfo{
                "Engine_MulIntNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kIntBinaryArgs.data(),
                                       kIntBinaryArgs.size()),
                TypeId::primitive("int32"),
            },

            // --- Conversions ---
            NodeTypeInfo{
                "Engine_IntToFloatNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kIntToFloatArgs.data(),
                                       kIntToFloatArgs.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_FloatToIntNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kFloatToIntArgs.data(),
                                       kFloatToIntArgs.size()),
                TypeId::primitive("int32"),
            },

            // --- Vector2 ---
            NodeTypeInfo{
                "Engine_Vector2Node",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kVec2CtorArgs.data(),
                                       kVec2CtorArgs.size()),
                TypeId::structType("Engine_Vector2"),
            },
            NodeTypeInfo{
                "Engine_Vector2XNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kVec2Arg.data(),
                                       kVec2Arg.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_Vector2YNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kVec2Arg.data(),
                                       kVec2Arg.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_Vector2AddNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kVec2BinaryArgs.data(),
                                       kVec2BinaryArgs.size()),
                TypeId::structType("Engine_Vector2"),
            },
            NodeTypeInfo{
                "Engine_Vector2SubNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kVec2BinaryArgs.data(),
                                       kVec2BinaryArgs.size()),
                TypeId::structType("Engine_Vector2"),
            },
            NodeTypeInfo{
                "Engine_Vector2ScaleNode",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kVec2ScaleArgs.data(),
                                       kVec2ScaleArgs.size()),
                TypeId::structType("Engine_Vector2"),
            },
        }};

        static_assert(kNodeTypes.size() == 20,
                      "kNodeTypes must have exactly 20 entries");

        // ---- Trigger modes ----
        //
        // Every node in this package is a Value node; none fires.

        static const std::array<TriggerMode, 20> kTriggerModes = {{
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

        // ---- Float executors ----

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
            ctx.results[i] = engine::ecs::CellValue::from_float(
                b != 0.0f ? a / b : 0.0f);
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

        // ---- Int executors ----

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

        // ---- Conversion executors ----

        void exec_int_to_float(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const std::int32_t a = cell_as_int(ctx.arg_values[node.args_offset + 0]);
            ctx.results[i] = engine::ecs::CellValue::from_float(
                static_cast<float>(a));
        }

        void exec_float_to_int(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const float a = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            ctx.results[i] = engine::ecs::CellValue::from_int(
                static_cast<std::int32_t>(a));
        }

        // ---- Vector2 executors ----
        //
        // A Vector2 value lives as the bytes of an engine::math::Vector2
        // in the graph's struct_results buffer for the node that
        // produced it. A node that consumes a Vector2 reads the
        // producer's buffer via engine::graph::arg_node_index.

        void exec_vec2_new(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const float x = cell_as_float(ctx.arg_values[node.args_offset + 0]);
            const float y = cell_as_float(ctx.arg_values[node.args_offset + 1]);

            engine::math::Vector2 v{x, y};
            auto &slot = ctx.struct_results[i];
            std::memcpy(slot.data(), &v, sizeof(v));
        }

        void exec_vec2_x(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const std::uint32_t src =
                engine::graph::arg_node_index(ctx.graph, node, 0);

            engine::math::Vector2 v;
            std::memcpy(&v, ctx.struct_results[src].data(), sizeof(v));
            ctx.results[i] = engine::ecs::CellValue::from_float(v.x);
        }

        void exec_vec2_y(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const std::uint32_t src =
                engine::graph::arg_node_index(ctx.graph, node, 0);

            engine::math::Vector2 v;
            std::memcpy(&v, ctx.struct_results[src].data(), sizeof(v));
            ctx.results[i] = engine::ecs::CellValue::from_float(v.y);
        }

        void exec_vec2_add(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const std::uint32_t a_idx =
                engine::graph::arg_node_index(ctx.graph, node, 0);
            const std::uint32_t b_idx =
                engine::graph::arg_node_index(ctx.graph, node, 1);

            engine::math::Vector2 a, b;
            std::memcpy(&a, ctx.struct_results[a_idx].data(), sizeof(a));
            std::memcpy(&b, ctx.struct_results[b_idx].data(), sizeof(b));

            engine::math::Vector2 r{a.x + b.x, a.y + b.y};
            std::memcpy(ctx.struct_results[i].data(), &r, sizeof(r));
        }

        void exec_vec2_sub(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const std::uint32_t a_idx =
                engine::graph::arg_node_index(ctx.graph, node, 0);
            const std::uint32_t b_idx =
                engine::graph::arg_node_index(ctx.graph, node, 1);

            engine::math::Vector2 a, b;
            std::memcpy(&a, ctx.struct_results[a_idx].data(), sizeof(a));
            std::memcpy(&b, ctx.struct_results[b_idx].data(), sizeof(b));

            engine::math::Vector2 r{a.x - b.x, a.y - b.y};
            std::memcpy(ctx.struct_results[i].data(), &r, sizeof(r));
        }

        void exec_vec2_scale(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const std::uint32_t v_idx =
                engine::graph::arg_node_index(ctx.graph, node, 0);
            const float s = cell_as_float(ctx.arg_values[node.args_offset + 1]);

            engine::math::Vector2 v;
            std::memcpy(&v, ctx.struct_results[v_idx].data(), sizeof(v));

            engine::math::Vector2 r{v.x * s, v.y * s};
            std::memcpy(ctx.struct_results[i].data(), &r, sizeof(r));
        }

        // ---- Executor table ----

        static const std::array<engine::graph::NodeExecutor, 20> kExecutors = {{
            // 0.. 8   float arithmetic
            exec_add_float,
            exec_sub_float,
            exec_mul_float,
            exec_div_float,
            exec_abs_float,
            exec_neg_float,
            exec_min_float,
            exec_max_float,
            exec_clamp_float,
            // 9..11   int arithmetic
            exec_add_int,
            exec_sub_int,
            exec_mul_int,
            // 12..13  conversions
            exec_int_to_float,
            exec_float_to_int,
            // 14..19  Vector2
            exec_vec2_new,
            exec_vec2_x,
            exec_vec2_y,
            exec_vec2_add,
            exec_vec2_sub,
            exec_vec2_scale,
        }};

        static_assert(kExecutors.size() == kNodeTypes.size(),
                      "kExecutors must be parallel to kNodeTypes");
        static_assert(kTriggerModes.size() == kNodeTypes.size(),
                      "kTriggerModes must be parallel to kNodeTypes");

    } // namespace

    void MathPackage::register_into(PackageBuilder &b)
    {
        // Structs first: a node type that uses Engine_Vector2 must be
        // registered after the struct type exists in the builder.
        for (const auto &s : kStructs)
        {
            b.add_struct(s);
        }

        for (std::size_t i = 0; i < kNodeTypes.size(); ++i)
        {
            b.add_node_type(kNodeTypes[i]);
            b.add_node_executor(kExecutors[i]);
            b.add_trigger_mode(kTriggerModes[i]);
        }

        // Editor metadata.
        b.add_node_editor_info("Engine_AddFloatNode", NodeEditorInfo{
                                                          "Add (float)", "a + b.", "", "math",
                                                          "node s = Engine_AddFloatNode(a, b)"});
        b.add_node_editor_info("Engine_SubFloatNode", NodeEditorInfo{
                                                          "Subtract (float)", "a - b.", "", "math",
                                                          "node d = Engine_SubFloatNode(a, b)"});
        b.add_node_editor_info("Engine_MulFloatNode", NodeEditorInfo{
                                                          "Multiply (float)", "a * b.", "", "math",
                                                          "node p = Engine_MulFloatNode(a, b)"});
        b.add_node_editor_info("Engine_DivFloatNode", NodeEditorInfo{
                                                          "Divide (float)", "a / b. Returns 0 if b is 0.", "", "math",
                                                          "node q = Engine_DivFloatNode(a, b)"});
        b.add_node_editor_info("Engine_AbsFloatNode", NodeEditorInfo{
                                                          "Abs (float)", "|a|.", "", "math",
                                                          "node r = Engine_AbsFloatNode(a)"});
        b.add_node_editor_info("Engine_NegFloatNode", NodeEditorInfo{
                                                          "Negate (float)", "-a.", "", "math",
                                                          "node n = Engine_NegFloatNode(a)"});
        b.add_node_editor_info("Engine_MinFloatNode", NodeEditorInfo{
                                                          "Min (float)", "min(a, b).", "", "math",
                                                          "node m = Engine_MinFloatNode(a, b)"});
        b.add_node_editor_info("Engine_MaxFloatNode", NodeEditorInfo{
                                                          "Max (float)", "max(a, b).", "", "math",
                                                          "node m = Engine_MaxFloatNode(a, b)"});
        b.add_node_editor_info("Engine_ClampFloatNode", NodeEditorInfo{
                                                            "Clamp (float)", "Clamp v to [lo, hi].", "", "math",
                                                            "node c = Engine_ClampFloatNode(v, lo, hi)"});
        b.add_node_editor_info("Engine_AddIntNode", NodeEditorInfo{
                                                        "Add (int)", "a + b.", "", "math",
                                                        "node s = Engine_AddIntNode(a, b)"});
        b.add_node_editor_info("Engine_SubIntNode", NodeEditorInfo{
                                                        "Subtract (int)", "a - b.", "", "math",
                                                        "node d = Engine_SubIntNode(a, b)"});
        b.add_node_editor_info("Engine_MulIntNode", NodeEditorInfo{
                                                        "Multiply (int)", "a * b.", "", "math",
                                                        "node p = Engine_MulIntNode(a, b)"});
        b.add_node_editor_info("Engine_IntToFloatNode", NodeEditorInfo{
                                                            "Int to Float", "Convert an int32 to a float32.", "", "convert",
                                                            "node f = Engine_IntToFloatNode(i)"});
        b.add_node_editor_info("Engine_FloatToIntNode", NodeEditorInfo{
                                                            "Float to Int", "Convert a float32 to an int32 (truncating).", "",
                                                            "convert", "node i = Engine_FloatToIntNode(f)"});
        b.add_node_editor_info("Engine_Vector2Node", NodeEditorInfo{
                                                         "Vector2", "Create a Vector2 from x and y.", "", "vector",
                                                         "node v = Engine_Vector2Node(1.0, 2.0)"});
        b.add_node_editor_info("Engine_Vector2XNode", NodeEditorInfo{
                                                          "Vector2 X", "Read the x component.", "", "vector",
                                                          "node x = Engine_Vector2XNode(v)"});
        b.add_node_editor_info("Engine_Vector2YNode", NodeEditorInfo{
                                                          "Vector2 Y", "Read the y component.", "", "vector",
                                                          "node y = Engine_Vector2YNode(v)"});
        b.add_node_editor_info("Engine_Vector2AddNode", NodeEditorInfo{
                                                            "Vector2 Add", "a + b, component-wise.", "", "vector",
                                                            "node s = Engine_Vector2AddNode(a, b)"});
        b.add_node_editor_info("Engine_Vector2SubNode", NodeEditorInfo{
                                                            "Vector2 Sub", "a - b, component-wise.", "", "vector",
                                                            "node d = Engine_Vector2SubNode(a, b)"});
        b.add_node_editor_info("Engine_Vector2ScaleNode", NodeEditorInfo{
                                                              "Vector2 Scale", "v * s, component-wise.", "", "vector",
                                                              "node r = Engine_Vector2ScaleNode(v, s)"});
    }

} // namespace engine