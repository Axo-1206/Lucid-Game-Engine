#include "engine/scene/scene.h"
#include "engine/scene/scene_schema.h"
#include "engine/scene/component_registry.h"
#include "engine/core/log.h"
#include "engine/core/diagnostics.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <string>

namespace engine
{

    namespace
    {

        using json = nlohmann::json;

        // Emit a scene-load failure as a runtime diagnostic and log
        // the same line. The Console sees the log; the Diagnostics
        // panel (phase 2) sees the diagnostic, with the file name.
        void report_scene_load_error(const std::string &path,
                                     const std::string &message)
        {
            engine::RuntimeDiagnostic d;
            d.severity = engine::LogLevel::Error;
            d.file = path;
            d.message = message;
            engine::emit_diagnostic(d); // fires sinks, then logs the line
        }

    } // namespace

    Scene SceneLoader_load(const std::string &path)
    {
        Scene s;

        std::ifstream file(path);
        if (!file.is_open())
        {
            report_scene_load_error(path, "Failed to open scene file: " + path);
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
            report_scene_load_error(
                path,
                "Failed to parse scene file '" + path + "': " + ex.what());
            return s;
        }

        const int version = j.value("version", 1);
        if (version > kSceneSchemaVersion)
        {
            report_scene_load_error(
                path,
                "Scene file '" + path + "' is version " +
                    std::to_string(version) +
                    "; this engine supports up to version " +
                    std::to_string(kSceneSchemaVersion) +
                    ". Refusing to load.");
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

        if (j.contains("cameras") && j["cameras"].is_array() && !j["cameras"].empty())
        {
            const auto &first = j["cameras"][0];
            s.set_active_camera_name(first.value("name", std::string("")));
        }

        if (j.contains("engine_state") && j["engine_state"].is_object())
        {
            s.set_next_entity_id(j["engine_state"].value("next_entity_id", std::uint64_t(0)));
        }

        log_info("Loaded scene '%s' from '%s'", s.name().c_str(), path.c_str());
        return s;
    }

    bool SceneLoader_load_into(engine::ecs::World &world,
                               const ComponentRegistry &components,
                               const std::string &path,
                               ApplyMode mode)
    {
        Scene s = SceneLoader_load(path);
        if (s.json_text().empty())
        {
            return false;
        }
        s.apply_to(world, components, mode);
        return true;
    }

} // namespace engine