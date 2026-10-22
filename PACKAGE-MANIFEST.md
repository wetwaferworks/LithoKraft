# LithoLattice / LithoKraft — Full Source Package

**Package date:** 2026-10-15
**Produced for:** Cortana session, Thursday 2026-10-15 (JD order, 04:23 EDT)
**Source tree:** `~/workspace/litholattice/` — the current production tree, packaged fresh on 2026-10-15. This supersedes the 2026-10-14 alpha package.

## Tree state at packaging

- Timing Handshake **PROMOTED to default ON** (2026-10-14, JD's direct order; legacy is opt-out via `--no-timing-handshake` / cvar `sim.timing_handshake 0` / settings).
- Rate scaling **reimplemented in production** (bidirectional TickRate ×16/÷1024, per-class rate flag, lithography rule + SelectOR-corner rate-up design).
- Full code review #2 fix run **landed 01:57 EDT 2026-10-15** (2 criticals + 6 highs addressed; gates below).
- Selftest: **1239 PASS / 0 FAIL, both arms**. Scenario hash: **aa82e7afb1eff9d3** (unmoved on all arms). ctest: **48/48 serial**.
- `--demo-test`: 42 pass / 1 fail — the sole failure is the known stale absolute hash pin (`5041cf1795c33a8c`); repin is JD's pending call.

## Contents

- Complete engine source (`engine/src`, `engine/include` tree as laid out, `engine/CMakeLists.txt`, `engine/cmake`) and game source (`game/LithoKraft/` — `engine/LithoKraft` is a symlink to it and is stored as a symlink entry).
- Build scripts and CMake files throughout, including `tools/build.py`.
- Resources and assets as they stand in-tree (`engine/assets`, `game/LithoKraft/assets`).
- In-tree tools, including `tools/systerian_translator.py`.
- In-tree docs (`docs/`, `engine/docs/`, `game/LithoKraft/docs/`, top-level README/handoff notes).
- `designers-editors/` — tonight's standalone designers/editors (see below).
- Historical snapshot archives that live in the tree root (`LithoKraft-game-src-100ecf3.zip`, `LithoLattice-engine-src-100ecf3.zip`, `litholattice-full-2026-10-11.zip`) are included exactly as found.

## Package numbers

- Files in zip: **1834** (+ this manifest; + 1 symlink entry)
- Total uncompressed size of packaged files: **28,150,824 bytes** (26.8 MiB)

## Exclusions (build/runtime output only — no source removed)

- `build-linux/` — Linux CMake build directory (objects, binaries, logs, `_deps`): entirely build output.
- `build-debug/` — second CMake build directory: entirely build output.
- `engine/build/` — nested CMake build directory inside `engine/`: entirely build output.
- `Testing/` — CTest temporary output at tree root.
- `game/LithoKraft/logs/` — runtime log files.
- All `__pycache__/` directories and `*.pyc` files.

## designers-editors/ — tonight's standalone tools

| File | What it is | md5 |
|---|---|---|
| `systerian_shield_editor.py` | Systerian Shield Designer/Editor, Python, **v5.1** (credits & history edition) | `92f569a401870b3f9b26099b54e0bfbe` |
| `emblem_designer.py` | Emblem Designer · Editor, Python, **v1** (four houses; registry built for 64 prime creatures) | `07d7391fd6bb91ba4736d68e406d5964` |
| `shield-Editor-History.md` | Shield editor chain-of-custody / version history | — |
| `emblem-Emblem-History.md` | Emblem designer credits & history | — |

Editor md5s were verified against the live files in `~/workspace/user/files/` at packaging time and match the expected values (`92f569a4…` shield, `07d7391f…` emblem).
