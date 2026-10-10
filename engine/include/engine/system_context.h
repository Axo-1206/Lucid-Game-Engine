#pragma once

#include "engine/clock.h"
#include "engine/ecs/world.h"
#include "engine/input.h"
#include "engine/render_list.h"
#include "engine/renderer.h"

namespace engine
{
    // Everything a system might need.
    //
    // `world` is the engine's live table store.
    // It is distinct from `lucid::sema::Registry`, which is the format
    // library's declaration of node vocabulary. A system never touches
    // the latter; that is the engine's node dispatch layer's concern.
    struct SystemContext
    {
        engine::ecs::World &world;
        Renderer &renderer;
        Input &input;
        Clock &clock;
        RenderList &render_list;
    };
}