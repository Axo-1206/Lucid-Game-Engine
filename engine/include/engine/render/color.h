#pragma once

namespace engine
{

    // An RGBA color. Components are in [0, 1].
    //
    // The struct is a value: copyable, comparable by component, and
    // usable as a field in a struct type in the format library's
    // registry. The renderer and the UI treat the four components
    // as linear-space values unless a caller converts them; the
    // engine does not do sRGB conversion on this type.
    //
    // A Color is a render concept, not a math one. It lives here,
    // next to the renderer's other types, not in engine.math.
    // engine.render owns it; the engine.render package declares a
    // Color struct in the format library's registry when the first
    // node takes one as an argument.
    struct Color
    {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;

        static Color white() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
        static Color black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }
        static Color red() { return {1.0f, 0.0f, 0.0f, 1.0f}; }
        static Color green() { return {0.0f, 1.0f, 0.0f, 1.0f}; }
        static Color blue() { return {0.0f, 0.0f, 1.0f, 1.0f}; }
        static Color yellow() { return {1.0f, 1.0f, 0.0f, 1.0f}; }
        static Color cyan() { return {0.0f, 1.0f, 1.0f, 1.0f}; }
        static Color magenta() { return {1.0f, 0.0f, 1.0f, 1.0f}; }
        static Color clear() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    };

} // namespace engine