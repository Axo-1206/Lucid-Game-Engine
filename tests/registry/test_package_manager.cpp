#include <catch2/catch_test_macros.hpp>

#include "engine/registry/package_manager.h"
#include "engine/registry/packages/core_package.h"
#include "engine/registry/packages/logic_package.h"
#include "engine/registry/editor_info.h"
#include "engine/ecs/core/entity.h"

#include "sema/NodeKind.hpp"
#include "sema/Sema.hpp"

using namespace engine;

TEST_CASE("package_manager: registers core and logic nodes",
          "[package]")
{
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.load_package(std::make_unique<LogicPackage>()));
    REQUIRE(mgr.apply_registration_only());
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

TEST_CASE("package_manager: package_infos includes both packages",
          "[package][editor]")
{
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.load_package(std::make_unique<LogicPackage>()));
    REQUIRE(mgr.apply_registration_only());

    const auto &infos = mgr.package_infos();
    REQUIRE(infos.size() == 2);

    REQUIRE(infos[0].name == "engine.core");
    REQUIRE(infos[0].prefix == "Engine");
    REQUIRE(infos[0].version == "0.1.0");
    REQUIRE(infos[0].description == "The engine's core nodes.");
    REQUIRE(infos[0].author == "Axo");
    REQUIRE(infos[0].node_type_count == 4);

    REQUIRE(infos[1].name == "engine.logic");
    REQUIRE(infos[1].prefix == "Engine");
    REQUIRE(infos[1].node_type_count == 3);
}

TEST_CASE("package_manager: explicit editor info is returned",
          "[package][editor]")
{
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.load_package(std::make_unique<LogicPackage>()));
    REQUIRE(mgr.apply_registration_only());

    const NodeEditorInfo *info = mgr.node_editor_info("Engine_PrintNode");
    REQUIRE(info != nullptr);
    REQUIRE(info->display_name == "Print");
    REQUIRE(info->doc_summary == "Print a value to the engine log.");
    REQUIRE(info->icon_id == "print");
}

TEST_CASE("package_manager: unknown node type returns nullptr",
          "[package][editor]")
{
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.apply_registration_only());

    REQUIRE(mgr.node_editor_info("Engine_NotANode") == nullptr);
    REQUIRE(mgr.node_editor_info("Alice_GravityNode") == nullptr);
}

TEST_CASE("package_manager: table editor info falls back",
          "[package][editor]")
{
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.apply_registration_only());

    const TableEditorInfo *info = mgr.table_editor_info("Engine_Entity");
    REQUIRE(info != nullptr);
    REQUIRE(info->display_name == "Entity");
}

TEST_CASE("package_manager: apply_registration_only twice is a no-op",
          "[package]")
{
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.apply_registration_only());
    REQUIRE(mgr.applied());

    // Second call: logs a warning, does not corrupt state.
    REQUIRE(mgr.apply_registration_only());
    REQUIRE(mgr.applied());

    // The registry is unchanged.
    REQUIRE(mgr.registry().nodeTypes.size() == 4);
}

TEST_CASE("package_manager: apply twice is a no-op",
          "[package]")
{
    engine::ecs::World world;
    engine::setup_core_tables(world);

    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.apply(world));
    REQUIRE(mgr.applied());

    // Second call: logs a warning, does not corrupt state, does not
    // throw (the world already has the core tables; a real re-apply
    // would try to create them again and throw).
    REQUIRE(mgr.apply(world));
    REQUIRE(mgr.applied());

    REQUIRE(mgr.registry().nodeTypes.size() == 4);
}

TEST_CASE("package_manager: apply after apply_registration_only is a no-op",
          "[package]")
{
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.apply_registration_only());
    REQUIRE(mgr.applied());

    engine::ecs::World world;
    engine::setup_core_tables(world);

    // The manager is already applied; this call is refused. It must
    // not throw (a real apply would try to create the package's
    // tables, and CorePackage registers zero tables, so this is not
    // observed).
    REQUIRE(mgr.apply(world));
    REQUIRE(mgr.applied());
    REQUIRE(mgr.registry().nodeTypes.size() == 4);
}

TEST_CASE("package_manager: apply_registration_only after apply is a no-op",
          "[package]")
{
    engine::ecs::World world;
    engine::setup_core_tables(world);

    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.apply(world));
    REQUIRE(mgr.applied());

    REQUIRE(mgr.apply_registration_only());
    REQUIRE(mgr.applied());
    REQUIRE(mgr.registry().nodeTypes.size() == 4);
}