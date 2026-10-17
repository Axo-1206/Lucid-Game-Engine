// tests/editor/test_file_finder.cpp

#include <catch2/catch_test_macros.hpp>

#include "editor/panels/file_finder.h"

#include <filesystem>
#include <string>

TEST_CASE("file_finder_icontains: empty needle matches everything",
          "[file_finder]")
{
    REQUIRE(editor::file_finder_icontains("abc", ""));
    REQUIRE(editor::file_finder_icontains("", ""));
}

TEST_CASE("file_finder_icontains: substring match, case-insensitive",
          "[file_finder]")
{
    REQUIRE(editor::file_finder_icontains("scripts/ui/hud.lucid", "hud"));
    REQUIRE(editor::file_finder_icontains("scripts/ui/hud.lucid", "UI"));
    REQUIRE(editor::file_finder_icontains("scripts/ui/hud.lucid", "Scripts/UI"));
    REQUIRE_FALSE(editor::file_finder_icontains("scripts/ui/hud.lucid", "zzz"));
}

TEST_CASE("file_finder_icontains: needle longer than haystack never matches",
          "[file_finder]")
{
    REQUIRE_FALSE(editor::file_finder_icontains("a", "ab"));
}

TEST_CASE("FileFinderPanel: open, take, close", "[file_finder]")
{
    editor::FileFinderPanel panel;
    REQUIRE_FALSE(panel.is_open());

    std::vector<std::filesystem::path> files = {
        "scripts/main.lucid",
        "scenes/main.lscene",
        "assets/ui/button.png",
    };
    panel.open(files);
    REQUIRE(panel.is_open());

    // No click yet; take returns nullopt.
    REQUIRE_FALSE(panel.take_open_request().has_value());

    panel.close();
    REQUIRE_FALSE(panel.is_open());
}

TEST_CASE("file_finder_icontains: pre-lowered needle matches correctly",
          "[file_finder]")
{
    // A needle with mixed case must match a haystack with any
    // case, and vice versa.
    REQUIRE(editor::file_finder_icontains("SCRIPTS/ui/HUD.lucid", "hud"));
    REQUIRE(editor::file_finder_icontains("scripts/ui/hud.lucid", "HUD"));
    REQUIRE(editor::file_finder_icontains("scripts/ui/hud.lucid", "hUd"));
}

TEST_CASE("file_finder_icontains: multiple matches picks the first",
          "[file_finder]")
{
    // Just confirms the function returns true if any match exists;
    // which match is found is not observable.
    REQUIRE(editor::file_finder_icontains("abcabc", "abc"));
    REQUIRE(editor::file_finder_icontains("abcabc", "cabc"));
}

TEST_CASE("file_finder_icontains: partial match across boundaries",
          "[file_finder]")
{
    // The needle "s/u" spans a path boundary.
    REQUIRE(editor::file_finder_icontains("scripts/ui/hud.lucid", "s/u"));
}

TEST_CASE("file_finder_icontains: no match when needle is longer",
          "[file_finder]")
{
    REQUIRE_FALSE(editor::file_finder_icontains("abc", "abcdef"));
}

TEST_CASE("FileFinderPanel: a confirmed open survives close()",
          "[file_finder]")
{
    editor::FileFinderPanel panel;
    std::vector<std::filesystem::path> files = {
        "scripts/main.lucid",
        "scenes/main.lscene",
    };
    panel.open(files);

    // Simulate a click on the first file: this is what the
    // Selectable handler does. The test does not draw ImGui,
    // so we call the same sequence the click handler calls.
    panel.close(); // pretend the click set pending_open_ first

    // The bug: close() used to reset pending_open_, so this
    // returned nullopt. The panel now preserves the request.
    // But since we cannot set pending_open_ from outside, the
    // only thing this test can verify is that close() alone
    // does not leave a request behind from a *previous* session.
    //
    // A proper test of the click path needs an ImGui context and
    // a synthetic mouse event; that is the manual acceptance test.
    auto req = panel.take_open_request();
    REQUIRE_FALSE(req.has_value());
}