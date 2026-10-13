#include "engine/scene/scene.h"
#include "engine/scene/scene_schema.h"
#include "engine/core/log.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <string>

namespace engine
{

    namespace
    {

        using json = nlohmann::json;

    } // namespace

    Scene SceneLoader_load(const std::string &path)
    {
        Scene s;

        std::ifstream file(path);
        if (!file.is_open())
        {
            log_error("Failed to open scene file: %s", path.c_str());
            return s;
        }

        std::stringstream buf;
        buf << file.rdbuf();
        const std::string text = buf.str();

        json j;
        try
        {
            j = json::parse(text);
        }
        catch (const std::exception &ex)
        {
            log_error("Failed to parse scene file '%s': %s", path.c_str(), ex.what());
            return s;
        }

        // Version check.
        const int version = j.value("version", 1);
        if (version > kSceneSchemaVersion)
        {
            log_error("Scene file '%s' is version %d; this engine supports up to version %d. Refusing to load.",
                      path.c_str(), version, kSceneSchemaVersion);
            return s;
        }
        if (version < kSceneSchemaVersion)
        {
            log_info("Scene file '%s' is version %d; this engine is version %d. Loading with best-effort compatibility.",
                     path.c_str(), version, kSceneSchemaVersion);
        }

        s.set_name(j.value("name", std::string("")));
        s.set_path(path);
        s.set_json_text(text);

        // The active camera name: the first camera's name, if any.
        if (j.contains("cameras") && j["cameras"].is_array() && !j["cameras"].empty())
        {
            const auto &first = j["cameras"][0];
            s.set_active_camera_name(first.value("name", std::string("")));
        }

        // The entity-id allocator value.
        if (j.contains("engine_state") && j["engine_state"].is_object())
        {
            s.set_next_entity_id(j["engine_state"].value("next_entity_id", std::uint64_t(0)));
        }

        log_info("Loaded scene '%s' from '%s'", s.name().c_str(), path.c_str());
        return s;
    }

    bool SceneLoader_load_into(engine::ecs::World &world,
                               const std::string &path,
                               ApplyMode mode)
    {
        Scene s = SceneLoader_load(path);
        if (s.json_text().empty())
        {
            // The file could not be read or parsed. Do not clear the
            // world; a failed load should not leave an empty world.
            return false;
        }
        s.apply_to(world, mode);
        return true;
    }

} // namespace engine