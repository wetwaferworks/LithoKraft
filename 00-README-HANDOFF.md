# STATUS FOR ELFIE (2026-10-17 07:20 ET, from Cortana; start here)

## Current state
- **Recommended build (JD promoted 2026-10-17 05:50 ET):** `build\Release\LithoKraft-0.0.1.59-b-packed12.exe`
  - SHA-256 `699C56526160461E31F96EF852D3B10872214CE8E097A75EEB1148A7D263C335`
  - Promoted with `gate.ps1 -Promote`, tag 59bpromo, FULL PASS.
  - `build\Release\RECOMMENDED.txt` names it, and the three desktop test-world shortcuts (Vulkan / OpenGL 3.3 / Software) target it with their args intact.
  - It supersedes 0.0.1.58-a. 0.0.1.59-a is superseded by 59-b; it stays listed in GATED.txt because that file is one exe name per line and is parsed strictly.
- **What 0.0.1.59 adds: Packed12.** Vulkan chunk vertices go from 16 to 12 bytes, with the layer high bits in z bits 9..15 and arena alignment 192.
  - About 25% less mesh memory (stream r64: 1224 vs 1648 MB).
  - fps and p99 unchanged within noise; hashes held.
  - Entities, GL33 and software stay on 16 bytes.
- **Toggle `LL_VK_PACKED12`:**
  - Default on.
  - `0` / `off` / `false` / `no` gives the 16-byte fallback path, the same as 0.0.1.58-a.
  - `1` / `on` / `true` / `yes` forces Packed12.
  - Unknown values keep the default and log a warning.
  - The startup log line `vulkan: chunk vertex format packed12|packed16` shows which path is active.
- **`build\Release\LithoKraft.exe` is JD's chosen test version, currently 0.0.1.44** (kept as `LithoKraft-0.0.1.44-keep.exe`). Never overwrite it with a new build.
- **Tests on 0.0.1.59-b (2026-10-17 05:45 ET, `p2logs\verify-59b.txt`):**
  - Gate FULL PASS with the toggle on and off.
  - Packed12 round-trip tests all pass (exhaustive z × all 2048 layers).
  - Engine suite: 2676 PASS, 6 FAIL. The 6 are the same symlink tests described below.
- **Earlier tests on 0.0.1.58-a (2026-10-17 04:55 ET, `p2logs\p2test-58a.txt`):**
  - Selftest: ALL SELFTESTS OK, PASS=1344 FAIL=0. The "ERROR … fail closed" lines are expected negative tests.
  - Handshake scenario hash: `cb32c824f8b3ea98`, twice.
  - Software A/B: diff=0 on both walk and orbit. Software bench: 15.2 fps on both walk and orbit, 0 errors.
  - `build\tests\Release\litholattice_tests.exe`: 2668 PASS, 6 FAIL (`p2logs\58a_engine_tests.txt`). All 6 are the sandbox symlink tests. They fail at "test symlinks created": Windows refuses symlink creation without Developer Mode or admin. This is environmental, not a code regression; rerun with Developer Mode on to confirm.

## Build / gate / promote
- **Build** (Release, 2 threads, idle priority, from the Pass2 root):
  `"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build --config Release --parallel 2 -- /p:CL_MPCount=2`
  - cmake is not on PATH.
  - The build regenerates the shader headers (`build\src\render_vk\generated\spv\*.h`) automatically.
- **Park the new build** as `LithoKraft-<version>-<letter>-<name>.exe` and restore 0.0.1.44 to `LithoKraft.exe`. `p2logs\cycle.ps1 -tag <t> -name <version-letter-name>` does both, then gates.
- **Gate:** `p2logs\gate.ps1 -exe <exe> -tag <t>`
  - Runs: selftests, hashes ×2, r4 square/round ×2, and 7 shots with a per-shot `--log`.
  - A pass appends the build to GATED.txt.
  - A full pass is required before anything is recommended.
- **Promote (JD only):** `gate.ps1 -exe <exe> -tag <t> -Promote`
  - Writes RECOMMENDED.txt, then runs `python tools\update_desktop_shortcuts.py`.
- **Tests:** `tools/pass2/p2test.ps1`. It targets `LithoKraft.exe`; to test a parked build, copy the script and point `$x` at that build.
- **Hash baselines (must hold):**
  - Cities on: `cb32c824f8b3ea98`
  - Cities off: `e015e51cf1559089`
  - r4 square: `cb32c824f8b3ea98`
  - r4 round: `966be55811fb23a7`

## Docs
- `CHANGES.md`, `BUGFIX_LOG.md` (last entry BUG-0032).
- `docs\VULKAN_HANDOFF_2026-10-16.md` (running handoff).
- `docs\VULKAN_REVIEW_2026-10-16.md` (review items R1-R21, plus the "Packed12 review" F59-1..6). The bit-drop review is in the same file (F55-1..4, fixed in 0.0.1.55; BUG-0017..0019).
- The original design docs are listed below this status block.

## Rules
- The Release signing fatal-exit check is untouchable.
- Keys and the Keys folder are never shared or read.
- `LithoKraft.exe` stays at JD's chosen test version.
- Make `.orig` backups of every file you edit.
- Nothing external.
- Don't kill JD's game: check for a running LithoKraft first, rename if a file is locked.
- Builds and tests run at 2 threads and idle priority, with no focus stealing.
- JD's loop: Implement / Review / Fix / Verify.
- Everything stays "Proposed" until JD approves.
- Credit format:
  - "Proposed by <name>, Built by Cortana".
  - For Argon: "Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; Built by Cortana".
- **Sub-build letters run a-f at most** (0.0.1.58-a .. -f). Past f, bump the version number (0.0.1.59-a).
- The sim timing internals are JD's private IP. Keep their details out of shared docs; private notes live only in JD's private folder.

## Parked 2026-10-17 07:20 ET (JD: park everything)
- **0.0.1.60-a-locktiming** (`build\Release\LithoKraft-0.0.1.60-a-locktiming.exe`, SHA-256 `6CA4255F...27CDA3`) is a diagnostic build, gate FULL PASS with the toggle off and on, NOT promoted. RECOMMENDED stays 0.0.1.59-b, and `LithoKraft.exe` stays 0.0.1.44.
- Diagnostic lock-timing toggle: `LL_LOCK_TIMING=1` (default off; zero behavior change when off) logs a `lock timing:` summary of main-thread lock waits every 120 frames. `engine\tools\vk-perf-summary.ps1` adds a `| LOCK wait ...` section to BENCH lines when present. Example: `$env:LL_LOCK_TIMING='1'` before a run, or `p2logs\vkbench.ps1 -Toggle LL_LOCK_TIMING` for interleaved on/off. Proposed by JD, Built by Cortana. Results are in JD's private notes.
- **Fixed: console stuck on 'waiting for other run'.** The wait check matched its own wrapper. It now goes through `p2logs\run-guard.ps1`, which excludes its own PID, ancestors, children and the tail console, and gives up after 120 s with a warning.
- **OPEN: the engine window does not come to the front in visible-run mode** (JD). It is centered with SetWindowPos without activation, so Windows can keep it behind the active app. Not fixed; parked for later.
- Parakeet Live (GPU) is running on TheWorkstation since about 06:52 ET, so fps from after that is not comparable with earlier runs.
- Nothing is running or looping: all bench, gate and console processes that agents started have exited.

## Open items
1. **Corrected 12-byte vertex:** DONE in 0.0.1.59-b (Packed12, toggle `LL_VK_PACKED12`). One open review item (F59-5); F59-6 is closed:
   - **F59-5 (watch):** no exact-zero pixel proof yet. Gate shots are not frame-deterministic (pulse, sparks, time). The on/off differences are within the noise of two identical 16-byte runs, and the static close-up shows 0 px > 24. A frozen-time capture mode would allow an exact pixel diff.
   - **F59-6 (CLOSED 2026-10-17, no code change):** the 3.11 vs 2.29 ms gap was an artifact of single-window GPU sampling on a streaming bench. At matched draws Packed12 is ~17% lower GPU ms (121/121 windows), with +8% fps at r8 and +13% at r64. See `docs\F59-6_GPU_TIME_2026-10-17.md`. For GPU A/B, use `p2logs\vkbench.ps1`; BENCH lines now carry gpuRunAvg/gpuRunMed and gpuLast@draws (`engine\tools\vk-perf-summary.ps1`).
   - **Visible-run mode (JD 2026-10-17, default for benches and smoke tests):** run `p2logs\vkbench.ps1` (switch `-ShowConsole`, default `$true`; use `-ShowConsole:$false` for no windows). It opens ONE console (`p2logs\vk-console-tail.ps1`, conhost) sized to the whole primary-monitor work area. While each run is going, the console is fed live from that run's engine `--log`: perf lines, chunk streaming, draws, GPU ms, hitches, the `update-split` and `bench-sec` lines, then the BENCH summary lines. The engine runs windowed Vulkan (never headless). Its window is CENTERED on the primary monitor at its standard windowed size, in front of the console (SWP_NOSIZE|SWP_NOACTIVATE: no resize, no focus steal). The engine is started with CREATE_NO_WINDOW because the exe is a console-subsystem app and would otherwise open a second, blank console. Everything closes itself: the game when its bench ends, the console 5 s after `results:`. The script is DPI-aware so the centering is right at 125% scaling. Example: `powershell -ExecutionPolicy Bypass -File p2logs\vkbench.ps1 -Exe LithoKraft-0.0.1.59-b-packed12.exe -Tag 59b -Scenarios smoke` (scenarios: smoke = 8 s orbit, stream64, still64, r8, testworld; `-Pairs N` for interleaved LL_VK_PACKED12 on/off; `-Screenshot x.png -ShotAt 12` grabs the desktop). Headless stays only for runs with no renderer: `--selftest`, the gate's `--handshake-scenario` hash runs, and run-bench-suite's `--perf-headless` step.
2. **8-byte vertex + per-quad SSBO:** Argon proposals, not started.
3. **Trace Bulb inner glowing core model:** needs JD's call.
   - The in-world bulb is a plain cube (`trace_tip_red/blue`, faces = `*_trace_bulb_ghost`).
   - The glass-core `.lthm` model (`game\LithoKraft\assets\models\trace_bulb\`) is only loaded by a selftest.
   - Once a core is meshed, Argon's dim shell can follow: shell alpha ≤ 0.12 with no glow mask, so only the core glows.
4. **Possible orphan trace-tree tips at chunk borders:** worldgen, hash-moving. Not confirmed; the sky bulbs seen so far sit on stems.
5. **Validation-layer run:** Vulkan SDK 1.4.363.0 installer is in JD's Downloads and needs JD's admin install.
6. **Main-thread lock-wait timing:** measure and attribute.
7. **Minor review items R5 and R7** (see VULKAN_REVIEW).

## Elfie's next work areas (as JD listed; pointers only, nothing implemented)
**Art & component wiring**
- **Repeator d2/d4/d8/d16 tintmasks:**
  - Incoming art: `game\LithoKraft\assets\art_incoming\repeator_2026-10-16\`. Contains `models\repeator_d{2,4,8,16}.voxels.json`, `repeator_index.json`, `build_repeator.py` and `NOTES.md`.
  - Block: `game\LithoKraft\src\content\blocks.cpp` (~l.51, Repeator, Shape::Custom; mesh is `repeatorMesh` in `foliage.h`/`foliage.cpp`).
  - Tint rule: `game\LithoKraft\src\content\appearance.cpp` (~l.167, a "tint:" mask gives kShadeTraceTint; `machineTintLook`).
  - Masks are registered in `game\LithoKraft\assets\packs\pack_32\resources.json` and `pack_64\resources.json`.
  - Logic: `src\content\components\logic_components.cpp`.
- **Remaining Wafer beta blocks (Red LED, Blue LED, Mere Glass):**
  - No source or assets found yet (grep for LED, "Mere Glass" and `red_led` returned nothing relevant).
  - Register in `game\LithoKraft\src\content\blocks.cpp` (Wafer block section ~l.93).
  - Looks go in `appearance.cpp` (faceLook).
  - Textures and masks go in `assets\packs\pack_{16,32,64}\resources.json` + `manifest.json`.
  - Material bindings go in `assets\lithokraft\materials\bindings\`.
- **Knelt Shift ×3 trigger:**
  - The existing kneel mechanics are in `game\LithoKraft\src\game\include\lithokraft\game\prayer.h` (kneel windows, kPrayerKneelTicks) and `game.cpp`.
  - Locomotion lives in `engine\src\entity\locomotion.{h,cpp}`; tests are in `wiring_selftest.cpp`.
  - Knelting is visual-only in the Creative beta.

**Lore & Logbook seeding**
- **Logbook:**
  - The item is `kItemFragment` "The Logbook" in `game\LithoKraft\src\content\items.cpp:34`. The id `lithokraft:fragment` must stay unchanged for saves.
  - Use and UI: `item_use.cpp`, `game.cpp`, `game_ui_selftest.cpp`, `items_selftest_main.cpp`.
- **Wafer Bible:** no copy in this repo. Herominde keeps the bible; get the current version from her before seeding pages.

---

# LithoLattice + LithoKraft: Handoff for Elfie (CONFIDENTIAL, JD and Elfie only)
October 10, 2026. From Cortana for JD. Elfie leads implementation discussion until the builder returns (about Oct 12), then the builder does code reviews, deeper implementation, and bug fixes.

## What's in the package
- `LithoLattice-engine-src-*.zip`: the engine (repo root without `LithoKraft/`), namespace `litholattice::`.
- `LithoKraft-game-src-*.zip`: the game, namespace `lithokraft::` (the `LithoKraft/` folder). It depends one way, game to engine.
- `docs/`: this README, ROADMAP, the full design brief, the mod-developer architecture brief, the Trace and components brief, the night update, SDK and architecture docs, and diagrams.
- Source is from branch `bench/high-tps` at commit `100ecf3` (main plus all of last night's Trace and benchmark work). Build: CMake + MSVC, Windows, Vulkan primary, GL fallback. `tools\build.ps1`. Self-tests: run the exe with `--selftest`.

## The heart of it: Trace and the tick system (Moore's law in the engine)
- Trace (red and blue) is the signal system. It's part of LithoLattice's timing engine, and it's what LithoKraft lives or dies on.
- Tick rate is the progression curve: as in-game tech advances, and as real hardware improves, Trace can tick faster. Launch baseline is 512 TPS. The cap is a power-of-2 setting (1024 up to 8192+). This makes it a "forever game" that scales with hardware.
- Proven tonight: 8192 TPS in static scenes, and every test screen (16x16 per-pixel, 32x32 and 64x64 pattern walls, sawtooth, wire-torture) at 60 FPS and 512 TPS on JD's RTX 3050 Ti laptop.
- Proposed OOD (for review, see the architecture brief): small inheritable interfaces IBlock, ITickable, IRandomTickable, ISignalNode, plus TickContext. One shared behavior object per block type and a compact per-block state. Block properties are data flags, not subclasses. A scheduler with active lists per section and tick domain. Graph compile and JIT with per-node and per-island profiling. Mods (C++ and AngelScript) implement the same interfaces.

## Questions for Elfie to drive
1. Lock the tick-interface design (above) or change it?
2. Catchor first-touch auto-connect: keep, or add a reset?
3. Via tier, Pattern ROM source and tier, and which idea-list blocks to promote.
4. Rename code identifiers to -OR names now (Pulsar to Pulsor, Memory Cell to Memor, Thresholder to Thresholdor, wire to Trace)? Block ID changes are cheapest now; saves use a name table.
5. Enthusiast tick tier: lore-gated or a settings toggle?
