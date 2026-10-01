#include "lucid/column.h"

#include <algorithm>
#include <stdexcept>

namespace lucid {

namespace {

void check_type(ColumnType actual, ColumnType expected, const char* op) {
    if (actual != expected) {
        throw std::runtime_error(
            std::string("Column type mismatch in ") + op +
            ": expected " + column_type_name(expected) +
            ", got " + column_type_name(actual));
    }
}

// Update a primary index map after setting row `row` to `new_key`.
// - If the row already held a different key, the old entry is removed.
// - If the row already held `new_key`, nothing happens (no-op).
// - If another row holds `new_key`, throws.
template <typename Map, typename Key>
void update_primary_map(Map& map, std::size_t row, const Key& new_key) {
    // Find the old entry for this row, if any.
    auto old_it = std::find_if(map.begin(), map.end(),
        [row](const auto& pair) { return pair.second == row; });

    if (old_it != map.end()) {
        if (old_it->first == new_key) return;  // no change
        map.erase(old_it);
    }

    auto result = map.emplace(new_key, row);
    if (!result.second) {
        throw std::runtime_error("Duplicate primary key");
    }
}

} // namespace

Column::Column(std::string name, ColumnType type)
    : name_(std::move(name))
    , type_(type)
{
    switch (type_) {
        case ColumnType::Bool:     storage_.emplace<std::vector<bool>>(); break;
        case ColumnType::Int8:     storage_.emplace<std::vector<std::int8_t>>(); break;
        case ColumnType::Int16:    storage_.emplace<std::vector<std::int16_t>>(); break;
        case ColumnType::Int32:    storage_.emplace<std::vector<std::int32_t>>(); break;
        case ColumnType::Int64:    storage_.emplace<std::vector<std::int64_t>>(); break;
        case ColumnType::Uint8:    storage_.emplace<std::vector<std::uint8_t>>(); break;
        case ColumnType::Uint16:   storage_.emplace<std::vector<std::uint16_t>>(); break;
        case ColumnType::Uint32:   storage_.emplace<std::vector<std::uint32_t>>(); break;
        case ColumnType::Uint64:   storage_.emplace<std::vector<std::uint64_t>>(); break;
        case ColumnType::Float32:  storage_.emplace<std::vector<float>>(); break;
        case ColumnType::Float64:  storage_.emplace<std::vector<double>>(); break;
        case ColumnType::Char:     storage_.emplace<std::vector<char>>(); break;
        case ColumnType::String:   storage_.emplace<std::vector<std::string>>(); break;
        case ColumnType::RowRef:   storage_.emplace<std::vector<RowRef>>(); break;
        case ColumnType::Function: storage_.emplace<std::vector<FunctionHandle>>(); break;
    }
}

Column::~Column() = default;

void Column::make_primary() {
    if (size() > 0) {
        throw std::runtime_error("make_primary called after data was added to column '" + name_ + "'");
    }
    is_primary_ = true;

    switch (type_) {
        case ColumnType::Bool:    primary_index_.emplace<std::unordered_map<bool, std::size_t>>(); break;
        case ColumnType::Int64:   primary_index_.emplace<std::unordered_map<std::int64_t, std::size_t>>(); break;
        case ColumnType::Uint64:  primary_index_.emplace<std::unordered_map<std::uint64_t, std::size_t>>(); break;
        case ColumnType::Float64: primary_index_.emplace<std::unordered_map<double, std::size_t>>(); break;
        case ColumnType::String:  primary_index_.emplace<std::unordered_map<std::string, std::size_t>>(); break;
        case ColumnType::RowRef:  primary_index_.emplace<std::unordered_map<RowRef, std::size_t>>(); break;
        default:
            throw std::runtime_error(
                std::string("Column type ") + column_type_name(type_) +
                " is not valid for @primary");
    }
}

// -----------------------------------------------------------------------------
// Row management
// -----------------------------------------------------------------------------

void Column::ensure_size(std::size_t count) {
    std::visit([&](auto& vec) {
        if (vec.size() < count) vec.resize(count);
    }, storage_);
}

void Column::resize(std::size_t count) {
    std::visit([&](auto& vec) { vec.resize(count); }, storage_);
    if (is_primary_) rebuild_primary_index();
}

void Column::clear() {
    std::visit([](auto& vec) { vec.clear(); }, storage_);
    if (is_primary_) rebuild_primary_index();
}

std::size_t Column::size() const {
    return std::visit([](const auto& vec) { return vec.size(); }, storage_);
}

// -----------------------------------------------------------------------------
// Getters
// -----------------------------------------------------------------------------

bool Column::get_bool(std::size_t row) const {
    check_type(type_, ColumnType::Bool, "get_bool");
    return std::get<std::vector<bool>>(storage_)[row];
}
std::int8_t Column::get_int8(std::size_t row) const {
    check_type(type_, ColumnType::Int8, "get_int8");
    return std::get<std::vector<std::int8_t>>(storage_)[row];
}
std::int16_t Column::get_int16(std::size_t row) const {
    check_type(type_, ColumnType::Int16, "get_int16");
    return std::get<std::vector<std::int16_t>>(storage_)[row];
}
std::int32_t Column::get_int32(std::size_t row) const {
    check_type(type_, ColumnType::Int32, "get_int32");
    return std::get<std::vector<std::int32_t>>(storage_)[row];
}
std::int64_t Column::get_int64(std::size_t row) const {
    check_type(type_, ColumnType::Int64, "get_int64");
    return std::get<std::vector<std::int64_t>>(storage_)[row];
}
std::uint8_t Column::get_uint8(std::size_t row) const {
    check_type(type_, ColumnType::Uint8, "get_uint8");
    return std::get<std::vector<std::uint8_t>>(storage_)[row];
}
std::uint16_t Column::get_uint16(std::size_t row) const {
    check_type(type_, ColumnType::Uint16, "get_uint16");
    return std::get<std::vector<std::uint16_t>>(storage_)[row];
}
std::uint32_t Column::get_uint32(std::size_t row) const {
    check_type(type_, ColumnType::Uint32, "get_uint32");
    return std::get<std::vector<std::uint32_t>>(storage_)[row];
}
std::uint64_t Column::get_uint64(std::size_t row) const {
    check_type(type_, ColumnType::Uint64, "get_uint64");
    return std::get<std::vector<std::uint64_t>>(storage_)[row];
}
float Column::get_float32(std::size_t row) const {
    check_type(type_, ColumnType::Float32, "get_float32");
    return std::get<std::vector<float>>(storage_)[row];
}
double Column::get_float64(std::size_t row) const {
    check_type(type_, ColumnType::Float64, "get_float64");
    return std::get<std::vector<double>>(storage_)[row];
}
char Column::get_char(std::size_t row) const {
    check_type(type_, ColumnType::Char, "get_char");
    return std::get<std::vector<char>>(storage_)[row];
}
std::string Column::get_string(std::size_t row) const {
    check_type(type_, ColumnType::String, "get_string");
    return std::get<std::vector<std::string>>(storage_)[row];
}
RowRef Column::get_row_ref(std::size_t row) const {
    check_type(type_, ColumnType::RowRef, "get_row_ref");
    return std::get<std::vector<RowRef>>(storage_)[row];
}
FunctionHandle Column::get_function(std::size_t row) const {
    check_type(type_, ColumnType::Function, "get_function");
    return std::get<std::vector<FunctionHandle>>(storage_)[row];
}

// -----------------------------------------------------------------------------
// Setters
// -----------------------------------------------------------------------------

void Column::set_bool(std::size_t row, bool v) {
    check_type(type_, ColumnType::Bool, "set_bool");
    auto& vec = std::get<std::vector<bool>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    if (is_primary_) {
        auto& map = std::get<std::unordered_map<bool, std::size_t>>(primary_index_);
        update_primary_map(map, row, v);
    }
    vec[row] = v;
}

void Column::set_int8(std::size_t row, std::int8_t v) {
    check_type(type_, ColumnType::Int8, "set_int8");
    auto& vec = std::get<std::vector<std::int8_t>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    vec[row] = v;
}

void Column::set_int16(std::size_t row, std::int16_t v) {
    check_type(type_, ColumnType::Int16, "set_int16");
    auto& vec = std::get<std::vector<std::int16_t>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    vec[row] = v;
}

void Column::set_int32(std::size_t row, std::int32_t v) {
    check_type(type_, ColumnType::Int32, "set_int32");
    auto& vec = std::get<std::vector<std::int32_t>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    vec[row] = v;
}

void Column::set_int64(std::size_t row, std::int64_t v) {
    check_type(type_, ColumnType::Int64, "set_int64");
    auto& vec = std::get<std::vector<std::int64_t>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    if (is_primary_) {
        auto& map = std::get<std::unordered_map<std::int64_t, std::size_t>>(primary_index_);
        update_primary_map(map, row, v);
    }
    vec[row] = v;
}

void Column::set_uint8(std::size_t row, std::uint8_t v) {
    check_type(type_, ColumnType::Uint8, "set_uint8");
    auto& vec = std::get<std::vector<std::uint8_t>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    vec[row] = v;
}

void Column::set_uint16(std::size_t row, std::uint16_t v) {
    check_type(type_, ColumnType::Uint16, "set_uint16");
    auto& vec = std::get<std::vector<std::uint16_t>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    vec[row] = v;
}

void Column::set_uint32(std::size_t row, std::uint32_t v) {
    check_type(type_, ColumnType::Uint32, "set_uint32");
    auto& vec = std::get<std::vector<std::uint32_t>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    vec[row] = v;
}

void Column::set_uint64(std::size_t row, std::uint64_t v) {
    check_type(type_, ColumnType::Uint64, "set_uint64");
    auto& vec = std::get<std::vector<std::uint64_t>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    if (is_primary_) {
        auto& map = std::get<std::unordered_map<std::uint64_t, std::size_t>>(primary_index_);
        update_primary_map(map, row, v);
    }
    vec[row] = v;
}

void Column::set_float32(std::size_t row, float v) {
    check_type(type_, ColumnType::Float32, "set_float32");
    auto& vec = std::get<std::vector<float>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    vec[row] = v;
}

void Column::set_float64(std::size_t row, double v) {
    check_type(type_, ColumnType::Float64, "set_float64");
    auto& vec = std::get<std::vector<double>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    vec[row] = v;
}

void Column::set_char(std::size_t row, char v) {
    check_type(type_, ColumnType::Char, "set_char");
    auto& vec = std::get<std::vector<char>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    vec[row] = v;
}

void Column::set_string(std::size_t row, std::string v) {
    check_type(type_, ColumnType::String, "set_string");
    auto& vec = std::get<std::vector<std::string>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    if (is_primary_) {
        auto& map = std::get<std::unordered_map<std::string, std::size_t>>(primary_index_);
        update_primary_map(map, row, v);
    }
    vec[row] = std::move(v);
}

void Column::set_row_ref(std::size_t row, RowRef v) {
    check_type(type_, ColumnType::RowRef, "set_row_ref");
    auto& vec = std::get<std::vector<RowRef>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    if (is_primary_) {
        auto& map = std::get<std::unordered_map<RowRef, std::size_t>>(primary_index_);
        update_primary_map(map, row, v);
    }
    vec[row] = v;
}

void Column::set_function(std::size_t row, FunctionHandle v) {
    check_type(type_, ColumnType::Function, "set_function");
    auto& vec = std::get<std::vector<FunctionHandle>>(storage_);
    if (row >= vec.size()) vec.resize(row + 1);
    vec[row] = v;
    // Function columns cannot be @primary; nothing to update.
}

// -----------------------------------------------------------------------------
// Primary lookup
// -----------------------------------------------------------------------------

std::optional<std::size_t> Column::find_primary_bool(bool v) const {
    const auto& map = std::get<std::unordered_map<bool, std::size_t>>(primary_index_);
    auto it = map.find(v);
    return it == map.end() ? std::nullopt : std::optional<std::size_t>(it->second);
}
std::optional<std::size_t> Column::find_primary_int64(std::int64_t v) const {
    const auto& map = std::get<std::unordered_map<std::int64_t, std::size_t>>(primary_index_);
    auto it = map.find(v);
    return it == map.end() ? std::nullopt : std::optional<std::size_t>(it->second);
}
std::optional<std::size_t> Column::find_primary_uint64(std::uint64_t v) const {
    const auto& map = std::get<std::unordered_map<std::uint64_t, std::size_t>>(primary_index_);
    auto it = map.find(v);
    return it == map.end() ? std::nullopt : std::optional<std::size_t>(it->second);
}
std::optional<std::size_t> Column::find_primary_double(double v) const {
    const auto& map = std::get<std::unordered_map<double, std::size_t>>(primary_index_);
    auto it = map.find(v);
    return it == map.end() ? std::nullopt : std::optional<std::size_t>(it->second);
}
std::optional<std::size_t> Column::find_primary_string(const std::string& v) const {
    const auto& map = std::get<std::unordered_map<std::string, std::size_t>>(primary_index_);
    auto it = map.find(v);
    return it == map.end() ? std::nullopt : std::optional<std::size_t>(it->second);
}
std::optional<std::size_t> Column::find_primary_row_ref(RowRef v) const {
    const auto& map = std::get<std::unordered_map<RowRef, std::size_t>>(primary_index_);
    auto it = map.find(v);
    return it == map.end() ? std::nullopt : std::optional<std::size_t>(it->second);
}

// -----------------------------------------------------------------------------
// Index cleanup
// -----------------------------------------------------------------------------

void Column::remove_index_for_row(std::size_t row) {
    if (!is_primary_) return;
    std::visit([&](auto& map) {
        for (auto it = map.begin(); it != map.end(); ) {
            if (it->second == row) {
                it = map.erase(it);
            } else {
                ++it;
            }
        }
    }, primary_index_);
}

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

void Column::rebuild_primary_index() {
    if (!is_primary_) return;

    switch (type_) {
        case ColumnType::Bool: {
            auto& map = std::get<std::unordered_map<bool, std::size_t>>(primary_index_);
            map.clear();
            const auto& vec = std::get<std::vector<bool>>(storage_);
            for (std::size_t i = 0; i < vec.size(); ++i) map[vec[i]] = i;
            break;
        }
        case ColumnType::Int64: {
            auto& map = std::get<std::unordered_map<std::int64_t, std::size_t>>(primary_index_);
            map.clear();
            const auto& vec = std::get<std::vector<std::int64_t>>(storage_);
            for (std::size_t i = 0; i < vec.size(); ++i) map[vec[i]] = i;
            break;
        }
        case ColumnType::Uint64: {
            auto& map = std::get<std::unordered_map<std::uint64_t, std::size_t>>(primary_index_);
            map.clear();
            const auto& vec = std::get<std::vector<std::uint64_t>>(storage_);
            for (std::size_t i = 0; i < vec.size(); ++i) map[vec[i]] = i;
            break;
        }
        case ColumnType::Float64: {
            auto& map = std::get<std::unordered_map<double, std::size_t>>(primary_index_);
            map.clear();
            const auto& vec = std::get<std::vector<double>>(storage_);
            for (std::size_t i = 0; i < vec.size(); ++i) map[vec[i]] = i;
            break;
        }
        case ColumnType::String: {
            auto& map = std::get<std::unordered_map<std::string, std::size_t>>(primary_index_);
            map.clear();
            const auto& vec = std::get<std::vector<std::string>>(storage_);
            for (std::size_t i = 0; i < vec.size(); ++i) map[vec[i]] = i;
            break;
        }
        case ColumnType::RowRef: {
            auto& map = std::get<std::unordered_map<RowRef, std::size_t>>(primary_index_);
            map.clear();
            const auto& vec = std::get<std::vector<RowRef>>(storage_);
            for (std::size_t i = 0; i < vec.size(); ++i) map[vec[i]] = i;
            break;
        }
        default: break;
    }
}

} // namespace lucid
