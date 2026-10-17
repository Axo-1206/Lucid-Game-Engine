#include "editor/platform_paths.h"

#include <cstdlib>
#include <string>
#include <system_error>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace editor::paths
{

    namespace
    {

        // ─── Environment / home ──────────────────────────────────────────

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

        // ─── Config / cache ───────────────────────────────────────────────

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

        // ─── Executable path ──────────────────────────────────────────────

#if defined(_WIN32)
        // GetModuleFileNameW(nullptr, ...) returns the full path of
        // the running .exe. A growing buffer handles paths longer
        // than MAX_PATH on systems where long-path support is on.
        std::filesystem::path executable_path_win()
        {
            std::wstring buf;
            buf.resize(MAX_PATH);

            for (;;)
            {
                const DWORD n = ::GetModuleFileNameW(
                    nullptr, buf.data(),
                    static_cast<DWORD>(buf.size()));

                if (n == 0)
                    return {};

                if (n < buf.size())
                {
                    buf.resize(n);
                    break;
                }

                // Buffer too small; double and retry.
                buf.resize(buf.size() * 2);
            }

            if (buf.empty())
                return {};

            return std::filesystem::path(buf);
        }
#elif defined(__APPLE__)
        // _NSGetExecutablePath is the canonical way. It can return
        // a relative path or one that goes through symlinks;
        // weakly_canonical normalizes both. If canonicalization
        // fails, return the raw path.
        std::filesystem::path executable_path_mac()
        {
            uint32_t size = 0;
            _NSGetExecutablePath(nullptr, &size);
            if (size == 0)
                return {};

            std::string buf;
            buf.resize(size);
            if (_NSGetExecutablePath(buf.data(), &size) != 0)
                return {};

            std::error_code ec;
            auto p = std::filesystem::weakly_canonical(
                std::filesystem::path(buf), ec);
            return ec ? std::filesystem::path(buf) : p;
        }
#else
        // /proc/self/exe is a symlink to the running executable.
        // It is present on every modern Linux, including inside
        // containers. A chroot without /proc is the only case
        // where it fails, and the caller falls back to CWD.
        std::filesystem::path executable_path_linux()
        {
            std::error_code ec;
            auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
            if (!ec && !p.empty())
                return p;
            return {};
        }
#endif

    } // namespace

    // ─── Public API ──────────────────────────────────────────────────────

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

    std::filesystem::path default_project_root()
    {
        const std::string home = home_dir();
        if (!home.empty())
            return std::filesystem::path(home) / "Lucid Projects";
        return std::filesystem::current_path() / "Lucid Projects";
    }

    std::filesystem::path executable_dir()
    {
        std::filesystem::path exe;

#if defined(_WIN32)
        exe = executable_path_win();
#elif defined(__APPLE__)
        exe = executable_path_mac();
#else
        exe = executable_path_linux();
#endif

        if (exe.empty())
        {
            // Degraded but functional: the caller gets a usable
            // directory, not an empty path. This only happens on
            // unusual platforms or in a chroot without /proc.
            return std::filesystem::current_path();
        }

        return exe.parent_path();
    }

} // namespace editor::paths