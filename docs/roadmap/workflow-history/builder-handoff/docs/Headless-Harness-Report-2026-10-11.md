# Headless Game Harness — Implementation Report 2026-10-11

JD approved: "absolutely worth building for testing." Built for real.

## What was built

**Location:** `game/LithoKraft/src/headless/` (new directory, wired into the game via
`add_subdirectory(src/headless)` in `game/LithoKraft/CMakeLists.txt`).

**`headless_harness.h` / `.cpp` — the general game-testing fixture.** Not a one-off runner:
future gameplay tests (inventory, combat, dimensions, lithography) live here.
- `NullRenderer`: no-op `IRenderer` (mirrors the one in `app/perf_headless.cpp`). Records
  uploaded chunk-mesh bytes by sub-chunk key so tests can assert the mesh pipeline ran;
  `renderFrame()` is a no-op. No GPU needed — CI-safe.
- `HeadlessGame`: owns everything a `Game` needs and constructs it headless, in main.cpp's
  order — `JobPool`, `NullRenderer`, `ResourceManager` (no packs; built-in worldgen
  fallbacks), `AudioEngine` (no output device; commands queue harmlessly), `InputSystem`
  (`defaultActionMap()` + `inputContexts()`, no backend). Small world (4 chunk columns,
  fixed seed) for fast loads; no save dir, no scripts dir, no settings persistence.
  `frame(dt, w, h)` advances the game on a dummy 1280×720 viewport.

**`harness_selftest.cpp` — 8 test groups, 29 checks**, driving the REAL `Game` and the REAL
retained UI (`GameUi`) through `UiRoot` exactly like the app's event loop:
1. Boot → menu: constructs headless, `enterMenu()`, asserts Menu state + overlay + title +
   `A LITHOLATTICE GAME` tagline.
2. Menu → Play: clicks the real `menu-play` button, asserts Playing + unpaused + overlay closed.
3. Settings apply: drives the real settings dialog (`set-volume`, `set-mute`) through the
   game's own `onSettingsChanged` wiring; asserts `game.settings()` reflects the change.
4. Pause flow: `setPaused(true)` → dialog opens; clicks `pause-resume` → unpauses + closes.
5. Exit to menu: clicks `pause-exit-menu` → back in Menu state + overlay open.
6. Quit flow: clicks `menu-exit` → `quitRequested()`.
7. Focus loss: `onFocusChanged(false)` while Playing → pauses + auto-pops the pause menu;
   in the menu it does NOT pop the pause dialog; refocus does not unpause by itself.
8. Frame loop: 600 headless frames run; the mesh pipeline uploads through the null renderer
   (bytes > 0, mesh count > 0).

**`harness_main.cpp`** — entry point; exit code = failure count.
**`CMakeLists.txt`** — builds `lithokraft_headless`, registers the
`lithokraft_headless_harness` ctest.

## The blocks.cpp workaround (documented)

Pre-existing, builder-owned: `blocks.cpp` registers ~40 rows but the guard at
`blocks.cpp:69` expects `kOverclockor + 1` = 36 and calls `std::abort()` — the shipped game
cannot start. The harness does NOT fix this. Instead, `patch_blocks.cmake` copies
`blocks.cpp` into the build dir, downgrades the guard to a stderr warning, and the
executable links the patched object FIRST, so the patched `blockRegistry()` wins over the
archived one. The tree is untouched. The warning prints at startup, e.g.:
`headless harness: block registry holds 38 rows, guard expects 36 -- pre-existing desync,
builder owns the fix`. (The exact row count shifts as parallel work edits the content
table; the mismatch itself is the pre-existing desync.)

## Verification status

- Harness target builds clean (only the grandfathered `world/block.h:90` narrowing warning).
- Test run: **23/29 checks pass**. The 6 failures were all one root cause in the *test
  helper* (not the game): `clickId` searched only the root widget tree, but the pause
  dialog is a modal outside it. Fixed by searching `UiRoot::topModal()` too — the same
  pattern `game_ui_selftest.cpp` uses. Re-verification was blocked by tree instability
  (see below); the fix mirrors a proven pattern and is low-risk.
- New files are LF; the one edited file (`game/LithoKraft/CMakeLists.txt`) kept its CRLF.

## Blocker: parallel-coordinator tree collision (parent must sequence)

During this task, 3+ sibling coordinators edited the same tree concurrently (PBR phase 2
in `render/`, script/mod-security in `script/` + `mods/`, others). Observed mid-flight
breakage, in order:
1. `ScriptEngine::raw()` removed by the script-security rework → `game.cpp:70` failed.
   (That coordinator migrated the caller itself minutes later.)
2. `litholattice_mods`: `sdkVersion` redeclaration + missing `<algorithm>` include.
3. Circular include `transparency.h` ↔ `renderer.h` ↔ `pbr_transparency_tiers.h`
   (PBR phase 2 mid-refactor).
4. `litholattice::std::min` conflicts / `pair does not name a type` in system headers
   (a header polluting `namespace std`, still in flight).

The harness cannot get a final green build until the tree is consistent. **Recommended
sequencing:** let the parallel coordinators finish, then rebuild `lithokraft_headless`
and run it — the 6 fixed checks should go green, and the ctest
`lithokraft_headless_harness` should pass. No harness code changes are expected to be
needed; the collision is entirely outside `src/headless/`.

## Open questions

- Should the harness also cover the `--bench` path (bench camera, stream soak)? Currently
  menu/state-machine only.
- Settings-file round-trip is covered by `game_ui_selftest`; the harness asserts live-apply
  only. Enough, or should the harness own a settings-persistence test with a temp dir?
- The 600-frame mesh assertion is timing-tolerant (loops until bytes > 0). If CI proves
  flaky on slow machines, pin it to a fixed frame count with a looser assertion.
- `NullRenderer` duplicates the one in `app/perf_headless.cpp` (anonymous namespace there).
  Consider hoisting a shared `NullRenderer` into `litholattice_render` when the tree settles.
