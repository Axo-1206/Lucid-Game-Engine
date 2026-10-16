#include "editor/project.h"

#include "engine/core/log.h"
#include "engine/core/version.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace editor
{

    namespace
    {

        // The default scene Project::create writes. A single
        // orthographic camera, no entities. The .lscene schema
        // is documented in docs/notes/SceneFormat.md.
        constexpr const char *kDefaultSceneJson = R"({
  "version": 1,
  "name": "main",
  "cameras": [
    {
      "name": "main",
      "pos": [0.0, 0.0, 10.0],
      "target": [0.0, 0.0, 0.0],
      "fov": 60.0,
      "near": 0.1,
      "far": 1000.0,
      "is_orthographic": true,
      "ortho_size": 5.0,
      "clear_color": [0.1, 0.1, 0.15, 1.0]
    }
  ],
  "entities": [],
  "engine_state": { "next_entity_id": 1 }
}
)";

        // The standard directories every project has. Project::create
        // makes them; Project::open warns if one is missing.
        constexpr const char *kStandardDirs[] = {
            "assets",
            "scenes",
            "scripts",
            "packages",
            ".lucid",
            ".lucid/cache",
            ".lucid/generated",
        };

        bool read_file(const std::filesystem::path &path,
                       std::string &out,
                       std::string &error)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
            {
                error = "cannot open file: " + path.string();
                return false;
            }
            std::ostringstream ss;
            ss << in.rdbuf();
            out = ss.str();
            return true;
        }

        bool write_file(const std::filesystem::path &path,
                        const std::string &text,
                        std::string &error)
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                error = "cannot open file for writing: " + path.string();
                return false;
            }
            out << text;
            if (!out)
            {
                error = "write failed: " + path.string();
                return false;
            }
            return true;
        }

        // Serialize a ProjectFile to JSON. Public because save()
        // and create() both need it.
        nlohmann::json to_json(const ProjectFile &f)
        {
            nlohmann::json j;
            j["format_version"] = f.format_version;
            j["engine_version"] = f.engine_version;
            j["name"] = f.name;
            j["packages"] = f.packages;
            j["active_scene"] = f.active_scene;
            j["active_camera"] = f.active_camera;
            return j;
        }

        // Parse a ProjectFile from JSON. Returns false and fills
        // `error` on a malformed document.
        bool from_json(const nlohmann::json &j,
                       ProjectFile &out,
                       std::string &error)
        {
            try
            {
                out.format_version = j.value("format_version", "1");
                out.engine_version = j.value("engine_version", "");
                out.name = j.value("name", "");
                out.packages = j.value("packages",
                                       std::vector<std::string>{});
                out.active_scene = j.value("active_scene", "");
                out.active_camera = j.value("active_camera", "");
            }
            catch (const nlohmann::json::exception &e)
            {
                error = std::string("Invalid lucid.project.json: ") + e.what();
                return false;
            }
            return true;
        }

    } // namespace

    // ---- Project ----

    Project::Project(std::filesystem::path root, ProjectFile file)
        : root_(std::move(root)), file_(std::move(file))
    {
    }

    std::filesystem::path Project::lucid_dir() const
    {
        return root_ / ".lucid";
    }

    std::filesystem::path Project::path_of(
        const std::filesystem::path &relative) const
    {
        return root_ / relative;
    }

    // ---- open ----

    std::unique_ptr<Project> Project::open(
        const std::filesystem::path &folder,
        std::string &error)
    {
        const auto project_file = folder / kProjectFileName;

        if (!std::filesystem::exists(project_file))
        {
            error = "Not a Lucid project: " +
                    std::string(kProjectFileName) + " not found in " +
                    folder.string();
            return nullptr;
        }

        std::string text;
        if (!read_file(project_file, text, error))
        {
            return nullptr;
        }

        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(text);
        }
        catch (const nlohmann::json::parse_error &e)
        {
            error = std::string("Invalid lucid.project.json: ") + e.what();
            return nullptr;
        }

        ProjectFile file;
        if (!from_json(j, file, error))
        {
            return nullptr;
        }

        if (file.name.empty())
        {
            error = "Invalid lucid.project.json: 'name' is required and "
                    "must be non-empty.";
            return nullptr;
        }

        // Refuse a newer format_version. Accept older ones with a
        // log line, on the same principle as the scene loader.
        if (file.format_version > kCurrentFormatVersion)
        {
            error = "Project file format_version '" + file.format_version +
                    "' is newer than this editor understands ('" +
                    std::string(kCurrentFormatVersion) + "').";
            return nullptr;
        }
        if (file.format_version < kCurrentFormatVersion)
        {
            engine::log_info(
                "Project '%s' uses format_version '%s' (current is '%s'). "
                "Opening anyway.",
                file.name.c_str(),
                file.format_version.c_str(),
                kCurrentFormatVersion);
        }

        // Informational check on the engine version.
        if (!file.engine_version.empty() &&
            file.engine_version != engine::kEngineVersion)
        {
            engine::log_warn(
                "Project '%s' was created with engine '%s'; "
                "running engine is '%s'.",
                file.name.c_str(),
                file.engine_version.c_str(),
                engine::kEngineVersion);
        }

        // Warn about missing standard directories. Not fatal: the
        // editor can recreate them on demand.
        for (const char *rel : kStandardDirs)
        {
            const auto p = folder / rel;
            if (!std::filesystem::exists(p))
            {
                engine::log_warn("Project '%s' is missing directory '%s'.",
                                 file.name.c_str(), rel);
            }
        }

        return std::unique_ptr<Project>(
            new Project(folder, std::move(file)));
    }

    // ---- create ----

    std::unique_ptr<Project> Project::create(
        const std::filesystem::path &folder,
        const std::string &name,
        std::string &error)
    {
        if (name.empty())
        {
            error = "Project name must be non-empty.";
            return nullptr;
        }

        const auto project_file = folder / kProjectFileName;

        if (std::filesystem::exists(project_file))
        {
            error = "A Lucid project already exists at " + folder.string();
            return nullptr;
        }

        std::error_code ec;

        // Create the folder itself if it does not exist.
        std::filesystem::create_directories(folder, ec);
        if (ec)
        {
            error = "Cannot create folder " + folder.string() + ": " +
                    ec.message();
            return nullptr;
        }

        // Create the standard directories.
        for (const char *rel : kStandardDirs)
        {
            std::filesystem::create_directories(folder / rel, ec);
            if (ec)
            {
                error = "Cannot create directory " + std::string(rel) +
                        ": " + ec.message();
                return nullptr;
            }
        }

        // Build the project file.
        ProjectFile file;
        file.format_version = kCurrentFormatVersion;
        file.engine_version = engine::kEngineVersion;
        file.name = name;
        file.packages = {
            "engine.core",
            "engine.logic",
            "engine.math",
            "engine.render",
            "engine.ui",
        };
        file.active_scene = "scenes/main.lscene";
        file.active_camera = "main";

        // Write lucid.project.json.
        {
            const std::string text = to_json(file).dump(2) + "\n";
            if (!write_file(project_file, text, error))
            {
                return nullptr;
            }
        }

        // Write the default scene. Overwrite is fine here: the
        // project file did not exist, so the folder is fresh.
        {
            const auto scene_path = folder / "scenes" / "main.lscene";
            if (!write_file(scene_path, kDefaultSceneJson, error))
            {
                return nullptr;
            }
        }

        return std::unique_ptr<Project>(
            new Project(folder, std::move(file)));
    }

    // ---- save ----

    bool Project::save(std::string &error) const
    {
        const auto project_file = root_ / kProjectFileName;
        const std::string text = to_json(file_).dump(2) + "\n";
        return write_file(project_file, text, error);
    }

} // namespace editor