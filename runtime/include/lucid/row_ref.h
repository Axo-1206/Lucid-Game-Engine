#pragma once

#include "lucid/types.h"

#include <cstddef>
#include <functional>

namespace lucid {

struct RowRef {
    RowIndex index = 0;
    Generation generation = 0;

    RowRef() = default;
    RowRef(RowIndex i, Generation g) : index(i), generation(g) {}

    bool is_nil() const { return index == 0; }
    bool operator==(const RowRef& other) const {
        return index == other.index && generation == other.generation;
    }
    bool operator!=(const RowRef& other) const { return !(*this == other); }
};

} // namespace lucid

namespace std {
template <>
struct hash<lucid::RowRef> {
    std::size_t operator()(const lucid::RowRef& r) const noexcept {
        return (static_cast<std::size_t>(r.index) << 32) ^ r.generation;
    }
};
} // namespace std
