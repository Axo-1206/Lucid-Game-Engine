#include <catch2/catch_test_macros.hpp>

#include "editor/diagnostics.h"

using namespace editor;

namespace
{
    DiagnosticEntry make_entry(const std::filesystem::path &file,
                               engine::CompileDiagnostic::Severity sev,
                               const std::string &msg)
    {
        DiagnosticEntry e;
        e.file = file;
        e.severity = sev;
        e.message = msg;
        e.line = 1;
        return e;
    }
}

TEST_CASE("diagnostic_store: replace_for_file replaces per file",
          "[diagnostics]")
{
    DiagnosticStore store;

    store.replace_for_file("/a.lucid",
                           {make_entry("/a.lucid",
                                       engine::CompileDiagnostic::Severity::Error,
                                       "first")});
    REQUIRE(store.size() == 1);

    store.replace_for_file("/a.lucid",
                           {make_entry("/a.lucid",
                                       engine::CompileDiagnostic::Severity::Warning,
                                       "second"),
                            make_entry("/a.lucid",
                                       engine::CompileDiagnostic::Severity::Note,
                                       "third")});
    REQUIRE(store.size() == 2);
    REQUIRE(store.all()[0].message == "second");
    REQUIRE(store.all()[1].message == "third");
}

TEST_CASE("diagnostic_store: replace_for_file leaves other files alone",
          "[diagnostics]")
{
    DiagnosticStore store;
    store.replace_for_file("/a.lucid",
                           {make_entry("/a.lucid",
                                       engine::CompileDiagnostic::Severity::Error,
                                       "error a")});
    store.replace_for_file("/b.lucid",
                           {make_entry("/b.lucid",
                                       engine::CompileDiagnostic::Severity::Error,
                                       "error b")});

    store.replace_for_file("/a.lucid",
                           {make_entry("/a.lucid",
                                       engine::CompileDiagnostic::Severity::Warning,
                                       "new a")});

    REQUIRE(store.size() == 2);

    const auto a = store.for_file("/a.lucid");
    REQUIRE(a.size() == 1);
    REQUIRE(a[0].message == "new a");

    const auto b = store.for_file("/b.lucid");
    REQUIRE(b.size() == 1);
    REQUIRE(b[0].message == "error b");
}

TEST_CASE("diagnostic_store: clear_for_file removes only that file",
          "[diagnostics]")
{
    DiagnosticStore store;
    store.replace_for_file("/a.lucid",
                           {make_entry("/a.lucid",
                                       engine::CompileDiagnostic::Severity::Error,
                                       "a")});
    store.replace_for_file("/b.lucid",
                           {make_entry("/b.lucid",
                                       engine::CompileDiagnostic::Severity::Error,
                                       "b")});

    store.clear_for_file("/a.lucid");

    REQUIRE(store.size() == 1);
    REQUIRE(store.for_file("/a.lucid").empty());
    REQUIRE(store.for_file("/b.lucid").size() == 1);
}

TEST_CASE("diagnostic_store: error and warning counts", "[diagnostics]")
{
    DiagnosticStore store;
    store.add(make_entry("/a.lucid",
                         engine::CompileDiagnostic::Severity::Error, "e1"));
    store.add(make_entry("/a.lucid",
                         engine::CompileDiagnostic::Severity::Fatal, "e2"));
    store.add(make_entry("/a.lucid",
                         engine::CompileDiagnostic::Severity::Warning, "w1"));
    store.add(make_entry("/a.lucid",
                         engine::CompileDiagnostic::Severity::Note, "n1"));

    REQUIRE(store.error_count() == 2);
    REQUIRE(store.warning_count() == 1);
}

TEST_CASE("diagnostic_store: clear empties the store", "[diagnostics]")
{
    DiagnosticStore store;
    store.add(make_entry("/a.lucid",
                         engine::CompileDiagnostic::Severity::Error, "e"));
    store.clear();
    REQUIRE(store.empty());
}