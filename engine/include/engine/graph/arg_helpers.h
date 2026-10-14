#pragma once

#include "sema/Graph.hpp"

#include <cstdint>

namespace engine::graph
{

    // The node index of the producer of a struct argument. Reads the
    // graph directly; the argument must be a NodeRef.
    inline std::uint32_t arg_node_index(const lucid::sema::Graph &g,
                                        const lucid::sema::NodeInstance &node,
                                        std::uint32_t arg_i)
    {
        const auto &arg = g.args[node.args_offset + arg_i];
        return static_cast<std::uint32_t>(arg.node_ref);
    }

} // namespace engine::graph