// tests/editor/test_find_in_files.cpp

#include <catch2/catch_test_macros.hpp>

#include "editor/find_in_files.h"

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
                   ("lucid_find_" +
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

    fs::path write_file(const fs::path &p, const std::string &content)
    {
        fs::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary);
        out << content;
        return p;
    }
}

TEST_CASE("find_in_files: finds all lines containing the query",
          "[find_in_files]")
{
    TempDir tmp;
    const auto file = write_file(tmp.path / "a.txt",
                                 "hello world\n"
                                 "goodbye\n"
                                 "hello again\n");

    std::vector<editor::FindMatch> out;
    editor::FindOptions opts;
    editor::search_file(file, "hello", opts, out);

    REQUIRE(out.size() == 2);
    REQUIRE(out[0].line == 1);
    REQUIRE(out[1].line == 3);
    REQUIRE(out[0].file == file);
}

TEST_CASE("find_in_files: case-insensitive by default",
          "[find_in_files]")
{
    TempDir tmp;
    const auto file = write_file(tmp.path / "a.txt",
                                 "Hello\nworld\nHELLO\n");

    std::vector<editor::FindMatch> out;
    editor::FindOptions opts; // case_sensitive = false
    editor::search_file(file, "hello", opts, out);

    REQUIRE(out.size() == 2);
    REQUIRE(out[0].line == 1);
    REQUIRE(out[1].line == 3);
}

TEST_CASE("find_in_files: case-sensitive when enabled",
          "[find_in_files]")
{
    TempDir tmp;
    const auto file = write_file(tmp.path / "a.txt",
                                 "Hello\nworld\nHELLO\nhello\n");

    std::vector<editor::FindMatch> out;
    editor::FindOptions opts;
    opts.case_sensitive = true;
    editor::search_file(file, "hello", opts, out);

    REQUIRE(out.size() == 1);
    REQUIRE(out[0].line == 4);
}

TEST_CASE("find_in_files: empty query produces no matches",
          "[find_in_files]")
{
    TempDir tmp;
    const auto file = write_file(tmp.path / "a.txt", "anything\n");

    std::vector<editor::FindMatch> out;
    editor::FindOptions opts;
    editor::search_file(file, "", opts, out);
    REQUIRE(out.empty());
}

TEST_CASE("find_in_files: Windows line endings do not break line numbers",
          "[find_in_files]")
{
    TempDir tmp;
    const auto file = write_file(tmp.path / "a.txt",
                                 "line one\r\nline two\r\nline three\r\n");

    std::vector<editor::FindMatch> out;
    editor::FindOptions opts;
    editor::search_file(file, "two", opts, out);

    REQUIRE(out.size() == 1);
    REQUIRE(out[0].line == 2);
    // Trailing CR is stripped from the recorded line text.
    REQUIRE(out[0].line_text == "line two");
}

TEST_CASE("find_in_files: binary files are skipped",
          "[find_in_files]")
{
    TempDir tmp;
    const auto file = write_file(tmp.path / "a.bin",
                                 std::string("text\0more", 9));

    std::vector<editor::FindMatch> out;
    editor::FindOptions opts;
    editor::search_file(file, "text", opts, out);
    REQUIRE(out.empty());
}

TEST_CASE("find_in_files: long lines are truncated in line_text",
          "[find_in_files]")
{
    TempDir tmp;
    std::string long_line(2000, 'x');
    long_line += "needle";
    const auto file = write_file(tmp.path / "a.txt", long_line + "\n");

    std::vector<editor::FindMatch> out;
    editor::FindOptions opts;
    editor::search_file(file, "needle", opts, out);

    REQUIRE(out.size() == 1);
    REQUIRE(out[0].line_text.size() == editor::kMaxLineText);
}

TEST_CASE("find_in_files: search_files walks a list of paths",
          "[find_in_files]")
{
    TempDir tmp;
    const auto a = write_file(tmp.path / "a.txt", "alpha\nbeta\n");
    const auto b = write_file(tmp.path / "b.txt", "beta\ngamma\n");
    const auto c = write_file(tmp.path / "c.txt", "gamma\n");

    std::vector<editor::FindMatch> out;
    editor::FindOptions opts;
    editor::search_files({a, b, c}, "beta", opts, out);

    REQUIRE(out.size() == 2);
    REQUIRE(out[0].file == a);
    REQUIRE(out[1].file == b);
}