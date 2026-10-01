#include "lucid/table.h"

#include <stdexcept>
#include <utility>

namespace lucid {

Table::Table(std::string name) : name_(std::move(name)) {
    slots_.push_back(Slot{1, false});
}

Table::~Table() = default;

const Column* Table::find_column(const std::string& name) const {
    auto it = column_index_.find(name);
    return it == column_index_.end() ? nullptr : &columns_[it->second];
}

Column* Table::find_column(const std::string& name) {
    auto it = column_index_.find(name);
    return it == column_index_.end() ? nullptr : &columns_[it->second];
}

std::size_t Table::column_index(const std::string& name) const {
    auto it = column_index_.find(name);
    if (it == column_index_.end()) throw std::runtime_error("Column not found: " + name);
    return it->second;
}

Column& Table::add_column(std::string name, ColumnType type, bool primary) {
    if (live_count_ > 0) throw std::runtime_error("Cannot add column '" + name + "' after rows exist");
    if (column_index_.count(name)) throw std::runtime_error("Duplicate column name: " + name);
    if (primary && has_primary()) throw std::runtime_error("Table '" + name_ + "' already has a primary column");

    std::size_t index = columns_.size();
    columns_.emplace_back(std::move(name), type);
    Column& column = columns_.back();
    column_index_[column.name()] = index;
    if (primary) {
        column.make_primary();
        primary_column_index_ = index;
    }
    return column;
}

std::size_t Table::primary_column_index() const {
    if (!has_primary()) throw std::runtime_error("Table '" + name_ + "' has no primary column");
    return primary_column_index_;
}

void Table::set_column_from_value(std::size_t row, Column& column, const CellValue& value) {
    switch (column.type()) {
        case ColumnType::Bool: column.set_bool(row, value.as_bool()); break;
        case ColumnType::Int8: column.set_int8(row, static_cast<std::int8_t>(value.as_int())); break;
        case ColumnType::Int16: column.set_int16(row, static_cast<std::int16_t>(value.as_int())); break;
        case ColumnType::Int32: column.set_int32(row, static_cast<std::int32_t>(value.as_int())); break;
        case ColumnType::Int64: column.set_int64(row, value.as_int()); break;
        case ColumnType::Uint8: column.set_uint8(row, static_cast<std::uint8_t>(value.as_int())); break;
        case ColumnType::Uint16: column.set_uint16(row, static_cast<std::uint16_t>(value.as_int())); break;
        case ColumnType::Uint32: column.set_uint32(row, static_cast<std::uint32_t>(value.as_int())); break;
        case ColumnType::Uint64: column.set_uint64(row, static_cast<std::uint64_t>(value.as_int())); break;
        case ColumnType::Float32: column.set_float32(row, static_cast<float>(value.as_float())); break;
        case ColumnType::Float64: column.set_float64(row, value.as_float()); break;
        case ColumnType::Char: column.set_char(row, value.as_char()); break;
        case ColumnType::String: column.set_string(row, value.as_string()); break;
        case ColumnType::RowRef: column.set_row_ref(row, value.as_row_ref()); break;
        case ColumnType::Function: column.set_function(row, value.as_function()); break;
    }
}

RowRef Table::add(const std::vector<CellValue>& values) {
    if (values.size() != columns_.size()) {
        throw std::runtime_error("Table '" + name_ + "': expected " +
            std::to_string(columns_.size()) + " values, got " + std::to_string(values.size()));
    }

    RowIndex index;
    if (free_slots_.empty()) {
        index = static_cast<RowIndex>(slots_.size());
        slots_.push_back(Slot{1, true});
    } else {
        index = free_slots_.back();
        free_slots_.pop_back();
        slots_[index].alive = true;
        slots_[index].generation += 1;
    }

    for (auto& column : columns_) column.ensure_size(slots_.size());
    for (std::size_t i = 0; i < columns_.size(); ++i) {
        set_column_from_value(index, columns_[i], values[i]);
    }

    ++live_count_;
    return RowRef(index, slots_[index].generation);
}

void Table::remove(RowRef ref) {
    if (!is_valid(ref)) return;
    if (has_primary()) columns_[primary_column_index()].remove_index_for_row(ref.index);
    slots_[ref.index].alive = false;
    free_slots_.push_back(ref.index);
    --live_count_;
}

void Table::clear() {
    for (auto& column : columns_) column.clear();
    free_slots_.clear();
    for (std::size_t i = 1; i < slots_.size(); ++i) {
        if (slots_[i].alive) {
            slots_[i].alive = false;
            ++slots_[i].generation;
        }
        free_slots_.push_back(static_cast<RowIndex>(i));
    }
    live_count_ = 0;
}

bool Table::is_valid(RowRef ref) const {
    if (ref.is_nil() || ref.index >= slots_.size()) return false;
    const Slot& slot = slots_[ref.index];
    return slot.alive && slot.generation == ref.generation;
}

RowRef Table::at(std::size_t user_index) const {
    std::size_t internal = user_index + 1;
    if (internal >= slots_.size() || !slots_[internal].alive) return RowRef();
    return RowRef(static_cast<RowIndex>(internal), slots_[internal].generation);
}

CellValue Table::get(RowRef ref, const std::string& column) const {
    if (!is_valid(ref)) return CellValue::nil();
    const Column* target = find_column(column);
    if (!target) throw std::runtime_error("No such column: " + column);
    switch (target->type()) {
        case ColumnType::Bool: return CellValue::from_bool(target->get_bool(ref.index));
        case ColumnType::Int8: return CellValue::from_int8(target->get_int8(ref.index));
        case ColumnType::Int16: return CellValue::from_int16(target->get_int16(ref.index));
        case ColumnType::Int32: return CellValue::from_int32(target->get_int32(ref.index));
        case ColumnType::Int64: return CellValue::from_int64(target->get_int64(ref.index));
        case ColumnType::Uint8: return CellValue::from_uint8(target->get_uint8(ref.index));
        case ColumnType::Uint16: return CellValue::from_uint16(target->get_uint16(ref.index));
        case ColumnType::Uint32: return CellValue::from_uint32(target->get_uint32(ref.index));
        case ColumnType::Uint64: return CellValue::from_uint64(target->get_uint64(ref.index));
        case ColumnType::Float32: return CellValue::from_float32(target->get_float32(ref.index));
        case ColumnType::Float64: return CellValue::from_float64(target->get_float64(ref.index));
        case ColumnType::Char: return CellValue::from_char(target->get_char(ref.index));
        case ColumnType::String: return CellValue::from_string(target->get_string(ref.index));
        case ColumnType::RowRef: return CellValue::from_row_ref(target->get_row_ref(ref.index));
        case ColumnType::Function: return CellValue::from_function(target->get_function(ref.index));
    }
    return CellValue::nil();
}

void Table::set(RowRef ref, const std::string& column, const CellValue& value) {
    if (!is_valid(ref)) return;
    Column* target = find_column(column);
    if (!target) throw std::runtime_error("No such column: " + column);
    set_column_from_value(ref.index, *target, value);
}

#define LUCID_TABLE_GETTER(method, type, column_method) \
    type Table::method(RowRef ref, const std::string& column) const { \
        const Column* target = find_column(column); \
        if (!target) throw std::runtime_error("No such column: " + column); \
        return target->column_method(ref.index); \
    }

LUCID_TABLE_GETTER(get_float32, float, get_float32)
LUCID_TABLE_GETTER(get_int32, std::int32_t, get_int32)
LUCID_TABLE_GETTER(get_uint64, std::uint64_t, get_uint64)
LUCID_TABLE_GETTER(get_string, std::string, get_string)
LUCID_TABLE_GETTER(get_row_ref, RowRef, get_row_ref)
LUCID_TABLE_GETTER(get_bool, bool, get_bool)
LUCID_TABLE_GETTER(get_function, FunctionHandle, get_function)

#undef LUCID_TABLE_GETTER

void Table::set_float32(RowRef ref, const std::string& column, float value) { set(ref, column, CellValue::from_float32(value)); }
void Table::set_int32(RowRef ref, const std::string& column, std::int32_t value) { set(ref, column, CellValue::from_int32(value)); }
void Table::set_uint64(RowRef ref, const std::string& column, std::uint64_t value) { set(ref, column, CellValue::from_uint64(value)); }
void Table::set_string(RowRef ref, const std::string& column, std::string value) { set(ref, column, CellValue::from_string(std::move(value))); }
void Table::set_row_ref(RowRef ref, const std::string& column, RowRef value) { set(ref, column, CellValue::from_row_ref(value)); }
void Table::set_bool(RowRef ref, const std::string& column, bool value) { set(ref, column, CellValue::from_bool(value)); }
void Table::set_function(RowRef ref, const std::string& column, FunctionHandle value) { set(ref, column, CellValue::from_function(value)); }

RowRef Table::find_by_primary(const std::string& column, const CellValue& value) const {
    const Column* target = find_column(column);
    if (!target) throw std::runtime_error("No such column: " + column);
    if (!target->is_primary()) throw std::runtime_error("Column '" + column + "' is not a primary column");

    std::optional<std::size_t> index;
    switch (target->type()) {
        case ColumnType::Bool: index = target->find_primary_bool(value.as_bool()); break;
        case ColumnType::Int64: index = target->find_primary_int64(value.as_int()); break;
        case ColumnType::Uint64: index = target->find_primary_uint64(value.as_uint()); break;
        case ColumnType::Float64: index = target->find_primary_double(value.as_float()); break;
        case ColumnType::String: index = target->find_primary_string(value.as_string()); break;
        case ColumnType::RowRef: index = target->find_primary_row_ref(value.as_row_ref()); break;
        default: throw std::runtime_error(std::string("Primary lookup not supported for column type ") + column_type_name(target->type()));
    }
    if (!index || *index >= slots_.size() || !slots_[*index].alive) return RowRef();
    return RowRef(static_cast<RowIndex>(*index), slots_[*index].generation);
}

} // namespace lucid
