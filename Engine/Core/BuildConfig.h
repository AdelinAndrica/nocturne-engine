#pragma once

// Exactly one must be defined by the build system
// NOC_DEBUG
// NOC_DEV
// NOC_SHIP

#if defined(NOC_DEBUG)
#define NOC_ENABLE_ASSERTS 1
#define NOC_ENABLE_LOGGING 1
#elif defined(NOC_DEV)
#define NOC_ENABLE_ASSERTS 1
#define NOC_ENABLE_LOGGING 1
#elif defined(NOC_SHIP)
#define NOC_ENABLE_ASSERTS 0
#define NOC_ENABLE_LOGGING 0
#else
#error "No build configuration defined"
#endif
