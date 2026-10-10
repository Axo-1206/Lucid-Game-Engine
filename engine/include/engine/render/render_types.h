#pragma once

#include <cstdint>

namespace engine {

// A color with RGBA components in [0, 1].
struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    static Color white()   { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    static Color black()   { return {0.0f, 0.0f, 0.0f, 1.0f}; }
    static Color red()     { return {1.0f, 0.0f, 0.0f, 1.0f}; }
    static Color green()   { return {0.0f, 1.0f, 0.0f, 1.0f}; }
    static Color blue()    { return {0.0f, 0.0f, 1.0f, 1.0f}; }
    static Color yellow()  { return {1.0f, 1.0f, 0.0f, 1.0f}; }
    static Color cyan()    { return {0.0f, 1.0f, 1.0f, 1.0f}; }
    static Color magenta() { return {1.0f, 0.0f, 1.0f, 1.0f}; }
    static Color clear()   { return {0.0f, 0.0f, 0.0f, 0.0f}; }
};

// Per-frame render statistics.
struct RenderStats {
    std::uint32_t draw_calls      = 0;
    std::uint32_t quads_drawn     = 0;
    std::uint32_t triangles_drawn = 0;
    std::uint32_t sprites_drawn   = 0;
};

} // namespace engine
