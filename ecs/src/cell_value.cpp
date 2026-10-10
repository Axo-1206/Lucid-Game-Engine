#include "engine/ecs/cell_value.h"

#include <stdexcept>

namespace engine::ecs
{

    // -----------------------------------------------------------------------------
    // Factories
    // -----------------------------------------------------------------------------

    CellValue CellValue::from_bool(bool v)
    {
        CellValue c;
        c.kind_ = Kind::Bool;
        c.b_ = v;
        return c;
    }

    CellValue CellValue::from_int(std::int64_t v)
    {
        CellValue c;
        c.kind_ = Kind::Int;
        c.i_ = v;
        return c;
    }

    CellValue CellValue::from_float(double v)
    {
        CellValue c;
        c.kind_ = Kind::Float;
        c.f_ = v;
        return c;
    }

    CellValue CellValue::from_string(std::string v)
    {
        CellValue c;
        c.kind_ = Kind::String;
        c.s_ = std::move(v);
        return c;
    }

    CellValue CellValue::from_row_ref(RowRef v)
    {
        CellValue c;
        c.kind_ = Kind::RowRef;
        c.r_ = v;
        return c;
    }

    CellValue CellValue::from_callback(CallbackHandle v)
    {
        CellValue c;
        c.kind_ = Kind::Callback;
        c.cb_ = v;
        return c;
    }

    // -----------------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------------

    bool CellValue::as_bool() const
    {
        if (kind_ != Kind::Bool)
        {
            throw std::runtime_error("CellValue: not a Bool");
        }
        return b_;
    }

    std::int64_t CellValue::as_int() const
    {
        if (kind_ != Kind::Int)
        {
            throw std::runtime_error("CellValue: not an Int");
        }
        return i_;
    }

    double CellValue::as_float() const
    {
        if (kind_ != Kind::Float)
        {
            throw std::runtime_error("CellValue: not a Float");
        }
        return f_;
    }

    const std::string &CellValue::as_string() const
    {
        if (kind_ != Kind::String)
        {
            throw std::runtime_error("CellValue: not a String");
        }
        return s_;
    }

    RowRef CellValue::as_row_ref() const
    {
        if (kind_ != Kind::RowRef)
        {
            throw std::runtime_error("CellValue: not a RowRef");
        }
        return r_;
    }

    CallbackHandle CellValue::as_callback() const
    {
        if (kind_ != Kind::Callback)
        {
            throw std::runtime_error("CellValue: not a Callback");
        }
        return cb_;
    }

    // -----------------------------------------------------------------------------
    // Comparison
    // -----------------------------------------------------------------------------
    //
    // Two CellValues are equal if they have the same kind and the same value.
    // A nil (default-constructed) CellValue is only equal to another nil.

    bool CellValue::operator==(const CellValue &other) const
    {
        if (kind_ != other.kind_)
            return false;

        switch (kind_)
        {
        case Kind::Nil:
            return true;
        case Kind::Bool:
            return b_ == other.b_;
        case Kind::Int:
            return i_ == other.i_;
        case Kind::Float:
            return f_ == other.f_;
        case Kind::String:
            return s_ == other.s_;
        case Kind::RowRef:
            return r_ == other.r_;
        case Kind::Callback:
            return cb_ == other.cb_;
        }
        return false;
    }

} // namespace engine::ecs