#include <catch2/catch_test_macros.hpp>
#include "engine/scene.h"
#include "engine/entity.h"
#include "engine/ecs/world.h"

#include <cstdio>
#include <filesystem>

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
        Fixture() { setup_core_tables(world); }
    };

} // namespace

TEST_CASE("scene: save and load round-trip", "[scene]")
{
    Fixture f;

    // Create a scene.
    Scene scene(f.world);
    scene.set_name("test");

    EntityRef a = create_entity(f.world, allocate_entity_id(f.world), "A");
    auto ta = add_component<TransformTag>(f.world, a);
    f.world.get_table("Transform")->set_float32(ta, "x", 100.0f);
    f.world.get_table("Transform")->set_float32(ta, "y", 200.0f);

    auto sa = add_component<SpriteTag>(f.world, a);
    f.world.get_table("Sprite")->set_float32(sa, "w", 32.0f);
    f.world.get_table("Sprite")->set_float32(sa, "h", 16.0f);

    EntityRef b = create_entity(f.world, allocate_entity_id(f.world), "B");
    set_parent(f.world, b, a);

    std::string path = make_temp_path("lucid_test_scene.lscene");
    REQUIRE(scene.save_to_file(path));

    // Clear and reload.
    scene.clear();
    REQUIRE(f.world.get_table("Entity")->count() == 0);

    REQUIRE(scene.load_from_file(path));
    REQUIRE(f.world.get_table("Entity")->count() == 2);

    // Verify entity A.
    auto a_id = f.world.get_table("Entity")->find_by_primary(
        "id", engine::ecs::CellValue::from_uint64(1));
    REQUIRE_FALSE(a_id.is_nil());
    REQUIRE(f.world.get_table("Entity")->get_string(a_id, "name") == "A");

    auto ta2 = get_component<TransformTag>(f.world, a_id);
    REQUIRE_FALSE(ta2.is_nil());
    REQUIRE(f.world.get_table("Transform")->get_float32(ta2, "x") == 100.0f);
    REQUIRE(f.world.get_table("Transform")->get_float32(ta2, "y") == 200.0f);

    // Verify entity B has parent A.
    auto b_id = f.world.get_table("Entity")->find_by_primary(
        "id", engine::ecs::CellValue::from_uint64(2));
    REQUIRE_FALSE(b_id.is_nil());
    REQUIRE(get_parent(f.world, b_id) == a_id);

    std::filesystem::remove(path);
}

TEST_CASE("scene: clear removes all entities and cameras", "[scene]")
{
    Fixture f;
    Scene scene(f.world);

    create_entity(f.world, 1, "A");
    create_entity(f.world, 2, "B");

    scene.clear();

    REQUIRE(f.world.get_table("Entity")->count() == 0);
    REQUIRE(f.world.get_table("Camera")->count() == 0);
}

TEST_CASE("scene: missing file returns false", "[scene]")
{
    Fixture f;
    Scene scene(f.world);
    REQUIRE_FALSE(scene.load_from_file("nonexistent_scene.lscene"));
}

TEST_CASE("scene: loading a scene with cameras sets active camera", "[scene]")
{
    Fixture f;
    Scene scene(f.world);

    std::string path = make_temp_path("lucid_test_scene_cams.lscene");

    // Save a scene with one camera.
    {
        auto *cam = f.world.get_table("Camera");
        auto ref = cam->add_partial("name", engine::ecs::CellValue::from_string("main"));
        cam->set_float32(ref, "pos_x", 640.0f);
        cam->set_float32(ref, "pos_y", 360.0f);
        cam->set_bool(ref, "is_orthographic", true);

        REQUIRE(scene.save_to_file(path));
    }

    scene.clear();
    REQUIRE(scene.load_from_file(path));
    REQUIRE_FALSE(scene.active_camera().is_nil());

    CameraData data = scene.get_active_camera_data();
    REQUIRE(data.name == "main");
    REQUIRE(data.pos_x == 640.0f);
    REQUIRE(data.pos_y == 360.0f);

    std::filesystem::remove(path);
}
