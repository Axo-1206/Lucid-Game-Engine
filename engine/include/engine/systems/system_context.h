#pragma once

#include "engine/core/clock.h"
#include "engine/ecs/world.h"
#include "engine/platform/input.h"
#include "engine/render/render_list.h"
#include "engine/render/renderer.h"
#include "engine/services.h"

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
    };

} // namespace engine