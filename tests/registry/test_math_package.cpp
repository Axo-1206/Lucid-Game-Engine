#include <catch2/catch_test_macros.hpp>

#include "engine/registry/package_manager.h"
#include "engine/registry/packages/math_package.h"
#include "engine/registry/packages/core_package.h"

#include "sema/NodeKind.hpp"
#include "sema/Sema.hpp"

using namespace engine;

TEST_CASE("math_package: registers the arithmetic and vector nodes",
          "[package][math]")
{
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<MathPackage>()));
    REQUIRE(mgr.apply_registration_only());
    REQUIRE_FALSE(mgr.has_conflicts());

    const auto &reg = mgr.registry();
    REQUIRE(reg.nodeTypes.size() == 20);

    // Float arithmetic (0..8).
    REQUIRE(reg.nodeTypes[0].name == "Engine_AddFloatNode");
    REQUIRE(reg.nodeTypes[1].name == "Engine_SubFloatNode");
    REQUIRE(reg.nodeTypes[2].name == "Engine_MulFloatNode");
    REQUIRE(reg.nodeTypes[3].name == "Engine_DivFloatNode");
    REQUIRE(reg.nodeTypes[4].name == "Engine_AbsFloatNode");
    REQUIRE(reg.nodeTypes[5].name == "Engine_NegFloatNode");
    REQUIRE(reg.nodeTypes[6].name == "Engine_MinFloatNode");
    REQUIRE(reg.nodeTypes[7].name == "Engine_MaxFloatNode");
    REQUIRE(reg.nodeTypes[8].name == "Engine_ClampFloatNode");

    // Int arithmetic (9..11).
    REQUIRE(reg.nodeTypes[9].name == "Engine_AddIntNode");
    REQUIRE(reg.nodeTypes[10].name == "Engine_SubIntNode");
    REQUIRE(reg.nodeTypes[11].name == "Engine_MulIntNode");

    // Conversions (12..13).
    REQUIRE(reg.nodeTypes[12].name == "Engine_IntToFloatNode");
    REQUIRE(reg.nodeTypes[13].name == "Engine_FloatToIntNode");

    // Vector2 (14..19).
    REQUIRE(reg.nodeTypes[14].name == "Engine_Vector2Node");
    REQUIRE(reg.nodeTypes[15].name == "Engine_Vector2XNode");
    REQUIRE(reg.nodeTypes[16].name == "Engine_Vector2YNode");
    REQUIRE(reg.nodeTypes[17].name == "Engine_Vector2AddNode");
    REQUIRE(reg.nodeTypes[18].name == "Engine_Vector2SubNode");
    REQUIRE(reg.nodeTypes[19].name == "Engine_Vector2ScaleNode");

    using lucid::sema::NodeKind;
    for (const auto &nt : reg.nodeTypes)
    {
        REQUIRE(nt.kind == NodeKind::Value);
    }
}

TEST_CASE("math_package: compiles a .lucid file using the math nodes",
          "[package][math]")
{
    // Core is needed for Float32Node, EveryFrame, and PrintNode.
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr.load_package(std::make_unique<MathPackage>()));
    REQUIRE(mgr.apply_registration_only());

    const char *src =
        "node a = Engine_Float32Node(2.0)\n"
        "node b = Engine_Float32Node(3.0)\n"
        "node s = Engine_AddFloatNode(a, b)\n"
        "node frame = Engine_EveryFrame()\n"
        "node p = Engine_PrintNode(s) on frame\n";

    auto result = lucid::sema::compile(src, "math_smoke.lucid",
                                       mgr.registry());
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

TEST_CASE("math_package: registers the vector nodes and the struct",
          "[package][math][vector]")
{
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<MathPackage>()));
    REQUIRE(mgr.apply_registration_only());

    const auto &reg = mgr.registry();
    REQUIRE(reg.structs.size() == 1);
    REQUIRE(reg.structs[0].name == "Engine_Vector2");
    REQUIRE(reg.structs[0].fields.size() == 2);
    REQUIRE(reg.structs[0].fields[0].name == "x");
    REQUIRE(reg.structs[0].fields[1].name == "y");
}