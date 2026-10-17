#pragma once

#include "editor/diagnostics.h"

#include "engine/registry/package_manager.h"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace editor
{

    // The result of a project build.
    //
    // A build walks <project_root>/scripts/, compiles every .lucid
    // file, and writes a .lucgraph next to each. Diagnostics are
    // recorded in the store the caller passes, and are also
    // returned here for convenience.
    struct BuildResult
    {
        std::size_t files_total = 0;
        std::size_t files_ok = 0;
        std::size_t files_failed = 0;
        std::size_t bytes_written = 0;
        std::vector<DiagnosticEntry> diagnostics;
    };

    // Compile every .lucid file under <project_root>/scripts/,
    // recursively. For each file that compiles clean, write a
    // .lucgraph next to it (same directory, .lucid replaced with
    // .lucgraph). For each file that fails, record the diagnostics
    // and continue.
    //
    // The store's entries for each compiled file are replaced with
    // the compile result, the same way a per-file compile does.
    // Diagnostics for other files are left alone.
    //
    // `packages` must have been applied; its registry is used for
    // every compile. If it has not been applied, the result
    // contains a single Error diagnostic and no files are
    // processed.
    //
    // The walk is deterministic: files are compiled in sorted
    // path order.
    BuildResult compile_project(const std::filesystem::path &project_root,
                                const engine::PackageManager &packages,
                                DiagnosticStore &store);

} // namespace editor