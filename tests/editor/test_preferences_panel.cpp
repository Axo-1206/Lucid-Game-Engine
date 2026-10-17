// tests/editor/test_preferences_panel.cpp

#include <catch2/catch_test_macros.hpp>

#include "editor/panels/preferences_panel.h"
#include "editor/settings.h"

#include <string>
#include <vector>

TEST_CASE("PreferencesPanel: open copies the starting state",
          "[preferences]")
{
    editor::Settings initial;
    initial.theme = "dark";
    initial.font_size = 14.0f;
    initial.show_console_on_start = true;
    initial.default_new_project_parent = "/projects";
    initial.project_roots = {"/a", "/b"};

    editor::PreferencesPanel panel;
    panel.open(initial);

    REQUIRE(panel.is_open());

    // The panel doesn't expose its working copy; the test can
    // only observe the state via apply(). That's fine — the
    // apply() test below covers the round-trip. This test only
    // verifies the open() state machine.
}

TEST_CASE("PreferencesPanel: apply dispatches the current state",
          "[preferences]")
{
    editor::Settings saved;
    bool fired = false;

    editor::PreferencesPanel panel;
    panel.set_save_callback(
        [&](const editor::Settings &s)
        {
            fired = true;
            saved = s;
        });

    editor::Settings initial;
    initial.theme = "dark";
    initial.font_size = 14.0f;
    initial.show_console_on_start = false;
    initial.default_new_project_parent = "/projects";
    initial.project_roots = {"/a", "/b"};

    panel.open(initial);
    panel.apply();

    REQUIRE(fired);
    REQUIRE(saved.theme == "dark");
    REQUIRE(saved.font_size == 14.0f);
    REQUIRE(saved.show_console_on_start == false);
    REQUIRE(saved.default_new_project_parent == "/projects");
    REQUIRE(saved.project_roots == std::vector<std::string>{"/a", "/b"});
    REQUIRE_FALSE(panel.is_open());
}

TEST_CASE("PreferencesPanel: close does not dispatch",
          "[preferences]")
{
    bool fired = false;

    editor::PreferencesPanel panel;
    panel.set_save_callback(
        [&](const editor::Settings &)
        { fired = true; });

    editor::Settings initial;
    initial.theme = "dark";
    panel.open(initial);
    panel.close();

    REQUIRE_FALSE(fired);
    REQUIRE_FALSE(panel.is_open());
}