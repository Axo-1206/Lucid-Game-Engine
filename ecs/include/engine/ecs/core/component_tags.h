#pragma once

namespace engine
{

    // The names of the engine's core tables.
    //
    // The *Tag structs are entity components: their tables are keyed
    // by `entity`. The constants below are tables that are not entity
    // components.
    //
    // To add a component type:
    //   1. Add its *Tag here.
    //   2. Add its table to setup_core_tables in
    //      ecs/src/core/entity.cpp.
    //   3. Add a loader and a saver in engine/src/scene/scene.cpp and
    //      engine/src/scene/scene_saver.cpp (until the ComponentRegistry
    //      takes over in S5).
    //
    // See docs/notes/WorldLifecycle.md.

    struct EntityTag
    {
        static constexpr const char *name = "Engine_Entity";
    };
    struct TransformTag
    {
        static constexpr const char *name = "Engine_Transform";
    };
    struct WorldTransformTag
    {
        static constexpr const char *name = "Engine_WorldTransform";
    };
    struct VelocityTag
    {
        static constexpr const char *name = "Engine_Velocity";
    };
    struct SpriteTag
    {
        static constexpr const char *name = "Engine_Sprite";
    };
    struct ScriptTag
    {
        static constexpr const char *name = "Engine_Script";
    };

    // The camera table's name. Unlike every *Tag above, the camera is
    // NOT an entity component: its rows are keyed by `name`, not by
    // `entity`, and it is not attached to an entity. Do not write
    // add_component<...> for it; there is no tag for it, because the
    // tag template API assumes an `entity` primary column.
    //
    // Code that needs the camera table name uses this constant.
    inline constexpr const char *kCameraTableName = "Engine_Camera";

} // namespace engine