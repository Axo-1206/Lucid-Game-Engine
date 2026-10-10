#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace engine::ecs
{

    // An opaque handle to a callback registered with the engine.
    //
    // This is a column element type: a table may store callback handles the
    // same way it stores integers or row refs. It is not a scripting-language
    // function pointer; the engine decides what a handle resolves to (a native
    // function, an event handler, a future script entry point).
    //
    // Handle 0 is nil.
    struct CallbackHandle
    {
        std::uint32_t id = 0;

        bool is_nil() const { return id == 0; }

        bool operator==(const CallbackHandle &other) const { return id == other.id; }
        bool operator!=(const CallbackHandle &other) const { return !(*this == other); }
    };

} // namespace engine::ecs

namespace std
{
    template <>
    struct hash<engine::ecs::CallbackHandle>
    {
        std::size_t operator()(const engine::ecs::CallbackHandle &h) const noexcept
        {
            return std::hash<std::uint32_t>{}(h.id);
        }
    };
} // namespace std