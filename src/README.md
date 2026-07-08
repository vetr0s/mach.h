# src/: mach's own source

The engine's own code (~1.9k lines), split into the parts that
`scripts/amalgamate.sh` stitches (together with the pristine libraries in
`vendor/`) into the single-file `mach.h`. **Edit here, not in `mach.h`**
(which is generated); then run `scripts/amalgamate.sh`.

`mach.h` is emitted in two phases, mirroring the file's structure: the public
interface (inside the `MACH_H` include guard), then the implementations (inside
the `MACH_IMPLEMENTATION` guard). The parts, in assembly order
(`scripts/manifest.txt`):

| part | phase | contents |
|---|---|---|
| `preamble.h` | banner | file banner, `#ifndef MACH_H` |
| `base.h` | interface | sized-int aliases, version macros |
| `debug.h` | interface | `MACH_LOG_*`, `MACH_DEBUG_ASSERT` |
| `vendor_rgfw_pre.h` | interface | RGFW glue (defines, warning pragmas) → wraps `vendor/RGFW.h` |
| `vendor_rgfw_post.h` | interface | close RGFW's guards, pragma pop |
| `interfaces.h` | interface | mem, math, color, gl, font, image, render2d, input decls |
| `vendor_clay_pre.h` | interface | Clay glue → wraps `vendor/clay.h` |
| `vendor_clay_post.h` | interface | pragma pop, end banner |
| `decls_tail.h` | interface | clay_ui + core decls, `#endif MACH_H`, open `MACH_IMPLEMENTATION` |
| `impl_mem_math.h` | impl | mem + math implementations |
| `vendor_stb_pre.h` | impl | stb glue → wraps `vendor/stb_image.h` |
| `vendor_stb_post.h` | impl | pragma pop, end banner |
| `impl_tail.h` | impl | image, font, render2d, clay_ui, core, input impls; close guard; license |

The `vendor_*_pre.h` / `_post.h` parts hold the mach-side wrapping so the files
in `vendor/` stay pristine and updatable; see `vendor/README.md`.

To reorder, add, or remove a part, edit `scripts/manifest.txt` (the order there
is authoritative) and regenerate.
