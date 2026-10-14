#pragma once

namespace engine::math
{

    // A 2D vector. Two float32 components, laid out in that order.
    //
    // The struct is a value: copyable, comparable by component, and
    // usable as a field in a struct type in the format library's
    // registry. When engine.math registers a Vector2 struct type,
    // the field order in the registry is x then y, matching the
    // member order here.
    //
    // The struct is a plain value. Arithmetic is done by nodes
    // (Engine_Vector2AddNode and friends), not by member functions
    // or operators, so the C++ type and the .lucid type have the
    // same shape.
    struct Vector2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

} // namespace engine::math