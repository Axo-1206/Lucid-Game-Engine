#include <catch2/catch_test_macros.hpp>
#include "engine/scene.h"
#include "engine/entity.h"
#include "lucid/runtime.h"

#include <cstdio>
#include <filesystem>

using namespace engine;

namespace {

std::string make_temp_path(const std::string& name) {
    auto dir = std::filesystem::temp_directory_path();
    return (dir / name).string();
}

struct Fixture {
    lucid::Runtime rt;
    Fixture() { setup_core_tables(rt); }
};

} // namespace

TEST_CASE("scene: save and load round-trip", "[scene]") {
    Fixture f;

    // Create a scene.
    Scene scene(f.rt);
    scene.set_name("test");

    EntityRef a = create_entity(f.rt, allocate_entity_id(f.rt), "A");
    auto ta = add_component<TransformTag>(f.rt, a);
    f.rt.get_table("Transform")->set_float32(ta, "x", 100.0f);
    f.rt.get_table("Transform")->set_float32(ta, "y", 200.0f);

    auto sa = add_component<SpriteTag>(f.rt, a);
    f.rt.get_table("Sprite")->set_float32(sa, "w", 32.0f);
    f.rt.get_table("Sprite")->set_float32(sa, "h", 16.0f);

    EntityRef b = create_entity(f.rt, allocate_entity_id(f.rt), "B");
    set_parent(f.rt, b, a);

    std::string path = make_temp_path("lucid_test_scene.lscene");
    REQUIRE(scene.save_to_file(path));

    // Clear and reload.
    scene.clear();
    REQUIRE(f.rt.get_table("Entity")->count() == 0);

    REQUIRE(scene.load_from_file(path));
    REQUIRE(f.rt.get_table("Entity")->count() == 2);

    // Verify entity A.
    auto a_id = f.rt.get_table("Entity")->find_by_primary(
        "id", lucid::CellValue::from_uint64(1));
    REQUIRE_FALSE(a_id.is_nil());
    REQUIRE(f.rt.get_table("Entity")->get_string(a_id, "name") == "A");

    auto ta2 = get_component<TransformTag>(f.rt, a_id);
    REQUIRE_FALSE(ta2.is_nil());
    REQUIRE(f.rt.get_table("Transform")->get_float32(ta2, "x") == 100.0f);
    REQUIRE(f.rt.get_table("Transform")->get_float32(ta2, "y") == 200.0f);

    // Verify entity B has parent A.
    auto b_id = f.rt.get_table("Entity")->find_by_primary(
        "id", lucid::CellValue::from_uint64(2));
    REQUIRE_FALSE(b_id.is_nil());
    REQUIRE(get_parent(f.rt, b_id) == a_id);

    std::filesystem::remove(path);
}

TEST_CASE("scene: clear removes all entities and cameras", "[scene]") {
    Fixture f;
    Scene scene(f.rt);

    create_entity(f.rt, 1, "A");
    create_entity(f.rt, 2, "B");

    scene.clear();

    REQUIRE(f.rt.get_table("Entity")->count() == 0);
    REQUIRE(f.rt.get_table("Camera")->count() == 0);
}

TEST_CASE("scene: missing file returns false", "[scene]") {
    Fixture f;
    Scene scene(f.rt);
    REQUIRE_FALSE(scene.load_from_file("nonexistent_scene.lscene"));
}

TEST_CASE("scene: loading a scene with cameras sets active camera", "[scene]") {
    Fixture f;
    Scene scene(f.rt);

    std::string path = make_temp_path("lucid_test_scene_cams.lscene");

    // Save a scene with one camera.
    {
        auto* cam = f.rt.get_table("Camera");
        auto ref = cam->add_partial("name", lucid::CellValue::from_string("main"));
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
