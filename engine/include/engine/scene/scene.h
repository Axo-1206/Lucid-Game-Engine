#pragma once

#include "engine/render/camera.h"
#include "engine/ecs/world.h"
#include "engine/scene/entity.h"

#include <string>

namespace engine
{

    class Scene
    {
    public:
        explicit Scene(engine::ecs::World &world);

        bool load_from_file(const std::string &path);
        bool save_to_file(const std::string &path) const;
        void clear();

        engine::ecs::RowRef active_camera() const { return active_camera_; }
        void set_active_camera(engine::ecs::RowRef cam) { active_camera_ = cam; }
        void set_active_camera_by_name(const std::string &name);

        CameraData get_active_camera_data() const;
        engine::ecs::RowRef find_camera(const std::string &name) const;

        const std::string &name() const { return name_; }
        void set_name(const std::string &n) { name_ = n; }

    private:
        engine::ecs::World &world_;
        engine::ecs::RowRef active_camera_;
        std::string name_;

        friend class SceneLoader;
        friend class SceneSaver;
    };

} // namespace engine