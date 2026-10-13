#pragma once

namespace engine
{

    // -----------------------------------------------------------------------------
    // The scene format version
    // -----------------------------------------------------------------------------
    //
    // The current version of the .lscene format. Bump this when the schema
    // changes. The loader accepts any older version if the change was
    // compatible; it refuses newer versions.
    //
    // This version is the scene schema's version. It is independent of the
    // format library's registry fingerprint (which versions compiled
    // behavior graphs) and of the engine's overall version. The scene format
    // evolves on its own schedule, driven by the engine's component set.
    //
    // See docs/notes/SceneFormat.md for the schema's full spec and its
    // evolution rules.
    constexpr int kSceneSchemaVersion = 1;

    // How a Scene is applied to a World.
    //
    // Clear:    the world's non-preserved tables are cleared first.
    //           The default. This is the "load a new level" mode.
    //
    // Additive: existing rows are kept; the scene's entities and
    //           cameras are added. Entity ids must not collide with
    //           existing ones. This is the "stream a chunk" mode, and
    //           it is a future extension; the loader accepts it but
    //           the current implementation only appends.
    enum class ApplyMode
    {
        Clear,
        Additive,
    };

    // -----------------------------------------------------------------------------
    // The component types the scene loader and saver know about
    // -----------------------------------------------------------------------------
    //
    // A scene's `components` object may contain any of these keys. Keys not
    // in this list are logged as warnings and ignored (the entity simply
    // doesn't get that component). Components in this list that a scene
    // omits are absent for that entity.
    //
    // To add a component:
    //   1. Add its tag to component_tags.h.
    //   2. Add its table to setup_core_tables in ecs/src/core/entity.cpp.
    //   3. Add a load_<component> function to scene_loader.cpp and a
    //      dispatch line in load_component.
    //   4. Add a save_<component> function to scene_saver.cpp and a
    //      dispatch line in save_component_by_name.
    //   5. Add its name here.
    //
    // Five places. Five focused edits.
    //
    // See docs/notes/SceneFormat.md for each component's fields and defaults.
    inline constexpr const char *kSceneComponents[] = {
        "Engine_Transform",      // 2D transform: x, y, z, rot_z
        "Engine_WorldTransform", // derived; usually not authored in a scene
        "Engine_Velocity",       // dx, dy, dz
        "Engine_Sprite",         // 2D sprite: texture_path, w, h, tint_rgba, layer
        "Engine_Script",         // reference to a .lucgraph file
    };

    // -----------------------------------------------------------------------------
    // The camera fields
    // -----------------------------------------------------------------------------
    //
    // Every camera in a scene has these fields. is_orthographic chooses the
    // projection: true for 2D, false for perspective. A scene with an
    // orthographic camera is 2D-looking; a scene with a perspective camera
    // is 3D-looking. There is no scene-level 2D/3D mode; the camera's mode is
    // per-camera, not per-scene.
    //
    // See docs/notes/SceneFormat.md for each field's type and default.
    inline constexpr const char *kCameraFields[] = {
        "name",
        "pos",    // [x, y, z]
        "target", // [x, y, z]
        "fov",
        "near",
        "far",
        "is_orthographic",
        "ortho_size",
        "clear_color", // [r, g, b, a]
    };

} // namespace engine