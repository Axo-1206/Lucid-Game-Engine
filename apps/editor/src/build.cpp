#include "editor/build.h"

#include "engine/core/log.h"
#include "engine/script/compiler.h"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <sstream>

namespace editor
{

    namespace
    {

        // Collect every .lucid file under `root`, recursively.
        // Returns sorted paths.
        std::vector<std::filesystem::path> collect_lucid_files(
            const std::filesystem::path &root)
        {
            std::vector<std::filesystem::path> files;

            std::error_code ec;
            if (!std::filesystem::is_directory(root, ec) || ec)
                return files;

            std::filesystem::recursive_directory_iterator it(
                root,
                std::filesystem::directory_options::skip_permission_denied,
                ec);
            if (ec)
                return files;

            for (const auto &entry : it)
            {
                std::error_code entry_ec;
                if (!entry.is_regular_file(entry_ec) || entry_ec)
                    continue;

                if (entry.path().extension() == ".lucid")
                    files.push_back(entry.path());
            }

            std::sort(files.begin(), files.end());
            return files;
        }

        // Read a file into a string. Returns false and fills
        // `error` on failure.
        bool read_file(const std::filesystem::path &path,
                       std::string &out,
                       std::string &error)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
            {
                error = "Cannot read file: " + path.string();
                return false;
            }
            std::ostringstream ss;
            ss << in.rdbuf();
            out = ss.str();
            return true;
        }

        // Write bytes to a file. Returns false and fills `error`
        // on failure.
        bool write_file(const std::filesystem::path &path,
                        const std::vector<std::uint8_t> &bytes,
                        std::string &error)
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                error = "Cannot write file: " + path.string();
                return false;
            }
            out.write(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()));
            if (!out)
            {
                error = "Write failed: " + path.string();
                return false;
            }
            return true;
        }

        // The output path for a .lucid input: same directory, same
        // stem, .lucgraph extension.
        std::filesystem::path output_path_for(
            const std::filesystem::path &lucid_path)
        {
            std::filesystem::path out = lucid_path;
            out.replace_extension(".lucgraph");
            return out;
        }

        // Convert an engine::CompileDiagnostic to a DiagnosticEntry.
        DiagnosticEntry to_entry(const std::filesystem::path &file,
                                 const engine::CompileDiagnostic &d)
        {
            DiagnosticEntry e;
            e.file = file;
            e.line = d.line;
            e.column = d.column;
            e.severity = d.severity;
            e.message = d.message;
            e.code = d.code;
            e.source = DiagnosticSource::Compiler;
            return e;
        }

    } // namespace

    BuildResult compile_project(const std::filesystem::path &project_root,
                                const engine::PackageManager &packages,
                                DiagnosticStore &store)
    {
        BuildResult result;

        if (!packages.applied())
        {
            DiagnosticEntry e;
            e.severity = engine::CompileDiagnostic::Severity::Error;
            e.message = "PackageManager has not been applied; "
                        "cannot build.";
            e.source = DiagnosticSource::Compiler;
            result.diagnostics.push_back(e);
            return result;
        }

        const auto scripts_dir = project_root / "scripts";
        const auto files = collect_lucid_files(scripts_dir);
        result.files_total = files.size();

        if (files.empty())
        {
            engine::log_info("Build: no .lucid files under %s",
                             scripts_dir.string().c_str());
            return result;
        }

        engine::log_info("Build: compiling %zu .lucid file%s",
                         files.size(), files.size() == 1 ? "" : "s");

        for (const auto &file : files)
        {
            // Read the source.
            std::string source;
            std::string read_error;
            if (!read_file(file, source, read_error))
            {
                DiagnosticEntry e;
                e.file = file;
                e.severity = engine::CompileDiagnostic::Severity::Error;
                e.message = read_error;
                e.source = DiagnosticSource::Compiler;
                store.replace_for_file(file, {e});
                result.diagnostics.push_back(e);
                ++result.files_failed;
                continue;
            }

            // Compile and serialize.
            const auto build = engine::compile_to_bytes(
                source, file.string(), packages);

            // Translate diagnostics into editor entries.
            std::vector<DiagnosticEntry> entries;
            entries.reserve(build.diagnostics.size());
            for (const auto &d : build.diagnostics)
                entries.push_back(to_entry(file, d));

            // Replace this file's diagnostics in the store.
            store.replace_for_file(file, entries);

            // Append to the result's diagnostics.
            for (auto &e : entries)
                result.diagnostics.push_back(std::move(e));

            if (!build.ok)
            {
                ++result.files_failed;
                continue;
            }

            // Write the .lucgraph next to the source.
            const auto out_path = output_path_for(file);
            std::string write_error;
            if (!write_file(out_path, build.bytes, write_error))
            {
                DiagnosticEntry e;
                e.file = file;
                e.severity = engine::CompileDiagnostic::Severity::Error;
                e.message = write_error;
                e.source = DiagnosticSource::Compiler;
                store.replace_for_file(file, {e});
                result.diagnostics.push_back(e);
                ++result.files_failed;
                continue;
            }

            ++result.files_ok;
            result.bytes_written += build.bytes.size();
        }

        return result;
    }

} // namespace editor