# File Hardening Implementation Report — 2026-10-11

## Scope
JD-approved follow-up to Phase B's file work: (1) O_NOFOLLOW + fd re-validation
hardening for mod file opens (closes the TOCTOU race between `ModSandbox::resolve()`
and `open()`), and (2) the backup-depth option (powers of two, 2–32, default 8).

## What was built

### 1. Hardened sandboxed opens — `SandboxedFile` / `readSandboxedFile()`
New files: `engine/src/storage/include/litholattice/storage/sandboxed_io.h`,
`engine/src/storage/sandboxed_io.cpp` (wired into `src/storage/CMakeLists.txt`).

Every mod file open now passes three gates, in order:
1. **`ModSandbox::resolve()`** — lexical + symlink containment, as before (unchanged).
2. **`O_NOFOLLOW` open (Linux)** — a trailing symlink fails closed with `ELOOP`
   instead of being followed. `O_NONBLOCK` is also set so a swapped-in FIFO can
   never block the opener (the fstat gate then refuses it as non-regular).
3. **fd re-validation** — `fstat` the descriptor (must be a regular file: no FIFOs,
   devices, directories, sockets), then read back the kernel's true path via
   `/proc/self/fd/N` and re-check it against the sandbox root. This catches
   intermediate-component swaps that `O_NOFOLLOW` cannot see. A ` (deleted)`
   suffix fails closed.

Any gate failure → invalid handle / `nullopt` + explanatory `err`. Fail closed, always.

Supporting change: `ModSandbox::contains(absPath)` — one-line public method exposing
the existing private `withinRoot()` check for the fd re-validation step.

`readSandboxedFile(sandbox, modPath)` is the recommended entry point: resolve and
open happen adjacent with no caller code between them, leaving no TOCTOU window at all.

### 2. Backup depth option
`engine/src/storage/include/litholattice/storage/file_contract.h`:
- `kBackupDepthMin = 2`, `kBackupDepthMax = 32`, `kBackupDepthDefault = 8`
  (JD 2026-10-11; 64 considered and rejected as too heavy).
- `isValidBackupDepth(d)` — true for 2/4/8/16/32. Depth 0 is intentionally *not*
  valid here: it remains the documented programmatic opt-out ("no backups"),
  not a position on the dial.
- `FileWriteOptions::keepBackups` default changed from `0` to `kBackupDepthDefault`
  (8). Rotation machinery itself is unchanged (any unsigned works); the validator
  governs the user-facing option set.
- `RegionFile` now uses the default (8) instead of its old hardcoded 1;
  `kRegionFileBackups` removed (it had exactly one use).

## Test results

New tests in `engine/tests/engine_tests.cpp` (`testSandboxedOpen`, `testBackupDepth`;
`FileWriteOptions` default exercised; one stale "no backups" test comment fixed to
opt out explicitly):

**Attack simulations (all PASS on Linux):**
- Trailing-component swap: resolve a real file → attacker replaces the validated
  file with a symlink to outside the root → hardened open **refused** (`ELOOP`,
  error explains itself). Legit file readable again after restoring it.
- Intermediate-component swap: resolve `sub/file.txt` while `sub/` is a real dir
  → attacker replaces `sub/` with a symlink outside the root → `O_NOFOLLOW`
  cannot see this, but the **fd containment re-check refuses it** and names the
  sandbox in the error.
- Non-regular files refused: directory and FIFO both refused (the FIFO case
  caught a real hang during development — opening a FIFO `O_RDONLY` blocks
  forever; `O_NONBLOCK` was added to the open flags, after which `fstat`
  refuses it cleanly).
- Traversal (`../outside/evil.txt`) rejected end-to-end; normal reads unaffected.

**Backup depth (all PASS):** constants are 2/4/8/16/32-validated, default 8;
rotation at depth 4 over 6 writes keeps `.bak.1`–`.bak.4` and drops `.bak.5`.

**Full verification:** `litholattice_storage` builds clean (zero new warnings);
`ctest engine_storage` passes including all pre-existing storage tests (70/70 from
Phase B plus the new ones). Full `ctest`: only the grandfathered failures remain
(`engine_render` golden hash, `engine_sim` block-names narrowing,
`lithokraft_selftest` SIGABRT from the `blocks.cpp` 40-vs-36 guard — all builder
fixes, out of scope). Two `engine_script` memory-quota checks failed in one full
run due to a stale binary (a concurrent agent had already fixed the test source
from `class Big { int data[8192]; }` — which AngelScript 2.38 cannot parse — to a
`Node`-chain version; the binary predated the fix). Unrelated to this work:
`src/script/` and `src/mods/` include no storage headers.

## Windows builder notes (not faked — Linux-only until implemented)
- `O_NOFOLLOW` equivalent: `CreateFile` with `FILE_FLAG_OPEN_REPARSE_POINT`
  opens the reparse point itself instead of following it; the builder must then
  explicitly reject reparse points (or resolve them against the sandbox).
- fd path re-check equivalent: `GetFinalPathNameByHandle`.
- Regular-file check equivalent: `GetFileInformationByHandle` +
  `FILE_ATTRIBUTE_DIRECTORY` / reparse-point attribute checks.
- Until implemented, non-Linux builds get resolve() + best-effort regular-file
  check only (documented in `sandboxed_io.h`, not silently weakened).

## Open questions / follow-ups for JD / builder
- The ` (deleted)` fail-closed policy: currently any unlink-after-open refuses the
  read. If a legitimate workflow needs to read already-unlinked files, this needs
  a policy flag (default stays fail-closed).
- Backup disk cost at depth 8 for large region files: JD asked for baseline
  testing of the cost — worth measuring once world saves are exercised.
- Cryptographically signed mods (especially first-party) remain a later
  implementation, per JD.

## Appendix: tree reconciliation (concurrent-coordinator collisions fixed)
Several other agents were editing the shared tree concurrently and left it
unbuildable; the following minimal, obviously-correct fixes were applied to
restore a green build (all verified by syntax checks and the full build):
- `render/renderer.h`: the PBR agent's edit created a circular include
  (`renderer.h` → `pbr_transparency_tiers.h` → `pbr_materials.h` →
  `transparency.h` → `renderer.h`, with `ShadeMode`/`PackedVertex` used before
  declaration). Fixed by declaring the two core types early in their own
  namespace block, ahead of the PBR includes. Declaration order only.
- New `render/transparency_method.h` (leaf header): extracted `TransparencyMethod`,
  `kBayer4x4`, and `screenDoorTest` so `pbr_materials.h` and
  `pbr_transparency_tiers.h` no longer pull in `transparency.h` (which needs
  `renderer.h`). `transparency.h` keeps everything else, unchanged behavior.
- `render/gl33/gl_loader.h`: restored dropped `\` line continuations in the
  `LITHOLATTICE_GL_FUNCS` X-macro list (the PBR agent's `TexBuffer` addition).
- `net/CMakeLists.txt`: linked `litholattice_core` (the net agent's new
  `confinement.cpp` includes `core/log.h`; matches the storage module pattern).
- `tests/engine_tests.cpp`: fixed the net agent's `const bool pow2 = [](...)`
  typo to `const auto pow2`.
