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
    REQUIRE(reg.nodeTypes.size() == 7);

    REQUIRE(reg.phases[0].name == "update");
    REQUIRE(reg.phases[1].name == "physics");
    REQUIRE(reg.phases[2].name == "render");

    REQUIRE(reg.nodeTypes[0].name == "Engine_Float32Node");
    REQUIRE(reg.nodeTypes[1].name == "Engine_Int32Node");
    REQUIRE(reg.nodeTypes[2].name == "Engine_EveryFrame");
    REQUIRE(reg.nodeTypes[3].name == "Engine_PrintNode");
    REQUIRE(reg.nodeTypes[4].name == "Engine_BoolNode");
    REQUIRE(reg.nodeTypes[5].name == "Engine_LessThanNode");
    REQUIRE(reg.nodeTypes[6].name == "Engine_IfNode");
}

TEST_CASE("default_registry: node kinds", "[nodes][registry]")
{
    const auto &reg = default_registry();

    using lucid::sema::NodeKind;

    REQUIRE(reg.nodeTypes[0].name == "Engine_Float32Node");
    REQUIRE(reg.nodeTypes[0].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[1].name == "Engine_Int32Node");
    REQUIRE(reg.nodeTypes[1].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[2].name == "Engine_EveryFrame");
    REQUIRE(reg.nodeTypes[2].kind == NodeKind::Trigger);

    REQUIRE(reg.nodeTypes[3].name == "Engine_PrintNode");
    REQUIRE(reg.nodeTypes[3].kind == NodeKind::Action);

    REQUIRE(reg.nodeTypes[4].name == "Engine_BoolNode");
    REQUIRE(reg.nodeTypes[4].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[5].name == "Engine_LessThanNode");
    REQUIRE(reg.nodeTypes[5].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[6].name == "Engine_IfNode");
    REQUIRE(reg.nodeTypes[6].kind == NodeKind::Trigger);
}

TEST_CASE("default_registry: runtime info parallels nodeTypes", "[nodes][registry]")
{
    REQUIRE(node_runtime_info_count() == default_registry().nodeTypes.size());

    REQUIRE(node_runtime_info(0).mode == TriggerMode::NotATrigger); // Engine_Float32Node
    REQUIRE(node_runtime_info(1).mode == TriggerMode::NotATrigger); // Engine_Int32Node
    REQUIRE(node_runtime_info(2).mode == TriggerMode::Source);      // Engine_EveryFrame
    REQUIRE(node_runtime_info(3).mode == TriggerMode::NotATrigger); // Engine_PrintNode
    REQUIRE(node_runtime_info(4).mode == TriggerMode::NotATrigger); // Engine_BoolNode
    REQUIRE(node_runtime_info(5).mode == TriggerMode::NotATrigger); // Engine_LessThanNode
    REQUIRE(node_runtime_info(6).mode == TriggerMode::Gating);      // Engine_IfNode
}

TEST_CASE("default_registry: compile a trivial file", "[nodes][compile]")
{
    const char *source =
        "node v = Engine_Float32Node(1.0)\n"
        "node frame = Engine_EveryFrame()\n"
        "node p = Engine_PrintNode(v) on frame\n";

    auto result = lucid::sema::compile(source, "smoke.lucid", default_registry());

    if (!result.ok)
    {
        for (const auto &d : result.diagnostics)
        {
            WARN(d.message);
        }
    }
    REQUIRE(result.ok);
    REQUIRE(result.graph != nullptr);
}

TEST_CASE("default_registry: compile a gated file", "[nodes][compile]")
{
    const char *source =
        "node frame = Engine_EveryFrame()\n"
        "node cond = Engine_BoolNode(true)\n"
        "node gate = Engine_IfNode(cond) on frame\n"
        "node p = Engine_PrintNode(1.0) on gate\n";

    auto result = lucid::sema::compile(source, "gate.lucid", default_registry());
    if (!result.ok)
    {
        for (const auto &d : result.diagnostics)
        {
            WARN(d.message);
        }
    }
    REQUIRE(result.ok);
    REQUIRE(result.graph != nullptr);
}