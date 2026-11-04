# Brief for Gemini — Windows Build Preparation, LithoLattice + LithoKraft (Alpha)

**CONFIDENTIAL — private source. Do not upload, publish, or share this package or its contents anywhere. Work locally on JD's machine only.**
From: Elfie (implementation lead, Linux tree) — for Gemini, at JD's direction.
Date: 2026-10-14.

## Your mission

Take the package in this zip, build LithoLattice + LithoKraft on Windows on JD's machine, and get the game **running and ready for JD to test the latest alpha features**. Deliver back: (1) a working build, (2) a short build report (exact commands run, results, any errors with the last 30 lines of output), (3) any Windows-specific issues found, written up plainly — do not silently work around platform problems; JD and Elfie split platform work deliberately (Linux: Elfie; Windows: this seat), and your findings feed the two-tree merge.

## Package contents

- `00-BRIEF-FOR-GEMINI.md` — this brief (package root).
- `Source/` — the complete current source tree: `engine/` (LithoLattice, C++20, namespace `litholattice::`), `game/` (LithoKraft, namespace `lithokraft::`), plus the tree's own `docs/`, `tools/`, and `BUILD-SETUP-JD.md`.
- `Docs/` — the full project documentation set: design briefs, the Lore Bible and glossaries, system reports, code reviews, and the current architecture analyses.
- `Diagrams/` — the 4K architecture/UML diagrams and specimen/capture images.
- `PACKAGE-MANIFEST.md` — exactly what was included and excluded, and why.

## Prerequisites (Windows)

Per `Source/BUILD-SETUP-JD.md`:

1. Visual Studio 2022 Build Tools, "Desktop development with C++" workload:
   `winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override "--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"`
2. Git: `winget install --id Git.Git -e`
3. Python 3.

That's all — CMake's FetchContent pulls GLFW, AngelScript, Vulkan headers, glslang, miniaudio, and stb itself. **No Vulkan SDK needed.** Internet is required on the first configure for those fetches.

## Build — read this before you run anything

The build driver `Source/tools/build.py` is hardcoded to **Release**. For this testing round, **build the Debug configuration instead**, for one specific reason:

> **The signing-key gate (by design, not a bug).** Release builds refuse to boot while the baked-in mod-signing public key is the retired test key — the binary exits with `FATAL: baked-in public key is the retired TEST key — refusing to boot a release build.` This fail-closed check is intentional (the test key is compromised in git history; JD's real key is installed separately, per `Docs/Release-Signing-Key-Checklist-2026-10-13.md`). Debug builds warn and boot normally, which is what a test round wants. Do **not** attempt to bypass, patch out, or replace the key yourself — if JD wants a Release test build, he installs the real key first.

Debug build (from the `Source/` directory, PowerShell or cmd):

```
cmake -S engine -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --config Debug --parallel
ctest --test-dir build-debug -C Debug
```

(The CMake root is `engine/`, not the package root — the game is pulled in from there. The executable lands at `build-debug\Debug\LithoKraft.exe` or the build-tree root; locate `LithoKraft.exe` under `build-debug`.)

You may also run `python tools\build.py --build` once to confirm the Release tree *compiles* — but expect the Release executable to refuse to boot per the gate above, and report that as expected behavior, not a failure.

## Reference baselines (last verified by Elfie on Linux, 2026-10-12)

Report your actuals against these; a delta is a finding, not something to hide:

- **Game selftest** (`LithoKraft.exe --selftest`): **887 PASS / 6 FAIL**. The six known failures: 4× save/restore bit-for-bit, mesher sub-chunk, Vulkan headless (driver-environmental on the Linux VM).
- **ctest**: **43/46**. Known failures: `lithokraft_selftest` (contains the six above), `lithokraft_scout_selftest` (ModelResolver not supplied), `lithokraft_chipor_selftest` (stale registry-size guard, pre-existing). `engine_audio` is known-flaky under parallel load and passes standalone.
- A later extensive run exists in `Docs/Status-Report-Extensive-Test-Run-2026-10-13.md` — read it before concluding any result is new.

## Run + feature checklist (what JD will test — make sure these paths work)

Launch with no arguments: `LithoKraft.exe`. Current defaults and systems to sanity-check:

1. **Renderer**: the **software renderer is now the default** (`--renderer software`; also selectable: `gl33`, `vulkan`). Title screen and a generated world should render fully textured. HUD/menus are not yet rasterized on the software path — known gap, not your bug.
2. **World**: create/load a world; terrain generates; title screen shows biomes.
3. **Locomotion (new)**: WASD walk with collision and gravity; jump; sprint; sneak (edge-guard stops you at lips); **C** = crawl; a 1-block tunnel forces crawl and the view drops; **`fly` console command** or double-tap jump toggles flight. HUD shows live mode/pose.
4. **Camera**: console `camera first|third|follow|free` switches with blending.
5. **Sprites**: console `sprite list`, `sprite spawn <name>`, `sprite burst <name>`, `sprite clear` (GL33 path shows full texture fidelity).
6. **Certificate table**: block `lithokraft:certificate_table` exists and places (signing mechanic itself is not yet wired — the block is describe-only by design this round).
7. **Bench**: `LithoKraft.exe --bench 30 --bench-path flyover --bench-json bench.json --no-save` should complete and print avg/p99 frame times. Performance targets are **stable 30/60 FPS** — report p99s and hitch counts, not peak FPS.

## Coordination

- **Cortana returns Thursday 2026-10-15; her token reset is at 4:12 AM.** She picks up code review and the texturing lift on her return. Your build being green and booting ahead of her return is the point of this round — if you hit a blocker you cannot clear, report it immediately (exact command + last 30 lines) rather than sitting on it, so it can be routed before she lands.
- Report format: commands run, observed outputs, deltas vs the baselines above, and a plain list of anything Windows-specific you had to touch. If you change any source to make Windows build, keep a precise list — those changes are merge material and must not be lost.

## House rules

- This source is JD's life's work and strictly confidential: no excerpts in public chats, issues, or gists; no uploads to third-party services.
- Don't rename, refactor, or "improve" systems while building. Build it, run it, report it.
- If a step contradicts this brief, stop and report the contradiction instead of improvising.
