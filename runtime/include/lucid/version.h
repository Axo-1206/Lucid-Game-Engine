#pragma once

namespace lucid {

// Returns the runtime's version string, e.g. "0.1.0".
const char* version_string();

// Returns the runtime's version as an integer: (major << 16) | (minor << 8) | patch.
unsigned int version_number();

} // namespace lucid
