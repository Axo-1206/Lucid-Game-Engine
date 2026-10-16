#include "editor/project_roots.h"

#include "editor/platform_paths.h"
#include "editor/project.h"

#include "engine/core/log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace editor
{

    namespace
    {

        // Read a project file's `name`. Returns the empty string
        // if the file is missing, malformed, or has no name.
        std::string read_project_name(const std::filesystem::path &folder)
        {
            const auto file = folder / Project::kProjectFileName;

            std::ifstream in(file, std::ios::binary);
            if (!in)
                return {};

            std::ostringstream ss;
            ss << in.rdbuf();

            try
            {
                auto j = nlohmann::json::parse(ss.str());
                return j.value("name", std::string{});
            }
            catch (const nlohmann::json::exception &)
            {
                return {};
            }
        }

    } // namespace

    ProjectRoots ProjectRoots::load(const Settings &settings,
                                    std::filesystem::path default_root)
    {
        ProjectRoots r;

        r.default_root_ = default_root.empty()
                              ? paths::default_project_root()
                              : std::move(default_root);

        // Create the default root if it does not exist. The user
        // expects "New Project" to work without any setup.
        std::error_code ec;
        std::filesystem::create_directories(r.default_root_, ec);
        if (ec)
        {
            engine::log_warn(
                "Could not create default project root '%s': %s",
                r.default_root_.string().c_str(),
                ec.message().c_str());
        }

        r.roots_.push_back({r.default_root_, /*is_default=*/true});

        for (const std::string &s : settings.project_roots)
        {
            if (s.empty())
                continue;

            const std::filesystem::path p(s);

            // Drop a user root whose directory no longer exists.
            // Do NOT drop an empty root; the user might be about
            // to put a project there.
            if (!std::filesystem::exists(p))
            {
                engine::log_warn(
                    "Project root '%s' does not exist; dropping it.",
                    p.string().c_str());
                continue;
            }

            // Skip the default root and duplicates.
            if (p == r.default_root_)
                continue;

            bool already = false;
            for (const auto &existing : r.roots_)
            {
                if (existing.path == p)
                {
                    already = true;
                    break;
                }
            }
            if (already)
                continue;

            r.roots_.push_back({p, /*is_default=*/false});
        }

        return r;
    }

    void ProjectRoots::add_root(const std::filesystem::path &path)
    {
        if (path.empty() || path == default_root_)
            return;

        for (const auto &r : roots_)
        {
            if (r.path == path)
                return;
        }

        roots_.push_back({path, /*is_default=*/false});
    }

    void ProjectRoots::remove_root(const std::filesystem::path &path)
    {
        if (path == default_root_)
            return;

        for (auto it = roots_.begin(); it != roots_.end(); ++it)
        {
            if (it->path == path && !it->is_default)
            {
                roots_.erase(it);
                return;
            }
        }
    }

    std::vector<ProjectEntry> ProjectRoots::scan() const
    {
        std::vector<ProjectEntry> out;

        for (const auto &root : roots_)
        {
            std::error_code ec;
            if (!std::filesystem::is_directory(root.path, ec))
                continue;

            std::filesystem::directory_iterator it(
                root.path,
                std::filesystem::directory_options::skip_permission_denied,
                ec);
            if (ec)
            {
                engine::log_warn("Cannot read project root '%s': %s",
                                 root.path.string().c_str(),
                                 ec.message().c_str());
                continue;
            }

            for (const auto &dir_entry : it)
            {
                std::error_code entry_ec;
                if (!dir_entry.is_directory(entry_ec) || entry_ec)
                    continue;

                const auto folder = dir_entry.path();
                const auto project_file = folder / Project::kProjectFileName;

                if (!std::filesystem::exists(project_file))
                    continue;

                ProjectEntry pe;
                pe.folder = folder;

                const std::string name = read_project_name(folder);
                if (name.empty())
                {
                    // Project file exists but is malformed or has
                    // no name. Show it with the folder name, marked
                    // invalid so the user can click it and see why.
                    pe.name = folder.filename().string();
                    pe.valid = false;
                }
                else
                {
                    pe.name = name;
                    pe.valid = true;
                }

                out.push_back(std::move(pe));
            }
        }

        // Sort by name, case-insensitive. Stable for equal names.
        std::sort(out.begin(), out.end(),
                  [](const ProjectEntry &a, const ProjectEntry &b)
                  {
                      return std::lexicographical_compare(
                          a.name.begin(), a.name.end(),
                          b.name.begin(), b.name.end(),
                          [](unsigned char ca, unsigned char cb)
                          {
                              return std::tolower(ca) < std::tolower(cb);
                          });
                  });

        return out;
    }

    std::vector<std::string> ProjectRoots::user_roots_as_strings() const
    {
        std::vector<std::string> out;
        out.reserve(roots_.size());
        for (const auto &r : roots_)
        {
            if (!r.is_default)
                out.push_back(r.path.string());
        }
        return out;
    }

} // namespace editor