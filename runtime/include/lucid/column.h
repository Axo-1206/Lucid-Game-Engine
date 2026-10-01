#pragma once

#include "lucid/function.h"
#include "lucid/row_ref.h"
#include "lucid/types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace lucid {

class Column {
public:
    Column(std::string name, ColumnType type);
    ~Column();

    Column(const Column&) = delete;
    Column& operator=(const Column&) = delete;
    Column(Column&&) noexcept = default;
    Column& operator=(Column&&) noexcept = default;

    const std::string& name() const { return name_; }
    ColumnType type() const { return type_; }
    bool is_primary() const { return is_primary_; }

    void make_primary();
    void ensure_size(std::size_t count);
    void resize(std::size_t count);
    void clear();
    std::size_t size() const;

    bool get_bool(std::size_t row) const;
    std::int8_t get_int8(std::size_t row) const;
    std::int16_t get_int16(std::size_t row) const;
    std::int32_t get_int32(std::size_t row) const;
    std::int64_t get_int64(std::size_t row) const;
    std::uint8_t get_uint8(std::size_t row) const;
    std::uint16_t get_uint16(std::size_t row) const;
    std::uint32_t get_uint32(std::size_t row) const;
    std::uint64_t get_uint64(std::size_t row) const;
    float get_float32(std::size_t row) const;
    double get_float64(std::size_t row) const;
    char get_char(std::size_t row) const;
    std::string get_string(std::size_t row) const;
    RowRef get_row_ref(std::size_t row) const;
    FunctionHandle get_function(std::size_t row) const;

    void set_bool(std::size_t row, bool v);
    void set_int8(std::size_t row, std::int8_t v);
    void set_int16(std::size_t row, std::int16_t v);
    void set_int32(std::size_t row, std::int32_t v);
    void set_int64(std::size_t row, std::int64_t v);
    void set_uint8(std::size_t row, std::uint8_t v);
    void set_uint16(std::size_t row, std::uint16_t v);
    void set_uint32(std::size_t row, std::uint32_t v);
    void set_uint64(std::size_t row, std::uint64_t v);
    void set_float32(std::size_t row, float v);
    void set_float64(std::size_t row, double v);
    void set_char(std::size_t row, char v);
    void set_string(std::size_t row, std::string v);
    void set_row_ref(std::size_t row, RowRef v);
    void set_function(std::size_t row, FunctionHandle v);

    std::optional<std::size_t> find_primary_bool(bool v) const;
    std::optional<std::size_t> find_primary_int64(std::int64_t v) const;
    std::optional<std::size_t> find_primary_uint64(std::uint64_t v) const;
    std::optional<std::size_t> find_primary_double(double v) const;
    std::optional<std::size_t> find_primary_string(const std::string& v) const;
    std::optional<std::size_t> find_primary_row_ref(RowRef v) const;

    void remove_index_for_row(std::size_t row);

private:
    using Storage = std::variant<
        std::vector<bool>, std::vector<std::int8_t>, std::vector<std::int16_t>,
        std::vector<std::int32_t>, std::vector<std::int64_t>,
        std::vector<std::uint8_t>, std::vector<std::uint16_t>,
        std::vector<std::uint32_t>, std::vector<std::uint64_t>,
        std::vector<float>, std::vector<double>, std::vector<char>,
        std::vector<std::string>, std::vector<RowRef>,
        std::vector<FunctionHandle>>;

    using PrimaryIndex = std::variant<
        std::unordered_map<bool, std::size_t>,
        std::unordered_map<std::int64_t, std::size_t>,
        std::unordered_map<std::uint64_t, std::size_t>,
        std::unordered_map<double, std::size_t>,
        std::unordered_map<std::string, std::size_t>,
        std::unordered_map<RowRef, std::size_t>>;

    std::string name_;
    ColumnType type_;
    bool is_primary_ = false;
    Storage storage_;
    PrimaryIndex primary_index_;

    void rebuild_primary_index();

    template <typename Map, typename Key>
    void update_primary(Map& map, std::size_t row, const Key& new_key);
};

} // namespace lucid
