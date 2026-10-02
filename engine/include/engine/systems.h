#pragma once

#include "engine/system_context.h"

namespace engine
{

    // Each system is a free function that takes the context and dt.
    void hierarchy_system(SystemContext &ctx, float dt);
    void movement_system(SystemContext &ctx, float dt);
    void render_system(SystemContext &ctx, float dt);

    // Placeholders for later steps.
    void physics_sync_system(SystemContext &ctx, float dt);
    void collision_system(SystemContext &ctx, float dt);
    void script_system(SystemContext &ctx, float dt);

} // namespace engine
