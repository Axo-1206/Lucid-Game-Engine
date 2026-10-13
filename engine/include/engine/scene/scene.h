#pragma once

#include "engine/ecs/world.h"
#include "engine/render/camera.h"
#include "engine/scene/scene_schema.h"

#include <cstdint>
#include <string>
#include <vector>

namespace engine
{

    class ComponentRegistry;

    // The scene's schema. A description of entities, components, and
    // cameras read from a .lscene file.
    //
    // A Scene is a value. It does not hold a World&. It does not clear,
    // load into, or save from a world on its own; apply_to() and
    // extract_from() do that. See docs/Decision.md, Decision 2.
    class Scene
    {
    public:
        Scene() = default;

        const std::string &name() const { return name_; }
        void set_name(std::string n) { name_ = std::move(n); }

        const std::string &path() const { return path_; }
        void set_path(std::string p) { path_ = std::move(p); }

        // The active camera, by name. The name is canonical; the world
        // caches a RowRef. See docs/Decision.md, Decision 3.
        const std::string &active_camera_name() const { return active_camera_name_; }
        void set_active_camera_name(std::string n) { active_camera_name_ = std::move(n); }

        // The id allocator value to restore after apply_to. 0 means
        // "compute from max(entity.id) + 1".
        std::uint64_t next_entity_id() const { return next_entity_id_; }
        void set_next_entity_id(std::uint64_t v) { next_entity_id_ = v; }

        // The raw JSON this scene was loaded from. Empty for a
        // Scene built by hand or by extract_from(). Used by the loader
        // to defer entity and component parsing to apply_to, so the
        // Scene type does not need to know the full component schema.
        //
        // This is a deliberate design choice: the Scene holds the
        // description as text, and apply_to parses it. A future version
        // may hoist the parsing into typed EntityDesc/CameraDesc
        // vectors; for now, the text is the source of truth and the
        // Scene is a thin envelope around it.
        const std::string &json_text() const { return json_text_; }
        void set_json_text(std::string t) { json_text_ = std::move(t); }

        // Apply this description to a world.
        //
        // Clear:    world.clear_rows() first, then add the scene's
        //           entities and cameras. The default.
        //
        // Additive: leave the world's rows; add the scene's entities
        //           and cameras. Entity ids in the scene must not
        //           collide with existing ones.
        void apply_to(engine::ecs::World &world,
                      const ComponentRegistry &components,
                      ApplyMode mode = ApplyMode::Clear) const;

        // Read the current world state into a description.
        static Scene extract_from(const engine::ecs::World &world,
                                  const ComponentRegistry &components);

    private:
        std::string name_;
        std::string path_;
        std::string json_text_;
        std::string active_camera_name_;
        std::uint64_t next_entity_id_ = 0;
    };

    // ---- Free functions ----
    //
    // The loader and saver are free functions, not members and not
    // friends. Scene is an aggregate. See docs/Decision.md, Decision 13.

    // Read a .lscene file into a Scene. On failure, returns a default
    // Scene and logs an error.
    Scene SceneLoader_load(const std::string &path);

    // Load a .lscene file and apply it to a world in one call.
    // Equivalent to SceneLoader_load(path).apply_to(world, mode).
    // Returns false if the file could not be read or parsed.
    bool SceneLoader_load_into(engine::ecs::World &world,
                               const ComponentRegistry &components,
                               const std::string &path,
                               ApplyMode mode = ApplyMode::Clear);

    // Write a Scene to a .lscene file. Returns false on failure.
    // Does not read the world; use SceneSaver_from_world first if the
    // scene should reflect the world's current state.
    bool SceneSaver_save(const Scene &scene, const std::string &path);

    // Read a world's current state into a Scene.
    Scene SceneSaver_from_world(const engine::ecs::World &world,
                                const ComponentRegistry &components);

} // namespace engine