#include "engine/ecs/types.h"

#include "engine/ecs/callback.h"
#include "engine/ecs/row_ref.h"

#include <string>

namespace engine::ecs
{

    const char *column_type_name(ColumnType type)
    {
        switch (type)
        {
        case ColumnType::Bool:
            return "Bool";
        case ColumnType::Int8:
            return "Int8";
        case ColumnType::Int16:
            return "Int16";
        case ColumnType::Int32:
            return "Int32";
        case ColumnType::Int64:
            return "Int64";
        case ColumnType::Uint8:
            return "Uint8";
        case ColumnType::Uint16:
            return "Uint16";
        case ColumnType::Uint32:
            return "Uint32";
        case ColumnType::Uint64:
            return "Uint64";
        case ColumnType::Float32:
            return "Float32";
        case ColumnType::Float64:
            return "Float64";
        case ColumnType::Char:
            return "Char";
        case ColumnType::String:
            return "String";
        case ColumnType::RowRef:
            return "RowRef";
        case ColumnType::Callback:
            return "Callback";
        }
        return "Unknown";
    }

    std::size_t column_type_size(ColumnType type)
    {
        switch (type)
        {
        case ColumnType::Bool:
            return sizeof(bool);
        case ColumnType::Int8:
            return sizeof(std::int8_t);
        case ColumnType::Int16:
            return sizeof(std::int16_t);
        case ColumnType::Int32:
            return sizeof(std::int32_t);
        case ColumnType::Int64:
            return sizeof(std::int64_t);
        case ColumnType::Uint8:
            return sizeof(std::uint8_t);
        case ColumnType::Uint16:
            return sizeof(std::uint16_t);
        case ColumnType::Uint32:
            return sizeof(std::uint32_t);
        case ColumnType::Uint64:
            return sizeof(std::uint64_t);
        case ColumnType::Float32:
            return sizeof(float);
        case ColumnType::Float64:
            return sizeof(double);
        case ColumnType::Char:
            return sizeof(char);
        case ColumnType::String:
            return sizeof(std::string);
        case ColumnType::RowRef:
            return sizeof(RowRef);
        case ColumnType::Callback:
            return sizeof(CallbackHandle);
        }
        return 0;
    }

} // namespace engine::ecs