#include "editor/platform_paths.h"

#include <cstdlib>
#include <string>

namespace editor::paths
{

    namespace
    {

        // Read an environment variable. Returns an empty string if
        // the variable is unset or empty.
        std::string env(const char *name)
        {
            const char *v = std::getenv(name);
            return (v && *v) ? std::string(v) : std::string{};
        }

        // Return the user's home directory. Empty if unknown.
        std::string home_dir()
        {
#if defined(_WIN32)
            std::string h = env("USERPROFILE");
            if (!h.empty())
                return h;
            // Fall back to HOMEDRIVE + HOMEPATH, which some
            // environments set instead of USERPROFILE.
            const std::string drive = env("HOMEDRIVE");
            const std::string path = env("HOMEPATH");
            if (!drive.empty() && !path.empty())
                return drive + path;
            return {};
#else
            return env("HOME");
#endif
        }

        // The fallback directory, used when no platform location
        // can be determined. Relative to the current working
        // directory.
        std::filesystem::path fallback_dir()
        {
            return std::filesystem::current_path() / "lucid_editor_config";
        }

#if defined(_WIN32)
        std::filesystem::path config_dir_win()
        {
            const std::string appdata = env("APPDATA");
            if (!appdata.empty())
                return std::filesystem::path(appdata) / "Lucid";
            return fallback_dir();
        }
        std::filesystem::path cache_dir_win()
        {
            const std::string local = env("LOCALAPPDATA");
            if (!local.empty())
                return std::filesystem::path(local) / "Lucid";
            return config_dir_win();
        }
#elif defined(__APPLE__)
        std::filesystem::path config_dir_mac()
        {
            const std::string home = home_dir();
            if (!home.empty())
                return std::filesystem::path(home) /
                       "Library" / "Application Support" / "Lucid";
            return fallback_dir();
        }
        std::filesystem::path cache_dir_mac()
        {
            const std::string home = home_dir();
            if (!home.empty())
                return std::filesystem::path(home) /
                       "Library" / "Caches" / "Lucid";
            return fallback_dir();
        }
#else
        std::filesystem::path config_dir_linux()
        {
            const std::string xdg = env("XDG_CONFIG_HOME");
            if (!xdg.empty())
                return std::filesystem::path(xdg) / "Lucid";
            const std::string home = home_dir();
            if (!home.empty())
                return std::filesystem::path(home) / ".config" / "Lucid";
            return fallback_dir();
        }
        std::filesystem::path cache_dir_linux()
        {
            const std::string xdg = env("XDG_CACHE_HOME");
            if (!xdg.empty())
                return std::filesystem::path(xdg) / "Lucid";
            const std::string home = home_dir();
            if (!home.empty())
                return std::filesystem::path(home) / ".cache" / "Lucid";
            return fallback_dir();
        }
#endif

    } // namespace

    std::filesystem::path global_config_dir()
    {
#if defined(_WIN32)
        return config_dir_win();
#elif defined(__APPLE__)
        return config_dir_mac();
#else
        return config_dir_linux();
#endif
    }

    std::filesystem::path global_cache_dir()
    {
#if defined(_WIN32)
        return cache_dir_win();
#elif defined(__APPLE__)
        return cache_dir_mac();
#else
        return cache_dir_linux();
#endif
    }

} // namespace editor::paths