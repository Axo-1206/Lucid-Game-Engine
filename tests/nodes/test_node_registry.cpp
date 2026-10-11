#include <catch2/catch_test_macros.hpp>

#include "engine/nodes/node_registry.h"
#include "engine/nodes/node_runtime.h"

#include "sema/Sema.hpp"

using namespace engine;

TEST_CASE("node_registry: shape", "[nodes][registry]")
{
    const auto &reg = node_registry();

    REQUIRE(reg.phases.size() == 3);
    REQUIRE(reg.enums.size() == 1);
    REQUIRE(reg.handles.size() == 1);
    REQUIRE(reg.nodeTypes.size() == 7);

    REQUIRE(reg.phases[0].name == "update");
    REQUIRE(reg.phases[1].name == "physics");
    REQUIRE(reg.phases[2].name == "render");

    REQUIRE(reg.nodeTypes[0].name == "Float32Node");
    REQUIRE(reg.nodeTypes[1].name == "Int32Node");
    REQUIRE(reg.nodeTypes[2].name == "EveryFrame");
    REQUIRE(reg.nodeTypes[3].name == "PrintNode");
    REQUIRE(reg.nodeTypes[4].name == "BoolNode");
    REQUIRE(reg.nodeTypes[5].name == "LessThanNode");
    REQUIRE(reg.nodeTypes[6].name == "IfNode");
}

TEST_CASE("node_registry: node kinds", "[nodes][registry]")
{
    const auto &reg = node_registry();

    using lucid::sema::NodeKind;

    REQUIRE(reg.nodeTypes[0].name == "Float32Node");
    REQUIRE(reg.nodeTypes[0].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[1].name == "Int32Node");
    REQUIRE(reg.nodeTypes[1].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[2].name == "EveryFrame");
    REQUIRE(reg.nodeTypes[2].kind == NodeKind::Trigger);

    REQUIRE(reg.nodeTypes[3].name == "PrintNode");
    REQUIRE(reg.nodeTypes[3].kind == NodeKind::Action);

    REQUIRE(reg.nodeTypes[4].name == "BoolNode");
    REQUIRE(reg.nodeTypes[4].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[5].name == "LessThanNode");
    REQUIRE(reg.nodeTypes[5].kind == NodeKind::Value);

    REQUIRE(reg.nodeTypes[6].name == "IfNode");
    REQUIRE(reg.nodeTypes[6].kind == NodeKind::Trigger);
}

TEST_CASE("node_registry: runtime info parallels nodeTypes", "[nodes][registry]")
{
    REQUIRE(node_runtime_info_count() == node_registry().nodeTypes.size());

    REQUIRE(node_runtime_info(0).mode == TriggerMode::NotATrigger); // Float32Node
    REQUIRE(node_runtime_info(1).mode == TriggerMode::NotATrigger); // Int32Node
    REQUIRE(node_runtime_info(2).mode == TriggerMode::Source);      // EveryFrame
    REQUIRE(node_runtime_info(3).mode == TriggerMode::NotATrigger); // PrintNode
    REQUIRE(node_runtime_info(4).mode == TriggerMode::NotATrigger); // BoolNode
    REQUIRE(node_runtime_info(5).mode == TriggerMode::NotATrigger); // LessThanNode
    REQUIRE(node_runtime_info(6).mode == TriggerMode::Gating);      // IfNode
}

TEST_CASE("node_registry: compile a trivial file", "[nodes][compile]")
{
    const char *source =
        "node v = Float32Node(1.0)\n"
        "node frame = EveryFrame()\n"
        "node p = PrintNode(v) on frame\n";

    auto result = lucid::sema::compile(source, "smoke.lucid", node_registry());

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

TEST_CASE("node_registry: compile a gated file", "[nodes][compile]")
{
    const char *source =
        "node frame = EveryFrame()\n"
        "node cond = BoolNode(true)\n"
        "node gate = IfNode(cond) on frame\n"
        "node p = PrintNode(1.0) on gate\n";

    auto result = lucid::sema::compile(source, "gate.lucid", node_registry());
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