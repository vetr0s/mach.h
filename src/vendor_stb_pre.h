// =============================================================================
// image (stb_image implementation lives here)
// =============================================================================

// Mach_Image implementation using stb_image (MACH_IMPLEMENTATION).

#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
// ///////////////////////////////////////////////////////////////////////////////
// EMBEDDED THIRD PARTY: stb_image v2.30
// public domain (or MIT), Sean Barrett -- https://github.com/nothings/stb
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
