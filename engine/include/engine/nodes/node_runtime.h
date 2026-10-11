#pragma once

#include <cstdint>

namespace engine
{

    enum class TriggerMode : std::uint8_t
    {
        NotATrigger,
        Source,
        Gating,
    };

    struct NodeTypeRuntimeInfo
    {
        TriggerMode mode = TriggerMode::NotATrigger;
    };

    const NodeTypeRuntimeInfo &node_runtime_info(std::uint32_t id);
    std::uint32_t node_runtime_info_count();

} // namespace engine