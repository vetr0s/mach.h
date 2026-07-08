
// =============================================================================
// base: fundamental types and version
// =============================================================================

// Fundamental types, type aliases, and base utilities.


#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Semantic versioning: MAJOR.MINOR.PATCH
#define MACH_VERSION_MAJOR 0
#define MACH_VERSION_MINOR 1
#define MACH_VERSION_PATCH 2

// Sized integer aliases. Define MACH_INT_DEFINED before including mach.h if
// your project already typedefs these names (they must match these widths).
#ifndef MACH_INT_DEFINED
#define MACH_INT_DEFINED
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float  f32;
typedef double f64;

typedef size_t    usize;
typedef ptrdiff_t isize;
#endif // MACH_INT_DEFINED

// 32-bit boolean. Used in place of bare `int` for truth values so intent is
// explicit and consistent across the codebase.
typedef i32 b32;
#define MACH_TRUE  1
#define MACH_FALSE 0

// Number of elements in a fixed-size array.
#define MACH_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

