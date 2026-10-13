#include "engine/ecs/table.h"

#include <stdexcept>

namespace engine::ecs
{

    // -----------------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------------

    Table::Table(std::string name)
        : name_(std::move(name))
    {
        // Slot 0 is reserved as the nil slot. It is never live.
        slots_.push_back(Slot{});
    }

    Table::~Table() = default;

    // -----------------------------------------------------------------------------
    // Columns
    // -----------------------------------------------------------------------------

    const Column *Table::find_column(const std::string &name) const
    {
        auto it = column_index_.find(name);
        if (it == column_index_.end())
            return nullptr;
        return &columns_[it->second];
    }

    Column *Table::find_column(const std::string &name)
    {
        auto it = column_index_.find(name);
        if (it == column_index_.end())
            return nullptr;
        return &columns_[it->second];
    }

    std::size_t Table::column_index(const std::string &name) const
    {
        auto it = column_index_.find(name);
        if (it == column_index_.end())
        {
            throw std::runtime_error("Table::column_index: no such column: " + name);
        }
        return it->second;
    }

    bool Table::has_column(const std::string &name) const
    {
        return column_index_.count(name) != 0;
    }

    Column &Table::add_column(std::string name, ColumnType type, bool primary)
    {
        if (has_column(name))
        {
            throw std::runtime_error("Table::add_column: duplicate column: " + name);
        }
        if (primary && has_primary())
        {
            throw std::runtime_error(
                "Table::add_column: table already has a primary column");
        }

        columns_.emplace_back(std::move(name), type);
        std::size_t idx = columns_.size() - 1;
        column_index_[columns_[idx].name()] = idx;

        if (primary)
        {
            columns_[idx].make_primary();
            primary_column_index_ = idx;
        }
        return columns_[idx];
    }

    // -----------------------------------------------------------------------------
    // Row creation
    // -----------------------------------------------------------------------------

    RowRef Table::add(const std::vector<CellValue> &values)
    {
        if (values.size() != columns_.size())
        {
            throw std::runtime_error(
                "Table::add: value count does not match column count");
        }

        // Find a slot: either a free slot from a previous removal, or a new one.
        RowIndex idx;
        if (!free_slots_.empty())
        {
            idx = free_slots_.back();
            free_slots_.pop_back();
        }
        else
        {
            idx = static_cast<RowIndex>(slots_.size());
            slots_.push_back(Slot{});
        }

        Slot &slot = slots_[idx];
        slot.alive = true;
        // generation was bumped when the slot was removed; it stays as is here.

        // Ensure every column can hold this row.
        for (auto &col : columns_)
            col.ensure_size(slots_.size());

        // Write the values.
        for (std::size_t c = 0; c < columns_.size(); ++c)
        {
            set_column_from_value(idx, columns_[c], values[c]);
        }

        ++live_count_;
        return RowRef(idx, slot.generation);
    }

    RowRef Table::add_partial(const std::string &column, const CellValue &value)
    {
        std::size_t idx = column_index(column);

        RowIndex slot_idx;
        if (!free_slots_.empty())
        {
            slot_idx = free_slots_.back();
            free_slots_.pop_back();
        }
        else
        {
            slot_idx = static_cast<RowIndex>(slots_.size());
            slots_.push_back(Slot{});
        }

        Slot &slot = slots_[slot_idx];
        slot.alive = true;

        for (auto &col : columns_)
            col.ensure_size(slots_.size());

        // Reset every column for this row to its type's default, so a
        // reused slot does not inherit the previous occupant's values.
        // Then set the named column.
        for (auto &col : columns_)
            reset_column(slot_idx, col);

        set_column_from_value(slot_idx, columns_[idx], value);

        ++live_count_;
        return RowRef(slot_idx, slot.generation);
    }

    void Table::remove(RowRef ref)
    {
        if (!is_valid(ref))
            return;

        std::size_t idx = ref.index;

        // Drop the row from any primary indexes.
        for (auto &col : columns_)
        {
            if (col.is_primary())
                col.remove_index_for_row(idx);
        }

        // Bump the generation so stale refs to this slot become invalid.
        Slot &slot = slots_[idx];
        slot.alive = false;
        slot.generation += 1;

        free_slots_.push_back(static_cast<RowIndex>(idx));
        --live_count_;
    }

    void Table::clear()
    {
        // Drop all live rows, keep columns.
        for (auto &col : columns_)
            col.clear();

        slots_.clear();
        slots_.push_back(Slot{}); // re-add the nil slot
        free_slots_.clear();
        live_count_ = 0;
    }

    // -----------------------------------------------------------------------------
    // Row access
    // -----------------------------------------------------------------------------

    bool Table::is_valid(RowRef ref) const
    {
        if (ref.index == 0)
            return false;
        if (ref.index >= slots_.size())
            return false;
        const Slot &slot = slots_[ref.index];
        return slot.alive && slot.generation == ref.generation;
    }

    RowRef Table::at(std::size_t user_index) const
    {
        std::size_t seen = 0;
        for (std::size_t i = 1; i < slots_.size(); ++i)
        {
            if (slots_[i].alive)
            {
                if (seen == user_index)
                {
                    return RowRef(static_cast<RowIndex>(i), slots_[i].generation);
                }
                ++seen;
            }
        }
        return RowRef{};
    }

    // -----------------------------------------------------------------------------
    // Reflective get/set
    // -----------------------------------------------------------------------------

    CellValue Table::get(RowRef ref, const std::string &column) const
    {
        const Column *col = find_column(column);
        if (!col)
        {
            throw std::runtime_error("Table::get: no such column: " + column);
        }
        if (!is_valid(ref))
        {
            return CellValue::nil();
        }

        switch (col->type())
        {
        case ColumnType::Bool:
            return CellValue::from_bool(col->get_bool(ref.index));
        case ColumnType::Int8:
            return CellValue::from_int8(col->get_int8(ref.index));
        case ColumnType::Int16:
            return CellValue::from_int16(col->get_int16(ref.index));
        case ColumnType::Int32:
            return CellValue::from_int32(col->get_int32(ref.index));
        case ColumnType::Int64:
            return CellValue::from_int64(col->get_int64(ref.index));
        case ColumnType::Uint8:
            return CellValue::from_uint8(col->get_uint8(ref.index));
        case ColumnType::Uint16:
            return CellValue::from_uint16(col->get_uint16(ref.index));
        case ColumnType::Uint32:
            return CellValue::from_uint32(col->get_uint32(ref.index));
        case ColumnType::Uint64:
            return CellValue::from_uint64(col->get_uint64(ref.index));
        case ColumnType::Float32:
            return CellValue::from_float32(col->get_float32(ref.index));
        case ColumnType::Float64:
            return CellValue::from_float64(col->get_float64(ref.index));
        case ColumnType::Char:
            return CellValue::from_char(col->get_char(ref.index));
        case ColumnType::String:
            return CellValue::from_string(col->get_string(ref.index));
        case ColumnType::RowRef:
            return CellValue::from_row_ref(col->get_row_ref(ref.index));
        case ColumnType::Callback:
            return CellValue::from_callback(col->get_callback(ref.index));
        }
        return CellValue::nil();
    }

    void Table::set(RowRef ref, const std::string &column, const CellValue &v)
    {
        Column *col = find_column(column);
        if (!col)
        {
            throw std::runtime_error("Table::set: no such column: " + column);
        }
        if (!is_valid(ref))
        {
            throw std::runtime_error("Table::set: invalid row");
        }
        set_column_from_value(ref.index, *col, v);
    }

    // -----------------------------------------------------------------------------
    // Typed getters
    // -----------------------------------------------------------------------------

    float Table::get_float32(RowRef ref, const std::string &column) const
    {
        return find_column(column)->get_float32(ref.index);
    }

    std::int32_t Table::get_int32(RowRef ref, const std::string &column) const
    {
        return find_column(column)->get_int32(ref.index);
    }

    std::uint64_t Table::get_uint64(RowRef ref, const std::string &column) const
    {
        return find_column(column)->get_uint64(ref.index);
    }

    std::string Table::get_string(RowRef ref, const std::string &column) const
    {
        return find_column(column)->get_string(ref.index);
    }

    RowRef Table::get_row_ref(RowRef ref, const std::string &column) const
    {
        return find_column(column)->get_row_ref(ref.index);
    }

    bool Table::get_bool(RowRef ref, const std::string &column) const
    {
        return find_column(column)->get_bool(ref.index);
    }

    CallbackHandle Table::get_callback(RowRef ref, const std::string &column) const
    {
        return find_column(column)->get_callback(ref.index);
    }

    // -----------------------------------------------------------------------------
    // Typed setters
    // -----------------------------------------------------------------------------

    void Table::set_float32(RowRef ref, const std::string &column, float v)
    {
        find_column(column)->set_float32(ref.index, v);
    }

    void Table::set_int32(RowRef ref, const std::string &column, std::int32_t v)
    {
        find_column(column)->set_int32(ref.index, v);
    }

    void Table::set_uint64(RowRef ref, const std::string &column, std::uint64_t v)
    {
        find_column(column)->set_uint64(ref.index, v);
    }

    void Table::set_string(RowRef ref, const std::string &column, std::string v)
    {
        find_column(column)->set_string(ref.index, std::move(v));
    }

    void Table::set_row_ref(RowRef ref, const std::string &column, RowRef v)
    {
        find_column(column)->set_row_ref(ref.index, v);
    }

    void Table::set_bool(RowRef ref, const std::string &column, bool v)
    {
        find_column(column)->set_bool(ref.index, v);
    }

    void Table::set_callback(RowRef ref, const std::string &column, CallbackHandle v)
    {
        find_column(column)->set_callback(ref.index, v);
    }

    // -----------------------------------------------------------------------------
    // Primary lookup
    // -----------------------------------------------------------------------------

    RowRef Table::find_by_primary(const std::string &column, const CellValue &v) const
    {
        const Column *col = find_column(column);
        if (!col || !col->is_primary())
            return RowRef{};

        switch (col->type())
        {
        case ColumnType::Bool:
        {
            auto idx = col->find_primary_bool(v.as_bool());
            if (!idx)
                return RowRef{};
            return RowRef(static_cast<RowIndex>(*idx), slots_[*idx].generation);
        }
        case ColumnType::Int8:
        case ColumnType::Int16:
        case ColumnType::Int32:
        case ColumnType::Int64:
        {
            auto idx = col->find_primary_int64(v.as_int());
            if (!idx)
                return RowRef{};
            return RowRef(static_cast<RowIndex>(*idx), slots_[*idx].generation);
        }
        case ColumnType::Uint8:
        case ColumnType::Uint16:
        case ColumnType::Uint32:
        case ColumnType::Uint64:
        {
            auto idx = col->find_primary_uint64(v.as_uint());
            if (!idx)
                return RowRef{};
            return RowRef(static_cast<RowIndex>(*idx), slots_[*idx].generation);
        }
        case ColumnType::Float32:
        case ColumnType::Float64:
        {
            auto idx = col->find_primary_double(v.as_float());
            if (!idx)
                return RowRef{};
            return RowRef(static_cast<RowIndex>(*idx), slots_[*idx].generation);
        }
        case ColumnType::Char:
        {
            auto idx = col->find_primary_int64(static_cast<std::int64_t>(v.as_char()));
            if (!idx)
                return RowRef{};
            return RowRef(static_cast<RowIndex>(*idx), slots_[*idx].generation);
        }
        case ColumnType::String:
        {
            auto idx = col->find_primary_string(v.as_string());
            if (!idx)
                return RowRef{};
            return RowRef(static_cast<RowIndex>(*idx), slots_[*idx].generation);
        }
        case ColumnType::RowRef:
        {
            auto idx = col->find_primary_row_ref(v.as_row_ref());
            if (!idx)
                return RowRef{};
            return RowRef(static_cast<RowIndex>(*idx), slots_[*idx].generation);
        }
        case ColumnType::Callback:
            return RowRef{};
        }
        return RowRef{};
    }

    // -----------------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------------

    std::size_t Table::primary_column_index() const
    {
        if (!has_primary())
        {
            throw std::runtime_error("Table::primary_column_index: no primary column");
        }
        return primary_column_index_;
    }

    void Table::set_column_from_value(std::size_t row, Column &col, const CellValue &v)
    {
        switch (col.type())
        {
        case ColumnType::Bool:
            col.set_bool(row, v.as_bool());
            return;
        case ColumnType::Int8:
            col.set_int8(row, static_cast<std::int8_t>(v.as_int()));
            return;
        case ColumnType::Int16:
            col.set_int16(row, static_cast<std::int16_t>(v.as_int()));
            return;
        case ColumnType::Int32:
            col.set_int32(row, static_cast<std::int32_t>(v.as_int()));
            return;
        case ColumnType::Int64:
            col.set_int64(row, v.as_int());
            return;
        case ColumnType::Uint8:
            col.set_uint8(row, static_cast<std::uint8_t>(v.as_uint()));
            return;
        case ColumnType::Uint16:
            col.set_uint16(row, static_cast<std::uint16_t>(v.as_uint()));
            return;
        case ColumnType::Uint32:
            col.set_uint32(row, static_cast<std::uint32_t>(v.as_uint()));
            return;
        case ColumnType::Uint64:
            col.set_uint64(row, v.as_uint());
            return;
        case ColumnType::Float32:
            col.set_float32(row, static_cast<float>(v.as_float()));
            return;
        case ColumnType::Float64:
            col.set_float64(row, v.as_float());
            return;
        case ColumnType::Char:
            col.set_char(row, v.as_char());
            return;
        case ColumnType::String:
            col.set_string(row, v.as_string());
            return;
        case ColumnType::RowRef:
            col.set_row_ref(row, v.as_row_ref());
            return;
        case ColumnType::Callback:
            col.set_callback(row, v.as_callback());
            return;
        }
    }

    void Table::reset_column(std::size_t row, Column &col)
    {
        switch (col.type())
        {
        case ColumnType::Bool:
            col.set_bool(row, false);
            return;
        case ColumnType::Int8:
            col.set_int8(row, 0);
            return;
        case ColumnType::Int16:
            col.set_int16(row, 0);
            return;
        case ColumnType::Int32:
            col.set_int32(row, 0);
            return;
        case ColumnType::Int64:
            col.set_int64(row, 0);
            return;
        case ColumnType::Uint8:
            col.set_uint8(row, 0);
            return;
        case ColumnType::Uint16:
            col.set_uint16(row, 0);
            return;
        case ColumnType::Uint32:
            col.set_uint32(row, 0);
            return;
        case ColumnType::Uint64:
            col.set_uint64(row, 0);
            return;
        case ColumnType::Float32:
            col.set_float32(row, 0.0f);
            return;
        case ColumnType::Float64:
            col.set_float64(row, 0.0);
            return;
        case ColumnType::Char:
            col.set_char(row, '\0');
            return;
        case ColumnType::String:
            col.set_string(row, "");
            return;
        case ColumnType::RowRef:
            col.set_row_ref(row, RowRef{});
            return;
        case ColumnType::Callback:
            col.set_callback(row, CallbackHandle{});
            return;
        }
    }

} // namespace engine::ecs