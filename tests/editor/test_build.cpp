#include <catch2/catch_test_macros.hpp>

#include "editor/build.h"
#include "editor/diagnostics.h"

#include "engine/registry/package_manager.h"
#include "engine/registry/packages/core_package.h"
#include "engine/registry/packages/logic_package.h"

#include <filesystem>
#include <fstream>
#include <memory>

using namespace editor;

namespace
{

    struct TempDir
    {
        std::filesystem::path path;
        TempDir()
        {
            path = std::filesystem::temp_directory_path() /
                   ("lucid_build_tests_" +
                    std::to_string(reinterpret_cast<std::uintptr_t>(this)));
            std::filesystem::remove_all(path);
            std::filesystem::create_directories(path);
        }
        ~TempDir()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    };

    void write_file(const std::filesystem::path &p, const std::string &content)
    {
        std::filesystem::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary);
        out << content;
    }

    void make_packages(engine::PackageManager &mgr)
    {
        mgr.load_package(std::make_unique<engine::CorePackage>());
        mgr.load_package(std::make_unique<engine::LogicPackage>());
        mgr.apply_registration_only();
    }

}

TEST_CASE("build: compiles a valid script and writes .lucgraph", "[build]")
{
    TempDir tmp;
    const auto root = tmp.path;

    write_file(root / "scripts" / "ok.lucid",
               "node frame = Engine_EveryFrame_Node()\n"
               "node v = Engine_Float_Node(1.0)\n"
               "node p = Engine_Print_Float_Node(v) on frame\n");

    engine::PackageManager mgr;
    make_packages(mgr);
    REQUIRE(mgr.applied());
    DiagnosticStore store;

    const auto result = compile_project(root, mgr, store);
    REQUIRE(result.files_total == 1);
    REQUIRE(result.files_ok == 1);
    REQUIRE(result.files_failed == 0);
    REQUIRE(result.bytes_written > 0);
    REQUIRE(std::filesystem::exists(root / "scripts" / "ok.lucgraph"));
}

TEST_CASE("build: a failing script does not write .lucgraph", "[build]")
{
    TempDir tmp;
    const auto root = tmp.path;

    write_file(root / "scripts" / "bad.lucid",
               "node v = Engine_NotANode(1.0)\n");

    engine::PackageManager mgr;
    make_packages(mgr);
    DiagnosticStore store;
    const auto result = compile_project(root, mgr, store);

    REQUIRE(result.files_total == 1);
    REQUIRE(result.files_ok == 0);
    REQUIRE(result.files_failed == 1);
    REQUIRE_FALSE(std::filesystem::exists(root / "scripts" / "bad.lucgraph"));
    REQUIRE_FALSE(result.diagnostics.empty());
    REQUIRE_FALSE(store.for_file(root / "scripts" / "bad.lucid").empty());
}

TEST_CASE("build: empty scripts dir yields zero files", "[build]")
{
    TempDir tmp;
    std::filesystem::create_directories(tmp.path / "scripts");

    engine::PackageManager mgr;
    make_packages(mgr);
    DiagnosticStore store;
    const auto result = compile_project(tmp.path, mgr, store);

    REQUIRE(result.files_total == 0);
    REQUIRE(result.files_ok == 0);
    REQUIRE(result.files_failed == 0);
}

TEST_CASE("build: recursive walk finds nested scripts", "[build]")
{
    TempDir tmp;
    const auto root = tmp.path;

    write_file(root / "scripts" / "a.lucid",
               "node frame = Engine_EveryFrame_Node()\n"
               "node p = Engine_Print_Float_Node(1.0) on frame\n");
    write_file(root / "scripts" / "sub" / "b.lucid",
               "node frame = Engine_EveryFrame_Node()\n"
               "node p = Engine_Print_Float_Node(2.0) on frame\n");

    engine::PackageManager mgr;
    make_packages(mgr);
    DiagnosticStore store;
    const auto result = compile_project(root, mgr, store);

    REQUIRE(result.files_total == 2);
    REQUIRE(result.files_ok == 2);
    REQUIRE(std::filesystem::exists(root / "scripts" / "a.lucgraph"));
    REQUIRE(std::filesystem::exists(root / "scripts" / "sub" / "b.lucgraph"));
}

TEST_CASE("build: without an applied manager reports an error", "[build]")
{
    TempDir tmp;
    write_file(tmp.path / "scripts" / "x.lucid", "");

    engine::PackageManager mgr; // not applied
    DiagnosticStore store;
    const auto result = compile_project(tmp.path, mgr, store);

    REQUIRE(result.files_total == 0);
    REQUIRE_FALSE(result.diagnostics.empty());
    REQUIRE(result.diagnostics.front().severity == engine::CompileDiagnostic::Severity::Error);
}