#pragma once

#include "engine/core/clock.h"
#include "engine/ecs/world.h"
#include "engine/platform/input.h"
#include "engine/render/render_list.h"
#include "engine/render/renderer.h"
#include "engine/services.h"
#include "engine/render/view.h"

namespace engine
{

    // Everything a system might need.
    //
    // `world` is the engine's live table store. `services` is the
    // engine-wide service bundle (physics backend, future audio,
    // etc.). Both are references; the caller owns them and keeps
    // them alive for the context's lifetime.
    //
    // A system that uses a service checks whether the field is null
    // before calling into it. A game that does not load physics
    // leaves `services.physics` null; the physics systems return
    // early.
    struct SystemContext
    {
        engine::ecs::World &world;
        Renderer &renderer;
        Input &input;
        Clock &clock;
        RenderList &render_list;
        Services &services;

        // The view currently being rendered, or nullptr if a system is
        // called outside a view loop (a test, a tool). A system that
        // needs the view's size or camera reads it here.
        const View *current_view = nullptr;
    };

} // namespace engine