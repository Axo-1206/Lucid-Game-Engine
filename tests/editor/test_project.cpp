#include <catch2/catch_test_macros.hpp>

#include "editor/project.h"
#include "engine/core/version.h"

#include <filesystem>
#include <fstream>

using namespace editor;

namespace
{

    // A temp directory unique to each test. Removed in the
    // destructor. The same pattern as test_graph_runtime.cpp.
    struct TempDir
    {
        std::filesystem::path path;

        TempDir()
        {
            path = std::filesystem::temp_directory_path() /
                   ("lucid_project_tests_" +
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

    bool file_exists(const std::filesystem::path &p)
    {
        return std::filesystem::exists(p);
    }

} // namespace

TEST_CASE("project: create makes a valid project", "[project]")
{
    TempDir tmp;
    const auto root = tmp.path / "MyGame";

    std::string error;
    auto p = Project::create(root, "MyGame", error);
    REQUIRE(p != nullptr);
    REQUIRE(error.empty());

    // The sentinel file.
    REQUIRE(file_exists(root / Project::kProjectFileName));

    // The standard directories.
    REQUIRE(file_exists(root / "assets"));
    REQUIRE(file_exists(root / "scenes"));
    REQUIRE(file_exists(root / "scripts"));
    REQUIRE(file_exists(root / "packages"));
    REQUIRE(file_exists(root / ".lucid"));
    REQUIRE(file_exists(root / ".lucid" / "cache"));
    REQUIRE(file_exists(root / ".lucid" / "generated"));

    // The default scene.
    REQUIRE(file_exists(root / "scenes" / "main.lscene"));

    // The project file's contents.
    REQUIRE(p->name() == "MyGame");
    REQUIRE(p->file().format_version == Project::kCurrentFormatVersion);
    REQUIRE(p->file().engine_version == engine::kEngineVersion);
    REQUIRE(p->file().active_scene == "scenes/main.lscene");
    REQUIRE(p->file().active_camera == "main");
    REQUIRE(p->file().packages.size() == 5);
    REQUIRE(p->file().packages[0] == "engine.core");
}

TEST_CASE("project: open reads what create wrote", "[project]")
{
    TempDir tmp;
    const auto root = tmp.path / "MyGame";

    std::string error;
    auto created = Project::create(root, "MyGame", error);
    REQUIRE(created != nullptr);

    auto opened = Project::open(root, error);
    REQUIRE(opened != nullptr);
    REQUIRE(opened->root() == root);
    REQUIRE(opened->name() == "MyGame");
    REQUIRE(opened->file().engine_version == engine::kEngineVersion);
}

TEST_CASE("project: open refuses a folder with no project file",
          "[project]")
{
    TempDir tmp;
    const auto root = tmp.path / "NotAGame";
    std::filesystem::create_directories(root);

    std::string error;
    auto p = Project::open(root, error);
    REQUIRE(p == nullptr);
    REQUIRE(error.find("lucid.project.json") != std::string::npos);
}

TEST_CASE("project: open refuses a malformed project file", "[project]")
{
    TempDir tmp;
    const auto root = tmp.path / "BadGame";
    std::filesystem::create_directories(root);

    {
        std::ofstream out(root / Project::kProjectFileName);
        out << "{ this is not json }";
    }

    std::string error;
    auto p = Project::open(root, error);
    REQUIRE(p == nullptr);
    REQUIRE(error.find("Invalid lucid.project.json") != std::string::npos);
}

TEST_CASE("project: open refuses a newer format_version", "[project]")
{
    TempDir tmp;
    const auto root = tmp.path / "FutureGame";
    std::filesystem::create_directories(root);

    {
        std::ofstream out(root / Project::kProjectFileName);
        out << R"({
  "format_version": "999",
  "name": "FutureGame",
  "packages": [],
  "active_scene": "",
  "active_camera": ""
})";
    }

    std::string error;
    auto p = Project::open(root, error);
    REQUIRE(p == nullptr);
    REQUIRE(error.find("newer than this editor") != std::string::npos);
}

TEST_CASE("project: open refuses a project with no name", "[project]")
{
    TempDir tmp;
    const auto root = tmp.path / "NoName";
    std::filesystem::create_directories(root);

    {
        std::ofstream out(root / Project::kProjectFileName);
        out << R"({
  "format_version": "1",
  "name": "",
  "packages": []
})";
    }

    std::string error;
    auto p = Project::open(root, error);
    REQUIRE(p == nullptr);
    REQUIRE(error.find("name") != std::string::npos);
}

TEST_CASE("project: create refuses an existing project folder",
          "[project]")
{
    TempDir tmp;
    const auto root = tmp.path / "MyGame";

    std::string error;
    auto first = Project::create(root, "MyGame", error);
    REQUIRE(first != nullptr);

    auto second = Project::create(root, "MyGame", error);
    REQUIRE(second == nullptr);
    REQUIRE(error.find("already exists") != std::string::npos);
}

TEST_CASE("project: save round-trips a modification", "[project]")
{
    TempDir tmp;
    const auto root = tmp.path / "MyGame";

    std::string error;
    auto created = Project::create(root, "MyGame", error);
    REQUIRE(created != nullptr);

    auto opened = Project::open(root, error);
    REQUIRE(opened != nullptr);
    REQUIRE(opened->file().active_scene == "scenes/main.lscene");

    // Mutating the ProjectFile is not possible through the public
    // API today (file() returns a const ref). The save round-trip
    // is tested by calling save and re-opening.
    REQUIRE(opened->save(error));

    auto reopened = Project::open(root, error);
    REQUIRE(reopened != nullptr);
    REQUIRE(reopened->file().active_scene == "scenes/main.lscene");
}

TEST_CASE("project: lucid_dir and path_of return the right paths",
          "[project]")
{
    TempDir tmp;
    const auto root = tmp.path / "MyGame";

    std::string error;
    auto p = Project::create(root, "MyGame", error);
    REQUIRE(p != nullptr);

    REQUIRE(p->lucid_dir() == root / ".lucid");
    REQUIRE(p->path_of("scenes/main.lscene") ==
            root / "scenes" / "main.lscene");
}