// tests/editor/test_symbol_index.cpp

#include <catch2/catch_test_macros.hpp>

#include "editor/symbol_index.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace
{
    struct TempDir
    {
        fs::path path;
        TempDir()
        {
            path = fs::temp_directory_path() /
                   ("lucid_symidx_" +
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
}

// ─────────────────────────────────────────────────────────────────────
// strip_comments_and_strings
// ─────────────────────────────────────────────────────────────────────

TEST_CASE("symbol_index: strip removes line comments", "[symbol_index]")
{
    const std::string src = "node A = X() // node B = Y()\nnode C = Z()\n";
    const std::string out = editor::strip_comments_and_strings(src);
    REQUIRE(out.find("node A") != std::string::npos);
    REQUIRE(out.find("node B") == std::string::npos);
    REQUIRE(out.find("node C") != std::string::npos);
    // Newlines are preserved for line-number accuracy.
    REQUIRE(std::count(out.begin(), out.end(), '\n') ==
            std::count(src.begin(), src.end(), '\n'));
}

TEST_CASE("symbol_index: strip removes block comments", "[symbol_index]")
{
    const std::string src =
        "/* node A = X() */\nnode B = Y()\n/* multi\nline\nnode C */\nnode D = Z()\n";
    const std::string out = editor::strip_comments_and_strings(src);
    REQUIRE(out.find("node A") == std::string::npos);
    REQUIRE(out.find("node B") != std::string::npos);
    REQUIRE(out.find("node C") == std::string::npos);
    REQUIRE(out.find("node D") != std::string::npos);
    // Line count preserved.
    REQUIRE(std::count(out.begin(), out.end(), '\n') ==
            std::count(src.begin(), src.end(), '\n'));
}

TEST_CASE("symbol_index: strip removes strings", "[symbol_index]")
{
    const std::string src =
        "node A = Print(\"node Fake = NotReal()\")\nnode B = X()\n";
    const std::string out = editor::strip_comments_and_strings(src);
    REQUIRE(out.find("node A") != std::string::npos);
    REQUIRE(out.find("node Fake") == std::string::npos);
    REQUIRE(out.find("node B") != std::string::npos);
}

TEST_CASE("symbol_index: escaped quotes inside strings do not end the string",
          "[symbol_index]")
{
    const std::string src =
        "node A = Print(\"a \\\" node Fake = X()\")\nnode B = Y()\n";
    const std::string out = editor::strip_comments_and_strings(src);
    REQUIRE(out.find("node Fake") == std::string::npos);
    REQUIRE(out.find("node B") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────
// scan_declarations
// ─────────────────────────────────────────────────────────────────────

TEST_CASE("symbol_index: scan finds all four declaration forms",
          "[symbol_index]")
{
    const std::string src =
        "import alice.math\n"
        "enum Direction { North, South }\n"
        "resource Config { speed: float32 = 1.0 }\n"
        "node Hero = X()\n";

    std::vector<editor::SymbolEntry> out;
    editor::scan_declarations("test.lucid", src, out);

    REQUIRE(out.size() == 4);
    REQUIRE(out[0].name == "alice");
    REQUIRE(out[0].kind == editor::SymbolKind::Import);
    REQUIRE(out[0].line == 1);
    REQUIRE(out[1].name == "Direction");
    REQUIRE(out[1].kind == editor::SymbolKind::Enum);
    REQUIRE(out[1].line == 2);
    REQUIRE(out[2].name == "Config");
    REQUIRE(out[2].kind == editor::SymbolKind::Resource);
    REQUIRE(out[3].name == "Hero");
    REQUIRE(out[3].kind == editor::SymbolKind::Node);
}

TEST_CASE("symbol_index: scan ignores declaration-like text in comments/strings",
          "[symbol_index]")
{
    const std::string raw =
        "node Real = X()\n"
        "// node Fake = Y()\n"
        "node AlsoReal = \"node StringFake = Z()\"\n";
    const std::string cleaned = editor::strip_comments_and_strings(raw);

    std::vector<editor::SymbolEntry> out;
    editor::scan_declarations("test.lucid", cleaned, out);

    REQUIRE(out.size() == 2);
    REQUIRE(out[0].name == "Real");
    REQUIRE(out[1].name == "AlsoReal");
}

TEST_CASE("symbol_index: scan handles attributes", "[symbol_index]")
{
    const std::string src = "@export node Hero = X()\n";
    std::vector<editor::SymbolEntry> out;
    editor::scan_declarations("test.lucid", src, out);
    REQUIRE(out.size() == 1);
    REQUIRE(out[0].name == "Hero");
}

// ─────────────────────────────────────────────────────────────────────
// SymbolIndex
// ─────────────────────────────────────────────────────────────────────

TEST_CASE("symbol_index: rebuild walks scripts/ recursively", "[symbol_index]")
{
    TempDir tmp;
    write_file(tmp.path / "scripts" / "a.lucid", "node A = X()\n");
    write_file(tmp.path / "scripts" / "sub" / "b.lucid",
               "enum E { X }\nnode B = Y()\n");
    // A .lucid outside scripts/ should be ignored.
    write_file(tmp.path / "other" / "c.lucid", "node C = Z()\n");

    editor::SymbolIndex idx;
    idx.rebuild(tmp.path);

    REQUIRE(idx.size() == 3);
    const auto &e = idx.entries();
    // Sorted by path.
    bool has_A = false, has_B = false, has_E = false, has_C = false;
    for (const auto &s : e)
    {
        if (s.name == "A")
            has_A = true;
        if (s.name == "B")
            has_B = true;
        if (s.name == "E")
            has_E = true;
        if (s.name == "C")
            has_C = true;
    }
    REQUIRE(has_A);
    REQUIRE(has_B);
    REQUIRE(has_E);
    REQUIRE_FALSE(has_C);
}

TEST_CASE("symbol_index: find is case-insensitive substring",
          "[symbol_index]")
{
    TempDir tmp;
    // "Other" was renamed to "Sword" so the `find("e")` assertion
    // has a predictable count: only Hero and Health contain 'e'.
    write_file(tmp.path / "scripts" / "a.lucid",
               "node Hero = X()\nnode Health = Y()\nnode Sword = Z()\n");

    editor::SymbolIndex idx;
    idx.rebuild(tmp.path);

    REQUIRE(idx.find("hero").size() == 1);
    REQUIRE(idx.find("HEALTH").size() == 1);
    REQUIRE(idx.find("e").size() == 2); // Hero, Health
    REQUIRE(idx.find("").size() == 3);
    REQUIRE(idx.find("zzz").empty());
}

TEST_CASE("symbol_index: refresh_file updates one file without a rebuild",
          "[symbol_index]")
{
    TempDir tmp;
    const auto file = tmp.path / "scripts" / "a.lucid";
    write_file(file, "node A = X()\n");

    editor::SymbolIndex idx;
    idx.rebuild(tmp.path);
    REQUIRE(idx.size() == 1);

    // Rewrite the file with a new symbol.
    write_file(file, "node A = X()\nnode B = Y()\n");
    idx.refresh_file(file);

    REQUIRE(idx.size() == 2);
    bool has_A = false, has_B = false;
    for (const auto &s : idx.entries())
    {
        if (s.name == "A")
            has_A = true;
        if (s.name == "B")
            has_B = true;
    }
    REQUIRE(has_A);
    REQUIRE(has_B);
}