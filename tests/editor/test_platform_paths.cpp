#include <catch2/catch_test_macros.hpp>

#include "editor/platform_paths.h"

#include <filesystem>

using namespace editor;

TEST_CASE("platform_paths: config dir is non-empty and absolute",
          "[platform_paths]")
{
    const auto p = paths::global_config_dir();
    REQUIRE_FALSE(p.empty());
    REQUIRE(p.is_absolute());
    // The last component is "Lucid", except in the fallback case
    // where it is "lucid_editor_config". Either way, the path has
    // a final component.
    REQUIRE(p.has_filename());
}

TEST_CASE("platform_paths: cache dir is non-empty and absolute",
          "[platform_paths]")
{
    const auto p = paths::global_cache_dir();
    REQUIRE_FALSE(p.empty());
    REQUIRE(p.is_absolute());
}

TEST_CASE("platform_paths: config and cache dirs differ on this platform",
          "[platform_paths]")
{
    // On Windows, %APPDATA% and %LOCALAPPDATA% differ. On Linux,
    // $XDG_CONFIG_HOME and $XDG_CACHE_HOME differ. On macOS,
    // Application Support and Caches differ. In the fallback
    // case, both are the same directory; the test allows for that.
    const auto cfg = paths::global_config_dir();
    const auto cache = paths::global_cache_dir();

    if (cfg != cache)
    {
        // Both are distinct; that's the normal case. The only
        // requirement is that both are non-empty, which the
        // other two tests check.
        REQUIRE_FALSE(cfg.empty());
        REQUIRE_FALSE(cache.empty());
    }
    else
    {
        // Fallback case: both point at lucid_editor_config.
        // Acceptable; the editor logs a warning at startup.
        REQUIRE(cfg.filename() == "lucid_editor_config");
    }
}

TEST_CASE("platform_paths: executable dir is non-empty, absolute, a directory",
          "[platform_paths]")
{
    const auto p = paths::executable_dir();
    REQUIRE_FALSE(p.empty());
    REQUIRE(p.is_absolute());
    // It must name an existing directory. If the platform mechanism
    // failed and fell back to CWD, CWD should at least be a real
    // directory — degraded but not broken.
    REQUIRE(std::filesystem::is_directory(p));
}

TEST_CASE("platform_paths: executable dir is not the CWD by accident",
          "[platform_paths]")
{
    // A broken executable_dir() typically falls back to CWD (or
    // /proc/self/exe fails and the fallback is taken). That fallback
    // is acceptable only when the binary actually lives in CWD.
    //
    // Example bug this catches: executable_dir() returns build/ while
    // the binaries are in build/bin/. ctest runs from build/, so CWD
    // is build/. executable_dir() == CWD, but build/lucid_editor doesn't
    // exist => test fails => bug exposed.
    const auto dir = paths::executable_dir();
    const auto cwd = std::filesystem::current_path();

    if (dir != cwd)
        return; // Not using the CWD fallback; nothing to check.

    const auto editor_exe = cwd /
#if defined(_WIN32)
        "lucid_editor.exe";
#else
        "lucid_editor";
#endif

    const auto game_exe = cwd /
#if defined(_WIN32)
        "lucid_game.exe";
#else
        "lucid_game";
#endif

    const bool has_sibling =
        std::filesystem::exists(editor_exe) ||
        std::filesystem::exists(game_exe);

    INFO("executable_dir() == CWD (" << cwd.string() << ")");
    REQUIRE(has_sibling);
}
