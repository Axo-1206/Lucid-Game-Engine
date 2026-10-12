#include <catch2/catch_test_macros.hpp>

#include "engine/registry/package_manager.h"
#include "engine/registry/packages/core_package.h"
#include "engine/registry/packages/logic_package.h"

#include "sema/NodeKind.hpp"
#include "sema/Sema.hpp"

using namespace engine;

TEST_CASE("package_manager: registers core and logic nodes",
          "[package]")
{
    engine::ecs::World world;
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.load_package(std::make_unique<LogicPackage>()));
    REQUIRE(mgr.apply(world));
    REQUIRE_FALSE(mgr.has_conflicts());

    const auto &reg = mgr.registry();
    REQUIRE(reg.nodeTypes.size() == 7);
    REQUIRE(reg.nodeTypes[0].name == "Engine_Float32Node");
    REQUIRE(reg.nodeTypes[1].name == "Engine_Int32Node");
    REQUIRE(reg.nodeTypes[2].name == "Engine_EveryFrame");
    REQUIRE(reg.nodeTypes[3].name == "Engine_PrintNode");
    REQUIRE(reg.nodeTypes[4].name == "Engine_BoolNode");
    REQUIRE(reg.nodeTypes[5].name == "Engine_LessThanNode");
    REQUIRE(reg.nodeTypes[6].name == "Engine_IfNode");
}

TEST_CASE("package_manager: node kinds and trigger modes",
          "[package]")
{
    engine::ecs::World world;
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.load_package(std::make_unique<LogicPackage>()));
    REQUIRE(mgr.apply(world));

    using lucid::sema::NodeKind;
    const auto &reg = mgr.registry();
    REQUIRE(reg.nodeTypes[0].kind == NodeKind::Value);
    REQUIRE(reg.nodeTypes[2].kind == NodeKind::Trigger);
    REQUIRE(reg.nodeTypes[3].kind == NodeKind::Action);
    REQUIRE(reg.nodeTypes[6].kind == NodeKind::Trigger);

    const auto &modes = mgr.trigger_modes();
    REQUIRE(modes.size() == 7);
    REQUIRE(modes[2].mode == TriggerMode::Source);
    REQUIRE(modes[6].mode == TriggerMode::Gating);
    REQUIRE(modes[0].mode == TriggerMode::NotATrigger);
}

TEST_CASE("package_manager: compile against the merged registry",
          "[package]")
{
    engine::ecs::World world;
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.load_package(std::make_unique<LogicPackage>()));
    REQUIRE(mgr.apply(world));

    const char *source =
        "node frame = Engine_EveryFrame()\n"
        "node cond = Engine_BoolNode(true)\n"
        "node gate = Engine_IfNode(cond) on frame\n"
        "node p = Engine_PrintNode(1.0) on gate\n";

    auto result = lucid::sema::compile(source, "merged.lucid", mgr.registry());
    if (!result.ok)
    {
        for (const auto &d : result.diagnostics)
        {
            WARN(d.message);
        }
    }
    REQUIRE(result.ok);
}

TEST_CASE("package_manager: duplicate package name is refused",
          "[package]")
{
    engine::ecs::World world;
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    // A second CorePackage has the same name.
    REQUIRE_FALSE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.has_conflicts());
}

TEST_CASE("package_manager: executors are parallel to node types",
          "[package]")
{
    engine::ecs::World world;
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.load_package(std::make_unique<LogicPackage>()));
    REQUIRE(mgr.apply(world));

    REQUIRE(mgr.node_executors().size() == mgr.registry().nodeTypes.size());
    REQUIRE(mgr.trigger_modes().size() == mgr.registry().nodeTypes.size());
}