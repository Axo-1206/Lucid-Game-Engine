#pragma once

#include "engine/render/color.h"

#include <cstdint>

namespace engine
{

    // Per-frame render statistics.
    struct RenderStats
    {
        std::uint32_t draw_calls = 0;
        std::uint32_t quads_drawn = 0;
        std::uint32_t triangles_drawn = 0;
        std::uint32_t sprites_drawn = 0;
    };

} // namespace engine