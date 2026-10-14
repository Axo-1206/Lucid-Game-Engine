#include <catch2/catch_test_macros.hpp>

#include "engine/registry/default_registry.h"

#include "sema/Sema.hpp"

using namespace engine;

TEST_CASE("default_registry: shape", "[nodes][registry]")
{
    const auto &reg = default_registry();

    REQUIRE(reg.phases.size() == 3);
    REQUIRE(reg.enums.size() == 1);
    REQUIRE(reg.handles.size() == 1);
    REQUIRE(reg.nodeTypes.size() == 11);

    REQUIRE(reg.phases[0].name == "update");
    REQUIRE(reg.phases[1].name == "physics");
    REQUIRE(reg.phases[2].name == "render");

    // Core package (4 nodes).
    REQUIRE(reg.nodeTypes[0].name == "Engine_Float_Node");
    REQUIRE(reg.nodeTypes[1].name == "Engine_Int_Node");
    REQUIRE(reg.nodeTypes[2].name == "Engine_EveryFrame_Node");
    REQUIRE(reg.nodeTypes[3].name == "Engine_Print_Float_Node");

    // Logic package (7 nodes).
    REQUIRE(reg.nodeTypes[4].name == "Engine_Bool_Node");
    REQUIRE(reg.nodeTypes[5].name == "Engine_LessThan_Float_Float_Node");
    REQUIRE(reg.nodeTypes[6].name == "Engine_GreaterThan_Float_Float_Node");
    REQUIRE(reg.nodeTypes[7].name == "Engine_EqualTo_Float_Float_Node");
    REQUIRE(reg.nodeTypes[8].name == "Engine_And_Bool_Bool_Node");
    REQUIRE(reg.nodeTypes[9].name == "Engine_Not_Bool_Node");
    REQUIRE(reg.nodeTypes[10].name == "Engine_If_Bool_Node");
}

TEST_CASE("default_registry: node kinds", "[nodes][registry]")
{
    const auto &reg = default_registry();

    using lucid::sema::NodeKind;

    // Core.
    REQUIRE(reg.nodeTypes[0].name == "Engine_Float_Node");
    REQUIRE(reg.nodeTypes[0].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[1].name == "Engine_Int_Node");
    REQUIRE(reg.nodeTypes[1].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[2].name == "Engine_EveryFrame_Node");
    REQUIRE(reg.nodeTypes[2].kind == NodeKind::Trigger);

    REQUIRE(reg.nodeTypes[3].name == "Engine_Print_Float_Node");
    REQUIRE(reg.nodeTypes[3].kind == NodeKind::Action);

    // Logic.
    REQUIRE(reg.nodeTypes[4].name == "Engine_Bool_Node");
    REQUIRE(reg.nodeTypes[4].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[5].name == "Engine_LessThan_Float_Float_Node");
    REQUIRE(reg.nodeTypes[5].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[6].name == "Engine_GreaterThan_Float_Float_Node");
    REQUIRE(reg.nodeTypes[6].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[7].name == "Engine_EqualTo_Float_Float_Node");
    REQUIRE(reg.nodeTypes[7].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[8].name == "Engine_And_Bool_Bool_Node");
    REQUIRE(reg.nodeTypes[8].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[9].name == "Engine_Not_Bool_Node");
    REQUIRE(reg.nodeTypes[9].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[10].name == "Engine_If_Bool_Node");
    REQUIRE(reg.nodeTypes[10].kind == NodeKind::Trigger);
}

TEST_CASE("default_registry: runtime info parallels nodeTypes", "[nodes][registry]")
{
    REQUIRE(node_runtime_info_count() == default_registry().nodeTypes.size());

    // Core.
    REQUIRE(node_runtime_info(0).mode == TriggerMode::NotATrigger); // Engine_Float_Node
    REQUIRE(node_runtime_info(1).mode == TriggerMode::NotATrigger); // Engine_Int_Node
    REQUIRE(node_runtime_info(2).mode == TriggerMode::Source);      // Engine_EveryFrame_Node
    REQUIRE(node_runtime_info(3).mode == TriggerMode::NotATrigger); // Engine_Print_Float_Node

    // Logic.
    REQUIRE(node_runtime_info(4).mode == TriggerMode::NotATrigger); // Engine_Bool_Node
    REQUIRE(node_runtime_info(5).mode == TriggerMode::NotATrigger); // Engine_LessThan_Float_Float_Node
    REQUIRE(node_runtime_info(6).mode == TriggerMode::NotATrigger); // Engine_GreaterThan_Float_Float_Node
    REQUIRE(node_runtime_info(7).mode == TriggerMode::NotATrigger); // Engine_EqualTo_Float_Float_Node
    REQUIRE(node_runtime_info(8).mode == TriggerMode::NotATrigger); // Engine_And_Bool_Bool_Node
    REQUIRE(node_runtime_info(9).mode == TriggerMode::NotATrigger); // Engine_Not_Bool_Node
    REQUIRE(node_runtime_info(10).mode == TriggerMode::Gating);     // Engine_If_Bool_Node
}