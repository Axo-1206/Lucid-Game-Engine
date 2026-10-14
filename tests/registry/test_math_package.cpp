#include <catch2/catch_test_macros.hpp>

#include "engine/registry/package_manager.h"
#include "engine/registry/packages/math_package.h"
#include "engine/registry/packages/core_package.h"

#include "sema/NodeKind.hpp"
#include "sema/Sema.hpp"

using namespace engine;

TEST_CASE("math_package: registers the scalar nodes", "[package][math]")
{
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<MathPackage>()));
    REQUIRE(mgr.apply_registration_only());
    REQUIRE_FALSE(mgr.has_conflicts());

    const auto &reg = mgr.registry();
    REQUIRE(reg.nodeTypes.size() == 16);

    REQUIRE(reg.nodeTypes[0].name == "Engine_AddFloatNode");
    REQUIRE(reg.nodeTypes[1].name == "Engine_SubFloatNode");
    REQUIRE(reg.nodeTypes[2].name == "Engine_MulFloatNode");
    REQUIRE(reg.nodeTypes[3].name == "Engine_DivFloatNode");
    REQUIRE(reg.nodeTypes[4].name == "Engine_AbsFloatNode");
    REQUIRE(reg.nodeTypes[5].name == "Engine_NegFloatNode");
    REQUIRE(reg.nodeTypes[6].name == "Engine_MinFloatNode");
    REQUIRE(reg.nodeTypes[7].name == "Engine_MaxFloatNode");
    REQUIRE(reg.nodeTypes[8].name == "Engine_ClampFloatNode");
    REQUIRE(reg.nodeTypes[9].name == "Engine_AddIntNode");
    REQUIRE(reg.nodeTypes[10].name == "Engine_SubIntNode");
    REQUIRE(reg.nodeTypes[11].name == "Engine_MulIntNode");
    REQUIRE(reg.nodeTypes[12].name == "Engine_GreaterThanNode");
    REQUIRE(reg.nodeTypes[13].name == "Engine_EqualToNode");
    REQUIRE(reg.nodeTypes[14].name == "Engine_AndNode");
    REQUIRE(reg.nodeTypes[15].name == "Engine_NotNode");

    using lucid::sema::NodeKind;
    for (const auto &nt : reg.nodeTypes)
    {
        REQUIRE(nt.kind == NodeKind::Value);
    }
}

TEST_CASE("math_package: compiles a .lucid file using the math nodes",
          "[package][math]")
{
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<MathPackage>()));
    REQUIRE(mgr.apply_registration_only());

    const char *source =
        "node a = Engine_Float32Node(2.0)\n" // placeholder; requires CorePackage
        ;

    // Compile with the core + math packages so Float32Node exists.
    PackageManager mgr2;
    REQUIRE(mgr2.load_package(std::make_unique<CorePackage>()));
    REQUIRE(mgr2.load_package(std::make_unique<MathPackage>()));
    REQUIRE(mgr2.apply_registration_only());

    (void)source;

    const char *src =
        "node a = Engine_Float32Node(2.0)\n"
        "node b = Engine_Float32Node(3.0)\n"
        "node s = Engine_AddFloatNode(a, b)\n"
        "node frame = Engine_EveryFrame()\n"
        "node p = Engine_PrintNode(s) on frame\n";

    auto result = lucid::sema::compile(src, "math_smoke.lucid",
                                       mgr2.registry());
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