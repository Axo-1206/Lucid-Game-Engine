#pragma once

#include <cstddef>
#include <cstdint>

namespace lucid {

using RowIndex = std::uint32_t;
using Generation = std::uint32_t;
using RowId = std::uint64_t;

enum class ColumnType : std::uint8_t {
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
    Function,
};

const char* column_type_name(ColumnType type);
std::size_t column_type_size(ColumnType type);

} // namespace lucid
