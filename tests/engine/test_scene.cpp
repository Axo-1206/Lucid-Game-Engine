#include <catch2/catch_test_macros.hpp>
#include "engine/scene/scene.h"
#include "engine/ecs/core/entity.h"
#include "engine/ecs/world.h"
#include "engine/scene/component_registry.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace engine;

namespace
{

    std::string make_temp_path(const std::string &name)
    {
        auto dir = std::filesystem::temp_directory_path();
        return (dir / name).string();
    }

    struct Fixture
    {
        engine::ecs::World world;
        engine::ComponentRegistry components;

        Fixture()
        {
            setup_core_tables(world);
            register_core_components(components);
        }
    };

} // namespace

TEST_CASE("scene: save and load round-trip", "[scene]")
{
    Fixture f;

    // Build a world.
    EntityRef a = create_entity(f.world, allocate_entity_id(f.world), "A");
    auto ta = add_component<TransformTag>(f.world, a);
    f.world.get_table("Engine_Transform")->set_float32(ta, "x", 100.0f);
    f.world.get_table("Engine_Transform")->set_float32(ta, "y", 200.0f);

    auto sa = add_component<SpriteTag>(f.world, a);
    f.world.get_table("Engine_Sprite")->set_float32(sa, "w", 32.0f);
    f.world.get_table("Engine_Sprite")->set_float32(sa, "h", 16.0f);

    EntityRef b = create_entity(f.world, allocate_entity_id(f.world), "B");
    set_parent(f.world, b, a);

    // Extract, name, save.
    Scene out = Scene::extract_from(f.world, f.components);
    out.set_name("test");

    const std::string path = make_temp_path("lucid_test_scene.lscene");
    REQUIRE(SceneSaver_save(out, path));

    // Clear the world's rows. The schema stays.
    f.world.clear_rows();
    REQUIRE(f.world.has_table("Engine_Entity"));
    REQUIRE(f.world.get_table("Engine_Entity")->count() == 0);

    // Load and apply.
    Scene in = SceneLoader_load(path);
    REQUIRE_FALSE(in.json_text().empty());
    in.apply_to(f.world, f.components, ApplyMode::Clear);

    REQUIRE(f.world.get_table("Engine_Entity")->count() == 2);

    // Verify entity A.
    auto a_id = f.world.get_table("Engine_Entity")->find_by_primary("id", engine::ecs::CellValue::from_uint64(1));
    REQUIRE_FALSE(a_id.is_nil());
    REQUIRE(f.world.get_table("Engine_Entity")->get_string(a_id, "name") == "A");

    auto ta2 = get_component<TransformTag>(f.world, a_id);
    REQUIRE_FALSE(ta2.is_nil());
    REQUIRE(f.world.get_table("Engine_Transform")->get_float32(ta2, "x") == 100.0f);
    REQUIRE(f.world.get_table("Engine_Transform")->get_float32(ta2, "y") == 200.0f);

    // Verify entity B has parent A.
    auto b_id = f.world.get_table("Engine_Entity")->find_by_primary("id", engine::ecs::CellValue::from_uint64(2));
    REQUIRE_FALSE(b_id.is_nil());
    REQUIRE(get_parent(f.world, b_id) == a_id);

    std::filesystem::remove(path);
}

TEST_CASE("scene: script component round-trip", "[scene]")
{
    Fixture f;

    EntityRef a = create_entity(f.world, allocate_entity_id(f.world), "A");
    auto sa = add_component<ScriptTag>(f.world, a);
    f.world.get_table("Engine_Script")->set_string(sa, "graph", "scripts/player.lucgraph");

    Scene out = Scene::extract_from(f.world, f.components);
    out.set_name("scripts");

    const std::string path = make_temp_path("lucid_test_scene_script.lscene");
    REQUIRE(SceneSaver_save(out, path));

    f.world.clear_rows();

    Scene in = SceneLoader_load(path);
    REQUIRE_FALSE(in.json_text().empty());
    in.apply_to(f.world, f.components, ApplyMode::Clear);

    auto a_id = f.world.get_table("Engine_Entity")->find_by_primary("id", engine::ecs::CellValue::from_uint64(1));
    REQUIRE_FALSE(a_id.is_nil());

    auto sa2 = get_component<ScriptTag>(f.world, a_id);
    REQUIRE_FALSE(sa2.is_nil());
    REQUIRE(f.world.get_table("Engine_Script")->get_string(sa2, "graph") == "scripts/player.lucgraph");

    std::filesystem::remove(path);
}

TEST_CASE("scene: refuses a newer version", "[scene]")
{
    Fixture f;

    const std::string path = make_temp_path("lucid_test_scene_newer.lscene");
    {
        std::ofstream out(path);
        out << R"({ "version": 999, "name": "future", "entities": [] })";
    }

    Scene s = SceneLoader_load(path);
    REQUIRE(s.json_text().empty()); // refused
    REQUIRE_FALSE(SceneLoader_load_into(f.world, f.components, path));

    std::filesystem::remove(path);
}

TEST_CASE("scene: apply_to with empty JSON clears the world's rows",
          "[scene]")
{
    Fixture f;

    // Populate the world.
    create_entity(f.world, 1, "A");
    REQUIRE(f.world.get_table("Engine_Entity")->count() == 1);

    // Load a file that does not exist; the loader returns an empty Scene.
    Scene s = SceneLoader_load("nonexistent_scene.lscene");
    REQUIRE(s.json_text().empty());

    s.apply_to(f.world, f.components, ApplyMode::Clear);

    // The rows are gone, the schema is not.
    REQUIRE(f.world.has_table("Engine_Entity"));
    REQUIRE(f.world.get_table("Engine_Entity")->count() == 0);
}

TEST_CASE("scene: SceneLoader_load_into on a missing file does not clear",
          "[scene]")
{
    Fixture f;

    create_entity(f.world, 1, "A");
    REQUIRE(f.world.get_table("Engine_Entity")->count() == 1);

    REQUIRE_FALSE(SceneLoader_load_into(f.world, f.components, "nonexistent_scene.lscene"));

    // The world is untouched.
    REQUIRE(f.world.get_table("Engine_Entity")->count() == 1);
}

TEST_CASE("scene: loading a scene with cameras sets active camera name",
          "[scene]")
{
    Fixture f;

    // Populate the camera table directly.
    {
        auto *cam = f.world.get_table("Engine_Camera");
        auto ref = cam->add_partial("name", engine::ecs::CellValue::from_string("main"));
        cam->set_float32(ref, "pos_x", 640.0f);
        cam->set_float32(ref, "pos_y", 360.0f);
        cam->set_bool(ref, "is_orthographic", true);
    }

    Scene out = Scene::extract_from(f.world, f.components);
    const std::string path = make_temp_path("lucid_test_scene_cams.lscene");
    REQUIRE(SceneSaver_save(out, path));

    f.world.clear_rows();

    Scene in = SceneLoader_load(path);
    REQUIRE(in.active_camera_name() == "main");
    in.apply_to(f.world, f.components, ApplyMode::Clear);

    // The camera row exists.
    REQUIRE(f.world.get_table("Engine_Camera")->count() == 1);
    auto ref = f.world.get_table("Engine_Camera")->find_by_primary("name", engine::ecs::CellValue::from_string("main"));
    REQUIRE_FALSE(ref.is_nil());
    REQUIRE(f.world.get_table("Engine_Camera")->get_float32(ref, "pos_x") == 640.0f);

    std::filesystem::remove(path);
}

TEST_CASE("scene: apply_to with Additive keeps existing entities",
          "[scene]")
{
    Fixture f;

    // Pre-populate with one entity.
    create_entity(f.world, 100, "Existing");
    REQUIRE(f.world.get_table("Engine_Entity")->count() == 1);

    // A scene with one new entity.
    const std::string json =
        R"({
            "version": 1,
            "name": "additive",
            "entities": [
                { "id": 200, "name": "New", "parent": 0, "components": {} }
            ]
        })";
    Scene s;
    s.set_json_text(json);
    s.apply_to(f.world, f.components, ApplyMode::Additive);

    // Both entities exist.
    REQUIRE(f.world.get_table("Engine_Entity")->count() == 2);
    auto existing = f.world.get_table("Engine_Entity")->find_by_primary("id", engine::ecs::CellValue::from_uint64(100));
    REQUIRE_FALSE(existing.is_nil());
    auto added = f.world.get_table("Engine_Entity")->find_by_primary("id", engine::ecs::CellValue::from_uint64(200));
    REQUIRE_FALSE(added.is_nil());
}

TEST_CASE("scene: an unknown component is logged and skipped", "[scene]")
{
    Fixture f;

    const std::string json =
        R"({
            "version": 1,
            "name": "unknown",
            "entities": [
                {
                    "id": 1, "name": "E", "parent": 0,
                    "components": { "Alice_Nonexistent": { "foo": 1 } }
                }
            ]
        })";
    Scene s;
    s.set_json_text(json);
    s.apply_to(f.world, f.components, ApplyMode::Clear);

    // The entity exists; the unknown component did not crash the load.
    REQUIRE(f.world.get_table("Engine_Entity")->count() == 1);
}

TEST_CASE("scene: a scene with no cameras has no active camera", "[scene]")
{
    Fixture f;

    const std::string json =
        R"({ "version": 1, "name": "no-camera", "entities": [] })";
    Scene s;
    s.set_json_text(json);
    s.apply_to(f.world, f.components, ApplyMode::Clear);

    REQUIRE(s.active_camera_name().empty());
    REQUIRE(f.world.get_table(kCameraTableName)->count() == 0);
}

TEST_CASE("scene: a duplicate camera name is skipped", "[scene]")
{
    Fixture f;

    const std::string json =
        R"({
            "version": 1,
            "name": "dup",
            "cameras": [
                { "name": "main", "pos": [0, 0, 0] },
                { "name": "main", "pos": [10, 10, 0] }
            ],
            "entities": []
        })";
    Scene s;
    s.set_json_text(json);
    s.apply_to(f.world, f.components, ApplyMode::Clear);

    REQUIRE(f.world.get_table(kCameraTableName)->count() == 1);
}