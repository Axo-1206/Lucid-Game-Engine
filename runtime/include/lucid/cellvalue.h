#pragma once

#include "lucid/function.h"
#include "lucid/row_ref.h"
#include "lucid/types.h"

#include <cstdint>
#include <string>

namespace lucid {

class CellValue {
public:
    enum class Kind : std::uint8_t {
        Nil,
        Bool,
        Int,
        Float,
        String,
        RowRef,
        Function,
    };

    CellValue() : kind_(Kind::Nil), b_(false), i_(0), f_(0.0) {}
    static CellValue nil() { return CellValue(); }

    static CellValue from_bool(bool v);
    static CellValue from_int(std::int64_t v);
    static CellValue from_float(double v);
    static CellValue from_string(std::string v);
    static CellValue from_row_ref(RowRef v);
    static CellValue from_function(FunctionHandle v);

    static CellValue from_int8(std::int8_t v) { return from_int(v); }
    static CellValue from_int16(std::int16_t v) { return from_int(v); }
    static CellValue from_int32(std::int32_t v) { return from_int(v); }
    static CellValue from_int64(std::int64_t v) { return from_int(v); }
    static CellValue from_uint8(std::uint8_t v) { return from_int(v); }
    static CellValue from_uint16(std::uint16_t v) { return from_int(v); }
    static CellValue from_uint32(std::uint32_t v) { return from_int(v); }
    static CellValue from_uint64(std::uint64_t v) {
        return from_int(static_cast<std::int64_t>(v));
    }
    static CellValue from_float32(float v) { return from_float(v); }
    static CellValue from_float64(double v) { return from_float(v); }
    static CellValue from_char(char v) {
        return from_int(static_cast<std::int64_t>(v));
    }

    Kind kind() const { return kind_; }
    bool is_nil() const { return kind_ == Kind::Nil; }

    bool as_bool() const;
    std::int64_t as_int() const;
    double as_float() const;
    const std::string& as_string() const;
    RowRef as_row_ref() const;
    FunctionHandle as_function() const;

    std::uint64_t as_uint() const { return static_cast<std::uint64_t>(as_int()); }
    char as_char() const { return static_cast<char>(as_int()); }

    bool operator==(const CellValue& other) const;
    bool operator!=(const CellValue& other) const { return !(*this == other); }

private:
    Kind kind_;
    union {
        bool b_;
        std::int64_t i_;
        double f_;
    };
    std::string s_;
    RowRef r_;
    FunctionHandle fn_;
};

} // namespace lucid
