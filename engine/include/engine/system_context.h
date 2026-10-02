#pragma once

#include "engine/clock.h"
#include "engine/input.h"
#include "engine/render_list.h"
#include "engine/renderer.h"
#include "lucid/runtime.h"

namespace engine
{

    // Everything a system might need.
    struct SystemContext
    {
        lucid::Runtime &runtime;
        Renderer &renderer;
        Input &input;
        Clock &clock;
        RenderList &render_list;
    };

} // namespace engine
