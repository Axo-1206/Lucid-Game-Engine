#include <catch2/catch_test_macros.hpp>

#include "editor/settings.h"
#include "editor/platform_paths.h"

#include <filesystem>
#include <fstream>

using namespace editor;

namespace
{

    // Redirect the config directory to a temp path for the
    // duration of a test. On all platforms, this is done by
    // setting the environment variable that paths::global_config_dir
    // reads. Windows reads APPDATA; Linux reads XDG_CONFIG_HOME;
    // macOS reads HOME.
    //
    // The tests set the variable to a temp directory, so the
    // real user's settings are never touched.
    struct IsolateConfigDir
    {
        std::filesystem::path tmp;
        std::string saved_value;
        bool had_saved_value = false;
        const char *var_name = nullptr;

        IsolateConfigDir()
        {
            tmp = std::filesystem::temp_directory_path() /
                  ("lucid_settings_tests_" +
                   std::to_string(reinterpret_cast<std::uintptr_t>(this)));
            std::filesystem::remove_all(tmp);
            std::filesystem::create_directories(tmp);

#if defined(_WIN32)
            var_name = "APPDATA";
#elif defined(__APPLE__)
            var_name = "HOME";
#else
            var_name = "XDG_CONFIG_HOME";
#endif

            const char *saved = std::getenv(var_name);
            if (saved)
            {
                saved_value = saved;
                had_saved_value = true;
            }
            set_env(var_name, tmp.string());
        }

        ~IsolateConfigDir()
        {
            if (had_saved_value)
                set_env(var_name, saved_value);
            else
                unset_env(var_name);

            std::error_code ec;
            std::filesystem::remove_all(tmp, ec);
        }

        // Platform-specific set/unset. On Windows, _putenv_s.
        // On POSIX, setenv/unsetenv.
        static void set_env(const char *name, const std::string &value);
        static void unset_env(const char *name);
    };

#if defined(_WIN32)
    void IsolateConfigDir::set_env(const char *name, const std::string &value)
    {
        _putenv_s(name, value.c_str());
    }
    void IsolateConfigDir::unset_env(const char *name)
    {
        _putenv_s(name, "");
    }
#else
    void IsolateConfigDir::set_env(const char *name, const std::string &value)
    {
        setenv(name, value.c_str(), 1);
    }
    void IsolateConfigDir::unset_env(const char *name)
    {
        unsetenv(name);
    }
#endif

} // namespace

TEST_CASE("settings: load_global returns defaults when file is missing",
          "[settings]")
{
    IsolateConfigDir iso;
    Settings s = SettingsStore::load_global();
    REQUIRE(s.theme == "dark");
    REQUIRE(s.font_size == 14.0f);
    REQUIRE(s.show_console_on_start);
    REQUIRE(s.default_new_project_parent.empty());
}

TEST_CASE("settings: save then load round-trips", "[settings]")
{
    IsolateConfigDir iso;

    Settings s;
    s.theme = "light";
    s.font_size = 16.0f;
    s.show_console_on_start = false;
    s.default_new_project_parent = "/tmp/projects";

    std::string error;
    REQUIRE(SettingsStore::save_global(s, error));
    REQUIRE(error.empty());

    Settings loaded = SettingsStore::load_global();
    REQUIRE(loaded.theme == "light");
    REQUIRE(loaded.font_size == 16.0f);
    REQUIRE_FALSE(loaded.show_console_on_start);
    REQUIRE(loaded.default_new_project_parent == "/tmp/projects");
}

TEST_CASE("settings: load_global with a malformed file returns defaults",
          "[settings]")
{
    IsolateConfigDir iso;

    // Write a malformed file directly.
    const auto path = paths::global_config_dir() / "settings.json";
    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream out(path);
        out << "{ not json }";
    }

    Settings s = SettingsStore::load_global();
    REQUIRE(s.theme == "dark");
    REQUIRE(s.font_size == 14.0f);
}

TEST_CASE("recent: load_recent returns empty when file is missing",
          "[settings]")
{
    IsolateConfigDir iso;
    RecentProjects r = SettingsStore::load_recent();
    REQUIRE(r.entries.empty());
}

TEST_CASE("recent: save then load round-trips", "[settings]")
{
    IsolateConfigDir iso;

    RecentProjects r;
    SettingsStore::push_recent(r, "/a/GameA", "GameA", 100);
    SettingsStore::push_recent(r, "/b/GameB", "GameB", 200);
    SettingsStore::push_recent(r, "/c/GameC", "GameC", 300);

    std::string error;
    REQUIRE(SettingsStore::save_recent(r, error));
    REQUIRE(error.empty());

    RecentProjects loaded = SettingsStore::load_recent();
    REQUIRE(loaded.entries.size() == 3);
    REQUIRE(loaded.entries[0].path == "/c/GameC");
    REQUIRE(loaded.entries[0].name == "GameC");
    REQUIRE(loaded.entries[1].path == "/b/GameB");
    REQUIRE(loaded.entries[2].path == "/a/GameA");
}

TEST_CASE("recent: push_recent moves an existing entry to the front",
          "[settings]")
{
    RecentProjects r;
    SettingsStore::push_recent(r, "/a/GameA", "GameA", 100);
    SettingsStore::push_recent(r, "/b/GameB", "GameB", 200);
    SettingsStore::push_recent(r, "/a/GameA", "GameA", 300);

    REQUIRE(r.entries.size() == 2);
    REQUIRE(r.entries[0].path == "/a/GameA");
    REQUIRE(r.entries[0].last_opened == 300);
    REQUIRE(r.entries[1].path == "/b/GameB");
}

TEST_CASE("recent: push_recent truncates to kMaxEntries", "[settings]")
{
    RecentProjects r;
    for (std::size_t i = 0; i < RecentProjects::kMaxEntries + 5; ++i)
    {
        const std::string path = "/p/Game" + std::to_string(i);
        SettingsStore::push_recent(r, path, "Game", static_cast<std::int64_t>(i));
    }
    REQUIRE(r.entries.size() == RecentProjects::kMaxEntries);
    // The newest entry (i = kMaxEntries + 4) is at the front.
    REQUIRE(r.entries[0].path ==
            "/p/Game" + std::to_string(RecentProjects::kMaxEntries + 4));
}