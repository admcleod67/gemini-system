#pragma once

namespace pick_system {

// Overridden per translation unit from CMake for the main executable.
#ifndef PICK_SYSTEM_VERSION
#define PICK_SYSTEM_VERSION "0.0.0-dev"
#endif

inline constexpr const char* version_string = PICK_SYSTEM_VERSION;
inline constexpr const char* system_title = "Pick System Developer Edition";

}  // namespace pick_system
