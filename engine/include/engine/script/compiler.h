#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace engine
{

    class PackageManager;

    // A single diagnostic from a compile. The engine's own type,
    // so a caller that must not include a lucid/* header (the
    // editor) can read a compile's outcome without seeing the
    // format library.
    //
    // The engine drops the format library's `code` (nothing
    // filters by it today) and its `file` (the caller knows which
    // buffer it compiled). The `location`, when known, becomes
    // 1-based line and column.
    struct CompileDiagnostic
    {
        enum class Severity : std::uint8_t
        {
            Hint,
            Note,
            Warning,
            Error,
            Fatal,
        };

        Severity severity = Severity::Note;
        std::uint32_t line = 0;   // 1-based; 0 means "unknown"
        std::uint32_t column = 0; // 1-based; 0 means "unknown"
        std::string message;
    };

    // The result of a diagnostics-only compile. The compiled graph
    // is not retained; the caller wanted to know "did this compile,
    // and what were the errors".
    //
    // Used by the editor's per-edit loop.
    struct CompileCheckResult
    {
        bool ok = false;
        std::vector<CompileDiagnostic> diagnostics;
    };

    // The result of a compile-to-bytes. The caller gets the
    // .lucgraph bytes and no graph. Used by the editor's Build
    // action.
    struct CompileBuildResult
    {
        bool ok = false;
        std::vector<CompileDiagnostic> diagnostics;
        std::vector<std::uint8_t> bytes; // empty when !ok
    };

    // Compile a .lucid source buffer for diagnostics only.
    //
    // The compile uses the registry built from the packages the
    // PackageManager holds. The manager must have been applied
    // (apply or apply_registration_only); if it has not, the
    // result carries an Error diagnostic and `ok` is false.
    //
    // The filename is for diagnostics; it is not opened or read.
    // Imports are not followed; a source that imports another
    // module compiles only if the import is not used (Sema reports
    // an unresolved import otherwise). A future variant takes
    // CompileOptions to follow imports; the editor does not need
    // it today.
    CompileCheckResult check_source(const std::string &source,
                                    const std::string &filename,
                                    const PackageManager &packages);

    // Compile a .lucid source buffer and serialize the graph to
    // .lucgraph bytes. Same registry and same preconditions as
    // check_source. On success, `bytes` holds the .lucgraph file
    // contents; on failure, `bytes` is empty.
    CompileBuildResult compile_to_bytes(const std::string &source,
                                        const std::string &filename,
                                        const PackageManager &packages);

} // namespace engine