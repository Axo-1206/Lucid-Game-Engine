// tests/editor/test_diagnostics_panel.cpp

#include <catch2/catch_test_macros.hpp>

#include "editor/runtime_diagnostics.h"

#include "engine/core/diagnostics.h"

#include <string>

namespace
{
    engine::RuntimeDiagnostic make_diag(std::uint32_t code,
                                        engine::LogLevel sev,
                                        const std::string &file,
                                        std::uint32_t line,
                                        const std::string &message)
    {
        engine::RuntimeDiagnostic d;
        d.code = code;
        d.severity = sev;
        d.file = file;
        d.line = line;
        d.message = message;
        return d;
    }
}

TEST_CASE("RuntimeDiagnosticStore: push and snapshot", "[diagnostics_panel]")
{
    editor::RuntimeDiagnosticStore store;

    store.push(make_diag(6041, engine::LogLevel::Error,
                         "a.lucgraph", 0, "mismatch"));
    store.push(make_diag(4001, engine::LogLevel::Error,
                         "b.lucid", 12, "type mismatch"));
    store.push(make_diag(0, engine::LogLevel::Warn,
                         "", 0, "conflict"));

    REQUIRE(store.size() == 3);
    REQUIRE(store.error_count() == 2);
    REQUIRE(store.warning_count() == 1);

    const auto snap = store.snapshot();
    REQUIRE(snap.size() == 3);
    REQUIRE(snap[0].code == 6041);
    REQUIRE(snap[1].file == std::filesystem::path("b.lucid"));
    REQUIRE(snap[1].line == 12);
}

TEST_CASE("RuntimeDiagnosticStore: cap drops oldest entries",
          "[diagnostics_panel]")
{
    editor::RuntimeDiagnosticStore store(3);

    for (int i = 0; i < 5; ++i)
    {
        store.push(make_diag(static_cast<std::uint32_t>(i),
                             engine::LogLevel::Info, "", 0,
                             "msg" + std::to_string(i)));
    }

    REQUIRE(store.size() == 3);
    const auto snap = store.snapshot();
    REQUIRE(snap[0].message == "msg2");
    REQUIRE(snap[2].message == "msg4");
}

TEST_CASE("RuntimeDiagnosticStore: clear removes everything",
          "[diagnostics_panel]")
{
    editor::RuntimeDiagnosticStore store;
    store.push(make_diag(1, engine::LogLevel::Error, "", 0, "x"));
    store.clear();
    REQUIRE(store.size() == 0);
    REQUIRE(store.snapshot().empty());
    REQUIRE(store.error_count() == 0);
}

TEST_CASE("RuntimeDiagnosticStore: a sink feeds the store end-to-end",
          "[diagnostics_panel]")
{
    // The panel's attach_to_engine registers a sink that pushes into
    // the store. This test replicates that wiring without ImGui:
    // register a sink that pushes into a store, emit a diagnostic,
    // and verify it arrived.
    engine::clear_diagnostic_sinks();

    editor::RuntimeDiagnosticStore store;
    const auto id = engine::add_diagnostic_sink(
        [&store](const engine::RuntimeDiagnostic &d)
        { store.push(d); });
    REQUIRE(id != 0);

    engine::emit_diagnostic(make_diag(6041, engine::LogLevel::Error,
                                      "scene.lucgraph", 0, "mismatch"));

    engine::remove_diagnostic_sink(id);

    REQUIRE(store.size() == 1);
    const auto snap = store.snapshot();
    REQUIRE(snap[0].code == 6041);
    REQUIRE(snap[0].file == std::filesystem::path("scene.lucgraph"));
}