#include "editor/settings.h"
#include "editor/platform_paths.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace editor
{

    namespace
    {

        constexpr const char *kSettingsFileName = "settings.json";
        constexpr const char *kRecentFileName = "recent_projects.json";

        // Write `text` to `path`, creating the parent directory
        // first. Returns false and fills `error` on failure.
        bool write_text(const std::filesystem::path &path,
                        const std::string &text,
                        std::string &error)
        {
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            if (ec)
            {
                error = "Cannot create directory " +
                        path.parent_path().string() + ": " + ec.message();
                return false;
            }

            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                error = "Cannot open file for writing: " + path.string();
                return false;
            }
            out << text;
            if (!out)
            {
                error = "Write failed: " + path.string();
                return false;
            }
            return true;
        }

        // Read `path` into `out`. Returns false if the file does
        // not exist or cannot be read. A read failure is not an
        // error at the load_* level; it just means "use defaults".
        bool read_text(const std::filesystem::path &path, std::string &out)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return false;
            std::ostringstream ss;
            ss << in.rdbuf();
            out = ss.str();
            return true;
        }

    } // namespace

    // ---- Settings ----

    Settings SettingsStore::load_global()
    {
        Settings s;

        std::string text;
        if (!read_text(paths::global_config_dir() / kSettingsFileName, text))
            return s;

        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(text);
        }
        catch (const nlohmann::json::parse_error &)
        {
            // Malformed file; return defaults. The next save will
            // overwrite it.
            return s;
        }

        try
        {
            s.theme = j.value("theme", s.theme);
            s.font_size = j.value("font_size", s.font_size);
            s.show_console_on_start =
                j.value("show_console_on_start", s.show_console_on_start);
            s.default_new_project_parent =
                j.value("default_new_project_parent",
                        s.default_new_project_parent);
            s.project_roots =
                j.value("project_roots", std::vector<std::string>{});
        }
        catch (const nlohmann::json::exception &)
        {
            // A field has the wrong type; return what we have.
            // The defaults for unparsed fields are already set.
        }

        return s;
    }

    bool SettingsStore::save_global(const Settings &settings,
                                    std::string &error)
    {
        nlohmann::json j;
        j["theme"] = settings.theme;
        j["font_size"] = settings.font_size;
        j["show_console_on_start"] = settings.show_console_on_start;
        j["default_new_project_parent"] = settings.default_new_project_parent;
        j["project_roots"] = settings.project_roots;

        const std::string text = j.dump(2) + "\n";
        return write_text(paths::global_config_dir() / kSettingsFileName,
                          text, error);
    }

    // ---- Recent projects ----

    RecentProjects SettingsStore::load_recent()
    {
        RecentProjects r;

        std::string text;
        if (!read_text(paths::global_config_dir() / kRecentFileName, text))
            return r;

        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(text);
        }
        catch (const nlohmann::json::parse_error &)
        {
            return r;
        }

        try
        {
            if (j.contains("entries") && j["entries"].is_array())
            {
                for (const auto &e : j["entries"])
                {
                    RecentEntry entry;
                    entry.path = e.value("path", "");
                    entry.name = e.value("name", "");
                    entry.last_opened = e.value("last_opened", std::int64_t{0});
                    if (!entry.path.empty())
                        r.entries.push_back(std::move(entry));
                }
            }
        }
        catch (const nlohmann::json::exception &)
        {
            // Partial parse; keep what we have.
        }

        // Truncate in case the file was written by a newer editor
        // with a higher cap.
        if (r.entries.size() > RecentProjects::kMaxEntries)
            r.entries.resize(RecentProjects::kMaxEntries);

        return r;
    }

    bool SettingsStore::save_recent(const RecentProjects &recent,
                                    std::string &error)
    {
        nlohmann::json entries = nlohmann::json::array();
        for (const auto &e : recent.entries)
        {
            nlohmann::json je;
            je["path"] = e.path;
            je["name"] = e.name;
            je["last_opened"] = e.last_opened;
            entries.push_back(std::move(je));
        }

        nlohmann::json j;
        j["entries"] = std::move(entries);

        const std::string text = j.dump(2) + "\n";
        return write_text(paths::global_config_dir() / kRecentFileName,
                          text, error);
    }

    void SettingsStore::push_recent(RecentProjects &recent,
                                    const std::string &path,
                                    const std::string &name,
                                    std::int64_t timestamp)
    {
        // Remove any existing entry with the same path.
        recent.entries.erase(
            std::remove_if(recent.entries.begin(), recent.entries.end(),
                           [&](const RecentEntry &e)
                           { return e.path == path; }),
            recent.entries.end());

        // Insert the new entry at the front.
        RecentEntry entry;
        entry.path = path;
        entry.name = name;
        entry.last_opened = timestamp;
        recent.entries.insert(recent.entries.begin(), std::move(entry));

        // Truncate.
        if (recent.entries.size() > RecentProjects::kMaxEntries)
            recent.entries.resize(RecentProjects::kMaxEntries);
    }

    std::int64_t SettingsStore::now_seconds()
    {
        using namespace std::chrono;
        return duration_cast<seconds>(
                   system_clock::now().time_since_epoch())
            .count();
    }

} // namespace editor