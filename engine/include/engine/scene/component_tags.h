#pragma once

namespace engine
{

    // Each core component table has a tag type. The tag's `name` is the
    // table's name in the world.
    //
    // These tags are used as template parameters for add_component<Tag>(),
    // get_component<Tag>(), has_component<Tag>(), and remove_component<Tag>().
    //
    // To add a new component type, add its table in setup_core_tables() and
    // add a tag here. Also add the component name to kSceneComponents in
    // engine/scene/scene_schema.h, and a loader/saver in the scene loader
    // and saver.

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

} // namespace engine