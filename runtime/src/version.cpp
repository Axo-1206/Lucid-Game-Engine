#include "lucid/version.h"

namespace lucid {

const char* version_string() {
    return "0.1.0";
}

unsigned int version_number() {
    return (0u << 16) | (1u << 8) | 0u;
}

} // namespace lucid
