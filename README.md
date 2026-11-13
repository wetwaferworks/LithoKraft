# LithoLattice + LithoKraft

This repository holds two projects that are built together:

- **LithoLattice** (`engine/`): a C++20 voxel engine. It has OpenGL 3.3 and Vulkan renderers, a simulation tick thread, procedural generation, a retained-mode UI, sandboxed AngelScript, resource packs and a retro synth.
- **LithoKraft** (`game/LithoKraft/`): a voxel CPU-building game on top of the engine. Players build working logic from blocks: traces, gates, clocks and memory.

The engine's CMake project builds both. See `engine/README.md` for the architecture and `game/LithoKraft/README.md` for gameplay and controls.

## Folder guide

| Folder | Contents |
|---|---|
| `engine/` | LithoLattice engine: `src/` modules, `assets/` (shaders, UI), `tests/`, `cmake/` helpers. This is the root CMake project. |
| `game/` | LithoKraft game: sources, game pack assets, scripts, config |
| `host/` | `lithocef`, the hardware/host layer interfaces (CEF and GPU stubs with smoke tests) |
| `wasm/` | WebAssembly build: scripts, packaging and test harnesses |
| `android-shell/` | Android wrapper app |
| `cmake/` | Cross-compile toolchain files (wasm32) |
| `tools/` | Build driver (`build.py`), build counter, generators, small utilities; `pass2/` and `demos/` |
| `designers-editors/` | Emblem and shield designer tools (Python) |
| `docs/` | Research, roadmap, reviews, playtests, safety and versioning records (see `docs/README.md`) |
| `Lore/` | World and story bible, briefs and lore drops |
| `PersonalControlHarness/`, `WetWaferWorks-Inc/` | Placeholders for related company material (kept empty in this repo) |

Root files: `CHANGES.md` / `CHANGES-PASS2.md` / `BUGFIX_LOG.md` (change history), `CLAIMS.md` (verified claims and their evidence), `GROUND-RULES.md` and `VERSIONING-RULES.md` (working rules), `PACKAGE-MANIFEST.md` / `PACKAGE-NOTES.md` (release packaging), `LITHOKRAFT_VERSION.txt` (current version), `00-README-HANDOFF.md` and `HANDOFF-0.0.1.61.txt` (handoff notes), and `demo_record_proof.lldemo` (a demo-recorder fixture that the selftest reads by relative path).

## Build

Requirements: CMake 3.16+, a C++20 compiler (Visual Studio 2022 Build Tools on Windows, or GCC/Clang on Linux), and Python 3. CMake fetches GLFW, AngelScript, glslang and the Vulkan headers itself; no Vulkan SDK is needed.

```
python tools/build.py          # configure engine/, build Release, run ctest
python tools/build.py --build  # build only
python tools/build.py --test   # run tests on an existing build
```

Or build directly: `cmake -S engine -B build -DCMAKE_BUILD_TYPE=Release` then `cmake --build build --parallel`, then run `ctest` from `build/`.
The game also runs `LithoKraft --selftest` as a quick smoke check. Two tests (`engine_sim` and `lithokraft_selftest`) are known baseline failures; see `BUILD-SETUP-JD.md`.

Build hashes and version numbers in reports are provisional unless a record says otherwise.