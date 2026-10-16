#include <catch2/catch_test_macros.hpp>

#include "editor/panels/explorer_panel.h"

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
                   ("lucid_explorer_tests_" +
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

    void write_file(const std::filesystem::path &p, const std::string &content = "")
    {
        std::filesystem::create_directories(p.parent_path());
        std::ofstream out(p);
        out << content;
    }

    // Find a child by name. Returns nullptr if not present.
    const ExplorerEntry *find_child(const ExplorerEntry &parent,
                                    const std::string &name)
    {
        for (const auto &c : parent.children)
        {
            if (c.name == name)
                return &c;
        }
        return nullptr;
    }

} // namespace

TEST_CASE("explorer: builds a tree of the project", "[explorer]")
{
    TempDir tmp;
    std::filesystem::create_directories(tmp.path / "assets");
    std::filesystem::create_directories(tmp.path / "scenes");
    std::filesystem::create_directories(tmp.path / "scripts");
    write_file(tmp.path / "assets" / "player.png", "PNG");
    write_file(tmp.path / "scenes" / "main.lscene", "{}");
    write_file(tmp.path / "scripts" / "player.lucid", "");

    auto tree = build_explorer_tree(tmp.path);

    REQUIRE(tree.kind == ExplorerEntryKind::Directory);
    REQUIRE(tree.children.size() == 3);

    // Directories first, alphabetical.
    REQUIRE(tree.children[0].name == "assets");
    REQUIRE(tree.children[1].name == "scenes");
    REQUIRE(tree.children[2].name == "scripts");

    const auto *assets = find_child(tree, "assets");
    REQUIRE(assets != nullptr);
    REQUIRE(assets->kind == ExplorerEntryKind::Directory);
    REQUIRE(assets->children.size() == 1);
    REQUIRE(assets->children[0].name == "player.png");
    REQUIRE(assets->children[0].kind == ExplorerEntryKind::File);
}

TEST_CASE("explorer: skips hidden directories", "[explorer]")
{
    TempDir tmp;
    std::filesystem::create_directories(tmp.path / ".lucid");
    std::filesystem::create_directories(tmp.path / ".git");
    std::filesystem::create_directories(tmp.path / "assets");
    write_file(tmp.path / "assets" / "x.txt");

    auto tree = build_explorer_tree(tmp.path);

    REQUIRE(find_child(tree, ".lucid") == nullptr);
    REQUIRE(find_child(tree, ".git") == nullptr);
    REQUIRE(find_child(tree, "assets") != nullptr);
}

TEST_CASE("explorer: skips build and tool directories", "[explorer]")
{
    TempDir tmp;
    std::filesystem::create_directories(tmp.path / "build");
    std::filesystem::create_directories(tmp.path / "out");
    std::filesystem::create_directories(tmp.path / "node_modules");
    std::filesystem::create_directories(tmp.path / "scripts");

    auto tree = build_explorer_tree(tmp.path);

    REQUIRE(find_child(tree, "build") == nullptr);
    REQUIRE(find_child(tree, "out") == nullptr);
    REQUIRE(find_child(tree, "node_modules") == nullptr);
    REQUIRE(find_child(tree, "scripts") != nullptr);
}

TEST_CASE("explorer: skips .lucgraph, temp, and backup files", "[explorer]")
{
    TempDir tmp;
    write_file(tmp.path / "scripts" / "player.lucid", "");
    write_file(tmp.path / "scripts" / "player.lucgraph", "BIN");
    write_file(tmp.path / "scripts" / "player.lucid.tmp", "");
    write_file(tmp.path / "scripts" / "player.lucid.bak", "");
    write_file(tmp.path / "scripts" / "player.lucid~", "");

    auto tree = build_explorer_tree(tmp.path);

    const auto *scripts = find_child(tree, "scripts");
    REQUIRE(scripts != nullptr);
    REQUIRE(scripts->children.size() == 1);
    REQUIRE(scripts->children[0].name == "player.lucid");
}

TEST_CASE("explorer: sorts directories before files, both alphabetical",
          "[explorer]")
{
    TempDir tmp;
    std::filesystem::create_directories(tmp.path / "zdir");
    std::filesystem::create_directories(tmp.path / "adir");
    write_file(tmp.path / "zfile.txt");
    write_file(tmp.path / "afile.txt");

    auto tree = build_explorer_tree(tmp.path);

    REQUIRE(tree.children.size() == 4);
    REQUIRE(tree.children[0].name == "adir");
    REQUIRE(tree.children[1].name == "zdir");
    REQUIRE(tree.children[2].name == "afile.txt");
    REQUIRE(tree.children[3].name == "zfile.txt");
}

TEST_CASE("explorer: empty directory is a leaf", "[explorer]")
{
    TempDir tmp;
    std::filesystem::create_directories(tmp.path / "empty");

    auto tree = build_explorer_tree(tmp.path);

    const auto *empty = find_child(tree, "empty");
    REQUIRE(empty != nullptr);
    REQUIRE(empty->kind == ExplorerEntryKind::Directory);
    REQUIRE(empty->children.empty());
}

TEST_CASE("explorer: nonexistent root yields an empty tree", "[explorer]")
{
    auto tree = build_explorer_tree("/does/not/exist");
    REQUIRE(tree.kind == ExplorerEntryKind::Directory);
    REQUIRE(tree.children.empty());
}