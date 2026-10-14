#include "engine/registry/packages/core_package.h"
#include "engine/registry/package_builder.h"
#include "engine/core/log.h"
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

        // --- Enums ---

        static const std::array<lucid::sema::EnumMemberInfo, 4> kKeyMembers = {{
            lucid::sema::EnumMemberInfo{"W", 0},
            lucid::sema::EnumMemberInfo{"A", 1},
            lucid::sema::EnumMemberInfo{"S", 2},
            lucid::sema::EnumMemberInfo{"D", 3},
        }};

        static const std::array<lucid::sema::EnumTypeInfo, 1> kEnums = {{
            lucid::sema::EnumTypeInfo{
                "Engine_Key",
                ArenaSpan<lucid::sema::EnumMemberInfo>(
                    const_cast<lucid::sema::EnumMemberInfo *>(kKeyMembers.data()),
                    kKeyMembers.size()),
            },
        }};

        // --- Handles ---

        static const std::array<lucid::sema::HandleTypeInfo, 1> kHandles = {{
            lucid::sema::HandleTypeInfo{"Engine_BodyRef"},
        }};

        // --- Nodes ---

        static const std::array<NodeArgInfo, 1> kFloat32Args = {{
            NodeArgInfo{"value", TypeId::primitive("float32")},
        }};

        static const std::array<NodeArgInfo, 1> kInt32Args = {{
            NodeArgInfo{"value", TypeId::primitive("int32")},
        }};

        static const std::array<NodeTypeInfo, 4> kNodeTypes = {{
            NodeTypeInfo{
                "Engine_Float_Node",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kFloat32Args.data()),
                                       kFloat32Args.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_Int_Node",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kInt32Args.data()),
                                       kInt32Args.size()),
                TypeId::primitive("int32"),
            },
            NodeTypeInfo{
                "Engine_EveryFrame_Node",
                NodeKind::Trigger,
                "Flow",
                0,
                ArenaSpan<NodeArgInfo>{},
                TypeId{},
            },
            NodeTypeInfo{
                "Engine_Print_Float_Node",
                NodeKind::Action,
                "Debug",
                0,
                ArenaSpan<NodeArgInfo>(const_cast<NodeArgInfo *>(kFloat32Args.data()),
                                       kFloat32Args.size()),
                TypeId{},
            },
        }};

        static const std::array<TriggerMode, 4> kTriggerModes = {{
            TriggerMode::NotATrigger, // Engine_Float_Node
            TriggerMode::NotATrigger, // Engine_Int_Node
            TriggerMode::Source,      // Engine_EveryFrame_Node
            TriggerMode::NotATrigger, // Engine_Print_Float_Node
        }};

        void exec_float32(engine::graph::TickContext &ctx, std::uint32_t node_index)
        {
            const auto &node = ctx.graph.nodes[node_index];
            ctx.results[node_index] = ctx.arg_values[node.args_offset];
        }

        void exec_int32(engine::graph::TickContext &ctx, std::uint32_t node_index)
        {
            const auto &node = ctx.graph.nodes[node_index];
            ctx.results[node_index] = ctx.arg_values[node.args_offset];
        }

        void exec_every_frame(engine::graph::TickContext &, std::uint32_t) {}

        void exec_print(engine::graph::TickContext &ctx, std::uint32_t node_index)
        {
            const auto &node = ctx.graph.nodes[node_index];
            const auto &v = ctx.arg_values[node.args_offset];
            double d = 0.0;
            if (v.kind() == engine::ecs::CellValue::Kind::Float)
            {
                d = v.as_float();
            }
            else if (v.kind() == engine::ecs::CellValue::Kind::Int)
            {
                d = static_cast<double>(v.as_int());
            }
            engine::log_info("[graph] Engine_Print_Float_Node: %f", d);
        }

        static const std::array<engine::graph::NodeExecutor, 4> kExecutors = {{
            exec_float32,
            exec_int32,
            exec_every_frame,
            exec_print,
        }};

    } // namespace

    void CorePackage::register_into(PackageBuilder &b)
    {
        for (std::size_t i = 0; i < kNodeTypes.size(); ++i)
        {
            b.add_node_type(kNodeTypes[i]);
            b.add_node_executor(kExecutors[i]);
            b.add_trigger_mode(kTriggerModes[i]);
        }

        for (const auto &e : kEnums)
            b.add_enum(e);
        for (const auto &h : kHandles)
            b.add_handle(h);

        b.add_node_editor_info("Engine_Float_Node",
                               NodeEditorInfo{
                                   "Float",
                                   "A float32 constant.",
                                   "",
                                   "constant",
                                   "node v = Engine_Float_Node(2.5)",
                                   Operation::None,
                               });
        b.add_node_editor_info("Engine_Int_Node",
                               NodeEditorInfo{
                                   "Int",
                                   "An int32 constant.",
                                   "",
                                   "constant",
                                   "node v = Engine_Int_Node(7)",
                                   Operation::None,
                               });
        b.add_node_editor_info("Engine_EveryFrame_Node",
                               NodeEditorInfo{
                                   "Every Frame",
                                   "Fires once per tick. A source trigger.",
                                   "",
                                   "clock",
                                   "node frame = Engine_EveryFrame_Node()",
                                   Operation::EveryFrame,
                               });
        b.add_node_editor_info("Engine_Print_Float_Node",
                               NodeEditorInfo{
                                   "Print",
                                   "Print a float32 value to the engine log.",
                                   "",
                                   "print",
                                   "node p = Engine_Print_Float_Node(v) on frame",
                                   Operation::Print,
                               });
    }

} // namespace engine