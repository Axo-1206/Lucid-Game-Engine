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
    REQUIRE(reg.nodeTypes[0].name == "Engine_Add_Float_Node");
    REQUIRE(reg.nodeTypes[1].name == "Engine_Sub_Float_Node");
    REQUIRE(reg.nodeTypes[2].name == "Engine_Mul_Float_Node");
    REQUIRE(reg.nodeTypes[3].name == "Engine_Div_Float_Node");
    REQUIRE(reg.nodeTypes[4].name == "Engine_Abs_Float_Node");
    REQUIRE(reg.nodeTypes[5].name == "Engine_Neg_Float_Node");
    REQUIRE(reg.nodeTypes[6].name == "Engine_Min_Float_Node");
    REQUIRE(reg.nodeTypes[7].name == "Engine_Max_Float_Node");
    REQUIRE(reg.nodeTypes[8].name == "Engine_Clamp_Float_Float_Float_Node");

    // Int arithmetic (9..11).
    REQUIRE(reg.nodeTypes[9].name == "Engine_Add_Int_Node");
    REQUIRE(reg.nodeTypes[10].name == "Engine_Sub_Int_Node");
    REQUIRE(reg.nodeTypes[11].name == "Engine_Mul_Int_Node");

    // Conversions (12..13).
    REQUIRE(reg.nodeTypes[12].name == "Engine_IntToFloat_Int_Node");
    REQUIRE(reg.nodeTypes[13].name == "Engine_FloatToInt_Float_Node");

    // Vector2 (14..19).
    REQUIRE(reg.nodeTypes[14].name == "Engine_Vector2_Float_Float_Node");
    REQUIRE(reg.nodeTypes[15].name == "Engine_X_Vector2_Node");
    REQUIRE(reg.nodeTypes[16].name == "Engine_Y_Vector2_Node");
    REQUIRE(reg.nodeTypes[17].name == "Engine_Add_Vector2_Vector2_Node");
    REQUIRE(reg.nodeTypes[18].name == "Engine_Sub_Vector2_Vector2_Node");
    REQUIRE(reg.nodeTypes[19].name == "Engine_Scale_Vector2_Float_Node");

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
        "node a = Engine_Float_Node(2.0)\n"
        "node b = Engine_Float_Node(3.0)\n"
        "node s = Engine_Add_Float_Node(a, b)\n"
        "node frame = Engine_EveryFrame_Node()\n"
        "node p = Engine_Print_Float_Node(s) on frame\n";

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

TEST_CASE("math_package: node editor info carries the operation",
          "[package][math][operation]")
{
    PackageManager mgr;
    REQUIRE(mgr.load_package(std::make_unique<MathPackage>()));
    REQUIRE(mgr.apply_registration_only());

    REQUIRE(mgr.node_editor_info("Engine_Add_Float_Node")->operation == Operation::Add);
    REQUIRE(mgr.node_editor_info("Engine_Add_Int_Node")->operation == Operation::Add);
    REQUIRE(mgr.node_editor_info("Engine_Add_Vector2_Vector2_Node")->operation == Operation::Add);
    REQUIRE(mgr.node_editor_info("Engine_Scale_Vector2_Float_Node")->operation == Operation::Mul);
    REQUIRE(mgr.node_editor_info("Engine_IntToFloat_Int_Node")->operation == Operation::IntToFloat);
    REQUIRE(mgr.node_editor_info("Engine_Vector2_Float_Float_Node")->operation == Operation::Vector2Ctor);
}