// Build the mach.h examples and tests. Bootstrap once, then it rebuilds itself:
//
//   cc -o nob nob.c   (or: cl nob.c)
//   ./nob             compiles every example under examples/
//   ./nob test        compiles every test under tests/, and runs them
//   ./nob hello       compiles just examples/hello.c
//
// Each .c file under examples/ is a standalone program built against mach.h
// (this repo's single header) into examples/build/<name>. No setup step: RGFW
// is embedded in mach.h (windowing), GL comes from the OS.
//
// The compiler comes from $CC when it is set, so CI can build the same sources
// with clang and gcc. The per-platform value below is only the default.

#define NOB_IMPLEMENTATION
#include "nob.h"

#define EXAMPLES_DIR "examples"
#define EXAMPLES_BUILD_DIR EXAMPLES_DIR "/build"
#define TESTS_DIR "tests"
#define TESTS_BUILD_DIR TESTS_DIR "/build"

// The libs RGFW + OpenGL need on each platform. The tests link them too: RGFW's
// implementation is compiled in even though the tests never open a window.
#if defined(_WIN32)
    #define DEFAULT_CC "cl.exe"
    static const char *PLATFORM_LIBS[] = {"opengl32.lib", "winmm.lib"};
#elif defined(__APPLE__)
    #define DEFAULT_CC "clang"
    static const char *PLATFORM_LIBS[] = {
        "-framework", "Cocoa", "-framework", "CoreVideo",
        "-framework", "IOKit", "-framework", "OpenGL",
    };
#elif defined(__linux__)
    #define DEFAULT_CC "clang"
    static const char *PLATFORM_LIBS[] = {"-lX11", "-lXrandr", "-lGL", "-lm", "-ldl"};
#else
    #error "Unsupported platform"
#endif

static const char *compiler(void) {
    const char *cc = getenv("CC");
    return (cc && *cc) ? cc : DEFAULT_CC;
}

// Compile one standalone .c against mach.h. `dir`/`build_dir` pick examples or tests.
static bool build_one(const char *dir, const char *build_dir, const char *name) {
    const char *src = nob_temp_sprintf("%s/%s.c", dir, name);
    const char *out = nob_temp_sprintf("%s/%s", build_dir, name);
    nob_log(NOB_INFO, "Compiling: %s", out);
    Nob_Cmd cmd = {0};
#if defined(_WIN32)
    nob_cmd_append(&cmd, compiler(), "/std:c11", "/W4", "/I.");
    nob_cmd_append(&cmd, nob_temp_sprintf("/Fe%s.exe", out));
    nob_cmd_append(&cmd, src, "/link");
#else
    nob_cmd_append(&cmd, compiler(), "-std=c99", "-Wall", "-Wextra", "-I.", "-o", out, src);
#endif
    for (size_t i = 0; i < NOB_ARRAY_LEN(PLATFORM_LIBS); i++) {
        nob_cmd_append(&cmd, PLATFORM_LIBS[i]);
    }
    return nob_cmd_run(&cmd);
}

static bool build_example(const char *name /* e.g. "hello" */) {
    return build_one(EXAMPLES_DIR, EXAMPLES_BUILD_DIR, name);
}

// Build every test and run it. A test that exits nonzero fails the build, which is
// the whole point: CI compiled things before this and executed none of them.
static bool run_tests(void) {
    if (!nob_mkdir_if_not_exists(TESTS_BUILD_DIR)) return false;

    Nob_File_Paths entries = {0};
    if (!nob_read_entire_dir(TESTS_DIR, &entries)) return false;

    size_t ran = 0;
    for (size_t i = 0; i < entries.count; i++) {
        Nob_String_View sv = nob_sv_from_cstr(entries.items[i]);
        if (!nob_sv_end_with(sv, ".c")) continue;

        char *name = nob_temp_strdup(entries.items[i]);
        name[strlen(name) - 2] = '\0';
        if (!build_one(TESTS_DIR, TESTS_BUILD_DIR, name)) return false;

        const char *exe = nob_temp_sprintf("%s/%s", TESTS_BUILD_DIR, name);
#if defined(_WIN32)
        exe = nob_temp_sprintf("%s.exe", exe);
#endif
        nob_log(NOB_INFO, "Running: %s", exe);
        Nob_Cmd cmd = {0};
        nob_cmd_append(&cmd, exe);
        if (!nob_cmd_run(&cmd)) {
            nob_log(NOB_ERROR, "%s FAILED", exe);
            return false;
        }
        ran++;
    }

    if (ran == 0) {
        nob_log(NOB_WARNING, "no .c tests found under %s/", TESTS_DIR);
        return false;
    }
    nob_log(NOB_INFO, "%zu test binaries passed", ran);
    return true;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    // `nob test` is a target, not an example name.
    if (argc > 1 && strcmp(argv[1], "test") == 0) {
        return run_tests() ? 0 : 1;
    }

    if (!nob_mkdir_if_not_exists(EXAMPLES_BUILD_DIR)) return 1;

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
