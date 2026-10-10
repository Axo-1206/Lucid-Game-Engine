#pragma once

#include "engine/ecs/cell_value.h"
#include "engine/ecs/column.h"
#include "engine/ecs/row_ref.h"
#include "engine/ecs/types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::ecs
{

    // A table: a set of named columns, stored row-wise in a slot array.
    //
    // Rows are addressed by RowRef (slot index + generation). Removing a row
    // bumps the slot's generation, so stale refs to it become invalid. Slots
    // are reused; a new row may land in a previously-removed slot, with a
    // fresh generation.
    //
    // A table may have at most one primary column. find_by_primary() looks
    // up a row by that column's value in O(1).
    class Table
    {
    public:
        explicit Table(std::string name);
        ~Table();

        Table(const Table &) = delete;
        Table &operator=(const Table &) = delete;
        Table(Table &&) noexcept = default;
        Table &operator=(Table &&) noexcept = default;

        const std::string &name() const { return name_; }
        std::size_t column_count() const { return columns_.size(); }

        const Column *find_column(const std::string &name) const;
        Column *find_column(const std::string &name);
        std::size_t column_index(const std::string &name) const;
        bool has_column(const std::string &name) const;

        Column &add_column(std::string name, ColumnType type, bool primary = false);

        // Add a row from a full vector of cell values, one per column in
        // declaration order. Throws if the count does not match.
        RowRef add(const std::vector<CellValue> &values);

        // Add a row, setting only one column. Other columns are default-
        // initialized. Used by entity.h's add_component<Tag> to key a new
        // component row by the entity ref.
        RowRef add_partial(const std::string &column, const CellValue &value);

        void remove(RowRef ref);
        void clear();

        std::size_t count() const { return live_count_; }
        std::size_t capacity() const { return slots_.size(); }

        // True if ref names a live row in this table.
        bool is_valid(RowRef ref) const;

        // RowRef for the user-visible index, 0-based, in insertion order.
        // Not the same as RowRef::index; this walks live slots.
        RowRef at(std::size_t user_index) const;

        // Reflective accessors. Prefer the typed getters below.
        CellValue get(RowRef ref, const std::string &column) const;
        void set(RowRef ref, const std::string &column, const CellValue &v);

        // Typed getters and setters. These are the fast path and are what
        // most code should call.
        float get_float32(RowRef ref, const std::string &column) const;
        std::int32_t get_int32(RowRef ref, const std::string &column) const;
        std::uint64_t get_uint64(RowRef ref, const std::string &column) const;
        std::string get_string(RowRef ref, const std::string &column) const;
        RowRef get_row_ref(RowRef ref, const std::string &column) const;
        bool get_bool(RowRef ref, const std::string &column) const;
        CallbackHandle get_callback(RowRef ref, const std::string &column) const;

        void set_float32(RowRef ref, const std::string &column, float v);
        void set_int32(RowRef ref, const std::string &column, std::int32_t v);
        void set_uint64(RowRef ref, const std::string &column, std::uint64_t v);
        void set_string(RowRef ref, const std::string &column, std::string v);
        void set_row_ref(RowRef ref, const std::string &column, RowRef v);
        void set_bool(RowRef ref, const std::string &column, bool v);
        void set_callback(RowRef ref, const std::string &column, CallbackHandle v);

        RowRef find_by_primary(const std::string &column, const CellValue &v) const;

        // Iterate live rows in slot order.
        template <typename Fn>
        void each(Fn &&fn) const
        {
            for (std::size_t i = 1; i < slots_.size(); ++i)
            {
                if (slots_[i].alive)
                {
                    fn(RowRef(static_cast<RowIndex>(i), slots_[i].generation));
                }
            }
        }

    private:
        struct Slot
        {
            Generation generation = 1;
            bool alive = false;
        };

        std::string name_;
        std::vector<Column> columns_;
        std::unordered_map<std::string, std::size_t> column_index_;
        std::vector<Slot> slots_;
        std::vector<RowIndex> free_slots_;
        std::size_t live_count_ = 0;
        std::size_t primary_column_index_ = SIZE_MAX;

        bool has_primary() const { return primary_column_index_ != SIZE_MAX; }
        std::size_t primary_column_index() const;
        void set_column_from_value(std::size_t row, Column &col, const CellValue &v);
    };

} // namespace engine::ecs