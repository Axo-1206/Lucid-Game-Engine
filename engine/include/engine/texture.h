#pragma once

#include <cstdint>

namespace engine {

// An opaque texture handle. Handle 0 is invalid.
//
// The renderer maps handles to GPU textures. The handle is stable for the
// texture's lifetime and is freed by free_texture() or renderer shutdown.
struct TextureHandle {
    std::uint32_t id = 0;

    bool is_valid() const { return id != 0; }

    bool operator==(const TextureHandle& other) const { return id == other.id; }
    bool operator!=(const TextureHandle& other) const { return !(*this == other); }
};

} // namespace engine
