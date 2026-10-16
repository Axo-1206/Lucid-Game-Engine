#include <catch2/catch_test_macros.hpp>

#include "editor/project_roots.h"
#include "editor/settings.h"

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
                   ("lucid_roots_tests_" +
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

    // Write a minimal valid project file into `folder`.
    void write_valid_project(const std::filesystem::path &folder,
                             const std::string &name)
    {
        std::filesystem::create_directories(folder);
        std::ofstream out(folder / "lucid.project.json");
        out << R"({"format_version":"1","name":")" << name << R"(","packages":[]})";
    }

    // Write a malformed project file into `folder`.
    void write_invalid_project(const std::filesystem::path &folder)
    {
        std::filesystem::create_directories(folder);
        std::ofstream out(folder / "lucid.project.json");
        out << "{ not json }";
    }

} // namespace

TEST_CASE("project_roots: scan finds projects one level deep", "[roots]")
{
    TempDir tmp;
    const auto default_root = tmp.path / "Default";
    std::filesystem::create_directories(default_root);

    write_valid_project(default_root / "Alpha", "Alpha");
    write_valid_project(default_root / "Bravo", "Bravo");
    write_invalid_project(default_root / "Charlie");

    // A directory with no project file: must not be listed.
    std::filesystem::create_directories(default_root / "NotAProject");

    Settings s;
    auto roots = ProjectRoots::load(s, default_root);
    auto found = roots.scan();

    REQUIRE(found.size() == 3);

    // Sorted by name. Charlie is invalid, so its display name is
    // the folder name. Alpha and Bravo use their project names.
    REQUIRE(found[0].name == "Alpha");
    REQUIRE(found[0].valid);
    REQUIRE(found[1].name == "Bravo");
    REQUIRE(found[1].valid);
    REQUIRE(found[2].name == "Charlie");
    REQUIRE_FALSE(found[2].valid);
}

TEST_CASE("project_roots: scan does not recurse", "[roots]")
{
    TempDir tmp;
    const auto default_root = tmp.path / "Default";
    std::filesystem::create_directories(default_root);

    // A project nested under a non-project folder.
    write_valid_project(default_root / "Container" / "Nested",
                        "Nested");

    Settings s;
    auto roots = ProjectRoots::load(s, default_root);
    auto found = roots.scan();

    // The nested project is not found because its parent is not
    // itself a project directory. Only one level is scanned.
    REQUIRE(found.empty());
}

TEST_CASE("project_roots: user roots are scanned", "[roots]")
{
    TempDir tmp;
    const auto default_root = tmp.path / "Default";
    const auto user_root = tmp.path / "User";
    std::filesystem::create_directories(default_root);
    std::filesystem::create_directories(user_root);

    write_valid_project(default_root / "InDefault", "InDefault");
    write_valid_project(user_root / "InUser", "InUser");

    Settings s;
    s.project_roots = {user_root.string()};

    auto roots = ProjectRoots::load(s, default_root);
    REQUIRE(roots.roots().size() == 2);
    REQUIRE(roots.roots()[0].is_default);
    REQUIRE_FALSE(roots.roots()[1].is_default);

    auto found = roots.scan();
    REQUIRE(found.size() == 2);
    REQUIRE(found[0].name == "InDefault");
    REQUIRE(found[1].name == "InUser");
}

TEST_CASE("project_roots: stale user roots are dropped on load",
          "[roots]")
{
    TempDir tmp;
    const auto default_root = tmp.path / "Default";
    std::filesystem::create_directories(default_root);

    Settings s;
    s.project_roots = {"/does/not/exist/anywhere"};

    auto roots = ProjectRoots::load(s, default_root);

    // Only the default root remains.
    REQUIRE(roots.roots().size() == 1);
    REQUIRE(roots.roots()[0].is_default);
}

TEST_CASE("project_roots: add_root ignores duplicates and the default",
          "[roots]")
{
    TempDir tmp;
    const auto default_root = tmp.path / "Default";
    const auto user_root = tmp.path / "User";
    std::filesystem::create_directories(default_root);
    std::filesystem::create_directories(user_root);

    Settings s;
    auto roots = ProjectRoots::load(s, default_root);

    roots.add_root(user_root);
    roots.add_root(user_root);    // duplicate
    roots.add_root(default_root); // equals default; ignored

    REQUIRE(roots.roots().size() == 2);
    REQUIRE(roots.roots()[1].path == user_root);
}

TEST_CASE("project_roots: remove_root ignores the default", "[roots]")
{
    TempDir tmp;
    const auto default_root = tmp.path / "Default";
    const auto user_root = tmp.path / "User";
    std::filesystem::create_directories(default_root);
    std::filesystem::create_directories(user_root);

    Settings s;
    auto roots = ProjectRoots::load(s, default_root);
    roots.add_root(user_root);
    REQUIRE(roots.roots().size() == 2);

    roots.remove_root(default_root);
    REQUIRE(roots.roots().size() == 2); // default still present

    roots.remove_root(user_root);
    REQUIRE(roots.roots().size() == 1);
    REQUIRE(roots.roots()[0].is_default);
}

TEST_CASE("project_roots: user_roots_as_strings excludes the default",
          "[roots]")
{
    TempDir tmp;
    const auto default_root = tmp.path / "Default";
    const auto user_root_a = tmp.path / "UserA";
    const auto user_root_b = tmp.path / "UserB";
    std::filesystem::create_directories(default_root);
    std::filesystem::create_directories(user_root_a);
    std::filesystem::create_directories(user_root_b);

    Settings s;
    auto roots = ProjectRoots::load(s, default_root);
    roots.add_root(user_root_a);
    roots.add_root(user_root_b);

    const auto strings = roots.user_roots_as_strings();
    REQUIRE(strings.size() == 2);
    REQUIRE(strings[0] == user_root_a.string());
    REQUIRE(strings[1] == user_root_b.string());
}