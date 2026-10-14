#pragma once

#include <cstdint>

namespace engine
{

    // A named thing a node does. The operation is a property of the
    // node's behavior, not of the types it operates on:
    // Engine_Add_Float_Node, Engine_Add_Int_Node, and
    // Engine_Add_Vector2_Vector2_Node all perform Operation::Add.
    //
    // Three categories of value:
    //
    //   None    — the node has no operation. A bare constant, a
    //             passthrough. The editor shows no operation name
    //             and no group.
    //
    //   Custom  — the node has an operation the enum does not name.
    //             The package fills in NodeEditorInfo's
    //             `custom_operation` and, optionally,
    //             `operation_group_override`. Used by a user package,
    //             or by an engine package that has not added the
    //             operation to the enum.
    //
    //   Named   — a well-known operation. Each has an entry in the
    //             operation table, mapping it to a canonical
    //             operation name and a visual group. The editor uses
    //             those.
    //
    // The enum is append-only. Adding a value is safe; reordering or
    // removing one breaks every package that names it.
    enum class Operation : std::uint8_t
    {
        // No operation. `custom_operation` and
        // `operation_group_override` are ignored.
        None = 0,

        // An operation the enum does not name. The editor reads
        // `custom_operation` for the name and
        // `operation_group_override` for the group.
        Custom = 1,

        // --- Arithmetic ---
        Add,
        Sub,
        Mul,
        Div,
        Abs,
        Neg,
        Min,
        Max,
        Clamp,

        // --- Comparison ---
        LessThan,
        GreaterThan,
        EqualTo,
        NotEqual,
        LessOrEqual,
        GreaterOrEqual,

        // --- Logic ---
        And,
        Or,
        Not,

        // --- Conversion ---
        IntToFloat,
        FloatToInt,

        // --- Vector ---
        Vector2Ctor,
        X,
        Y,
        LengthSquared,

        // --- Flow ---
        EveryFrame,
        If,
        While,

        // --- Debug ---
        Print,
    };

    // The metadata for one well-known operation. Both fields are
    // non-owning and live as long as the process. `name` is the
    // canonical operation name; `group` is the visual group. The
    // editor uses `group` as a key into its color table.
    struct OperationInfo
    {
        const char *name;
        const char *group;
    };

    // The canonical operation name, e.g. "Add" for Operation::Add.
    // Returns "" for Operation::None and Operation::Custom.
    const char *operation_name(Operation op);

    // The visual group, e.g. "Arithmetic" for Operation::Add.
    // Returns "" for Operation::None and Operation::Custom.
    const char *operation_group(Operation op);

} // namespace engine