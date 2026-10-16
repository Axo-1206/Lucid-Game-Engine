#include <catch2/catch_test_macros.hpp>

#include "editor/document.h"

#include <filesystem>
#include <fstream>

using namespace editor;

namespace
{

    struct TempDir
    {
        std::filesystem::path path;

        TempDir()
        {
            path = std::filesystem::temp_directory_path() /
                   ("lucid_document_tests_" +
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
        std::ofstream out(p, std::ios::binary);
        out << content;
    }

    std::string read_file(const std::filesystem::path &p)
    {
        std::ifstream in(p, std::ios::binary);
        std::string s((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
        return s;
    }

} // namespace

TEST_CASE("document: open reads a file", "[document]")
{
    TempDir tmp;
    const auto path = tmp.path / "test.lucid";
    write_file(path, "node a = 1\n");

    std::string error;
    auto doc = Document::open(path, error);
    REQUIRE(doc != nullptr);
    REQUIRE(error.empty());
    REQUIRE(doc->path() == path);
    REQUIRE(doc->name() == "test.lucid");
    REQUIRE(doc->text() == "node a = 1\n");
    REQUIRE_FALSE(doc->dirty());
}

TEST_CASE("document: open of a missing file fails", "[document]")
{
    TempDir tmp;
    const auto path = tmp.path / "missing.lucid";

    std::string error;
    auto doc = Document::open(path, error);
    REQUIRE(doc == nullptr);
    REQUIRE_FALSE(error.empty());
}

TEST_CASE("document: mark_dirty and save clear it", "[document]")
{
    TempDir tmp;
    const auto path = tmp.path / "test.lucid";
    write_file(path, "original\n");

    std::string error;
    auto doc = Document::open(path, error);
    REQUIRE(doc != nullptr);
    REQUIRE_FALSE(doc->dirty());

    doc->mutable_text() = "modified\n";
    doc->mark_dirty();
    REQUIRE(doc->dirty());

    REQUIRE(doc->save(error));
    REQUIRE_FALSE(doc->dirty());
    REQUIRE(read_file(path) == "modified\n");
}

TEST_CASE("document: save_as changes the path", "[document]")
{
    TempDir tmp;
    const auto path_a = tmp.path / "a.lucid";
    const auto path_b = tmp.path / "b.lucid";
    write_file(path_a, "content\n");

    std::string error;
    auto doc = Document::open(path_a, error);
    REQUIRE(doc != nullptr);

    doc->mutable_text() = "changed\n";
    doc->mark_dirty();

    REQUIRE(doc->save_as(path_b, error));
    REQUIRE_FALSE(doc->dirty());
    REQUIRE(doc->path() == path_b);
    REQUIRE(read_file(path_a) == "content\n");
    REQUIRE(read_file(path_b) == "changed\n");
}