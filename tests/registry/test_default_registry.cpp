#include <catch2/catch_test_macros.hpp>

#include "engine/registry/default_registry.h"

#include "sema/NodeKind.hpp"
#include "sema/Sema.hpp"

#include <cstdint>
#include <string_view>

using namespace engine;

namespace
{

    // The registry's node types, looked up by name.
    //
    // The tests in this file spot-check the built-in packages. The
    // exact shape of each package is asserted by that package's
    // own test (test_package_manager.cpp for core and logic,
    // test_math_package.cpp for math). This file checks that the
    // shim builds a registry with three phases and each package's
    // content present.

    const lucid::sema::NodeTypeInfo *find_node(
        const lucid::sema::Registry &reg,
        std::string_view name)
    {
        for (const auto &nt : reg.nodeTypes)
        {
            if (nt.name == name)
                return &nt;
        }
        return nullptr;
    }

    std::uint32_t find_node_index(
        const lucid::sema::Registry &reg,
        std::string_view name)
    {
        for (std::size_t i = 0; i < reg.nodeTypes.size(); ++i)
        {
            if (reg.nodeTypes[i].name == name)
                return static_cast<std::uint32_t>(i);
        }
        return UINT32_MAX;
    }

    bool has_enum(const lucid::sema::Registry &reg, std::string_view name)
    {
        for (const auto &e : reg.enums)
        {
            if (e.name == name)
                return true;
        }
        return false;
    }

    bool has_handle(const lucid::sema::Registry &reg, std::string_view name)
    {
        for (const auto &h : reg.handles)
        {
            if (h.name == name)
                return true;
        }
        return false;
    }

} // namespace

TEST_CASE("default_registry: shape", "[nodes][registry]")
{
    const auto &reg = default_registry();

    REQUIRE(reg.phases.size() == 3);
    REQUIRE(reg.phases[0].name == "update");
    REQUIRE(reg.phases[1].name == "physics");
    REQUIRE(reg.phases[2].name == "render");

    REQUIRE(has_enum(reg, "Engine_Key"));
    REQUIRE(has_handle(reg, "Engine_BodyRef"));

    // One node from each built-in package. The exact list lives
    // in the per-package tests.
    REQUIRE(find_node(reg, "Engine_Float_Node") != nullptr);               // core
    REQUIRE(find_node(reg, "Engine_EveryFrame_Node") != nullptr);          // core
    REQUIRE(find_node(reg, "Engine_If_Bool_Node") != nullptr);             // logic
    REQUIRE(find_node(reg, "Engine_Add_Float_Node") != nullptr);           // math
    REQUIRE(find_node(reg, "Engine_Vector2_Float_Float_Node") != nullptr); // math
}

TEST_CASE("default_registry: node kinds", "[nodes][registry]")
{
    const auto &reg = default_registry();

    using lucid::sema::NodeKind;

    // Core.
    {
        const auto *nt = find_node(reg, "Engine_Float_Node");
        REQUIRE(nt != nullptr);
        REQUIRE(nt->kind == NodeKind::Value);
    }
    {
        const auto *nt = find_node(reg, "Engine_EveryFrame_Node");
        REQUIRE(nt != nullptr);
        REQUIRE(nt->kind == NodeKind::Trigger);
    }
    {
        const auto *nt = find_node(reg, "Engine_Print_Float_Node");
        REQUIRE(nt != nullptr);
        REQUIRE(nt->kind == NodeKind::Action);
    }
    {
        const auto *nt = find_node(reg, "Engine_Print_Bool_Node");
        REQUIRE(nt != nullptr);
        REQUIRE(nt->kind == NodeKind::Action);
    }

    // Logic.
    {
        const auto *nt = find_node(reg, "Engine_If_Bool_Node");
        REQUIRE(nt != nullptr);
        REQUIRE(nt->kind == NodeKind::Trigger);
    }
    {
        const auto *nt = find_node(reg, "Engine_LessThan_Int_Int_Node");
        REQUIRE(nt != nullptr);
        REQUIRE(nt->kind == NodeKind::Value);
    }

    // Math.
    {
        const auto *nt = find_node(reg, "Engine_Add_Float_Node");
        REQUIRE(nt != nullptr);
        REQUIRE(nt->kind == NodeKind::Value);
    }
    {
        const auto *nt = find_node(reg, "Engine_Vector2_Float_Float_Node");
        REQUIRE(nt != nullptr);
        REQUIRE(nt->kind == NodeKind::Value);
    }
}

TEST_CASE("default_registry: runtime info parallels nodeTypes",
          "[nodes][registry]")
{
    const auto &reg = default_registry();
    REQUIRE(node_runtime_info_count() == reg.nodeTypes.size());

    // A source trigger and a gating trigger. The rest are
    // NotATrigger; the per-package tests assert them.
    {
        const auto idx = find_node_index(reg, "Engine_EveryFrame_Node");
        REQUIRE(idx != UINT32_MAX);
        REQUIRE(node_runtime_info(idx).mode == TriggerMode::Source);
    }
    {
        const auto idx = find_node_index(reg, "Engine_If_Bool_Node");
        REQUIRE(idx != UINT32_MAX);
        REQUIRE(node_runtime_info(idx).mode == TriggerMode::Gating);
    }
    {
        const auto idx = find_node_index(reg, "Engine_Float_Node");
        REQUIRE(idx != UINT32_MAX);
        REQUIRE(node_runtime_info(idx).mode == TriggerMode::NotATrigger);
    }
    {
        const auto idx = find_node_index(reg, "Engine_Add_Float_Node");
        REQUIRE(idx != UINT32_MAX);
        REQUIRE(node_runtime_info(idx).mode == TriggerMode::NotATrigger);
    }
}

TEST_CASE("default_registry: compile a trivial file", "[nodes][compile]")
{
    const char *source =
        "node v = Engine_Float_Node(1.0)\n"
        "node frame = Engine_EveryFrame_Node()\n"
        "node p = Engine_Print_Float_Node(v) on frame\n";

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
        "node frame = Engine_EveryFrame_Node()\n"
        "node cond = Engine_Bool_Node(true)\n"
        "node gate = Engine_If_Bool_Node(cond) on frame\n"
        "node p = Engine_Print_Float_Node(1.0) on gate\n";

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