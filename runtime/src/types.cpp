#include "lucid/types.h"

namespace lucid {

const char* column_type_name(ColumnType type) {
    switch (type) {
        case ColumnType::Bool: return "bool";
        case ColumnType::Int8: return "int8";
        case ColumnType::Int16: return "int16";
        case ColumnType::Int32: return "int32";
        case ColumnType::Int64: return "int64";
        case ColumnType::Uint8: return "uint8";
        case ColumnType::Uint16: return "uint16";
        case ColumnType::Uint32: return "uint32";
        case ColumnType::Uint64: return "uint64";
        case ColumnType::Float32: return "float32";
        case ColumnType::Float64: return "float64";
        case ColumnType::Char: return "char";
        case ColumnType::String: return "string";
        case ColumnType::RowRef: return "&T";
        case ColumnType::Function: return "fn";
    }
    return "?";
}

std::size_t column_type_size(ColumnType type) {
    switch (type) {
        case ColumnType::Bool: return sizeof(bool);
        case ColumnType::Int8: return sizeof(std::int8_t);
        case ColumnType::Int16: return sizeof(std::int16_t);
        case ColumnType::Int32: return sizeof(std::int32_t);
        case ColumnType::Int64: return sizeof(std::int64_t);
        case ColumnType::Uint8: return sizeof(std::uint8_t);
        case ColumnType::Uint16: return sizeof(std::uint16_t);
        case ColumnType::Uint32: return sizeof(std::uint32_t);
        case ColumnType::Uint64: return sizeof(std::uint64_t);
        case ColumnType::Float32: return sizeof(float);
        case ColumnType::Float64: return sizeof(double);
        case ColumnType::Char: return sizeof(char);
        case ColumnType::String:
        case ColumnType::RowRef:
        case ColumnType::Function: return 0;
    }
    return 0;
}

} // namespace lucid
