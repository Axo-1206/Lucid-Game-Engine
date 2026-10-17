// tests/editor/test_reload_packages.cpp
//
// Tests for Step 2 — Reload Packages:
//   - ScriptEditorPanel::recompile_all refreshes diagnostics across open documents
//   - Preserves buffer text and re-evaluates errors against the PackageManager
//
// Note on warnings: Sema reports dead-code warnings (Warn_DeadNode,
// Warn_UnusedNode) for value nodes that nothing references. Those are
// diagnostics, not errors. A "clean" file in these tests means "no
// errors"; warnings are allowed and expected. The assertions check
// errors, not the total diagnostic count.

#include <catch2/catch_test_macros.hpp>

#include "editor/diagnostics.h"
#include "editor/panels/script_editor_panel.h"

#include "engine/registry/package_manager.h"
#include "engine/registry/packages/core_package.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace editor;

namespace
{

    struct TempDir
    {
        fs::path path;

        TempDir()
        {
            path = fs::temp_directory_path() /
                   ("lucid_reload_test_" +
                    std::to_string(reinterpret_cast<std::uintptr_t>(this)));
            fs::remove_all(path);
            fs::create_directories(path);
        }

        ~TempDir()
        {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    };

    void write_file(const fs::path &p, const std::string &content)
    {
        fs::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary);
        out << content;
    }

    // True if the entries contain at least one Error or Fatal.
    bool has_errors(const std::vector<DiagnosticEntry> &entries)
    {
        using S = engine::CompileDiagnostic::Severity;
        for (const auto &e : entries)
            if (e.severity == S::Error || e.severity == S::Fatal)
                return true;
        return false;
    }

    // The name of the first registered Value node whose result type
    // is float32. Lets the test construct a clean .lucid without
    // hard-coding a name that might change in core_package.cpp.
    std::string a_float_value_node_name(const engine::PackageManager &pm)
    {
        for (const auto &nt : pm.registry().nodeTypes)
        {
            using lucid::sema::NodeKind;
            if (nt.kind == NodeKind::Value &&
                nt.resultType.isPrimitive() &&
                nt.resultType.name == "float32")
            {
                return std::string(nt.name);
            }
        }
        return {};
    }

    std::string a_missing_node_name(const engine::PackageManager &pm)
    {
        // A name that is guaranteed not to be registered.
        for (int i = 0; i < 1000; ++i)
        {
            const std::string candidate = "Engine_NotANode_" + std::to_string(i);
            bool found = false;
            for (const auto &nt : pm.registry().nodeTypes)
                if (nt.name == candidate)
                {
                    found = true;
                    break;
                }
            if (!found)
                return candidate;
        }
        return "Engine_NotANode_definitely_missing";
    }

} // namespace

TEST_CASE("ScriptEditorPanel: recompile_all refreshes diagnostics", "[reload]")
{
    engine::PackageManager pm;
    REQUIRE(pm.load_package(std::make_unique<engine::CorePackage>()));
    REQUIRE(pm.apply_registration_only());

    const std::string good_node = a_float_value_node_name(pm);
    REQUIRE_FALSE(good_node.empty());

    TempDir tmp;
    const auto good = tmp.path / "good.lucid";
    const auto bad = tmp.path / "bad.lucid";

    // `good` references a real node. It may still produce warnings
    // (e.g. Warn_DeadNode — the value is declared but unused); the
    // test checks for errors only.
    write_file(good, "node A = " + good_node + "(1.0)\n");

    // `bad` references a node that does not exist. That produces an
    // error (Name_UndefinedNode or Type_UnknownNodeType).
    write_file(bad, "node B = " + a_missing_node_name(pm) + "(1.0)\n");

    DiagnosticStore store;
    ScriptEditorPanel panel;
    panel.set_package_manager(&pm);
    panel.set_diagnostic_store(&store);

    std::string error;
    REQUIRE(panel.open(good, error) >= 0);
    REQUIRE(panel.open(bad, error) >= 0);

    // First compile: bad has errors, good is error-free.
    {
        const auto g = store.for_file(good);
        for (const auto &d : g)
            UNSCOPED_INFO("first good: " << d.message);
        REQUIRE_FALSE(has_errors(g));

        const auto b = store.for_file(bad);
        for (const auto &d : b)
            UNSCOPED_INFO("first bad: " << d.message);
        REQUIRE(has_errors(b));
    }

    // Clear and recompile.
    store.clear();
    REQUIRE(store.all().empty());
    panel.recompile_all();

    // Second compile: same error status.
    {
        const auto g = store.for_file(good);
        for (const auto &d : g)
            UNSCOPED_INFO("second good: " << d.message);
        REQUIRE_FALSE(has_errors(g));

        const auto b = store.for_file(bad);
        for (const auto &d : b)
            UNSCOPED_INFO("second bad: " << d.message);
        REQUIRE(has_errors(b));
    }
}