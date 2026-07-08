# vendor/ — embedded third-party libraries

These are the pristine upstream bodies of mach's three dependencies. They are
pasted in verbatim and stitched into `mach.h` by `scripts/amalgamate.sh`. They
are **not** modified by mach — the mach-side wrapping around each (the warning
pragmas, the `#define` that turns on its implementation, the banner comments)
lives in the `src/vendor_*_pre.h` / `src/vendor_*_post.h` parts, so updating a
library does not disturb it.

| file | upstream | version | license |
|---|---|---|---|
| `RGFW.h` | https://github.com/ColleagueRiley/RGFW | 2.0.0-dev | zlib |
| `clay.h` | https://github.com/nicbarker/clay | 0.14 | zlib |
| `stb_image.h` | https://github.com/nothings/stb | 2.30 | public domain / MIT |

## Updating a library

1. Replace the file here with the new upstream release, unmodified.
2. Regenerate: `scripts/amalgamate.sh`.
3. Build and run `examples/build.sh`; run `scripts/check_namespace.sh`.
4. If upstream added or renamed the `*_IMPLEMENTATION` macro or its include
   guard, adjust the matching `src/vendor_<lib>_pre.h` / `_post.h` glue.

Keep the version and license columns above (and the table in the top-level
`README.md`) current when you bump a version.
