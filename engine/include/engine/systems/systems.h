#pragma once

#include "engine/systems/system_context.h"

namespace engine
{

    // Each system is a free function that takes the context and dt.
    void hierarchy_system(SystemContext &ctx, float dt);
    void movement_system(SystemContext &ctx, float dt);
    void render_system(SystemContext &ctx, float dt);

    // Placeholders for later steps.
    void physics_sync_system(SystemContext &ctx, float dt);
    void collision_system(SystemContext &ctx, float dt);

    // Draw the UI pass. Walks Engine_UIElement and submits draw
    // items to the render list. Reads the view's size from
    // ctx.current_view for anchor resolution.
    void ui_system(SystemContext &ctx, float dt);

} // namespace engine
