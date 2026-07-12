// =============================================================================
// debug: leveled logging and assertions
// =============================================================================

// Debug utilities: leveled logging and assertions.
//
// Logging levels:
//   MACH_LOG_INFO  - lifecycle / high-level events (always compiled in)
//   MACH_LOG_ERROR - failures and recoverable errors (always compiled in)
//   MACH_LOG_DEBUG - verbose per-frame / per-event tracing (debug builds only)
//
// All output goes to stderr with a level tag so logs are greppable.

#include <stdio.h>

#define MACH_LOG_INFO(fmt, ...) fprintf(stderr, "[INFO]  " fmt "\n", ##__VA_ARGS__)

#define MACH_LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

// Break into the debugger, per compiler. clang spells it __builtin_debugtrap (not
// __builtin_debugbreak, which is MSVC's name and does not exist as a clang builtin).
// gcc has neither, so it gets __builtin_trap: that kills the process instead of
// pausing it, but it still stops exactly at the failed assertion.
#if defined(_MSC_VER)
#define MACH_DEBUGBREAK() __debugbreak()
#elif defined(__clang__)
#define MACH_DEBUGBREAK() __builtin_debugtrap()
#else
#define MACH_DEBUGBREAK() __builtin_trap()
#endif

#ifdef NDEBUG
// The operand of sizeof is unevaluated, so the condition does not run in a release
// build -- MACH_DEBUG_ASSERT(expensive_check(w)) costs nothing and has no side
// effects. It still counts as a *use* of whatever it names, which keeps -Wunused
// quiet for variables that only appear inside an assertion.
#define MACH_DEBUG_ASSERT(x) ((void)sizeof((x) ? 1 : 0))
#define MACH_LOG_DEBUG(fmt, ...) (void)0
#else
#define MACH_DEBUG_ASSERT(x)                                                                       \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            MACH_LOG_ERROR("assertion failed: %s (%s:%d)", #x, __FILE__, __LINE__);                \
            MACH_DEBUGBREAK();                                                                     \
        }                                                                                          \
    } while (0)
#define MACH_LOG_DEBUG(fmt, ...) fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#endif
