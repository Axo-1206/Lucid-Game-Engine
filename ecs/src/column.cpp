#include "engine/ecs/column.h"

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace engine::ecs
{

    // -----------------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------------

    Column::Column(std::string name, ColumnType type)
        : name_(std::move(name)), type_(type)
    {
        switch (type_)
        {
        case ColumnType::Bool:
            storage_ = std::vector<bool>{};
            break;
        case ColumnType::Int8:
            storage_ = std::vector<std::int8_t>{};
            break;
        case ColumnType::Int16:
            storage_ = std::vector<std::int16_t>{};
            break;
        case ColumnType::Int32:
            storage_ = std::vector<std::int32_t>{};
            break;
        case ColumnType::Int64:
            storage_ = std::vector<std::int64_t>{};
            break;
        case ColumnType::Uint8:
            storage_ = std::vector<std::uint8_t>{};
            break;
        case ColumnType::Uint16:
            storage_ = std::vector<std::uint16_t>{};
            break;
        case ColumnType::Uint32:
            storage_ = std::vector<std::uint32_t>{};
            break;
        case ColumnType::Uint64:
            storage_ = std::vector<std::uint64_t>{};
            break;
        case ColumnType::Float32:
            storage_ = std::vector<float>{};
            break;
        case ColumnType::Float64:
            storage_ = std::vector<double>{};
            break;
        case ColumnType::Char:
            storage_ = std::vector<char>{};
            break;
        case ColumnType::String:
            storage_ = std::vector<std::string>{};
            break;
        case ColumnType::RowRef:
            storage_ = std::vector<RowRef>{};
            break;
        case ColumnType::Callback:
            storage_ = std::vector<CallbackHandle>{};
            break;
        }
    }

    Column::~Column() = default;

    // -----------------------------------------------------------------------------
    // Primary flag
    // -----------------------------------------------------------------------------

    void Column::make_primary()
    {
        if (is_primary_)
            return;
        is_primary_ = true;
        rebuild_primary_index();
    }

    // -----------------------------------------------------------------------------
    // Size management
    // -----------------------------------------------------------------------------

    void Column::ensure_size(std::size_t count)
    {
        std::visit([count](auto &vec)
                   {
        if (vec.size() < count) vec.resize(count); }, storage_);
    }

    void Column::resize(std::size_t count)
    {
        std::visit([count](auto &vec)
                   { vec.resize(count); }, storage_);
    }

    void Column::clear()
    {
        std::visit([](auto &vec)
                   { vec.clear(); }, storage_);
        if (is_primary_)
        {
            std::visit([](auto &map)
                       { map.clear(); }, primary_index_);
        }
    }

    std::size_t Column::size() const
    {
        return std::visit([](const auto &vec) -> std::size_t
                          { return vec.size(); }, storage_);
    }

    // -----------------------------------------------------------------------------
    // Typed getters
    // -----------------------------------------------------------------------------

    bool Column::get_bool(std::size_t row) const
    {
        return std::get<std::vector<bool>>(storage_)[row];
    }

    std::int8_t Column::get_int8(std::size_t row) const
    {
        return std::get<std::vector<std::int8_t>>(storage_)[row];
    }

    std::int16_t Column::get_int16(std::size_t row) const
    {
        return std::get<std::vector<std::int16_t>>(storage_)[row];
    }

    std::int32_t Column::get_int32(std::size_t row) const
    {
        return std::get<std::vector<std::int32_t>>(storage_)[row];
    }

    std::int64_t Column::get_int64(std::size_t row) const
    {
        return std::get<std::vector<std::int64_t>>(storage_)[row];
    }

    std::uint8_t Column::get_uint8(std::size_t row) const
    {
        return std::get<std::vector<std::uint8_t>>(storage_)[row];
    }

    std::uint16_t Column::get_uint16(std::size_t row) const
    {
        return std::get<std::vector<std::uint16_t>>(storage_)[row];
    }

    std::uint32_t Column::get_uint32(std::size_t row) const
    {
        return std::get<std::vector<std::uint32_t>>(storage_)[row];
    }

    std::uint64_t Column::get_uint64(std::size_t row) const
    {
        return std::get<std::vector<std::uint64_t>>(storage_)[row];
    }

    float Column::get_float32(std::size_t row) const
    {
        return std::get<std::vector<float>>(storage_)[row];
    }

    double Column::get_float64(std::size_t row) const
    {
        return std::get<std::vector<double>>(storage_)[row];
    }

    char Column::get_char(std::size_t row) const
    {
        return std::get<std::vector<char>>(storage_)[row];
    }

    std::string Column::get_string(std::size_t row) const
    {
        return std::get<std::vector<std::string>>(storage_)[row];
    }

    RowRef Column::get_row_ref(std::size_t row) const
    {
        return std::get<std::vector<RowRef>>(storage_)[row];
    }

    CallbackHandle Column::get_callback(std::size_t row) const
    {
        return std::get<std::vector<CallbackHandle>>(storage_)[row];
    }

    // -----------------------------------------------------------------------------
    // Typed setters
    // -----------------------------------------------------------------------------

    void Column::set_bool(std::size_t row, bool v)
    {
        auto &vec = std::get<std::vector<bool>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = v;
        if (is_primary_)
            update_primary(std::get<std::unordered_map<bool, std::size_t>>(primary_index_), row, v);
    }

    void Column::set_int8(std::size_t row, std::int8_t v)
    {
        auto &vec = std::get<std::vector<std::int8_t>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = v;
        if (is_primary_)
            update_primary(std::get<std::unordered_map<std::int64_t, std::size_t>>(primary_index_), row, static_cast<std::int64_t>(v));
    }

    void Column::set_int16(std::size_t row, std::int16_t v)
    {
        auto &vec = std::get<std::vector<std::int16_t>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = v;
        if (is_primary_)
            update_primary(std::get<std::unordered_map<std::int64_t, std::size_t>>(primary_index_), row, static_cast<std::int64_t>(v));
    }

    void Column::set_int32(std::size_t row, std::int32_t v)
    {
        auto &vec = std::get<std::vector<std::int32_t>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = v;
        if (is_primary_)
            update_primary(std::get<std::unordered_map<std::int64_t, std::size_t>>(primary_index_), row, static_cast<std::int64_t>(v));
    }

    void Column::set_int64(std::size_t row, std::int64_t v)
    {
        auto &vec = std::get<std::vector<std::int64_t>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = v;
        if (is_primary_)
            update_primary(std::get<std::unordered_map<std::int64_t, std::size_t>>(primary_index_), row, v);
    }

    void Column::set_uint8(std::size_t row, std::uint8_t v)
    {
        auto &vec = std::get<std::vector<std::uint8_t>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = v;
        if (is_primary_)
            update_primary(std::get<std::unordered_map<std::uint64_t, std::size_t>>(primary_index_), row, static_cast<std::uint64_t>(v));
    }

    void Column::set_uint16(std::size_t row, std::uint16_t v)
    {
        auto &vec = std::get<std::vector<std::uint16_t>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = v;
        if (is_primary_)
            update_primary(std::get<std::unordered_map<std::uint64_t, std::size_t>>(primary_index_), row, static_cast<std::uint64_t>(v));
    }

    void Column::set_uint32(std::size_t row, std::uint32_t v)
    {
        auto &vec = std::get<std::vector<std::uint32_t>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = v;
        if (is_primary_)
            update_primary(std::get<std::unordered_map<std::uint64_t, std::size_t>>(primary_index_), row, static_cast<std::uint64_t>(v));
    }

    void Column::set_uint64(std::size_t row, std::uint64_t v)
    {
        auto &vec = std::get<std::vector<std::uint64_t>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = v;
        if (is_primary_)
            update_primary(std::get<std::unordered_map<std::uint64_t, std::size_t>>(primary_index_), row, v);
    }

    void Column::set_float32(std::size_t row, float v)
    {
        auto &vec = std::get<std::vector<float>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = v;
        if (is_primary_)
            update_primary(std::get<std::unordered_map<double, std::size_t>>(primary_index_), row, static_cast<double>(v));
    }

    void Column::set_float64(std::size_t row, double v)
    {
        auto &vec = std::get<std::vector<double>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = v;
        if (is_primary_)
            update_primary(std::get<std::unordered_map<double, std::size_t>>(primary_index_), row, v);
    }

    void Column::set_char(std::size_t row, char v)
    {
        auto &vec = std::get<std::vector<char>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = v;
        if (is_primary_)
            update_primary(std::get<std::unordered_map<std::int64_t, std::size_t>>(primary_index_), row, static_cast<std::int64_t>(v));
    }

    void Column::set_string(std::size_t row, std::string v)
    {
        auto &vec = std::get<std::vector<std::string>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = std::move(v);
        if (is_primary_)
            update_primary(std::get<std::unordered_map<std::string, std::size_t>>(primary_index_), row, vec[row]);
    }

    void Column::set_row_ref(std::size_t row, RowRef v)
    {
        auto &vec = std::get<std::vector<RowRef>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = v;
        if (is_primary_)
            update_primary(std::get<std::unordered_map<RowRef, std::size_t>>(primary_index_), row, v);
    }

    void Column::set_callback(std::size_t row, CallbackHandle v)
    {
        auto &vec = std::get<std::vector<CallbackHandle>>(storage_);
        if (vec.size() <= row)
            vec.resize(row + 1);
        vec[row] = v;
    }

    // -----------------------------------------------------------------------------
    // Primary index
    // -----------------------------------------------------------------------------
    //
    // The primary index is one of several unordered_maps. Only the map matching
    // the column's type is live. rebuild_primary_index() fills it from the
    // current column contents.

    void Column::rebuild_primary_index()
    {
        if (!is_primary_)
            return;

        switch (type_)
        {
        case ColumnType::Bool:
        {
            auto &map = primary_index_.emplace<std::unordered_map<bool, std::size_t>>();
            map.clear();
            const auto &vec = std::get<std::vector<bool>>(storage_);
            for (std::size_t i = 0; i < vec.size(); ++i)
                map[vec[i]] = i;
            break;
        }
        case ColumnType::Int8:
        case ColumnType::Int16:
        case ColumnType::Int32:
        case ColumnType::Int64:
        {
            auto &map = primary_index_.emplace<std::unordered_map<std::int64_t, std::size_t>>();
            map.clear();
            if (type_ == ColumnType::Int8)
            {
                const auto &vec = std::get<std::vector<std::int8_t>>(storage_);
                for (std::size_t i = 0; i < vec.size(); ++i)
                    map[vec[i]] = i;
            }
            else if (type_ == ColumnType::Int16)
            {
                const auto &vec = std::get<std::vector<std::int16_t>>(storage_);
                for (std::size_t i = 0; i < vec.size(); ++i)
                    map[vec[i]] = i;
            }
            else if (type_ == ColumnType::Int32)
            {
                const auto &vec = std::get<std::vector<std::int32_t>>(storage_);
                for (std::size_t i = 0; i < vec.size(); ++i)
                    map[vec[i]] = i;
            }
            else
            {
                const auto &vec = std::get<std::vector<std::int64_t>>(storage_);
                for (std::size_t i = 0; i < vec.size(); ++i)
                    map[vec[i]] = i;
            }
            break;
        }
        case ColumnType::Uint8:
        case ColumnType::Uint16:
        case ColumnType::Uint32:
        case ColumnType::Uint64:
        {
            auto &map = primary_index_.emplace<std::unordered_map<std::uint64_t, std::size_t>>();
            map.clear();
            if (type_ == ColumnType::Uint8)
            {
                const auto &vec = std::get<std::vector<std::uint8_t>>(storage_);
                for (std::size_t i = 0; i < vec.size(); ++i)
                    map[vec[i]] = i;
            }
            else if (type_ == ColumnType::Uint16)
            {
                const auto &vec = std::get<std::vector<std::uint16_t>>(storage_);
                for (std::size_t i = 0; i < vec.size(); ++i)
                    map[vec[i]] = i;
            }
            else if (type_ == ColumnType::Uint32)
            {
                const auto &vec = std::get<std::vector<std::uint32_t>>(storage_);
                for (std::size_t i = 0; i < vec.size(); ++i)
                    map[vec[i]] = i;
            }
            else
            {
                const auto &vec = std::get<std::vector<std::uint64_t>>(storage_);
                for (std::size_t i = 0; i < vec.size(); ++i)
                    map[vec[i]] = i;
            }
            break;
        }
        case ColumnType::Float32:
        case ColumnType::Float64:
        {
            auto &map = primary_index_.emplace<std::unordered_map<double, std::size_t>>();
            map.clear();
            if (type_ == ColumnType::Float32)
            {
                const auto &vec = std::get<std::vector<float>>(storage_);
                for (std::size_t i = 0; i < vec.size(); ++i)
                    map[vec[i]] = i;
            }
            else
            {
                const auto &vec = std::get<std::vector<double>>(storage_);
                for (std::size_t i = 0; i < vec.size(); ++i)
                    map[vec[i]] = i;
            }
            break;
        }
        case ColumnType::Char:
        {
            auto &map = primary_index_.emplace<std::unordered_map<std::int64_t, std::size_t>>();
            map.clear();
            const auto &vec = std::get<std::vector<char>>(storage_);
            for (std::size_t i = 0; i < vec.size(); ++i)
                map[vec[i]] = i;
            break;
        }
        case ColumnType::String:
        {
            auto &map = primary_index_.emplace<std::unordered_map<std::string, std::size_t>>();
            map.clear();
            const auto &vec = std::get<std::vector<std::string>>(storage_);
            for (std::size_t i = 0; i < vec.size(); ++i)
                map[vec[i]] = i;
            break;
        }
        case ColumnType::RowRef:
        {
            auto &map = primary_index_.emplace<std::unordered_map<RowRef, std::size_t>>();
            map.clear();
            const auto &vec = std::get<std::vector<RowRef>>(storage_);
            for (std::size_t i = 0; i < vec.size(); ++i)
                map[vec[i]] = i;
            break;
        }
        case ColumnType::Callback:
            // Callback columns are not indexed as primary.
            break;
        }
    }

    template <typename Map, typename Key>
    void Column::update_primary(Map &map, std::size_t row, const Key &new_key)
    {
        // Remove any existing entry pointing at this row.
        for (auto it = map.begin(); it != map.end();)
        {
            if (it->second == row)
            {
                it = map.erase(it);
            }
            else
            {
                ++it;
            }
        }
        map[new_key] = row;
    }

    // -----------------------------------------------------------------------------
    // Primary lookups
    // -----------------------------------------------------------------------------

    std::optional<std::size_t> Column::find_primary_bool(bool v) const
    {
        const auto &map = std::get<std::unordered_map<bool, std::size_t>>(primary_index_);
        auto it = map.find(v);
        if (it == map.end())
            return std::nullopt;
        return it->second;
    }

    std::optional<std::size_t> Column::find_primary_int64(std::int64_t v) const
    {
        const auto &map = std::get<std::unordered_map<std::int64_t, std::size_t>>(primary_index_);
        auto it = map.find(v);
        if (it == map.end())
            return std::nullopt;
        return it->second;
    }

    std::optional<std::size_t> Column::find_primary_uint64(std::uint64_t v) const
    {
        const auto &map = std::get<std::unordered_map<std::uint64_t, std::size_t>>(primary_index_);
        auto it = map.find(v);
        if (it == map.end())
            return std::nullopt;
        return it->second;
    }

    std::optional<std::size_t> Column::find_primary_double(double v) const
    {
        const auto &map = std::get<std::unordered_map<double, std::size_t>>(primary_index_);
        auto it = map.find(v);
        if (it == map.end())
            return std::nullopt;
        return it->second;
    }

    std::optional<std::size_t> Column::find_primary_string(const std::string &v) const
    {
        const auto &map = std::get<std::unordered_map<std::string, std::size_t>>(primary_index_);
        auto it = map.find(v);
        if (it == map.end())
            return std::nullopt;
        return it->second;
    }

    std::optional<std::size_t> Column::find_primary_row_ref(RowRef v) const
    {
        const auto &map = std::get<std::unordered_map<RowRef, std::size_t>>(primary_index_);
        auto it = map.find(v);
        if (it == map.end())
            return std::nullopt;
        return it->second;
    }

    // -----------------------------------------------------------------------------
    // Row removal
    // -----------------------------------------------------------------------------
    //
    // Called by Table::remove when a row is removed. Drops the row's entry from
    // the primary index, if this column has one.

    void Column::remove_index_for_row(std::size_t row)
    {
        if (!is_primary_)
            return;
        std::visit([row](auto &map)
                   {
        for (auto it = map.begin(); it != map.end(); ) {
            if (it->second == row) {
                it = map.erase(it);
            } else {
                ++it;
            }
        } }, primary_index_);
    }

} // namespace engine::ecs