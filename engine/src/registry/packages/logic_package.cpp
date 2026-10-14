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

        static std::array<NodeArgInfo, 2> kIntCompareArgs = {{
            NodeArgInfo{"a", TypeId::primitive("int32")},
            NodeArgInfo{"b", TypeId::primitive("int32")},
        }};

        static std::array<NodeArgInfo, 1> kIfArgs = {{
            NodeArgInfo{"cond", TypeId::primitive("bool")},
        }};

        // ---- Node type table ----
        //
        // The order of this array is the order of NodeId values in
        // every .lucgraph compiled against this registry. Append-only.
        //
        // 10 nodes:
        //   0  Engine_Bool_Node
        //   1  Engine_LessThan_Float_Float_Node
        //   2  Engine_GreaterThan_Float_Float_Node
        //   3  Engine_EqualTo_Float_Float_Node
        //   4  Engine_And_Bool_Bool_Node
        //   5  Engine_Not_Bool_Node
        //   6  Engine_If_Bool_Node
        //   7  Engine_LessThan_Int_Int_Node
        //   8  Engine_GreaterThan_Int_Int_Node
        //   9  Engine_EqualTo_Int_Int_Node

        static const std::array<NodeTypeInfo, 10> kNodeTypes = {{
            NodeTypeInfo{
                "Engine_Bool_Node",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kBoolUnaryArgs.data(),
                                       kBoolUnaryArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_LessThan_Float_Float_Node",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kFloatCompareArgs.data(),
                                       kFloatCompareArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_GreaterThan_Float_Float_Node",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kFloatCompareArgs.data(),
                                       kFloatCompareArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_EqualTo_Float_Float_Node",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kFloatCompareArgs.data(),
                                       kFloatCompareArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_And_Bool_Bool_Node",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kBoolBinaryArgs.data(),
                                       kBoolBinaryArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_Not_Bool_Node",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kBoolUnaryArgs.data(),
                                       kBoolUnaryArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_If_Bool_Node",
                NodeKind::Trigger,
                "Flow",
                0,
                ArenaSpan<NodeArgInfo>(kIfArgs.data(), kIfArgs.size()),
                TypeId{},
            },
            NodeTypeInfo{
                "Engine_LessThan_Int_Int_Node",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kIntCompareArgs.data(),
                                       kIntCompareArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_GreaterThan_Int_Int_Node",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kIntCompareArgs.data(),
                                       kIntCompareArgs.size()),
                TypeId::primitive("bool"),
            },
            NodeTypeInfo{
                "Engine_EqualTo_Int_Int_Node",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kIntCompareArgs.data(),
                                       kIntCompareArgs.size()),
                TypeId::primitive("bool"),
            },
        }};

        static_assert(kNodeTypes.size() == 10,
                      "kNodeTypes must have exactly 10 entries");

        static const std::array<TriggerMode, 10> kTriggerModes = {{
            TriggerMode::NotATrigger, // Engine_Bool_Node
            TriggerMode::NotATrigger, // Engine_LessThan_Float_Float_Node
            TriggerMode::NotATrigger, // Engine_GreaterThan_Float_Float_Node
            TriggerMode::NotATrigger, // Engine_EqualTo_Float_Float_Node
            TriggerMode::NotATrigger, // Engine_And_Bool_Bool_Node
            TriggerMode::NotATrigger, // Engine_Not_Bool_Node
            TriggerMode::Gating,      // Engine_If_Bool_Node
            TriggerMode::NotATrigger, // Engine_LessThan_Int_Int_Node
            TriggerMode::NotATrigger, // Engine_GreaterThan_Int_Int_Node
            TriggerMode::NotATrigger, // Engine_EqualTo_Int_Int_Node
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

        void exec_less_than_int(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const std::int32_t a = cell_as_int(ctx.arg_values[node.args_offset + 0]);
            const std::int32_t b = cell_as_int(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_bool(a < b);
        }

        void exec_greater_than_int(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const std::int32_t a = cell_as_int(ctx.arg_values[node.args_offset + 0]);
            const std::int32_t b = cell_as_int(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_bool(a > b);
        }

        void exec_equal_to_int(engine::graph::TickContext &ctx, std::uint32_t i)
        {
            const auto &node = ctx.graph.nodes[i];
            const std::int32_t a = cell_as_int(ctx.arg_values[node.args_offset + 0]);
            const std::int32_t b = cell_as_int(ctx.arg_values[node.args_offset + 1]);
            ctx.results[i] = engine::ecs::CellValue::from_bool(a == b);
        }

        static const std::array<engine::graph::NodeExecutor, 10> kExecutors = {{
            exec_bool,
            exec_less_than,
            exec_greater_than,
            exec_equal_to,
            exec_and,
            exec_not,
            exec_if,
            exec_less_than_int,
            exec_greater_than_int,
            exec_equal_to_int,
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

        // --- Existing ---

        b.add_node_editor_info("Engine_Bool_Node",
                               NodeEditorInfo{
                                   "Bool",
                                   "A boolean constant.",
                                   "",
                                   "constant",
                                   "node c = Engine_Bool_Node(true)",
                                   Operation::None,
                               });
        b.add_node_editor_info("Engine_LessThan_Float_Float_Node",
                               NodeEditorInfo{
                                   "Less Than (float)",
                                   "True if a < b, comparing two float32 values.",
                                   "",
                                   "less",
                                   "node c = Engine_LessThan_Float_Float_Node(a, b)",
                                   Operation::LessThan,
                               });
        b.add_node_editor_info("Engine_GreaterThan_Float_Float_Node",
                               NodeEditorInfo{
                                   "Greater Than (float)",
                                   "True if a > b, comparing two float32 values.",
                                   "",
                                   "greater",
                                   "node c = Engine_GreaterThan_Float_Float_Node(a, b)",
                                   Operation::GreaterThan,
                               });
        b.add_node_editor_info("Engine_EqualTo_Float_Float_Node",
                               NodeEditorInfo{
                                   "Equal To (float)",
                                   "True if a == b, comparing two float32 values.",
                                   "",
                                   "equal",
                                   "node c = Engine_EqualTo_Float_Float_Node(a, b)",
                                   Operation::EqualTo,
                               });
        b.add_node_editor_info("Engine_And_Bool_Bool_Node",
                               NodeEditorInfo{
                                   "And",
                                   "True if both a and b.",
                                   "",
                                   "and",
                                   "node c = Engine_And_Bool_Bool_Node(a, b)",
                                   Operation::And,
                               });
        b.add_node_editor_info("Engine_Not_Bool_Node",
                               NodeEditorInfo{
                                   "Not",
                                   "True if a is false.",
                                   "",
                                   "not",
                                   "node c = Engine_Not_Bool_Node(a)",
                                   Operation::Not,
                               });
        b.add_node_editor_info("Engine_If_Bool_Node",
                               NodeEditorInfo{
                                   "If",
                                   "Fires its subscribers when the condition is true.",
                                   "",
                                   "if",
                                   "node gate = Engine_If_Bool_Node(cond) on parent",
                                   Operation::If,
                               });

        // --- Int comparison ---

        b.add_node_editor_info("Engine_LessThan_Int_Int_Node",
                               NodeEditorInfo{
                                   "Less Than (int)",
                                   "True if a < b, comparing two int32 values exactly.",
                                   "",
                                   "less",
                                   "node c = Engine_LessThan_Int_Int_Node(a, b)",
                                   Operation::LessThan,
                               });
        b.add_node_editor_info("Engine_GreaterThan_Int_Int_Node",
                               NodeEditorInfo{
                                   "Greater Than (int)",
                                   "True if a > b, comparing two int32 values exactly.",
                                   "",
                                   "greater",
                                   "node c = Engine_GreaterThan_Int_Int_Node(a, b)",
                                   Operation::GreaterThan,
                               });
        b.add_node_editor_info("Engine_EqualTo_Int_Int_Node",
                               NodeEditorInfo{
                                   "Equal To (int)",
                                   "True if a == b, comparing two int32 values exactly.",
                                   "",
                                   "equal",
                                   "node c = Engine_EqualTo_Int_Int_Node(a, b)",
                                   Operation::EqualTo,
                               });
    }

} // namespace engine