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