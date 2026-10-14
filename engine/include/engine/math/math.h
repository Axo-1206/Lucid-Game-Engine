#pragma once

// The umbrella header for the engine's math types. A caller who
// wants the math package's public types includes this file; a
// caller who wants one type includes that type's header directly.
//
// The types live in their own headers. This file adds no
// declarations of its own; it includes what is there.
//
// engine.math the package is the registration side: it declares
// the Vector2 struct type in the format library's registry and
// registers the nodes that take and return it. This header is the
// C++ side; the package's register_into is the registry side. The
// two are kept in sync by the package's author.

#include "engine/math/vector2.h"