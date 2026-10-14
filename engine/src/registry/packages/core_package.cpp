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

        // ---- Enums ----
        //
        // The array's size is explicit; the static_assert at the
        // bottom of the anonymous namespace checks it. A mismatch
        // would otherwise pass a default-constructed entry into the
        // registry, and a default-constructed EnumMemberInfo's name
        // is not a valid pointer.

        static const std::array<lucid::sema::EnumMemberInfo, 4> kKeyMembers = {{
            lucid::sema::EnumMemberInfo{"W", 0},
            lucid::sema::EnumMemberInfo{"A", 1},
            lucid::sema::EnumMemberInfo{"S", 2},
            lucid::sema::EnumMemberInfo{"D", 3},
        }};

        static_assert(kKeyMembers.size() == 4,
                      "kKeyMembers must have 4 entries");

        static const std::array<lucid::sema::EnumTypeInfo, 1> kEnums = {{
            lucid::sema::EnumTypeInfo{
                "Engine_Key",
                ArenaSpan<lucid::sema::EnumMemberInfo>(
                    const_cast<lucid::sema::EnumMemberInfo *>(kKeyMembers.data()),
                    kKeyMembers.size()),
            },
        }};

        static_assert(kEnums.size() == 1,
                      "kEnums must have 1 entry");

        // ---- Handles ----

        static const std::array<lucid::sema::HandleTypeInfo, 1> kHandles = {{
            lucid::sema::HandleTypeInfo{"Engine_BodyRef"},
        }};

        static_assert(kHandles.size() == 1,
                      "kHandles must have 1 entry");

        // ---- Argument lists ----
        //
        // One list per argument shape. Non-const because the
        // library's ArenaSpan takes a non-const pointer for
        // read-only data; nothing here modifies them.

        static std::array<NodeArgInfo, 1> kFloat32Args = {{
            NodeArgInfo{"value", TypeId::primitive("float32")},
        }};

        static std::array<NodeArgInfo, 1> kInt32Args = {{
            NodeArgInfo{"value", TypeId::primitive("int32")},
        }};

        static std::array<NodeArgInfo, 1> kBoolArgs = {{
            NodeArgInfo{"value", TypeId::primitive("bool")},
        }};

        // ---- Node types ----
        //
        // The order of this array is the order of NodeId values in
        // every .lucgraph compiled against this registry. Append-only.
        //
        // 5 nodes:
        //   0  Engine_Float_Node
        //   1  Engine_Int_Node
        //   2  Engine_EveryFrame_Node
        //   3  Engine_Print_Float_Node
        //   4  Engine_Print_Bool_Node

        static const std::array<NodeTypeInfo, 5> kNodeTypes = {{
            NodeTypeInfo{
                "Engine_Float_Node",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kFloat32Args.data(), kFloat32Args.size()),
                TypeId::primitive("float32"),
            },
            NodeTypeInfo{
                "Engine_Int_Node",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kInt32Args.data(), kInt32Args.size()),
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
                ArenaSpan<NodeArgInfo>(kFloat32Args.data(), kFloat32Args.size()),
                TypeId{},
            },
            NodeTypeInfo{
                "Engine_Print_Bool_Node",
                NodeKind::Action,
                "Debug",
                0,
                ArenaSpan<NodeArgInfo>(kBoolArgs.data(), kBoolArgs.size()),
                TypeId{},
            },
        }};

        static_assert(kNodeTypes.size() == 5,
                      "kNodeTypes must have exactly 5 entries");

        // ---- Trigger modes ----

        static const std::array<TriggerMode, 5> kTriggerModes = {{
            TriggerMode::NotATrigger, // Engine_Float_Node
            TriggerMode::NotATrigger, // Engine_Int_Node
            TriggerMode::Source,      // Engine_EveryFrame_Node
            TriggerMode::NotATrigger, // Engine_Print_Float_Node
            TriggerMode::NotATrigger, // Engine_Print_Bool_Node
        }};

        static_assert(kTriggerModes.size() == kNodeTypes.size(),
                      "kTriggerModes must be parallel to kNodeTypes");

        // ---- Executors ----
        //
        // Naming: exec_<operation>_<type>. The type is the node's
        // primary argument (or the constant's type). One executor
        // per node type. Do not reorder; the order matches
        // kNodeTypes.

        void exec_const_float(engine::graph::TickContext &ctx, std::uint32_t node_index)
        {
            const auto &node = ctx.graph.nodes[node_index];
            ctx.results[node_index] = ctx.arg_values[node.args_offset];
        }

        void exec_const_int(engine::graph::TickContext &ctx, std::uint32_t node_index)
        {
            const auto &node = ctx.graph.nodes[node_index];
            ctx.results[node_index] = ctx.arg_values[node.args_offset];
        }

        void exec_every_frame(engine::graph::TickContext &, std::uint32_t) {}

        void exec_print_float(engine::graph::TickContext &ctx, std::uint32_t node_index)
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

        void exec_print_bool(engine::graph::TickContext &ctx, std::uint32_t node_index)
        {
            const auto &node = ctx.graph.nodes[node_index];
            const auto &v = ctx.arg_values[node.args_offset];
            bool b = false;
            switch (v.kind())
            {
            case engine::ecs::CellValue::Kind::Bool:
                b = v.as_bool();
                break;
            case engine::ecs::CellValue::Kind::Int:
                b = v.as_int() != 0;
                break;
            case engine::ecs::CellValue::Kind::Float:
                b = v.as_float() != 0.0;
                break;
            default:
                break;
            }
            engine::log_info("[graph] Engine_Print_Bool_Node: %s",
                             b ? "true" : "false");
        }

        static const std::array<engine::graph::NodeExecutor, 5> kExecutors = {{
            exec_const_float, // Engine_Float_Node
            exec_const_int,   // Engine_Int_Node
            exec_every_frame, // Engine_EveryFrame_Node
            exec_print_float, // Engine_Print_Float_Node
            exec_print_bool,  // Engine_Print_Bool_Node
        }};

        static_assert(kExecutors.size() == kNodeTypes.size(),
                      "kExecutors must be parallel to kNodeTypes");

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
                                   "Print (float)",
                                   "Print a float32 value to the engine log.",
                                   "",
                                   "print",
                                   "node p = Engine_Print_Float_Node(v) on frame",
                                   Operation::Print,
                               });
        b.add_node_editor_info("Engine_Print_Bool_Node",
                               NodeEditorInfo{
                                   "Print (bool)",
                                   "Print a bool value to the engine log.",
                                   "",
                                   "print",
                                   "node p = Engine_Print_Bool_Node(v) on frame",
                                   Operation::Print,
                               });
    }

} // namespace engine