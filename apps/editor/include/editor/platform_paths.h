#pragma once

#include <filesystem>

namespace editor::paths
{

    // Where machine-local editor state lives. The functions return
    // a path; they do not create the directory. A caller that
    // writes creates it first.
    //
    // Windows:  %APPDATA%/Lucid/          or  %LOCALAPPDATA%/Lucid/
    // Linux:    $XDG_CONFIG_HOME/Lucid/   or  $XDG_CACHE_HOME/Lucid/
    // macOS:    ~/Library/Application Support/Lucid/
    //           ~/Library/Caches/Lucid/
    //
    // If the environment variable is missing, the function falls
    // back to the platform's default location. If that is also
    // missing, it returns ./lucid_editor_config/ (a directory in
    // the current working directory) and the editor logs a
    // warning at startup. Degraded but functional.

    std::filesystem::path global_config_dir();
    std::filesystem::path global_cache_dir();

    // Where the editor puts new projects by default. The caller
    // (ProjectRoots::load) creates the directory if it does not
    // exist.
    //
    // Windows:  %USERPROFILE%/Lucid Projects/
    // Linux:    ~/Lucid Projects/
    // macOS:    ~/Lucid Projects/
    //
    // If the home directory cannot be determined, returns
    // ./Lucid Projects/ in the current working directory. The
    // editor logs a warning at startup in that case.
    std::filesystem::path default_project_root();

} // namespace editor::paths