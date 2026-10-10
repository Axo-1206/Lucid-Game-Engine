#pragma once

#include "engine/ecs/types.h"

#include <cstddef>
#include <functional>

namespace engine::ecs
{

    // A reference to a row in a table. Two components:
    //   - index:      the slot index
    //   - generation: the slot's generation at the time the ref was made
    //
    // A ref is valid only while the slot's generation matches. Removing a row
    // bumps its slot's generation, so any ref to it becomes stale. A stale
    // ref is not a crash; it is is_nil() == false but is_valid() == false
    // against a Table.
    struct RowRef
    {
        RowIndex index = 0;
        Generation generation = 0;

        RowRef() = default;
        RowRef(RowIndex i, Generation g) : index(i), generation(g) {}

        // Slot 0 is the nil slot. A default-constructed RowRef is nil.
        bool is_nil() const { return index == 0; }

        bool operator==(const RowRef &other) const
        {
            return index == other.index && generation == other.generation;
        }
        bool operator!=(const RowRef &other) const { return !(*this == other); }
    };

} // namespace engine::ecs

namespace std
{
    template <>
    struct hash<engine::ecs::RowRef>
    {
        std::size_t operator()(const engine::ecs::RowRef &r) const noexcept
        {
            return (static_cast<std::size_t>(r.index) << 32) ^ r.generation;
        }
    };
} // namespace std