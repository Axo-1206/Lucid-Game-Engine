#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace editor
{

    // The parsed contents of lucid.project.json.
    //
    // This is plain data. It is the schema, not the behavior. The
    // editor reads it, the editor writes it, nothing else touches it.
    struct ProjectFile
    {
        // The project file's schema version. Bumped when the shape
        // of this struct changes. The editor refuses a project whose
        // format_version is newer than the one it understands.
        std::string format_version = "1";

        // The engine version the project was created against.
        // Informational today; Project::open logs a warning if it
        // differs from engine::kEngineVersion.
        std::string engine_version;

        // The display name. Required, non-empty.
        std::string name;

        // The packages the project loads, in load order. Names are
        // package names, e.g. "engine.core".
        std::vector<std::string> packages;

        // The scene to open when the editor starts, or empty. A
        // project created by Project::create has a default scene
        // at "scenes/main.lscene".
        std::string active_scene;

        // The camera to activate, or empty.
        std::string active_camera;
    };

    // A Lucid project on disk.
    //
    // A Project always corresponds to a real folder that contains
    // a valid lucid.project.json. There is no "in-memory project"
    // and no way to construct one without a folder. Use
    // Project::open or Project::create.
    class Project
    {
    public:
        // The project file's name and the schema version this
        // editor understands.
        static constexpr const char *kProjectFileName = "lucid.project.json";
        static constexpr const char *kCurrentFormatVersion = "1";

        // Open an existing project. Reads folder/lucid.project.json,
        // parses and validates it. Returns nullptr and fills `error`
        // if the folder is not a valid Lucid project.
        static std::unique_ptr<Project> open(const std::filesystem::path &folder,
                                             std::string &error);

        // Create a new project at the given folder. The folder must
        // not already contain a lucid.project.json. Creates the
        // folder structure and writes a default project file and
        // a default scene. Returns nullptr and fills `error` on
        // failure.
        static std::unique_ptr<Project> create(const std::filesystem::path &folder,
                                               const std::string &name,
                                               std::string &error);

        // Not copyable, not movable. A Project is a handle to one
        // folder; duplicating it would be a bug.
        Project(const Project &) = delete;
        Project &operator=(const Project &) = delete;
        Project(Project &&) = delete;
        Project &operator=(Project &&) = delete;

        // The project's root folder.
        const std::filesystem::path &root() const { return root_; }

        // The parsed project file.
        const ProjectFile &file() const { return file_; }

        // The project-local editor state directory, root/.lucid.
        std::filesystem::path lucid_dir() const;

        // Convenience: root/<relative_path>.
        std::filesystem::path path_of(const std::filesystem::path &relative) const;

        // The project's display name. Equal to file().name.
        const std::string &name() const { return file_.name; }

        // Write the current ProjectFile back to lucid.project.json.
        // Returns false on I/O failure.
        bool save(std::string &error) const;

    private:
        Project(std::filesystem::path root, ProjectFile file);

        std::filesystem::path root_;
        ProjectFile file_;
    };

} // namespace editor