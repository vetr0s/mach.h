# Engine redesign discussion — 2026-06-28

> **Superseded.** This archives the discussion that landed on owning an SDL_GPU
> renderer with an offline HLSL shader pipeline. That direction was later
> **reversed** in favor of a minimal 2D engine on SDL_Renderer (no shaders, no
> GPU pipeline). See `ARCHITECTURE.md` for the current thesis. Kept as a record of
> the reasoning — the exploration is what made the 2D decision clear.

Archive of the design conversation that produced the original `ARCHITECTURE.md`.
Kept so the reasoning isn't lost. This is a digest of the dialogue, not a verbatim
transcript.

## Starting point: the worry

After several days of fast, AI-assisted ("vibe coded") development, the concern
was that the engine had grown too much, too fast, possibly in the wrong
direction — and that a project like **sokol** had found a cleaner, minimal-
dependency approach worth pivoting toward, while keeping the same technical
abilities.

## What the codebase actually looked like

A full read of the hand-written surface (≈2,500 lines, excluding the generated
shader header and SDL):

- `src/mach.c` — unity build root, `main()` wires engine + game in ~15 lines.
- `src/engine/app.h` — `Engine_App` callback vtable (init/handle_event/update/
  render/shutdown).
- `src/engine/render/gpu.{h,c}` — SDL_GPU renderer: device, swapchain, depth,
  two pipelines (lit 3D + 2D overlay), frame lifecycle, batched overlay.
- `src/engine/core/core.c` — engine-owned loop, timing, FPS, window events.
- `src/engine/math/` — hand-rolled vec/mat library.
- `src/game/` — factory sim: world (fat-struct entities + 256×256 grid),
  iso ortho camera, miner/storage placement, `world_tick`.
- Offline HLSL → SPIR-V/MSL shader bake into a committed
  `shaders_generated.h` (two parallel scripts: bash + PowerShell).

**Assessment:** not a runaway. The code is small, clean, well-commented, with a
real one-directional engine/game boundary. Code *quality* was never the problem.

## The three concerns raised

1. **`gpu.c` is "pure magic"** — hundreds of lines the author couldn't follow.
   - Reframed: it's code you *own* but don't *understand* — the worst quadrant.
   - But it's only ~6 concepts repeated with SDL ceremony: device/swapchain,
     shader, pipeline, frame lifecycle, draw, buffer/transfer.
   - Resolution chosen later: **master it, don't hide it** — it becomes the
     engine's centerpiece, not a black box.

2. **The callback boundary feels clunky vs. raylib.**
   - It's "inversion of control" (engine owns the loop, calls down).
   - raylib's nicer feel comes from the game owning the loop and calling *into*
     the library — and you can adopt that **without losing** the engine/game
     separation, because separation lives in the dependency direction
     (engine never names a game type), not in the callbacks.

3. **Memory is a "ticking time bomb"; arenas are the answer.**
   - Correction: there is **no** bomb today — one fixed-size malloc in
     `world_create`, freed once; everything else fixed/stack; no per-frame
     allocation.
   - But arenas are still the right move — to set the allocation idiom *before*
     it's needed (assets, levels, strings, undo).
   - Latent issue flagged: `world_despawn` swap-remove reassigns entity ids, so
     ids held across a despawn go stale → generational handles later.

## The key technical insight (sokol vs. SDL_GPU)

The "should I be more like sokol?" framing was the wrong axis:

- sokol's value is `sokol_gfx` — a generic backend abstraction. **SDL_GPU already
  is that** (it's "sokol_gfx maintained by the SDL team").
- raylib feels lighter because it sits on **OpenGL** — an implicit state-machine
  API that accepts GLSL *strings at runtime*. SDL_GPU sits on modern *explicit*
  APIs (Metal/Vulkan/D3D12) that require precompiled native bytecode.
- So `gpu.c`'s verbosity **and** the cross-platform shader bake are the *same
  root cause*: choosing a modern explicit backend. The complexity is inherited
  from that choice, not gratuitous — and given SDL_GPU, the offline bake is the
  *lightest* available shader strategy.
- Tradeoff named: SDL_GPU is future-proof and native (OpenGL is deprecated on
  macOS); the cost is exactly the explicit-API ceremony.

An option surfaced but not chosen: SDL3 also ships `SDL_Renderer` (simple 2D,
zero new deps) — relevant only if the game were pure 2D sprites.

## The decisive reframe: engine-first

The author chose to be **engine-first** — more passionate about building a
well-designed engine than about what any specific game requires. This changed the
evaluation criterion from "what does the game need" to "what makes this a quality
engine."

Key warning recorded: **"quality engine" ≠ "engine that supports everything."**
The engine-first failure mode is over-generalizing with no game pulling on the
design. The admired engines (sokol, raylib, Handmade Hero) are all *narrow and
opinionated* — their quality is their point of view. So engine-first sharpens the
scoping decision into: **what is the engine's thesis?**

The author's tension: earlier they felt the engine had "too much overhead," now
they want to go deep on engine craft. Same `gpu.c` gets opposite verdicts
depending on thesis:

- **Depth/control thesis** → the complexity is the product; master it.
- **Minimalism/taste thesis** → the complexity is the enemy; thin it.

## The resolution (what was decided)

The author chose to **own the GPU stack**, conditioned on:

1. The engine/game API becomes **raylib-style** (game owns the loop).
2. The cross-platform shader bake becomes **better/more professional**.
3. The **game owns its shaders**; the engine provides an interface to use them.
4. The engine offers an **option to skip real 3D** and do 2D sprites.
5. Stay **minimal & opinionated** — a middle ground between sokol and the
   current code.

This converges on a single coherent design: **sokol's architecture, rebuilt on
SDL_GPU as owned code, with raylib's batteries on top.**

- Layer 1 `gpu` core (sokol_gfx-shaped): generic pipelines/buffers/passes.
- Shader tool (sokol-shdc-shaped): one cross-platform tool, HLSL → bytecode +
  **reflection**; the game runs it on its own shaders.
- Layer 2 batteries (raylib-shaped): sprite/text/mesh helpers with shipped
  shaders, so 2D needs no shader-writing.
- Loop flips to game-owned; directory/dependency rule preserved.
- Arenas set the allocation idiom.

Full spec lives in `ARCHITECTURE.md`. Migration order: loop flip → split gpu.c →
rebuild shader bake with reflection → prove with a 2D sprite path → arenas.
