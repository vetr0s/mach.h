// Build the mach.h examples. Bootstrap once, then it rebuilds itself:
//
//   cc -o nob nob.c   (or: cl nob.c)
//   ./nob
//
// Each .c file under examples/ is a standalone program built against mach.h
// (this repo's single header) into examples/build/<name>. No setup step: RGFW
// is embedded in mach.h (windowing), GL comes from the OS.

#define NOB_IMPLEMENTATION
#include "nob.h"

#define EXAMPLES_DIR "examples"
#define BUILD_DIR    EXAMPLES_DIR "/build"

// The libs RGFW + OpenGL need on each platform.
#if defined(_WIN32)
    #define CC "cl.exe"
    static const char *PLATFORM_LIBS[] = {"opengl32.lib", "winmm.lib"};
#elif defined(__APPLE__)
    #define CC "clang"
    static const char *PLATFORM_LIBS[] = {
        "-framework", "Cocoa", "-framework", "CoreVideo",
        "-framework", "IOKit", "-framework", "OpenGL",
    };
#elif defined(__linux__)
    #define CC "clang"
    static const char *PLATFORM_LIBS[] = {"-lX11", "-lXrandr", "-lGL", "-lm", "-ldl"};
#else
    #error "Unsupported platform"
#endif

static bool build_example(const char *name /* e.g. "hello" */) {
    const char *src = nob_temp_sprintf("%s/%s.c", EXAMPLES_DIR, name);
    const char *out = nob_temp_sprintf("%s/%s", BUILD_DIR, name);
    nob_log(NOB_INFO, "Compiling: %s", out);
    Nob_Cmd cmd = {0};
#if defined(_WIN32)
    nob_cmd_append(&cmd, CC, "/std:c11", "/W4", "/I.");
    nob_cmd_append(&cmd, nob_temp_sprintf("/Fe%s.exe", out));
    nob_cmd_append(&cmd, src, "/link");
#else
    nob_cmd_append(&cmd, CC, "-std=c99", "-Wall", "-Wextra", "-I.", "-o", out, src);
#endif
    for (size_t i = 0; i < NOB_ARRAY_LEN(PLATFORM_LIBS); i++) {
        nob_cmd_append(&cmd, PLATFORM_LIBS[i]);
    }
    return nob_cmd_run(&cmd);
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!nob_mkdir_if_not_exists(BUILD_DIR)) return 1;

    // Build the examples named on the command line, or every .c in examples/.
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            if (!build_example(argv[i])) return 1;
        }
        return 0;
    }

    Nob_File_Paths entries = {0};
    if (!nob_read_entire_dir(EXAMPLES_DIR, &entries)) return 1;
    bool any = false;
    for (size_t i = 0; i < entries.count; i++) {
        Nob_String_View sv = nob_sv_from_cstr(entries.items[i]);
        if (!nob_sv_end_with(sv, ".c")) continue;
        // Strip the ".c" to get the program name.
        char *name = nob_temp_strdup(entries.items[i]);
        name[strlen(name) - 2] = '\0';
        if (!build_example(name)) return 1;
        any = true;
    }
    if (!any) nob_log(NOB_WARNING, "no .c examples found under %s/", EXAMPLES_DIR);
    return 0;
}
