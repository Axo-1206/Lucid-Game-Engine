#pragma once

#include "engine/camera.h"
#include "engine/entity.h"
#include "lucid/runtime.h"

#include <string>

namespace engine {

// A scene: a collection of entities and cameras, loaded from and saved to
// a .lscene file.
//
// The Scene class owns no storage. It's a facade over the runtime's tables.
// It knows which tables belong to the scene, how to clear them, and how to
// serialize them.
//
// Multiple scenes could share a runtime, but for v1, one scene is loaded
// at a time.
class Scene {
public:
    explicit Scene(lucid::Runtime& rt);

    // Load a scene from a .lscene file. Clears all current entities and
    // cameras first. Returns false on failure (logs the error).
    bool load_from_file(const std::string& path);

    // Save the current scene to a .lscene file. Returns false on failure.
    bool save_to_file(const std::string& path) const;

    // Remove all entities and cameras, reset the engine state.
    void clear();

    // The active camera. The loader sets it to the first camera. The
    // editor or a script may change it.
    lucid::RowRef active_camera() const { return active_camera_; }
    void          set_active_camera(lucid::RowRef cam) { active_camera_ = cam; }
    void          set_active_camera_by_name(const std::string& name);

    // Read the active camera's fields, or return a default camera if no
    // active camera exists.
    CameraData get_active_camera_data() const;

    // Convenience: get a camera by name.
    lucid::RowRef find_camera(const std::string& name) const;

    // The scene's name (from the last load, or "" if unset).
    const std::string& name() const { return name_; }
    void set_name(const std::string& n) { name_ = n; }

private:
    lucid::Runtime& runtime_;
    lucid::RowRef   active_camera_;
    std::string     name_;

    friend class SceneLoader;
    friend class SceneSaver;
};

} // namespace engine
