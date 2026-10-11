#include "engine/nodes/node_registry.h"
#include "engine/nodes/node_runtime.h"

#include "sema/NodeKind.hpp"
#include "sema/TypeId.hpp"
#include "core/memory/ArenaSpan.hpp"

#include <array>

namespace engine
{

    namespace
    {

        using namespace lucid::sema;

        // The format library's ArenaSpan is a global-namespace template
        // with a non-constexpr constructor. It cannot be used in a
        // constexpr variable, so every array below is `static const`
        // rather than `constexpr`. Static storage duration gives us
        // stable addresses and one-time initialization, which is what
        // we need: the Registry holds views into these arrays for the
        // rest of the process.
        using ::ArenaSpan;

        // -----------------------------------------------------------------------------
        // Phases — the engine's main-loop order.
        // Index into this array is a node type's `phase` field.
        // Append-only; never reorder.
        // -----------------------------------------------------------------------------
        static const std::array<PhaseInfo, 3> kPhases = {{
            PhaseInfo{"update"},  // 0
            PhaseInfo{"physics"}, // 1
            PhaseInfo{"render"},  // 2
        }};

        // -----------------------------------------------------------------------------
        // Enums — host-provided integer values. Script writes Key.W; Sema
        // resolves it to 0; the graph stores the integer.
        // -----------------------------------------------------------------------------
        static const std::array<EnumMemberInfo, 4> kKeyMembers = {{
            EnumMemberInfo{"W", 0},
            EnumMemberInfo{"A", 1},
            EnumMemberInfo{"S", 2},
            EnumMemberInfo{"D", 3},
        }};

        static const std::array<EnumTypeInfo, 1> kEnums = {{
            EnumTypeInfo{"Key", ArenaSpan<EnumMemberInfo>(kKeyMembers.data(), kKeyMembers.size())},
        }};

        // -----------------------------------------------------------------------------
        // Handles — opaque reference types. The format library never inspects
        // what a handle points at.
        // -----------------------------------------------------------------------------
        static const std::array<HandleTypeInfo, 1> kHandles = {{
            HandleTypeInfo{"BodyRef"},
        }};

        // -----------------------------------------------------------------------------
        // Node argument lists. Each node type declares its arguments positionally.
        // Sema checks every call site against the list: count and type.
        // The name field is for diagnostics only.
        // -----------------------------------------------------------------------------
        static const std::array<NodeArgInfo, 1> kFloat32Args = {{
            NodeArgInfo{"value", TypeId::primitive("float32")},
        }};

        static const std::array<NodeArgInfo, 1> kInt32Args = {{
            NodeArgInfo{"value", TypeId::primitive("int32")},
        }};

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

        // -----------------------------------------------------------------------------
        // Node types. This array's index is the NodeId space in every .lucgraph.
        // Append only.
        // -----------------------------------------------------------------------------
        static const std::array<NodeTypeInfo, 7> kNodeTypes = {{
            // 0: Engine_Float32Node — Value, category Math
            NodeTypeInfo{
                "Engine_Float32Node",
                NodeKind::Value,
                "Math",
                0, // phase (unused for Value)
                ArenaSpan<NodeArgInfo>(kFloat32Args.data(), kFloat32Args.size()),
                TypeId::primitive("float32"), // result type
            },
            // 1: Engine_Int32Node — Value, category Math
            NodeTypeInfo{
                "Engine_Int32Node",
                NodeKind::Value,
                "Math",
                0,
                ArenaSpan<NodeArgInfo>(kInt32Args.data(), kInt32Args.size()),
                TypeId::primitive("int32"),
            },
            // 2: Engine_EveryFrame — Trigger, category Flow
            NodeTypeInfo{
                "Engine_EveryFrame",
                NodeKind::Trigger,
                "Flow",
                0,
                ArenaSpan<NodeArgInfo>{}, // no arguments
                TypeId{},                 // no result
            },
            // 3: Engine_PrintNode — Action, category Debug, phase 0 (update)
            NodeTypeInfo{
                "Engine_PrintNode",
                NodeKind::Action,
                "Debug",
                0, // phase index: "update"
                ArenaSpan<NodeArgInfo>(kFloat32Args.data(), kFloat32Args.size()),
                TypeId{}, // no result
            },
            // 4: Engine_BoolNode — Value, category Logic
            NodeTypeInfo{
                "Engine_BoolNode",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kBoolArgs.data(), kBoolArgs.size()),
                TypeId::primitive("bool"),
            },
            // 5: Engine_LessThanNode — Value, category Logic
            NodeTypeInfo{
                "Engine_LessThanNode",
                NodeKind::Value,
                "Logic",
                0,
                ArenaSpan<NodeArgInfo>(kLessThanArgs.data(), kLessThanArgs.size()),
                TypeId::primitive("bool"),
            },
            // 6: Engine_IfNode — Trigger, category Flow
            NodeTypeInfo{
                "Engine_IfNode",
                NodeKind::Trigger,
                "Flow",
                0,
                ArenaSpan<NodeArgInfo>(kIfArgs.data(), kIfArgs.size()),
                TypeId{},
            },
        }};

        // -----------------------------------------------------------------------------
        // Runtime metadata — parallel to kNodeTypes, indexed by the same id.
        // -----------------------------------------------------------------------------
        static const std::array<NodeTypeRuntimeInfo, 7> kNodeRuntimeInfo = {{
            NodeTypeRuntimeInfo{TriggerMode::NotATrigger}, // Engine_Float32Node
            NodeTypeRuntimeInfo{TriggerMode::NotATrigger}, // Engine_Int32Node
            NodeTypeRuntimeInfo{TriggerMode::Source},      // Engine_EveryFrame
            NodeTypeRuntimeInfo{TriggerMode::NotATrigger}, // Engine_PrintNode
            NodeTypeRuntimeInfo{TriggerMode::NotATrigger}, // Engine_BoolNode
            NodeTypeRuntimeInfo{TriggerMode::NotATrigger}, // Engine_LessThanNode
            NodeTypeRuntimeInfo{TriggerMode::Gating},      // Engine_IfNode
        }};

        // -----------------------------------------------------------------------------
        // The registry. Built once, on first call, from the arrays above.
        // -----------------------------------------------------------------------------
        const Registry &build_registry()
        {
            static const Registry reg = []
            {
                Registry r;
                r.phases = ArenaSpan<PhaseInfo>(kPhases.data(), kPhases.size());
                r.enums = ArenaSpan<EnumTypeInfo>(kEnums.data(), kEnums.size());
                r.handles = ArenaSpan<HandleTypeInfo>(kHandles.data(), kHandles.size());
                r.nodeTypes = ArenaSpan<NodeTypeInfo>(kNodeTypes.data(), kNodeTypes.size());
                return r;
            }();
            return reg;
        }

    } // namespace

    const lucid::sema::Registry &node_registry()
    {
        return build_registry();
    }

    const NodeTypeRuntimeInfo &node_runtime_info(std::uint32_t id)
    {
        if (id >= kNodeRuntimeInfo.size())
        {
            static const NodeTypeRuntimeInfo kFallback{};
            return kFallback;
        }
        return kNodeRuntimeInfo[id];
    }

    std::uint32_t node_runtime_info_count()
    {
        return static_cast<std::uint32_t>(kNodeRuntimeInfo.size());
    }

} // namespace engine