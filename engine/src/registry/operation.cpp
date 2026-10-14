#include "engine/registry/operation.h"

namespace engine
{

    namespace
    {

        struct Entry
        {
            Operation op;
            const char *name;
            const char *group;
        };

        // One row per Operation. `None` and `Custom` carry no name
        // and no group; their name and group come from the caller
        // (NodeEditorInfo::custom_operation and
        // ::operation_group_override) or are empty.
        //
        // The named rows are the engine's vocabulary. Adding a row
        // is safe; changing an existing row's `name` or `group`
        // updates every node that uses the operation. Removing a
        // row leaves the corresponding Operation value with an
        // empty name and group.
        constexpr Entry kOperations[] = {
            {Operation::None, "", ""},
            {Operation::Custom, "", ""},

            // Arithmetic.
            {Operation::Add, "Add", "Arithmetic"},
            {Operation::Sub, "Sub", "Arithmetic"},
            {Operation::Mul, "Mul", "Arithmetic"},
            {Operation::Div, "Div", "Arithmetic"},
            {Operation::Abs, "Abs", "Arithmetic"},
            {Operation::Neg, "Neg", "Arithmetic"},
            {Operation::Min, "Min", "Arithmetic"},
            {Operation::Max, "Max", "Arithmetic"},
            {Operation::Clamp, "Clamp", "Arithmetic"},

            // Comparison.
            {Operation::LessThan, "LessThan", "Comparison"},
            {Operation::GreaterThan, "GreaterThan", "Comparison"},
            {Operation::EqualTo, "EqualTo", "Comparison"},
            {Operation::NotEqual, "NotEqual", "Comparison"},
            {Operation::LessOrEqual, "LessOrEqual", "Comparison"},
            {Operation::GreaterOrEqual, "GreaterOrEqual", "Comparison"},

            // Logic.
            {Operation::And, "And", "Logic"},
            {Operation::Or, "Or", "Logic"},
            {Operation::Not, "Not", "Logic"},

            // Conversion.
            {Operation::IntToFloat, "IntToFloat", "Conversion"},
            {Operation::FloatToInt, "FloatToInt", "Conversion"},

            // Vector.
            {Operation::Vector2Ctor, "Vector2", "Vector"},
            {Operation::X, "X", "Vector"},
            {Operation::Y, "Y", "Vector"},
            {Operation::LengthSquared, "LengthSquared", "Vector"},

            // Flow.
            {Operation::EveryFrame, "EveryFrame", "Flow"},
            {Operation::If, "If", "Flow"},
            {Operation::While, "While", "Flow"},

            // Debug.
            {Operation::Print, "Print", "Debug"},
        };

        const Entry *find(Operation op)
        {
            for (const auto &e : kOperations)
            {
                if (e.op == op)
                    return &e;
            }
            return nullptr;
        }

    } // namespace

    const char *operation_name(Operation op)
    {
        const Entry *e = find(op);
        return e ? e->name : "";
    }

    const char *operation_group(Operation op)
    {
        const Entry *e = find(op);
        return e ? e->group : "";
    }

} // namespace engine