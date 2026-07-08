// =============================================================================
// RGFW (embedded): windowing, input, GL context
// =============================================================================

// base already defines the u8/i64-style aliases RGFW would otherwise typedef.
#define RGFW_INT_DEFINED
// RGFW_OPENGL must be defined before RGFW or the GL context API is compiled out.
#define RGFW_OPENGL
// The implementation TU compiles RGFW's implementation right here, so on that
// pass the OS headers (windows.h / Xlib.h / Cocoa runtime) enter above all mach
// declarations -- every mach name is Mach_/mach_/MACH_-prefixed for exactly
// this reason.
#ifdef MACH_IMPLEMENTATION
#define RGFW_IMPLEMENTATION
#endif
// ///////////////////////////////////////////////////////////////////////////////
// EMBEDDED THIRD PARTY: RGFW
// zlib license, (c) ColleagueRiley -- https://github.com/ColleagueRiley/RGFW
// Pasted verbatim; its license text rides along inside this section.
// ///////////////////////////////////////////////////////////////////////////////
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-macros"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wimplicit-int-conversion"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif
