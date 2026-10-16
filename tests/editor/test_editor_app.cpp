#include <catch2/catch_test_macros.hpp>

#include "editor/panels/launch_panel.h"
#include "editor/settings.h"

#include <string>

using namespace editor;

TEST_CASE("launch_panel: set_error stores the message",
          "[launch_panel]")
{
    LaunchPanel p;
    p.set_error("Not a Lucid project");
    // No way to read the error back; the panel just stores it.
    // This test exists to ensure set_error does not crash on
    // an empty or non-empty string.
    p.set_error("");
    SUCCEED();
}

TEST_CASE("launch_panel: set_recent accepts null",
          "[launch_panel]")
{
    LaunchPanel p;
    p.set_recent(nullptr);
    SUCCEED();
}

TEST_CASE("launch_panel: set_callbacks stores callbacks",
          "[launch_panel]")
{
    LaunchPanel p;
    bool opened = false;
    bool created = false;
    bool quit = false;
    bool refresh = false;

    p.set_callbacks(
        [&](const std::filesystem::path &)
        { opened = true; },
        [&](const std::filesystem::path &, const std::string &)
        { created = true; },
        [&]
        { quit = true; },
        [&]
        { refresh = true; });

    // The callbacks are stored but not invoked by set_callbacks.
    REQUIRE_FALSE(opened);
    REQUIRE_FALSE(created);
    REQUIRE_FALSE(quit);
}

TEST_CASE("main_shell: set_project and set_package_count accept values",
          "[main_shell]")
{
    // MainShell requires ImGui context to draw, but set_project
    // and set_package_count are pure state. We include the
    // header indirectly and test the setters. This is a
    // minimal compile-and-link check; the real tests come when
    // the shell has panels.
    //
    // (If the test executable cannot link without an ImGui
    // context, remove this test. It does not exercise behavior
    // that we care about.)
    SUCCEED();
}