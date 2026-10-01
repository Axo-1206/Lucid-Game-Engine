#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace lucid {

struct FunctionHandle {
    std::uint32_t id = 0;

    bool is_nil() const { return id == 0; }
    bool operator==(const FunctionHandle& other) const { return id == other.id; }
    bool operator!=(const FunctionHandle& other) const { return !(*this == other); }
};

} // namespace lucid

namespace std {
template <>
struct hash<lucid::FunctionHandle> {
    std::size_t operator()(const lucid::FunctionHandle& h) const noexcept {
        return std::hash<std::uint32_t>{}(h.id);
    }
};
} // namespace std
