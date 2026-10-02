#pragma once

namespace engine
{

    // Each core component table has a tag type. The tag's `name` is the
    // table's name in the runtime.
    //
    // These tags are used as template parameters for add_component<Tag>(),
    // get_component<Tag>(), has_component<Tag>(), and remove_component<Tag>().
    //
    // To add a new component type, add its table in setup_core_tables() and
    // add a tag here.

    struct EntityTag
    {
        static constexpr const char *name = "Entity";
    };
    struct TransformTag
    {
        static constexpr const char *name = "Transform";
    };
    struct WorldTransformTag
    {
        static constexpr const char *name = "WorldTransform";
    };
    struct VelocityTag
    {
        static constexpr const char *name = "Velocity";
    };
    struct SpriteTag
    {
        static constexpr const char *name = "Sprite";
    };
    struct ScriptTag
    {
        static constexpr const char *name = "Script";
    };
    struct CameraTag
    {
        static constexpr const char *name = "Camera";
    }; // not an entity component

} // namespace engine
