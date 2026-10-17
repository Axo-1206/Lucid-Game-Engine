#include <catch2/catch_test_macros.hpp>

#include "engine/script/compiler.h"
#include "engine/registry/package_manager.h"
#include "engine/registry/packages/core_package.h"
#include "engine/registry/packages/logic_package.h"

#include <memory>

using namespace engine;

namespace
{

    // A manager with the core and logic packages applied
    // (registration only; no World is needed to compile).
    //
    void apply_packages(PackageManager &mgr)
    {
        mgr.load_package(std::make_unique<CorePackage>());
        mgr.load_package(std::make_unique<LogicPackage>());
        mgr.apply_registration_only();
    }

} // namespace

TEST_CASE("script_compiler: check_source on valid source",
          "[script][compiler]")
{
    PackageManager mgr;
    apply_packages(mgr);
    REQUIRE(mgr.applied());

    const char *source =
        "node frame = Engine_EveryFrame_Node()\n"
        "node v = Engine_Float_Node(1.0)\n"
        "node p = Engine_Print_Float_Node(v) on frame\n";

    auto result = check_source(source, "ok.lucid", mgr);
    REQUIRE(result.ok);

    // No Error or Fatal. Warnings and Notes are allowed.
    for (const auto &d : result.diagnostics)
    {
        REQUIRE(d.severity != CompileDiagnostic::Severity::Error);
        REQUIRE(d.severity != CompileDiagnostic::Severity::Fatal);
    }
}

TEST_CASE("script_compiler: check_source on invalid source",
          "[script][compiler]")
{
    PackageManager mgr;
    apply_packages(mgr);
    REQUIRE(mgr.applied());

    const char *source =
        "node v = Engine_NotANode(1.0)\n";

    auto result = check_source(source, "bad.lucid", mgr);
    REQUIRE_FALSE(result.ok);
    REQUIRE_FALSE(result.diagnostics.empty());

    bool found_error_with_location = false;
    for (const auto &d : result.diagnostics)
    {
        if (d.severity == CompileDiagnostic::Severity::Error)
        {
            // The parser/Sema should have given a location.
            REQUIRE(d.line == 1);
            REQUIRE(d.column > 0);
            found_error_with_location = true;
        }
    }
    REQUIRE(found_error_with_location);
}

TEST_CASE("script_compiler: check_source without an applied manager",
          "[script][compiler]")
{
    PackageManager mgr; // not applied
    REQUIRE_FALSE(mgr.applied());

    const char *source = "node v = Engine_Float_Node(1.0)\n";
    auto result = check_source(source, "no_mgr.lucid", mgr);

    REQUIRE_FALSE(result.ok);
    REQUIRE_FALSE(result.diagnostics.empty());
    REQUIRE(result.diagnostics.front().severity == CompileDiagnostic::Severity::Error);
}

TEST_CASE("script_compiler: compile_to_bytes produces bytes",
          "[script][compiler]")
{
    PackageManager mgr;
    apply_packages(mgr);
    REQUIRE(mgr.applied());

    const char *source =
        "node frame = Engine_EveryFrame_Node()\n"
        "node v = Engine_Float_Node(1.0)\n"
        "node p = Engine_Print_Float_Node(v) on frame\n";

    auto result = compile_to_bytes(source, "ok.lucid", mgr);
    REQUIRE(result.ok);
    REQUIRE_FALSE(result.bytes.empty());
}

TEST_CASE("script_compiler: compile_to_bytes on invalid source "
          "returns empty bytes",
          "[script][compiler]")
{
    PackageManager mgr;
    apply_packages(mgr);
    REQUIRE(mgr.applied());

    const char *source =
        "node v = Engine_NotANode(1.0)\n";

    auto result = compile_to_bytes(source, "bad.lucid", mgr);
    REQUIRE_FALSE(result.ok);
    REQUIRE(result.bytes.empty());
    REQUIRE_FALSE(result.diagnostics.empty());
}

TEST_CASE("engine::check_source is idempotent", "[reload][engine]")
{
    engine::PackageManager pm;
    REQUIRE(pm.load_package(std::make_unique<engine::CorePackage>()));
    REQUIRE(pm.apply_registration_only());

    const std::string src = "node A = Engine_Float_Node(1.0)\n";

    const auto a = engine::check_source(src, "a.lucid", pm);
    const auto b = engine::check_source(src, "a.lucid", pm);
    const auto c = engine::check_source(src, "a.lucid", pm);

    UNSCOPED_INFO("a: ok=" << a.ok << " diags=" << a.diagnostics.size());
    UNSCOPED_INFO("b: ok=" << b.ok << " diags=" << b.diagnostics.size());
    UNSCOPED_INFO("c: ok=" << c.ok << " diags=" << c.diagnostics.size());
    for (const auto &d : a.diagnostics)
        UNSCOPED_INFO("a diag: " << d.message);
    for (const auto &d : b.diagnostics)
        UNSCOPED_INFO("b diag: " << d.message);
    for (const auto &d : c.diagnostics)
        UNSCOPED_INFO("c diag: " << d.message);

    REQUIRE(a.ok == b.ok);
    REQUIRE(a.ok == c.ok);
    REQUIRE(a.diagnostics.size() == b.diagnostics.size());
    REQUIRE(a.diagnostics.size() == c.diagnostics.size());
}