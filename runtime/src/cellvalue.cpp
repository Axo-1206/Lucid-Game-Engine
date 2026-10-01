#include "lucid/cellvalue.h"

#include <stdexcept>
#include <utility>

namespace lucid {

CellValue CellValue::from_bool(bool v) {
    CellValue val;
    val.kind_ = Kind::Bool;
    val.b_ = v;
    return val;
}

CellValue CellValue::from_int(std::int64_t v) {
    CellValue val;
    val.kind_ = Kind::Int;
    val.i_ = v;
    return val;
}

CellValue CellValue::from_float(double v) {
    CellValue val;
    val.kind_ = Kind::Float;
    val.f_ = v;
    return val;
}

CellValue CellValue::from_string(std::string v) {
    CellValue val;
    val.kind_ = Kind::String;
    val.s_ = std::move(v);
    return val;
}

CellValue CellValue::from_row_ref(RowRef v) {
    CellValue val;
    val.kind_ = Kind::RowRef;
    val.r_ = v;
    return val;
}

CellValue CellValue::from_function(FunctionHandle v) {
    CellValue val;
    val.kind_ = Kind::Function;
    val.fn_ = v;
    return val;
}

bool CellValue::as_bool() const {
    if (kind_ != Kind::Bool) throw std::runtime_error("CellValue is not a bool");
    return b_;
}

std::int64_t CellValue::as_int() const {
    if (kind_ != Kind::Int) throw std::runtime_error("CellValue is not an int");
    return i_;
}

double CellValue::as_float() const {
    if (kind_ != Kind::Float) throw std::runtime_error("CellValue is not a float");
    return f_;
}

const std::string& CellValue::as_string() const {
    if (kind_ != Kind::String) throw std::runtime_error("CellValue is not a string");
    return s_;
}

RowRef CellValue::as_row_ref() const {
    if (kind_ != Kind::RowRef) throw std::runtime_error("CellValue is not a row ref");
    return r_;
}

FunctionHandle CellValue::as_function() const {
    if (kind_ != Kind::Function) throw std::runtime_error("CellValue is not a function");
    return fn_;
}

bool CellValue::operator==(const CellValue& other) const {
    if (kind_ != other.kind_) return false;
    switch (kind_) {
        case Kind::Nil: return true;
        case Kind::Bool: return b_ == other.b_;
        case Kind::Int: return i_ == other.i_;
        case Kind::Float: return f_ == other.f_;
        case Kind::String: return s_ == other.s_;
        case Kind::RowRef: return r_ == other.r_;
        case Kind::Function: return fn_ == other.fn_;
    }
    return false;
}

} // namespace lucid
