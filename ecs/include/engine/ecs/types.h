#pragma once

#include <cstddef>
#include <cstdint>

namespace engine::ecs
{

    // A row's slot index inside a table. Slot 0 is reserved as the nil slot;
    // the first live row is index 1.
    using RowIndex = std::uint32_t;

    // A generation counter, bumped every time a slot is reused. Combined with
    // RowIndex, it forms a RowRef that survives row removal.
    using Generation = std::uint32_t;

    // A stable, monotonically increasing row identifier. Unlike RowRef, it is
    // not invalidated by row removal; it is intended for serialization.
    using RowId = std::uint64_t;

    // The type of a column. One value per supported element type.
    enum class ColumnType : std::uint8_t
    {
        Bool,
        Int8,
        Int16,
        Int32,
        Int64,
        Uint8,
        Uint16,
        Uint32,
        Uint64,
        Float32,
        Float64,
        Char,
        String,
        RowRef,
        Callback, // was: Function
    };

    // Human-readable name of a column type, e.g. "Float32".
    const char *column_type_name(ColumnType type);

    // Size of one element of a column type, in bytes. For String and Callback,
    // returns the size of the element's handle, not the pointed-to data.
    std::size_t column_type_size(ColumnType type);

} // namespace engine::ecs