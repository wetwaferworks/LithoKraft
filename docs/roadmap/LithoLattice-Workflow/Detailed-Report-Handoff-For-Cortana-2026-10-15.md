# Detailed Report Handoff for Cortana — Elfie's Full Review of the 2026-10-15 Handoff

**From:** Elfie (review crew of three + synthesis), for Cortana, at JD's request 2026-10-15 13:20 EDT.
**Subject package:** `LithoLattice-LithoKraft-Elfie-Handoff-2026-10-15-1222` — built on Elfie's
full-source package of 2026-10-15 04:23 (verified exact: your tree = that package + 62 additions
− 0 removals; your edit set reconciles to **48 files modified + 1 new**).
**Method:** three independent reviewers — engine core, render+game, evidence+compat — each
diffing your files against the pristine input package (your `.orig` backups were not in the
package; see Finding P-1) and reading the code. REVIEW ONLY: nothing was modified.
**Companion documents:** `Handoff-Review-Cortana-2026-10-15.md` (JD's copy, with the verdicts
and his decision list) and `Notes-For-Cortana-2026-10-15.md` (the short version of this file).

## Overall verdict

No critical defects in the shipped code. Every load-bearing claim that could be checked from the
package checked out: determinism untouched (handshake / control-stream / trace files
byte-identical; sim diff is threading-only; `aa82e7afb1eff9d3` on all three Debug arms in your
scenario log; never repinned), ll_u128 bit-correct (NIST + RFC 8032 vectors in your crypto log),
SAV-H1 a strict world-permanence win, renderer math and band-pool threading correct in code (not
just in prose), focus/click root causes real, SIM-H1 revert surgically clean, handshake-coupled
meshing verifiably absent (design-only, as stated — JD now reports you have since built it; it
will be reviewed with the merge).

## The findings that need you (summary; full detail in the sections below)

- **P-1 (process, high): the `.orig` backup set, the `.reverted-SIM-H1` file, the 0841 design
  docs, and the four diagnosis/source-review documents were not shipped.** Diffs were recovered
  from the pristine input zip, so nothing went unaudited — please include them next time so the
  record travels complete.
- **P-2 (evidence, high): the claimed final test state (44/48) has no shipped log.** Newest
  ctest logs: 05:11 Release, parallel, 40/48 — including `engine_audio` and
  `lithokraft_selftest` SEGFAULTs after the /STACK fix, unmentioned in the letter — and 05:28
  Debug at 41/48, which also contradicts the letter's "Before: 42/48" (the port notes say
  41/48). The claimed final failure set is internally consistent with the shipped failures
  minus the three later-fixed tests, but it is an expectation, not a run. Please send the final
  ctest + selftest logs, and look at the 05:11 Release segfaults (parallel methodology may
  explain them; currently unexplained). Elfie's post-merge serial Debian battery will produce
  the comparison numbers.
- **D-1 (JD decision, high): signing guard coverage.** `bakedKeyIsTestKey()` recognizes only the
  retired test key, so the dev key boots Release without tripping the guard. JD to ratify:
  interim beta key / extend the guard / re-bake before any Release ships. Flagged for you both;
  no action asked of you until he rules.
- **M-1 (merge hygiene):** `config/config.lth` in the package is playtest-persisted state
  (13 cvars, no CHANGES row) including `render.model_dither "1"` against your 1215 default-off;
  31 `saves/world/` runtime files also ride in the tree. Elfie will exclude both from the merge.
  A ledger line for runtime-persisted config keeps CHANGES.md airtight.
- **E-1:** pixel-identity PNGs not included — the claim stands on the log alone; include the
  PNGs or their hash list next time.
- **Small code notes:** `interchange_capture.cpp:126,177,178` still hardcodes the three
  /home/hatch paths (biome_capture's twin — still Windows-broken); `mesher.cpp jobDef()` returns
  a reference into a growable vector (safe today — a comment or reserve() would armour it);
  `queryCellsSaturated` span() precondition (|a−b| < 2^63, enforced at call sites) worth a
  comment; band pool wants an exception/nested-run note at the run mutex; the post-tick section
  now calls game `inspect()`/`activeCount()` under blockMutex_ — safe with today's callbacks;
  keep future callbacks non-blocking there. CHUNK-STALL precision note: `stop()/start()` remains
  as a deliberate fail-closed fallback if the hold fails — summaries shouldn't claim it vanished.
- **Quantified for JD:** the 4-thread render default costs ~34% FPS at 1080p vs 8 threads
  (19.4 vs 29.4, your own table) for sim/mesher headroom; it stands as he ruled it.

Everything else — per-file verdicts, line-level evidence, the log audit, the Windows-compat
sweep (includes: no backslash literals introduced, all 373 changed-file includes case-exact,
CRLF in 7 files largely pre-existing convention, 14 files pass g++ -std=c++20 -fsyntax-only) —
is in the three sections that follow, verbatim from the reviewers.

---
---

# Appendix A — Engine core

# R1 — Engine Core Review: Cortana Handoff 2026-10-15-1222

Reviewer: R1 (engine core). REVIEW ONLY — nothing was modified.
Trees: H = handoff tree (`LithoLattice-LithoKraft-Elfie-Handoff-2026-10-15-1222/`),
P = Elfie production tree (`~/workspace/litholattice/`), used as the diff base.

## 0. Method note — the .orig backups are NOT in the package (process finding, MEDIUM)

CHANGES.md states every changed file "has a backup beside it as
`<file>.orig-2026-10-15-HHMM` in the working tree," and that the reverted
SIM-H1 copy "is saved as `sim_thread.cpp.reverted-SIM-H1-2026-10-15-1035`."
Neither exists in the shipped package: a full listing of the outer zip
(1,971 entries) and of the extracted tree contains **zero** `*.orig-*` files
and **zero** `*.reverted*` files (verified by name search over the whole
workspace). The same applies to the two design docs the handoff note cites
(`ChunkLoad-Sim-Fix-2026-10-15-0841.md`, `Handshake-Chunk-Streaming-2026-10-15-0841.md`)
— not in the package. All three exist, if anywhere, only on the workstation.

Consequences for this review: Cortana's diffs were recovered as **H vs P**
(the full-source package built from P on 2026-10-15 is the tree her package
derived from; an untouched control file, `engine/src/sim/timing_handshake.cpp`,
is md5-identical between H and P, and every differing engine file reconciles
against CHANGES.md — see §8). The diffs so recovered are small, single-purpose,
and marker-commented, so confidence is high — but the revert audit in §4 is
necessarily indirect, and any future handoff should ship the `.orig` set so
the reviewer diffs her exact edits rather than a reconstructed base.

Line endings are LF throughout H (no CRLF contamination).

---

## 1. Crypto PORT (0438) — ll_u128 + sha512 / ed25519

**VERDICT: sound.**

### 1.1 u128.h (new) — MSVC hi/lo struct audited line by line

- `operator+` / `operator-`: carry/borrow computed from the wrapped low word
  (`l < a.lo`, `a.lo < b.lo`) — correct mod-2^128 semantics (u128.h:23-30).
- `mul64` (u128.h:31-38): standard 32-bit limb decomposition. Each partial
  product is < 2^64 (operands ≤ 2^32−1); `mid` is < 3·2^32 so `mid << 32`
  discards exactly the bits that `(mid >> 32)` adds into the high word.
  Verified by hand against the schoolbook expansion — correct.
- `operator*` (u128.h:39-43): full low×low product plus the two cross terms
  into `hi` mod 2^64; the hi×hi term correctly contributes only above 2^128.
  Correct.
- Shifts (u128.h:44-55): all four regimes handled — s ≤ 0 (returns `a`;
  native shift-by-negative is UB, so the struct is *safer* than the native
  type here), s ≥ 128 (returns 0; native is UB), s ≥ 64 (single-word shift,
  shift count s−64 ∈ [0,63]), and the 1..63 cross-word path (counts 64−s ∈
  [1,63]). No shift-by-64 or shift-by-≥width anywhere. Correct.
- Conversion `operator T()` returns `static_cast<T>(lo)` — identical to
  native `__int128` truncation for every integral T.
- The struct offers exactly `+ - * << >> +=`, construction from `uint64_t`,
  and explicit conversions. Every use in the ported files (below) stays
  inside that set, and the MSVC build + selftest evidence (§1.3) confirms it.

### 1.2 Call sites — pure token substitution, semantics preserved

- `sha512.h`: `total_` becomes `ll_u128`. `sha512.cpp`: the only arithmetic
  is `total_ += len` (update), `bitlen = total_ * 8`, and
  `uint8_t(bitlen >> (120 - 8*i))` in final() — shifts up to 120 exercise
  the struct's s ≥ 64 branch, which yields `hi >> 56` in the low word;
  byte extraction via the truncating conversion matches the native path
  bit-for-bit.
- `ed25519.cpp`: the diff is a mechanical `__uint128_t` → `ll_u128`
  substitution at 9 sites. The two idioms that matter check out under the
  struct: (a) the multiply-accumulate `s = (ll_u128)a.w[i] * b.w[j] +
  t[i+j] + carry` — full 128-bit product from `mul64`, carries via
  `s >> 64`, truncation via `(uint64_t)s`; (b) the borrow idiom
  `d = (ll_u128)w[i] - kP[i] - borrow; borrow = (uint64_t)(d >> 64) & 1` —
  operands are single limbs, so `d` mod 2^128 is either the true
  non-negative result (hi = 0) or 2^128 − k (hi = 2^64−1), and `& 1`
  extracts the borrow identically on both implementations.
- On the GCC/Clang path `ll_u128` is a typedef of `unsigned __int128`, so
  the Linux build is behaviorally untouched — consistent with the scenario
  hash holding.
- No aliasing/type-punning is introduced; the struct is two plain
  `uint64_t` members.

### 1.3 Evidence the MSVC path was actually exercised

`run-crypto-0510.log` (shipped, UTF-16 transcript of the Windows run):
**37 [PASS] / 0 [FAIL]**, including SHA-256/SHA-512 NIST vectors, Ed25519
RFC 8032 vectors 1–3 (keypair/sign/verify), tamper rejection, and the
signing round-trips. (Note for the reader: `ctest-crypto-0500.log`'s tail
shows a PowerShell `NativeCommandError` transcript artifact from the
wrapper script, not a test failure; the direct crypto runs pass.)

### 1.4 signing.cpp POSIX guards (0449)

**VERDICT: sound.** The include guard (`#if defined(__unix__) ||
defined(__APPLE__)` around fcntl/sys/stat/unistd, signing.cpp:7-11)
matches the pre-existing conditional structure of the body: the POSIX
`::open(O_NOFOLLOW)/::fchmod` branch of `writePrivateKeyFile`
(signing.cpp:160-176) is under the identical guard, and the `#else`
branch (ofstream write + `std::filesystem::permissions` tightening to
owner read/write) is the sane Windows equivalent. Nothing POSIX is
referenced outside the guard.

### 1.5 crypto_selftest.cpp close-before-remove (0459)

**VERDICT: sound.** `f2.close()` is inserted immediately after `sig2`
is read into memory and before the determinism check, which compares
only the two in-memory strings (crypto_selftest.cpp, H ~line 171).
No later use of the stream. Removes the Windows "cannot delete an open
file" failure at the final `remove_all`. Correct and minimal.

---

## 2. SIGNING (0733) — dev public key baked in

**VERDICT: sound as implemented; one HIGH decision item for JD (the change
disarms the release boot guard as a side effect — disclosed in Cortana's
own handoff note, but JD should ratify it explicitly).**

Evidence:
- `mod_signature.h:45-46` (diff vs P): the old test key
  `3bb44b55…b787` is fully replaced by
  `04c1766ef29e93b7d36fdf185f64ee958e9dded4c8c82f0dca03029e8a30011c` in the
  single `kBakedPublicKeyHex` assignment. No second copy of a baked key
  exists in code; the new key string appears elsewhere only in the three
  ledger documents.
- `mod_signature.cpp` is **byte-identical** to P, so `bakedKeyIsTestKey()`
  (mod_signature.cpp:122-130) and the fatal-exit boot guard that calls it
  (game main.cpp:206) are mechanically untouched — CHANGES.md's "the
  fatal-exit check is intact" is literally true.
- No private material anywhere in the package: grep over all text files
  in H for private-key markers/seeds found nothing, and no `*.priv`,
  `*.key`, or key-named file ships in the tree. The handoff note states
  the private half lives outside the tree under JD's profile (JD-only
  ACL), never read or printed — consistent with everything observable
  from the package.

**What JD must still decide (HIGH, decision — not a defect):**
`bakedKeyIsTestKey()` returns true **only** when the baked key equals the
retired test key `3bb44b55…`. With the dev key baked in, the function
returns false, so the release-build fatal exit no longer fires — as
Cortana's note itself headlines, "**Release now boots on the dev key.**"
That means a Release build from this tree will boot and extend full
"Verified" trust to mods signed by a keypair whose private half lives on
the workstation — a materially weaker posture than the production key
ceremony JD has always planned (offline master key, replacement was and
remains JD's offline action). Options for JD: (a) accept the dev key as
the interim beta key and retire the guard concern explicitly; (b) extend
the non-shipping check to also recognize the dev key, so Release still
fails closed until the real key lands; (c) treat the dev key as
workstation-only and re-bake before any Release leaves the building.
Also note the behavioral corollary: mods signed with the old test key no
longer verify (they now fail closed as Invalid) — expected, dev-only.

**LOW (documentation drift):** the header block in mod_signature.h
(lines 12, 31, 42-43) still says "THROWAWAY TEST KEY… every mod signed
with the test key verifies," which is no longer true of the dev key now
baked in; and the trailing comment's "is still rejected by
bakedKeyIsTestKey()" misdescribes the mechanism (the function detects
the *test* key; the old key's signatures are rejected because the baked
key changed). Worth a comment refresh whenever JD rules on the above.

---

## 3. Sim/chunk (0825) — holdBetweenTicks + postLock

**VERDICT: sound-with-notes.**

What the diff shows (sim_thread.h +11 lines, sim_thread.cpp +19/−0
across three hunks — the whole change):

- `holdBetweenTicks()` (sim_thread.cpp, new at ~line 209): acquires
  `blockMutex_` into a caller-owned `unique_lock`, then — under the lock —
  refuses (returns false, lock released by RAII, caller's `hold`
  untouched) if the handshake is frozen or has workers outstanding;
  otherwise moves the lock out. Header contract (sim_thread.h:145-149)
  matches the implementation, including the documented fail-closed
  fallback to `stop()`.
- The post-tick section of the sim loop (flushDirty snapshots, publish
  events, stats, HUD inspect) now runs under a fresh
  `postLock(blockMutex_)`, explicitly unlocked before the tail wait
  (sim_thread.cpp:~636, ~707). Previously this section ran after the
  tick scope had released the mutex — i.e., it read `chunks_` unlocked,
  which is precisely the race CHUNK-STALL set out to close.

Deadlock/starvation analysis:
- The safety argument for the freeze check is sound: freezes are raised
  on the sim thread inside a tick, and while the caller holds
  `blockMutex_` the sim thread cannot be inside a tick, so the
  frozen/workers state observed under the lock cannot change under the
  caller. A still-running pull worker forces the false path
  (fail-closed). 
- There is a deliberate mutex-release/re-acquire gap between the tick
  scope and `postLock`. It is benign: the sim thread touches `chunks_`
  only while holding `blockMutex_` in either section, so a stream
  insert/erase that lands in the gap is fully serialized before the
  post-tick section starts.
- The post section's blocking surface is bounded (snapshot + remesh
  submit, stats publish, at most ~120 flushes/s), and the tail wait
  runs unlocked, so a `holdBetweenTicks()` caller waits at most one
  post-tick section — no starvation pattern, and no lock is held across
  the sleep. `blockMutex_` is recursive, so game-side re-entry via
  `runBetweenTicks` composes.

Notes for Cortana:
- **(MEDIUM, contract)** The post-tick section now calls game-implemented
  `IWorldSimulation` methods (`inspect()`, `activeCount()`) *while
  holding `blockMutex_`*, where before it called them unlocked. The
  engine-side invariant is fine, but it sharpens the contract on the
  game side: nothing reachable from those callbacks may block on a lock
  the game thread holds while calling `holdBetweenTicks()` (the stream
  insert/evict path), or the two threads can invert. R2 is reviewing
  game.cpp; flagging the contract here so the note reaches her either
  way.
- **(INFO)** holdBetweenTicks wait time now includes a full post-tick
  flush when it lands behind one — the right trade for correctness
  (it replaces a full stop()/start()), just observable in stream
  latency under heavy remesh load.

### SIM-H1 revert audit

**VERDICT: the shipped file is clean; the audit trail is incomplete in
the package (see §0).**

- The complete H-vs-P diff of sim_thread.cpp is the three hunks above:
  the marker comment, `holdBetweenTicks()`, and the postLock pair. There
  is **no residue** of the reverted approach — no unlock/yield inside
  the catch-up loop (`while (acc_ >= stepDur_ && steps < 16)` is
  untouched; the only `yield` in the file is the pre-existing pacing
  spin helper, also present in P).
- The `.reverted-SIM-H1` file itself was not shipped (§0), so the
  "differs from the bad copy in exactly the reverted region" check
  cannot be executed from the package. Indirect corroboration is
  consistent: the shipped full ctest log (`ctest-2026-10-15-0511.log`)
  shows `engine_sim` **Passed 3.11 sec** in the shipped state, versus
  the 900 s timeout reported for the reverted-out attempt.
- Hazard question (would the .reverted file compile if present?): the
  sim module's CMakeLists uses an **explicit source list**
  (`add_library(litholattice_sim STATIC sim_thread.cpp …)`), and a grep
  of all engine CMakeLists finds `file(GLOB …)` only for shader files
  in render. So even on the workstation, `sim_thread.cpp.reverted-…`
  and `*.orig-*` siblings are inert — they cannot be picked up by the
  build. Residual risk is human, not mechanical (someone compiling the
  wrong file by hand).

---

## 4. Console / MCP (0928–1020)

### 4.1 concmd.cpp dangling-pointer fix (CONSOLE, 0928)

**VERDICT: sound and complete.** The bug: `std::strtol(std::string(raw)
.c_str(), &end, 10)` left `end` pointing into a temporary destroyed at
the end of the full expression; the `*end != '\0'` validation then read
through a dangling pointer. The fix binds `const std::string tmp(raw)`
before the call at **both** sites (Int ~line 363, Float ~line 374), so
`end` stays valid through the checks. Completeness verified by grep:
the remaining `strtol`/`strtod` sites in concmd.cpp (lines 758, 817,
1243-1253) all parse from named, living strings (`s`, map values,
`argv[i]`) — no other temporary-`c_str()` site exists in the file.

### 4.2 iworldquery.h — queryCellsSaturated + kMaxQueryCells (MCP-H1 / CON-H2)

**VERDICT: sound-with-notes.**

- The function saturates at `kMaxQueryCells + 1` (= 4194305), so callers'
  existing `cells > kMaxQueryCells` tests trip with the same semantics
  as an exact count, and exact counts are returned whenever the true
  count is ≤ the cap.
- Overflow math: each span is pre-checked against the cap before any
  multiplication; after the early return both multiplicands are ≤ 2^22,
  so the products are ≤ 2^44 — the old wrap (three ~2^32 spans
  overflowing uint64 to a small, cap-passing number) is genuinely
  closed. concmd.cpp's query guard now calls it (replacing the
  hand-rolled product), and server.cpp's MCP guard calls the same
  function — one implementation, both surfaces.
- **Note (LOW):** `span()` computes `b - a` in **int64_t** before the
  uint64 cast, so the helper's contract silently assumes |a−b| < 2^63.
  That holds at every current call site — concmd parses int-range
  coordinates, and the MCP handler rejects out-of-int32 coordinates
  *before* calling (server.cpp, world_query_blocks arg validation:
  `v < INT32_MIN || v > INT32_MAX` throws) — but a future caller feeding
  raw int64 extremes would hit signed-overflow UB inside the guard
  itself. Worth one comment line on the function stating the int32-range
  precondition its callers enforce.

### 4.3 mcp/server.cpp — cap + _read(0) (MCP-H1 / MCP-H2)

**VERDICT: sound.** The saturated cap replaces the local span lambda
(same call site, same error path). The Windows stdio change
(server.cpp ~1914): under `_WIN32`, `_read(0, chunk, 8192)` returns
whatever the pipe currently holds instead of `std::cin.read` blocking
until the buffer fills — the stated bug (short JSON-RPC requests never
answered on Windows) is real for iostream reads on pipes, and the fix
mirrors the POSIX loop's structure, including the `!lineDone` /
EOF-break conditions. CRLF is safe either way: the line assembler
strips a trailing '\r' (server.cpp:1935), so text-mode translation
differences can't corrupt a request. The POSIX branch is untouched.

### 4..4 mcp/sockets.cpp — SO_EXCLUSIVEADDRUSE (MCP-H3)

**VERDICT: sound.** In the Windows bind path, `SO_REUSEADDR` (which on
Windows permits a second process to bind the same port and steal
connections) is replaced by `SO_EXCLUSIVEADDRUSE`, set on the socket
before `bind()` — the only ordering that takes effect. Restart behavior
is preserved (the option blocks *concurrent* binds, not rebind after
close), and failure now fails closed at bind time instead of silently
sharing the port. POSIX path untouched.

---

## 5. Storage (0452 / 1020) — WORLD PERMANENCE surface

### 5.1 anvil_region.cpp — SAV-H1 copy-on-write + sidecar .tmp→rename

**VERDICT: sound-with-notes. This is a strict permanence improvement:
after this change there is no crash point that leaves torn chunk data
behind an index entry.**

Ordering audit (stage() / commit() read in full):
- **Inline path:** stage() allocates a *free* run (`allocRun` consults
  the bitmap, in which the old run is still marked used), writes the
  record, and `flushFile()`s it — payload durable **before** any index
  write. commit() re-reads and `parseRecord`-verifies the staged bytes
  (codec + payload length must match), then writes the timestamp entry,
  then the location entry **last** (the documented single commit
  point), flushes again, and only then publishes in memory and
  `freeRun()`s the old run. Crash anywhere before the location write:
  the index still points at the old, intact record — the write is lost,
  never torn. The free-list is in-memory and rebuilt from the index on
  open (`rebuildBitmap`), so a crashed attempt leaks nothing on disk.
- **Sidecar path:** stage() writes `<name>.tmp` with fwrite + syncFile
  + fclose. commit() verifies the staged copy by reading it back, then
  `std::filesystem::rename(tmp, live)` — same directory, same volume —
  and only **after** the rename writes the index (timestamp, then
  location last) and flushes. Ordering (payload → verify → rename →
  index) is correct: the index never references a sidecar whose bytes
  were not fully written, synced, and validated.
- Rename semantics: POSIX rename(2) is an atomic replace. On Windows,
  `std::filesystem::rename` resolves to a replace-existing move on the
  same volume; if the live sidecar is momentarily open elsewhere and
  the replace is refused, commit returns false **before** touching the
  index — fail-closed, old state intact, error reported. Good.
- **External→inline transition in commit()** removes the orphaned live
  sidecar after the index has flipped ✓ (and inline→external retires
  the old run via the same freeRun path).

Notes for Cortana:
- **(LOW, semantic asymmetry)** The two paths fail opposite ways on a
  crash in the rename→index window: inline keeps the *old* record
  (index untouched), but external has already replaced the live
  sidecar, so a reopen serves the **new** payload under the **old**
  generation number (readExternal labels the record with the index's
  gen; the sidecar format embeds no generation of its own). The data
  served is always a complete, verified record — never corruption —
  but a consumer comparing generations to detect changes would miss
  this one. If generation fidelity ever becomes load-bearing, embed
  the gen in the sidecar record or rename after the index write with
  a recovery pass; today it is safe.
- **(LOW, regression trade, deliberate)** Always-CoW means a rewrite
  that fits the old run now fails if the region file has no free run,
  where the old in-place path would have succeeded. That is the
  standard price of crash safety and the right call under the
  permanence doctrine — flagging it only so the behavior change is on
  the record. (The now-unreachable "in-place shrink" freeRun branch in
  commit() is harmless vestige.)
- **(INFO)** A staged `.tmp` orphaned by a crash between stage() and
  commit() lingers until the next write to that chunk overwrites it;
  erase() removes the live sidecar but not a stale `.tmp`. Cosmetic.

### 5.2 sandboxed_io.cpp — PLT-H1 Windows reparse-safe open

**VERDICT: sound-with-notes.**

- Gate A: `CreateFileW(..., FILE_FLAG_OPEN_REPARSE_POINT)` opens the
  final component itself rather than following it; the
  `BY_HANDLE_FILE_INFORMATION` check then refuses reparse points and
  directories, and `GetFileType == FILE_TYPE_DISK` excludes devices
  and pipes — parity with the Linux O_NOFOLLOW + fstat gates.
- Gate C is the anti-TOCTOU core and it is done right: the containment
  check runs on `GetFinalPathNameByHandleW` of the **already-open
  handle**, so no path swap after the check can redirect the read.
  Intermediate-component junctions/symlinks are resolved into the
  final path and caught by the same check if they escape.
  `FILE_SHARE_READ` (no write/delete sharing) prevents the opened file
  being replaced mid-read. Handle→CRT-fd ownership transfer
  (`_open_osfhandle` / `_fdopen`, with correct close-on-failure at
  each step) is watertight.
- The `\\?\` and `\\?\UNC\` prefix normalization before the check is
  correct (substr counts verified: 4 and 8).
- `ModSandbox::withinRoot` (mod_sandbox.h) is component-boundary
  aware — the `/game/mod` vs `/game/modevil` sibling-prefix escape is
  explicitly handled — and the final-path lookup returns long names,
  which neutralizes 8.3 short-path aliases in the checked path.

Notes:
- **(LOW)** The containment compare is a byte-wise, case-sensitive
  string compare of `path.string()`. On Windows, if the sandbox root's
  stored case (or an 8.3 component in the root itself) differs from
  the on-disk canonical form returned for the handle, `contains()`
  *refuses* — fail-closed, so the failure mode is a spurious
  "escaped the sandbox" error on a legitimate file, never an escape.
  A support papercut, not a hole.
- **(INFO)** Hardlinks are the known blind spot of any final-path
  check (a hardlink inside the sandbox whose other name lives outside
  reports the in-sandbox name). The Linux gate has the identical
  property; accepted threat model, noting for parity.

---

## 6. Small PORTs

- **struct_registry_selftest.cpp (0450)** — **sound.** `MaterialId{10+i}`
  → `static_cast<MaterialId>(10 + i)`; values 10–12 unchanged, test-only.
- **engine_tests.cpp (0455)** — **sound.** GNU zero-length array
  `empty[] = {}` → `empty[1] = {0}`; the array is only ever used as
  `span(empty, 0)`, whose length is 0, so contents are never read.
- **assets_selftest.cpp (1050)** — **sound-with-note.** On `_WIN32`, a
  symlink-fixture failure with error 1314 (privilege not held) prints
  `[SKIP]` and skips both the fixture check and the dependent symlink
  checks, via a correctly-formed `#ifdef`/`else` chain: any *other*
  error still falls into `check(!lec, …)` and fails, and non-Windows
  behavior is byte-identical. **Note (INFO):** on an unprivileged
  Windows account the symlink-signature coverage now silently doesn't
  run — CI machines need Developer Mode/admin for that coverage to
  exist at all. (Before this change it hard-failed, so this is
  strictly better; just don't mistake a green Windows run for
  symlink coverage.)
- **input_selftest.cpp (1145)** — **sound.** The fixed 2.5 s sleep is
  replaced by a poll on `pump.dropped()` (an `std::atomic<uint64_t>`
  load — input_thread.h:88,109 — so the cross-thread poll is
  well-defined) until the overrun threshold or a 10 s deadline; the
  assertions after the wait are untouched, exactly as CHANGES claims
  ("Same assertions"). Flakiness strictly decreases (success path
  exits as soon as the condition holds); the only cost is a broken
  pump now takes 10 s instead of 2.5 s to fail.

---

## 7. Items in scope with no changes found

- `http_server.cpp` (0449, windows.h before bcrypt.h) is listed in
  CHANGES and differs H-vs-P; it sits outside R1's assigned items but
  the diff is a pure include-order change, consistent with its row.

## 8. Completeness cross-check (CHANGES.md vs recovered diffs)

Every engine file that differs between H and P is named in CHANGES.md,
and every CHANGES engine row corresponds to a real diff (residuals from
the automated comparison are all artifacts of CHANGES' combined rows,
e.g. "sha512.h, sha512.cpp, ed25519.cpp" and "glfw_input_backend.h/.cpp").
The only genuinely new engine file is `u128.h`, as ledgered. The two
rows naming files *outside* the differing set are accurate as written
(`engine/tools/run-bench-suite.ps1` — explicitly "left unchanged";
`engine/LithoKraft` — the symlink fallback is in CMake, and the symlink
itself ships as a stored entry). **No unlisted engine-core changes
were found.** The one ledger gap is §0: the artifacts CHANGES
*references* (.orig backups, the .reverted file, the 0841 design docs)
were not shipped in the package.

## Verdict summary

| Item | Verdict |
|---|---|
| 1. Crypto PORT (u128/sha512/ed25519/signing guard/selftest) | **sound** |
| 2. SIGNING dev key | **sound as implemented; HIGH decision for JD** (release boot guard disarmed by design of the check; disclosed in her note) |
| 3. holdBetweenTicks / postLock | **sound-with-notes** (callback-under-lock contract for R2's side) |
| 3b. SIM-H1 revert | **shipped file clean**; audit trail incomplete in package (§0, medium) |
| 4. concmd dangling ptr | **sound, complete** |
| 4b. queryCellsSaturated | **sound-with-notes** (int32-range precondition worth a comment) |
| 4c. server _read(0) / sockets exclusive-bind | **sound** |
| 5. SAV-H1 CoW + sidecar rename | **sound-with-notes** (gen-label asymmetry on one crash window; always-CoW space trade) |
| 5b. PLT-H1 sandbox open | **sound-with-notes** (fail-closed case/8.3 papercut; hardlink blind spot = parity) |
| 6. Four small PORTs | **sound** (assets [SKIP]: coverage note) |


---

# Appendix B — Render + game

# Review R2 — Render + Game changes, Cortana handoff 2026-10-15-1222

**Reviewer:** Elfie (second-model review, JD's order 2026-10-15 12:25 EDT). **REVIEW ONLY — nothing modified.**
**Method:** every changed file in scope was diffed against two baselines: (a) the exact package Cortana received (`LithoLattice-LithoKraft-Full-Source-Package-2026-10-15.zip`, extracted as the baseline tree) and (b) Elfie's production tree `~/workspace/litholattice/`. The `<file>.orig-2026-10-15-HHMM` sibling backups named in CHANGES.md are **not in the handoff package** (verified: no `.orig-*` or `.reverted-*` files anywhere in H) — the diffs above stand in for them, and they line up with CHANGES.md's timestamps and finding IDs throughout.
**Ledger read:** H/CHANGES.md, H/Elfie-Handoff-2026-10-15-1222.md, H/Handshake-Meshing-Impl-2026-10-15-1222.md.

**Headline:** the render work is the real thing — the perspective-correctness claim survives a line-by-line math audit, the band pool's completion guarantee is actually implemented (not just asserted), and the 1213 alt-tab click fix addresses the true root cause with no new dead-click path found. Two things need Cortana's/JD's attention before merge: H's `config.lth` is a session artifact that silently re-enables the dither JD just turned off (§6.2), and the portability sweep missed biome_capture's twin tool (§7).

---

## 1. Software renderer (1010–1116)

### 1.1 RND-H3 — one divide per triangle, incremental edges, perspective-correct varyings — **SOUND**
`software_renderer.cpp` (H) rasterizeTriangle / rasterizeTriangleShaded: barycentrics are seeded per row with a single `edgeFn` evaluation at the row's first pixel, then stepped by constant `dw*dx` increments; depth steps by `dzdx`. The math audit the finding turns on:

- Screen-space barycentrics λᵢ are affine in (x, y), so incremental stepping is exact in real arithmetic — the scheme does not approximate them.
- The shaded path forms `pw_i = λ_i · (1/w_i)`, `viewDepth = 1/Σ pw`, `w_i = pw_i · viewDepth`, i.e. **w_i = (λᵢ/wᵢ) / Σ(λⱼ/wⱼ)** — the textbook perspective-correct weights — and interpolates normals, UVs and world position with them (H software_renderer.cpp:378-398). Perspective correctness is genuinely preserved, not just claimed. The reciprocal is computed once per depth-passing pixel, after the depth test.
- Depth interpolates NDC z with the screen-affine λ — z_ndc is affine in screen space, so this stays exact, as before.
- Note (not a defect): within a row, λ accumulates by repeated float addition instead of a fresh edge evaluation, so coverage at a triangle edge can differ from the old code by ~1 ulp on very long rows. Rows re-seed every scanline, bounding the drift. The handoff's pixel-identity evidence (SHA-256 over 7 biome captures after each step) is consistent with this; identity is empirical over those captures, not a proof for all scenes — acceptable, and the captures are the right canary. (The 7 PNGs themselves are not in H, so the hashes could not be re-run here.)

### 1.2 RND-H2 — near-plane clipping at w ≥ 1/128 — **SOUND WITH NOTES**
renderMesh's per-pixel path: quads failing `quadVisible` (any clip w ≤ 0) now go through a single-plane Sutherland–Hodgman clip in homogeneous space against `w >= 1/128` (H software_renderer.cpp:518-579). Read in full:

- Edge iteration emits `a` when inside plus the crossing point — correct S–H form, winding preserved; `t = (kNearW − a.w)/(b.w − a.w)` is only evaluated when the endpoints straddle, so no divide-by-zero.
- A convex quad clipped by one half-plane yields ≤ 5 vertices; the `out[8]` buffer is ample. The fan triangulation passes matching varyings pointers.
- Varyings are lerped in clip space (exact for vertex attributes along an edge); flat fields (layer, materialId) are taken from edge start `a` — reasonable for flat-shaded fields.
- Notes: (a) the clip engages only when some w ≤ 0; a quad with all w in (0, 1/128) still takes the fast path with very large screen coordinates — identical to baseline behaviour, not a regression. (b) The flat path (`renderFlatQuads`, sprites/flat shading) still rejects crossing quads outright — the "walls beside the camera no longer vanish" fix covers the per-pixel path, which is the shipping world path; worth one line in the ledger so nobody expects the sprite path to clip too.

### 1.3 RND-H4 — front-to-back draw + drawOrder_ — **SOUND WITH NOTES**
Chunk pass (H software_renderer.cpp:1245-1260): culling runs first (`cullProto_.decide`), survivors enter `drawOrder_` keyed by squared distance to the sub-chunk **centre**, `std::stable_sort` ascending. `stats_.drawn` is now `+= drawOrder_.size()` after the pool run — post-cull, matching the GPU backends, as the in-code comment says.

- Order-invariance: the chunk pass is depth-tested opaque (blended sprites live in a separate later pass with its own back-to-front producer order, untouched), so draw order cannot change the image except at **exact depth ties** (coplanar faces), where first-drawn wins. Stable sort keeps tie behaviour deterministic frame-to-frame. The biome-capture SHA evidence covers the shipped scenes.
- Note: distance is to the mesh centre, so a long diagonal view can still shade some hidden pixels of a "near-centre" mesh before a "far-centre" one — an efficiency quirk, not a correctness one. The win is real (her table: render 87.9 → 23.0 ms across the batch at 720p).

### 1.4 RND-MT band pool — **SOUND WITH NOTES** (race audit)
`SwBandPool` (H software_renderer.cpp:120-192) + the two `run()` call sites (transform-once at :1300, strip draw at :1351):

- **Completion guarantee — verified in code.** `run()` holds `runM_` for the whole call, publishes `fn_` + `remaining_ = bands_ − 1` under `m_`, runs band 0 itself, then waits on `done_` until every worker has decremented `remaining_` to 0. It cannot return with strips outstanding, so post passes, `grabFramebuffer` and MCP vision captures always see a resolved frame — the handoff's claim (§8) is accurate.
- **MCP serialization — verified.** The pool is a process-wide singleton shared by all SoftwareRenderer instances; `runM_` serializes whole frames between the game view and MCP capture renderers. The two per-frame runs (transform, then draw) are separately atomic; another renderer's frame could interleave *between* them, but per-instance state (`frameScreen_`, `drawOrder_`) makes that a scheduling quirk, not a data hazard.
- **Shared-state inventory.** fb pixels/depth: band threads write disjoint 32-row strips; both rasterizers clamp rows to `[tlBandY0, tlBandY1)` (H :259-261, :344-346), and strips are handed out by an atomic counter so no two threads share a strip. `frameScreen_` ranges are disjoint per mesh index in the transform split. `groundFx_` / `meshOrigin*` are `static thread_local` and are loaded per mesh inside the band loop (H :1329-1336) and reset at band end. `stats_` writes in the chunk path moved out of the loop (post-run add, caller thread); the remaining `++stats_.drawn` sites (:1080, :1362) are in serial passes. `tlScreen`/`tlPreScreen` are thread_local and the pre-transformed pointer is consumed-then-cleared per mesh (H :465-468, :1341-1352). No unsynchronized shared writes found.
- **Notes:** (a) if a band function ever threw, `remaining_` would never reach 0 and `run()` would hang — the raster code is plain arithmetic with no throwing paths today, but the pool has no exception guard; a one-line comment saying "fn must not throw" would armour it. (b) A nested `run()` (a renderer invoked from inside a band function) would deadlock on the non-recursive `runM_` — no current path does this; worth the same comment. (c) Thread-count changes apply between frames inside `run()` (stopAll → start), exactly as the header comment promises.

### 1.5 CMakeLists AVX2 scoping — **SOUND**
`engine/src/render/CMakeLists.txt` (1105): the option `LL_SW_AVX2` (default ON) applies `COMPILE_OPTIONS` to `software_renderer.cpp` only, guarded by `CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64|amd64)$"` plus an arm64-OSX exclusion. Opt-out works (`-DLL_SW_AVX2=OFF`). The FMA claim checks out: GCC/Clang get `-mavx2` without `-mfma` (FMA is a separate flag), and MSVC gets `/arch:AVX2` with no `/fp:fast`, so no contraction is introduced on either toolchain and float results stay aligned with the SSE2 build. Sim/circuit code is untouched by the flag — the hash-safety reasoning is sound.

---

## 2. Mesher RND-H1 (1020) + llCtz

### 2.1 mesher.cpp per-job thread_local def table — **SOUND WITH NOTES**
H mesher.cpp:111-131: `jobGen`/`jobSlot` (each sized 1<<16 — `BlockId` is `uint16_t` per engine/src/world block.h, so the tables exactly cover the id space) plus a per-thread `jobDefs` vector; each id's `BlockDef` is fetched once per pass under the registry lock, then read lock-free.

- **Consistency:** this is strictly *more* consistent than baseline — baseline re-read `reg.def()` per voxel and could mix pre/post-hot-swap definitions inside one mesh; the pass now meshes from one snapshot, and the pre-existing epoch guard still forces a re-mesh after a swap. The "stale defs after content reload" worry resolves in the design's favour.
- **Lifetime — the one sharp edge.** `jobDef()` returns `const BlockDef&` into a vector it can grow via `push_back` (reallocation). The per-cell reference at H mesher.cpp:145 (`const BlockDef& def = jobDef(b)`) stays valid **only because the current cell body never calls `jobDef()` again while `def` is live** — I read the whole cell body (attachable/custom/cross/cube paths, face loop): all further definition access goes through `reg`/`mats`/`look`, never `jobDef`. The inline comment says exactly this. It is correct today and fragile tomorrow: the first future edit that fetches a neighbour def via `jobDef` mid-cell silently dangles. **Note for Cortana:** consider `jobDefs.reserve(1 << 12)` head-room plus a "do not call jobDef while holding a def reference" comment at the lambda, or return the hot fields by value. Not a merge blocker.
- `gen` wraparound is handled (reset-and-fill at 2³²); the pad loop reads `.flags` by value immediately — safe.

### 2.2 binary_greedy_mesher.cpp llCtz — **SOUND**
The MSVC path uses `_BitScanForward/64`, which leaves the output undefined at input 0 — the same contract as `__builtin_ctz(0)`. Both call sites were read: `llCtz32(row)` sits inside `while (row)` (H :106) and `llCtz64(word)` inside `while (word)` (H :152), so the argument is non-zero by construction and the intrinsic's result is always written before use. Behaviour matches the GCC path at every reachable input. (A defensive `if (!v) return 32/64;` would cost nothing, but as written it is correct.)

---

## 3. GAME-H1 — entity origin, game side + GL — **SOUND**
game.cpp uploadEntities (H :2467-2496, :2543-2545): origin is the player position floored to a 16-block boundary minus 128 per axis; `enc()` rounds `(v − o) · 16` and clamps to [0, 65535].

- **Range/precision audit:** offsets are 1/16-block units (6.25 cm — the chunk encoding's own precision). An entity at the player sits at 2048 units; the 64-block spawn/query sphere lands in ≈ [1024, 3072], as the comment says. The window reaches ~3,968 blocks above the player before the clamp engages, and the clamp saturates instead of wrapping — so even a pathological far entity degrades to "drawn at the boundary," never to the pre-fix behaviour (whole-block values decoded as 1/16 → entities at 1/16 position; negative coordinates cast to uint16 = UB). The fix is real and the failure mode it removes was severe.
- Origin moves in 16-block steps as the player crosses boundaries; entities re-encode every upload, so there's no swimming within a step and no stale-origin frame (origin is set on the renderer before encoding, same function).
- GL side (gl33_renderer.cpp:442, :592, :830): `setEntityOrigin` stores the batch origin and the entity pass now passes it as `uOffset` instead of the hard-coded 0 — consistent with the software side's `emvp = viewProj · translation(entityOrigin)` (software_renderer.cpp:1357-1361). The virtual's default no-op in renderer.h is honest: Vulkan draws no entities yet (already on Cortana's pending list in the handoff note).
- Style wart, not a defect: `entityOrigin_` is a **public** member in software_renderer.h (:91-92) amid an otherwise encapsulated class; the GL backend keeps its copy private. Worth tidying in a later pass.

---

## 4. Focus / alt-tab set (0935–1218) + the 1213 click bug — **SOUND**

**Machinery.** window.h/.cpp: `cursorReallyCaptured()` reads GLFW's actual cursor mode (not the cached flag); `takeFocusEdges()` forwards to the input backend's counters, which the GLFW focus callback increments (glfw_input_backend.cpp:124) — callbacks fire on the polling (main) thread, so the plain-int counters don't race. `raiseAndFocus()` shows + focuses, with a GLFW_FLOATING toggle as the Windows foreground-lock fallback; `setTopmost(bool)` maps to GLFW_FLOATING and no-ops when unchanged. main.cpp: `raiseAndFocus()` at first show (:843); per frame, edges are consumed and combined with the attribute poll (`lostN > 0 || (wasFocused && !foc)` → one `onFocusChanged(false)`; symmetric gain path), `wasFocused` is maintained (:907). Capture is applied only when `wantCap = wantsCursorCaptured() && window->focused()`, compared against `cursorReallyCaptured()`, and topmost is set to `wantCap && cursorReallyCaptured()` every frame — the JD-specified order focus → capture → topmost is what the code does, and topmost drops automatically when capture/focus goes away. ✔ matches the LAUNCH-FOCUS description.

**The 1213 ALT-TAB CLICK BUG — root cause confirmed, fix verified end-to-end.** Baseline `routeUiInput` had `pointerToUi = !captured_ || inventoryOpen_ || editorOpen()`; a focus pause left `captured_ == true`, so every pointer event was skipped — pause-menu buttons dead, resume click unseen. The three fix elements are all present and interlocking:

1. `pointerToUi` now includes `paused_` (game.cpp:1561-1565) — and since `paused_` mirrors `pauseState_ != Playing`, UI pointer routing also works in TickFrozen/HsFrozen, where `onFocusChanged` deliberately doesn't clear `captured_`.
2. Focus loss sets `captured_ = false` (game.cpp:1374-1379), guarded by `pauseState_ == Playing` so it can't stomp an existing menu/frozen state; it also sets `pausedByFocus_`.
3. An unconsumed PointerDown sets `freePointerDown_` (reset at the top of every `routeUiInput`, consumed the same frame in `update()`); the resume block (game.cpp:1790-1799) fires only when `pausedByFocus_ && !uiClick && menuState_ == Playing`, then sets `uiClick = true` so the resuming click can't also break a block.

**Dead-click hunt (no new path found):** pause-menu buttons now receive events while paused (consumed → `uiClick`, resume-block correctly stands down); a click on a modal backdrop is consumed and doesn't resume; Esc runs *before* the resume block and the new Esc toggle (`setPaused(pauseState_ != MenuPaused)`, game.cpp:1313) means Esc-resume clears `pausedByFocus_` via the housekeeping line at :1792 (the flag only survives while state is MenuPaused, which is exactly the focus-pause signature — that line looks suspicious on first read and is in fact the correct invariant). Focus loss with inventory/editor open still doesn't pause (pre-existing guard, unchanged). Quick away-and-back within one frame fires pause-then-gain in order; the game stays paused with a live menu — the designed behaviour.
- Minor note: the resume block's second trigger, `input_.devices().mousePressed(Left)`, reads the raw device rather than the routed event stream, so *holding* LMB through an alt-tab back could resume without a fresh click. In context (the user is actively returning to the window) this reads as intent, not a bug — flagging only so it's a known behaviour.

**Esc interplay:** Esc while MenuPaused resumes (new); Esc while Playing pauses; Esc in frozen states routes to MenuPaused exactly as baseline's unconditional `setPaused(true)` did. No regression found.

---

## 5. CHUNK-STALL, game side (0825) — **SOUND WITH NOTES**
Insert pump (game.cpp:929-994) and evict pump (:1047-1092) now call `sim_->holdBetweenTicks(simHold)` first; the engine side (sim_thread.cpp:209-221) takes `blockMutex_` and refuses — returning false with nothing held — when the handshake is frozen or has workers outstanding, and the post-tick section now also runs under `blockMutex_` (sim_thread.cpp:636-639, :707), which is what makes the hold meaningful.

- **Contract check, precisely:** on the normal path the sim is never stopped/started for streaming — the hitch source is gone. But stop()/start() has **not** been removed from the stream path: it remains as a deliberate fail-closed fallback, gated on `holdBetweenTicks() == false` (game.cpp:935, :1054 area), with `applySimPause()` re-applied only when the sim was actually stopped. Any summary that says "no stop/start remains in the stream path" would overstate it; CHANGES.md's own wording ("instead of stopping and restarting it") is accurate for the path that runs in practice. Grep confirms the only other `sim_->stop()/start()` sites in game.cpp are world load/teardown (:824-877), not streaming.
- The sequencing detail at :994 — releasing `simHold` before `evictFarColumns` because eviction's own fallback may `stop()` — is correct and subtle; holding a recursive lock across a stop() would have risked exactly the failure the fallback exists to avoid.
- **Pause interplay:** a held (not stopped) sim keeps its pause computation, so `applySimPause()` is rightly skipped on the hold path; a paused sim's between-ticks hold still succeeds because the mutex is free between iterations. No interplay defect found.

---

## 6. Settings & defaults

### 6.1 displayMode default 2 (Borderless) — **SOUND**
Verified in both places: settings.h:43 (`int displayMode = 2;`) and H config/settings.json:5 (`"displayMode": 2`). `--display` still overrides per the handoff (main.cpp parsing unchanged in the diff).

### 6.2 modelDither default false (1215) — **SOUND IN CODE, CONCERN IN THE SHIPPED TREE STATE**
The single-source claim is structurally true: `render.model_dither` is a settings-bridge cvar whose getter/setter read/write `GameSettings::modelDither` (settings_cvars.cpp:157-163), the renderer is driven from that same bool (game.cpp:526), settings.h:96 now defaults it to `false`, and settings.cpp doesn't serialize it separately. There is no second lurking default in code.

**But:** H's `game/LithoKraft/config/config.lth` contains `render.model_dither "1"` (see §9). config.lth is the archived-cvar file executed at startup (game.cpp:139-150), and archived cvars flow through the bridge into settings — so launching from H's tree state turns dither back **on**, defeating the 1215 ruling until the file is regenerated. The line is a leftover from a pre-1215 workstation run (the dtor's `saveArchived` wrote the then-live value). **For the merge:** do not carry H's config.lth over production's (production's copy has no dither line, so the new default governs). Recommend Cortana note config.lth/settings.json as runtime state, not source, in future handoffs — and that the dither-off playtest verification be re-run with a clean config.lth present.

### 6.3 TPS ladder F7 32..512 (1218) — **SOUND**
game.cpp:1115-1139 now steps `kSettingsTpsModes` (32/64/128/256/512, wrapping) instead of the 4-entry `kTickModes`; controls_panel.cpp's label updated to match ("TPS 32/64/128/256/512"). The engine's `kTickModes[] = {32, 64, 128, 256}` in sim_thread.h is **byte-identical** in B and H (only line numbers shifted by the CHUNK-STALL comment) — the claim "engine kTickModes untouched, hash safe" is verified. There is indeed no EUV gate in the F7 path.

### 6.4 renderThreads + `--render-threads` (1156) — **SOUND**
- Engine: `swResolveThreads` (software_renderer.cpp:105-113) rounds the request **down** to a power of two, never exceeds `hardware_concurrency()`, caps at 64, floors at 1; 0 means the default 4. Reconfiguration happens inside `run()` under the run mutex — between frames, as documented.
- Precedence: main.cpp:659-662 — `setRenderThreads(renderThreadsArg > 0 ? renderThreadsArg : settings.renderThreads)`; the CLI parse clamps to 1..64 (:516). **CLI wins — verified.** settings.cpp clamps the key to 0..64 on load and saves it (:48, :73); a non-pow2 settings value (e.g. 24) is tolerated because the engine rounds down.
- Doc nit for Cortana: handoff note §8 still describes the pool as "largest power of two ≤ 8 and ≤ hardware threads − 4" — that's the superseded 1100 sizing. The shipped 1156 code resolves against full hardware_concurrency with default 4. The note's later "Display default and render threads" section has it right; §8's sentence is stale.

### 6.5 The 8→4 band-pool default — the trade JD accepted, factually
From the handoff's own table B (1080p borderless, orbit, 512 TPS): **29.4 FPS / 31.9 ms render at 8 threads vs 19.4 FPS / 49.5 ms at 4** — the shipped default costs ≈ 34% of frame rate (render time +55%) on the i5-12450H (4P+4E, 12 threads), leaving the 30 FPS target unmet at 1080p on this machine, in exchange for reserving the other hardware threads for the 512 TPS sim, handshake workers and mesher, and for headroom on the entry/older consumer hardware the code comment cites (software_renderer.cpp:101-104). At 720p the 8-thread build met the target (35.5-40.7 FPS); no 720p 4-thread figure was published. JD's playtest felt the trade directly (11-15 FPS ground-level at 4 threads, 1,731 meshes) and the handoff's own remedy stands: `--render-threads 8` ≈ 1.5×. The default is a values choice (sim headroom + weak-hardware safety over dev-laptop FPS), fully recoverable per-run via CLI or the settings key — characterized, not second-guessed.

---

## 7. Portability leftovers — **CONCERN (minor, dev-tools only)**
biome_capture.cpp's `sourceRoot()` (0750) is a good fix: existence-probed seed order (legacy path → `LITHO_SOURCE_ROOT` → walk up from cwd and exe dir), cached, with the old path as final fallback. The two UTF-16 smoke logs in H (05:43/05:57, pre-fix) show exactly the failure it addresses. But the sweep is incomplete:

- **`game/LithoKraft/src/app/interchange_capture.cpp:126,177,178` still hardcodes the same three `/home/hatch/...` paths** biome_capture had — the twin capture tool was missed. It will fail on Windows the same way biome_capture did.
- `game/LithoKraft/src/game/sprite_host_selftest.cpp:52` returns `/home/hatch/workspace/user/files` and `engine/src/procgen/litho_stamp_feature_selftest.cpp:106` hardcodes the wafer.json path — both pre-existing in the shipped baseline (not Cortana regressions); the latter matches her noted `engine_litho_formula` environment failure. Candidates for the same skip-if-missing / sourceRoot treatment.
- biome_capture itself retains the `/home/hatch` literal as first-try seed and last-resort fallback (biome_capture.cpp:77, :88) — behaviour-correct on both platforms (existence-probed), noted only so the string's survival is a decision, not an oversight.
- getpid guards (selftest.cpp, game_ui_selftest.cpp, input_selftest.cpp) are the identical, correct `_WIN32 → <process.h> / _getpid` pattern in all three files. screenshot_selftest's `weakly_canonical` root (:366-370) is the right fix for 8.3 TEMP paths and a no-op on Linux.
- **Ledger nit:** `litho_formula_gallery_selftest.cpp` is listed in CHANGES.md as a 0452 Windows path fix, but against the shipped baseline its only diff is the marker comment — the file already used `LITHOKRAFT_MODELS_SRC_DIR` + `fs::path`. No functional delta; the row overstates what this package changed.

---

## 8. Handshake-coupled meshing — design-only claim **VERIFIED**
Tree-wide grep for `verify-mesh`, `verifyMesh`, staging-list hooks and phase-publish symbols across H's engine/ and game/ sources: **zero hits**. The mesher diff is exactly the RND-H1 table and nothing else; no half-built verify or staging code shipped. The design doc's "NOT implemented… nothing half-built" statement is accurate.

---

## 9. Changes in scope not (fully) in CHANGES.md
1. **`game/LithoKraft/config/config.lth` differs and has no CHANGES row.** It's a runtime artifact: a `saveArchived` dump from workstation runs (41 lines vs production's 28), adding `ai.enabled "0"`, `render.aa "off"`, the ascii cvars, `render.fps_cap "0"`, `render.ground_traces "1"`, `sim.handshake_phases "4"`, `sim.timing_handshake "1"` — and the consequential `render.model_dither "1"` (§6.2). Values are consistent with shipped defaults except dither. Treat as session state; recommend it not merge as-is.
2. **`config/settings.json` gained keys beyond the ledgered displayMode change:** `fpsCap: 0`, `timingHandshake: true`, `handshakePhases: 4`, `renderThreads: 0` — a settings-save round-trip during the session. All match engine/game defaults; benign, but the CHANGES row for settings.json mentions only displayMode.
3. **`game/LithoKraft/saves/` exists only in H** (world saves + `blocks.names.bak.1-8`) — runtime state from playtests, expected in a working tree; noted so it isn't mistaken for content.
4. The **only new source file** in H vs the shipped package is `engine/src/crypto/include/litholattice/crypto/u128.h` (PORT, ledgered ✔).

---

## 10. Textures & shapes — inventory + first impression (JD's ask)
**Inventory result, stated plainly: no new texture or asset content arrived in this handoff.** A full sweep of `engine/assets` + `game/LithoKraft/assets` — 1,024 files in the shipped package, 1,024 in H, **identical name lists and zero content differences**; extending the sweep tree-wide, there is not one new or changed `.png`/`.ltxr` anywhere in H. The "lots of new textures" from the session are new *to the workstation* — they rode in with this morning's full-source package (which itself carried the 2026-10-14 materials/icon work) replacing the older 100ecf3 skeleton — not something produced during today's session. What today added on the visual side is all in the renderer: near-plane clipping (walls no longer vanish at the camera), perspective-correct varyings (UV swim on near quads gone per the math in §1.1), and the dither default-off ruling — behaviour changes to existing shapes, not new shapes.

**Capture-log read:** the two `smoke-biome-capture` logs in H are pre-fix failure logs (§7), not result captures; the 7 biome PNGs whose SHA-256s anchor the pixel-identity claim are not included in the package, so that verification stands on Cortana's log alone — worth including the PNG set (or their hashes) in the next handoff so the second pair of eyes can re-run the comparison. The `run-input-0517.log` does corroborate the focus work from the test side: "focus lost / gained: held keys and buttons are released (no stuck movement)" passes.

---

## Notes for Cortana (consolidated)
1. config.lth is doing silent damage in the package (§6.2/§9.1) — regenerate or exclude it next handoff; dither-off needs one verification run with a clean config.lth.
2. interchange_capture.cpp wants the same sourceRoot() treatment as biome_capture (§7).
3. Armour two invariants with comments: jobDef-reference lifetime in mesher.cpp (§2.1), and SwBandPool's no-throw / no-nesting contract (§1.4).
4. Handoff note §8's pool-sizing sentence is stale vs the 1156 code (§6.4).
5. Include the biome capture PNGs (or a hash list) next time so pixel-identity is re-checkable (§10).
6. Ledger accuracy: the litho_formula_gallery row (§7) and the settings.json row (§9.2) both understate/overstate their diffs — the rest of CHANGES.md matched the tree exactly, which made these two easy to spot. Good ledger overall.

**Scope verdict:** render + game work is merge-quality, with the config.lth caveat handled as a merge-hygiene item rather than a code change. The two genuine code-level follow-ups (interchange_capture portability, mesher comment armour) are small and can ride the next session.


---

# Appendix C — Evidence & Windows compatibility

# R3 — Evidence & Windows-Compatibility Audit

**Scope:** Cortana's 2026-10-15 handoff tree **H** (`handoff-review-2026-10-15/LithoLattice-LithoKraft-Elfie-Handoff-2026-10-15-1222/`) against Elfie's production base **P** (`~/workspace/litholattice/`) and against the pristine input package (`~/workspace/user/files/LithoLattice-LithoKraft-Full-Source-Package-2026-10-15.zip`, 1,836 entries), which is the exact tree Cortana received.
**Method:** full-tree md5 comparison (H vs P over `engine/ game/ tools/ docs/ designers-editors/`), file-list reconciliation against the input zip, line diffs of changed files, log transcription (all 26 shipped logs are UTF-16LE; decoded with `iconv` into scratch before grepping), targeted greps, and `g++ -std=c++20 -fsyntax-only` on copies in scratch. Nothing in H or P was modified.

**Structural discovery that frames everything below:** the `.orig-2026-10-15-HHMM` backups and the `sim_thread.cpp.reverted-SIM-H1-2026-10-15-1035` file that CHANGES.md and the handoff letter describe **are not in the package** (exhaustive filename search for `*orig*`, `*reverted*`, `*.bak*` over all 1,898 files in H: zero hits, apart from runtime save-file `.bak.N` chains under `game/LithoKraft/saves/`). Cortana's edits therefore cannot be diffed against sibling backups from H. They *can* be diffed exactly against the input package, because the reconciliation in §1 proves H = input package + additions only, zero deletions. All diff statements below are H-vs-P (= H-vs-input-package for every shared path).

---

## 1. Ledger completeness (three-way: CHANGES.md ↔ backups ↔ H-vs-P diff)

### The exact change set
- **48 files modified** (content differs from P), **1 file created** (`engine/src/crypto/include/litholattice/crypto/u128.h`), **0 files deleted**.
- Reconciliation against the input zip is exact: H has 1,898 files = 1,836 zip entries **+ 62 additions − 0 removals**. The 62 additions are fully enumerated: 26 root logs, 4 new root documents (`CHANGES.md`, `Elfie-Handoff-2026-10-15-1222.md`, `Windows-Port-Notes-for-Elfie-2026-10-15-0544.md`, `Handshake-Meshing-Impl-2026-10-15-1222.md`), `u128.h`, and **31 runtime save files** under `game/LithoKraft/saves/world/` (region files, `player.dat`, `level.json`, `blocks.names`, each with `.bak.1–.8` chains). Nothing that was in the input package is missing from H.

### Cross-check against CHANGES.md (47 data rows)
- **48 of the 49 changed in-tree files have a CHANGES.md row**, and every row's described edit was spot-verified against the real diff for the load-bearing files (CMakeLists, build.py, sim_thread.\*, signing key, controls_panel, engine_tests, biome_capture, settings.json).
- **[MEDIUM] One changed file has NO row: `game/LithoKraft/config/config.lth`.** It gained 13 cvar lines not present in P (`ai.enabled "0"`, `render.aa "off"`, the `render.ascii_*` set, `render.fps_cap "0"`, `render.ground_traces "1"`, `render.model_dither "1"` + near/far, `sim.handshake_phases "4"`, `sim.timing_handshake "1"`). This is plainly the game re-writing its config during JD's playtest, not a hand edit — but it is a real H≠P delta with no ledger entry, and a verbatim merge would import it. Note `render.model_dither "1"` in particular: it persists dither ON at the config layer in the same package whose `settings.h` change turns the default OFF (DITHER OFF, 1215).
- **[LOW] `game/LithoKraft/config/settings.json` delta is larger than its row states.** The row says only `"displayMode": 0` becomes `2`. The actual diff also adds four persisted keys: `"fpsCap": 0`, `"timingHandshake": true`, `"handshakePhases": 4`, `"renderThreads": 0`. All four values equal current defaults (and `renderThreads` is ledgered as a new key in the RENDER-THREADS rows), so a merge is semantically near-neutral — but the row understates the file's delta; the extra keys are runtime-persisted state, like config.lth.
- **[LOW] One row has no package diff: `hitch-logs/run-batch.ps1`** (BENCH, 1153, plus the 1200 follow-up mentioned in the handoff letter). CHANGES.md itself tags it "TOOLS (outside tree)"; the file and its two claimed `.orig` backups are not shipped, so the edit is unverifiable from the package.
- **[HIGH] The backup ledger itself is absent.** CHANGES.md states "Each change has a backup beside it as `<file>.orig-2026-10-15-HHMM` in the working tree" and the handoff letter says the reverted SIM-H1 copy "is kept as `sim_thread.cpp.reverted-SIM-H1-2026-10-15-1035`". True of Cortana's Windows working tree, presumably; **false of this package** — neither the ~49 backups nor the reverted file were shipped. Consequences: (a) intermediate edit states cannot be audited — most importantly, what SIM-H1 actually changed can only be inferred from the marker comment in `sim_thread.cpp` ("SIM-H1 tried 1010, reverted 1035") and the current file's verified-clean diff (§3); (b) if the .orig set is wanted for the record, it has to be re-exported from the workstation.
- **[LOW] The marker claim is overstated.** CHANGES.md: "Each changed file starts with a `// Cortana 2026-10-15 HHMM:` marker." 46 of 49 changed files carry it. Exceptions: `controls_panel.cpp` (a real, ledgered edit — the TPS label `"TPS 32/64/128/256"` → `"…/512"` — with **no marker**), `settings.json` (JSON cannot hold a `//` comment; excusable), and `config.lth` (un-ledgered, above).

## 2. Log audit

All 26 shipped logs are **UTF-16LE with CRLF** (PowerShell `*>` redirection) — they must be transcoded before any Linux-side grep; three are **empty** (`run-audio-0517.log`, `run-crypto-0501.log`, `run-crypto-2026-10-15-0459.log`, 0 bytes) and evidence nothing. **[LOW]**

### What the logs prove (all from the port era, 04:30–05:44)
- **Builds:** `build-2026-10-15-0430.log` is a prerequisites failure ("Missing 'cmake'"), not a code error. The error storm burns down exactly as the port narrative implies: 0440 = 186 MSVC errors, 0442 = 284, 0449-b = 1 (the C2397 narrowing error in `struct_registry_selftest.cpp`, the file the 0450 row then fixes), 0450-c = 7, 0452-d = 5, and **0455-e onward = 0 errors** in every subsequent Release and Debug build log. Debug configure log confirms the toolchain: VS 18 BuildTools, MSVC 14.51, `ml64.exe` — **not** the "Visual Studio 2022" that the stale `BUILD-SETUP-JD.md` (an Oct-11 doc, byte-identical in P) still names. **[LOW — stale doc, pre-existing]**
- **Warnings:** final builds carry hundreds of MSVC warnings, dominated by pre-existing classes (Debug full build: 547× C4100 unreferenced-parameter of 692 total; C4244 ×57, C4324 ×30, C4996 ×28). No new warning *class* is attributable to the port from sampling; not exhaustively classified. **[INFO]**
- **ctest-debug-2026-10-15-0528.log** (Debug, serial) = **41/48, 7 failed**: `engine_systerian`, `engine_storage`, `engine_litho_formula`, `engine_assets`, `engine_input`, `engine_console`, `lithokraft_selftest` (661.65 s). This matches the Windows-Port-Notes results section exactly (41/48, same seven, same attributions; the `--output-on-failure` dump visibly contains the systerian masters-missing lines, the six sandbox symlink `[FAIL]`s, the wafer.json line, the sig-symlink line, the input-pump line, and the console coercion lines). **Verified** as the port-era baseline.
- **ctest-2026-10-15-0511.log** (Release) = **40/48, 8 failed**: the same environmental set *minus* console, *plus* **`engine_audio` SEGFAULT** and **`lithokraft_selftest` SEGFAULT**, run with **`--parallel 12`** (the Debug runs were serial — methodology differs, noted for fairness). This run postdates the `/STACK` fix (0504) yet still segfaults audio in Release. The handoff letter's Tests section quotes only Debug numbers and never mentions this run. **[MEDIUM — Release evidence in the package is materially worse than the narrative's focus; cause not determinable from the logs.]**
- **Embedded ctest in `build-2026-10-15-0455-e.log`** (Release, via build.py) = 39/48, 9 failed incl. `engine_crypto` exit 0xc0000409 — the pre-fix state, consistent with the crypto fix landing in the 0459 build. `ctest-crypto-0500.log` shows `engine_crypto` still crashing (0xc0000409) in a Release rerun at 05:00; **`run-crypto-0510.log` then shows the full crypto selftest passing** — all SHA-256/SHA-512 NIST and Ed25519 RFC 8032 vectors, signing round-trips, framing checks: "crypto: 0 failure(s)", "ENGINE TESTS OK". That is real evidence the `ll_u128` port is bit-correct on Windows. **Verified.**
- **`run-input-0517.log`**: the input suite with exactly one failure — the queue-overflow check the port notes describe — i.e., the pre-fix state of the test the 1145 row later reworks. Consistent.
- **`selftest-2026-10-15-0516.log`** is only the **sim self-test subset**: 88 `[PASS]`, 0 `[FAIL]`, "SELFTEST OK (0 failures)". It does **not** evidence the full-game counts.
- **Smoke (biome_capture):** the first run failed all 7 writes ("write failed", "7 total failures" — missing out-dir, as the port notes admit); the `-b` run wrote all PNGs, "0 total failures". Both runs log missing texture packs ("pack folder not found", 73 block + 30 item icons missing, "1 texture layers loaded") — the captures were effectively untextured, as the port notes concede. **Verified, with the notes' own caveats.**

### The 42/48 → 44/48 story
- **[HIGH] "Before: 42/48" is contradicted by the shipped evidence; "Now: 44/48" has no shipped evidence at all.** The only port-era Debug ctest in the package says **41/48**, and no log in the package shows 42/48 (Elfie's Linux baseline was 48/48, so 42 is not that either). For the claimed final state, **no post-05:44 ctest log of any kind ships** — the newest full runs are 05:11 (Release) and 05:28 (Debug), predating the signing change (0733), CHUNK-STALL (0825), the console fix (0928), all the 1010–1116 review/render batches, the display default (1153), and the playtest fixes (1208–1218). The claimed remaining-failure set {`engine_systerian`, `engine_litho_formula`, `engine_storage`, `lithokraft_selftest`} is *internally consistent* — it is exactly the shipped 0528 failure set minus the three tests the ledger says were fixed later (`engine_assets` 1050, `engine_input` 1145, `engine_console` 0928) — but consistency is not a run. The handoff letter says "44/48 expected (see the final run)"; the final run is not in the package.
- **[MEDIUM] The selftest counts "1236 PASS / 2 FAIL" (port notes) appear in no shipped log** ("1236" occurs nowhere). The 0528 dump contains #40's output but no such total line.
- **engine_core batch-4 hang: no log evidence either way.** `engine_core` **passes** in every shipped run (0.53–1.7 s: 0455-e, 0504-f, 0511, 0528). No log from the renderer-batch period (~11:00) ships at all, so the claimed one-off hang, the kill, and the "3/3 reruns" are unverifiable from the package. **[could-not-verify]** (Separately, the *port-era* Release `engine_sim` hang the port notes describe **is** partially corroborated: the embedded ctest in `build-2026-10-15-0504-f.log` prints 32 test results, shows "Start 11: engine_sim", and has **no result line for Test #11 and no summary** — a killed run, as narrated.)
- **Performance tables** (orbit/stream FPS, thread scaling) in the handoff letter: **no bench logs ship** (`--bench` output is not in the package). **[could-not-verify]**
- **Pixel-identity claim** (SHA-256 of all 7 biome_capture PNGs identical across renderer batches): **no PNGs and no hash lists ship**; the only capture logs are the 05:43 port-era pair, which predate the renderer work and ran without textures. **[could-not-verify]**

### Scenario hash
- `scenario-hash-debug-2026-10-15.log` (UTF-16; decoded): at **2048 ticks**, `aa82e7afb1eff9d3` appears for **legacy**, **handshake workers=2 phases=4**, and **handshake workers=4 phases=4** — the determinism hash in Debug, exactly as claimed. At 256 ticks all four arms (incl. phases=8) agree on `ba9c8fd2d17d9020`, matching the port notes. **Verified for Debug.** No Release scenario log ships, so "hash holds in Release" rests on narrative only. **[could-not-verify for Release]**

## 3. Hash-claim verification (static): "no sim or circuit math changed"

**Verified.** The complete H-vs-P diff set (49 files) was checked path-by-path:
- Inside `engine/src/sim/`, exactly two files differ: `sim_thread.cpp` and `include/litholattice/sim/sim_thread.h` — both ledgered CHUNK-STALL. `timing_handshake.cpp`, `control_stream.cpp`, `demo_record.cpp`, `det_sched.cpp`, `trace_jit_basic.cpp`, `random_ticks.cpp` are **byte-identical** to P.
- The sim_thread diff itself (19 lines) is purely threading: it adds `holdBetweenTicks()` (waits out an in-flight iteration on `blockMutex_`, refuses when the handshake is frozen or has workers outstanding) and wraps the existing post-tick section in a `postLock` on the same mutex, unlocking before the tail wait. **No arithmetic, no tick logic, no catch-up change** — and no remnant of the reverted SIM-H1 (no unlock/yield between catch-up steps anywhere in the diff).
- No file with `signal`, `trace`, or `circuit` in its path differs anywhere in the tree; the game-side signal code is untouched.
- Caveat of scope: render *math* did change (RND-H2/H3/H4 in `software_renderer.cpp`, `mesher.cpp`) — ledgered as renderer work with a pixel-identity claim (§2, unverifiable from the package), not as sim math. The signing key change (§6) changes which key verifies, not any math.

## 4. Windows-compatibility sweep of the changed files

- **CRLF [LOW, mostly pre-existing]:** 7 changed files contain CR line endings in H — `assets_selftest.cpp` (mixed, 319/527 lines), `input_selftest.cpp` (242/242), `glfw_input_backend.cpp/.h` (all), `window.cpp` (227/227), `window.h` (111/112), `controls_panel.cpp` (132/132). Attribution matters: **the same files were already CRLF or mixed in P** (P: controls_panel 132/132 CRLF, window.cpp 208 CR lines, window.h 100, glfw pair 166/53, assets_selftest 312 CR lines, input_selftest 234). Cortana's edits preserved each file's local convention; **no previously-LF file was converted**, and the other 42 changed files are pure LF. GCC compiles all of these fine; the cost is diff noise in exactly the platform files future cross-platform merges will touch most.
- **Backslash path literals:** none introduced. The only `\\` hit in changed sources is an escaped NUL inside a crypto test *name* string. **Clean.**
- **Include case-sensitivity:** all **373** project `#include "litholattice/…"/"lithokraft/…"` directives in the 49 changed files resolve to on-disk files in H with exact case. **Clean** — no Windows-case-masking hazard in the change set.
- **POSIX-isms in changed files:** all guarded. `signing.cpp`'s POSIX headers sit under `#if defined(__unix__) || defined(__APPLE__)` (as the PORT row claims); `concmd.cpp`'s under `#if defined(__linux__)`; the game selftests' `getpid` comes from `<process.h>` + `#define getpid _getpid` under `_WIN32` with `unistd.h` in the `#else`. `engine_tests.cpp`'s `/tmp/...` path is **pre-existing P code** — that file's entire diff is the marker + the `empty[1]` fix (verified by full diff).
- **MSVC-isms / reverse direction (would it break GCC?):**
  - `_read(0, …)` in `mcp/server.cpp` is inside `#ifdef _WIN32` with the POSIX `std::cin` path in `#else`. ✓
  - `llCtz32/64` (`binary_greedy_mesher.cpp`) branches `_MSC_VER && !__clang__` → `_BitScanForward(64)`, else the original `__builtin_ctz/ctzll`. ✓
  - `LL_SQRTF` (`software_shaders.h`): MSVC branch includes `<cmath>` and uses `std::sqrt(float)`; other compilers keep `__builtin_sqrtf`. ✓
  - `u128.h`: MSVC gets a hi/lo struct (constexpr +−\* >> <<, explicit integral cast returning `lo`); GCC/Clang get `using ll_u128 = unsigned __int128` — the pre-port type, so the Linux crypto path is the original code with a renamed token. The struct's multiply is a correct 32-bit-limb schoolbook with mod-2^128 wraparound, and its bit-correctness on MSVC is evidenced by `run-crypto-0510.log` (§2). ✓
  - `SO_EXCLUSIVEADDRUSE` (sockets.cpp) and the reparse-point-safe open (sandboxed_io.cpp) are Windows-branch code in files whose POSIX paths are unchanged in behavior; sockets.cpp passed GCC syntax check (below).
- **Syntax-only spot checks (GCC 13.3, `-std=c++20 -fsyntax-only`, copies in scratch, tree include layout):**
  - Headers: `u128.h` (also compiled **and run**: +,\*,>> and the uint64 cast produce correct results on the GCC native path), `sha512.h`, `software_shaders.h`, `iworldquery.h`, `sim_thread.h`, `software_renderer.h`, `window.h` — **all OK**.
  - Translation units: `sha512.cpp`, `ed25519.cpp`, `signing.cpp`, `binary_greedy_mesher.cpp`, `sim_thread.cpp`, `sockets.cpp`, `sandboxed_io.cpp` — **all OK**.
  - **Could not be checked standalone:** TUs needing CMake-fetched dependencies not present in the tree (GLFW headers for `window.cpp`/`glfw_input_backend.cpp`/`main.cpp`/`game.cpp`, AngelScript, etc.). A full GCC build remains the real test — Cortana discloses it was not run; these 14 clean syntax checks are partial positive evidence, not a build.

## 5. Package hygiene

- **PACKAGE-MANIFEST.md vs H:** the manifest describes the *input* package, and it is accurate for it (1,834 files + manifest + 1 symlink entry = the zip's 1,836 entries; the `designers-editors/` table matches H's four files, and the two editor md5s were not re-verified here — they are Elfie-side tools Cortana did not change; all four files are byte-identical between H and P per the tree diff). H itself has no manifest of its own; CHANGES.md serves as one, with the gaps in §1. **[INFO]**
- **Nested zips [INFO]:** `LithoKraft-game-src-100ecf3.zip` (3.2 MB), `LithoLattice-engine-src-100ecf3.zip` (1.2 MB), `litholattice-full-2026-10-11.zip` (8.6 MB) are **byte-identical to the copies at P's tree root** and internally dated 2026-10-10/11 — historical snapshots inherited from the input package, ~13.0 MB of H's 38 MB. They are not Cortana additions, but they are a merge hazard of the confusion kind: `litholattice-full-2026-10-11.zip` contains a complete stale source tree and must never be extracted over P.
- **`demo_sw.cpp` and `demo_record_proof.lldemo` [INFO]:** both sit at H's root and are **byte-identical to P's root copies** — pre-existing tree-root files (a standalone software-render demo source from Oct 11, and a 398-byte LLDM demo-recording proof artifact), not handoff strays. They belong exactly as much as they ever did.
- **`game/LithoKraft/saves/` [MEDIUM]:** 31 files of JD's live playtest world (region files `r.0.0.llr`/`r.0.-1.llr`, `player.dat`, `level.json`, `blocks.names`, plus `.bak` chains) ride inside the source tree. Harmless as evidence of the playtest; must be excluded from any merge into P.
- **The `.reverted-SIM-H1` file and `.orig` backups [see §1, HIGH]:** absent from H, so there is nothing to merge — but the *merge-hazard question for Cortana's working tree* was assessed anyway: C++ sources are **explicitly listed** in the module CMakeLists (the only `file(GLOB …)` calls in the tree cover `*.vert/*.frag/*.glsl` shaders and worldgen `*.json`), and backup filenames (`*.cpp.orig-2026-10-15-HHMM`, `sim_thread.cpp.reverted-SIM-H1-…`) do not end in `.cpp`. Even a wholesale merge of the Windows working tree would therefore **not compile any backup**; the hazard is clutter and human confusion, not build contamination. **[INFO]**
- **`engine/LithoKraft` [INFO]:** in H it is the 18-byte regular file containing `../game/LithoKraft` (the input zip's symlink entry, materialized as text — the exact artifact the CMake fallback exists for). If it were merged verbatim over P's symlink, the new `IS_DIRECTORY` fallback would route around it and the build would still work; the correct merge keeps the symlink.

## 6. BUILD-SETUP-JD.md, build.py, and the CMake changes

All verified against the actual diffs (not just the notes' embedded diff, which matches the files):

- **`tools/build.py`:** the hardcoded `"Visual Studio 17 2022"` generator is replaced by: use `$LL_CMAKE_GENERATOR` if set, else pass **no** `-G` (CMake picks the newest VS) — `-A x64` retained. Exactly as CHANGES.md claims. Note build.py remains Release-only (`-DCMAKE_BUILD_TYPE=Release` untouched), consistent with the port notes' disclosure that Debug was driven by raw cmake/ctest.
- **`/STACK:8388608`:** in `engine/CMakeLists.txt`, `if(MSVC) add_link_options(/STACK:8388608) endif()` — MSVC-only as claimed. Scope note: `add_link_options` is directory-scoped, so it applies to every target linked under `engine/` (all test exes and the game), not a single binary — broad but intended, and inert on GCC/Clang.
- **Symlink fallback:** `engine/CMakeLists.txt` now does `if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/LithoKraft")` → use it; else resolve `../game/LithoKraft` to an absolute path; then `add_subdirectory("${LITHOKRAFT_GAME_DIR}" game)`. On Windows with the 18-byte file present, `IS_DIRECTORY` is false and it falls back to the sibling game dir — **it is a path fallback: no copy, no junction, no error**. On Linux the symlink resolves as a directory, so behavior is byte-for-byte the old one. (Edge: on Linux with the symlink *deleted*, the build would silently use `../game/LithoKraft` — the same target the symlink points to, so still correct.)
- **`engine/src/render/CMakeLists.txt` (RND-MT):** AVX2 is scoped to `software_renderer.cpp` only, gated on `LL_SW_AVX2` (default ON) and an x86-64 processor match with an arm64 exclusion; GCC/Clang get `-mavx2` (no `-mfma`, matching the no-contraction comment), MSVC gets `/arch:AVX2`. As claimed — with one consequence Cortana's "GCC untested" caveat covers but the letter doesn't spell out: **the Linux x86-64 build now also compiles the rasterizer for AVX2 by default**, so produced binaries require an AVX2 CPU at runtime; the opt-out is `-DLL_SW_AVX2=OFF`. **[LOW]**
- **`mod_signature.h` (SIGNING, 0733):** the diff is exactly the marker plus one line — the baked public key changes from the old test key `3bb44b55…b787` to `04c1766ef29e93b7d36fdf185f64ee958e9dded4c8c82f0dca03029e8a30011c`, matching the handoff letter digit-for-digit, and the new comment states the retired test key is still rejected by `bakedKeyIsTestKey()`. No other logic in the file changed, so "the fatal-exit check is intact" holds at diff level. (Whether the corresponding private key exists and signs correctly cannot be verified from the package — by design it is not in it.) **[INFO]**
- **`BUILD-SETUP-JD.md`:** unchanged from P (Oct-11 doc). It tells JD to install "Visual Studio 2022 Build Tools" (actual toolchain used: VS 18 2026, MSVC 19.51) and says `engine_sim` and `lithokraft_selftest` are grandfathered failures that "fail here too, that's expected, don't chase them" — contradicted by the Oct-15 state, where `engine_sim` passes in every shipped run. Stale-doc hazard for the next Windows session, not a Cortana edit. **[LOW]**

---

## Claims ledger

### Verified (evidence in the package supports the claim)
1. Determinism hash `aa82e7afb1eff9d3` at 2048 ticks, legacy + handshake 2w/4p + 4w/4p, in **Debug** (scenario log); 256-tick arms mutually consistent (`ba9c8fd2d17d9020`).
2. "No sim or circuit math changed" — sim-dir diff is threading-only (`holdBetweenTicks`, `postLock`); all other sim/trace/signal files byte-identical (§3).
3. The complete edit set is ledgered in CHANGES.md — 48 of 49 in-tree files (§1), with row descriptions matching real diffs on every spot check.
4. Port-era Debug ctest 41/48 with the seven named failures and environmental attributions (ctest-debug-0528 + its output dump).
5. `ll_u128` crypto port is bit-correct on Windows (run-crypto-0510: all NIST/RFC 8032 vectors pass).
6. Signing key rotation: new public key `04c1766e…011c` baked in, old test key replaced, no other logic touched in `mod_signature.h`.
7. build.py VS-generator selection + `LL_CMAKE_GENERATOR` override; `/STACK:8388608` MSVC-only; symlink fallback is a path fallback (no copy/junction/error); AVX2 scoped to `software_renderer.cpp` on x86-64.
8. MSVC error burn-down across the build logs (284 → 0), with the 0450 narrowing fix matching its log predecessor's error.
9. biome_capture `sourceRoot()` exists and behaves as described (with the /home/hatch path retained as first seed + final fallback — a nuance on the word "replaces").
10. Release `engine_sim` hang after the /STACK change (port notes) — partially: the 0504-f embedded ctest starts Test #11 and never reports it, with no summary.

### Contradicted (package evidence says otherwise)
1. **Backups/reverted file shipped** — CHANGES.md's "backup beside it in the working tree" and the letter's "the bad copy is kept as `sim_thread.cpp.reverted-SIM-H1-…`" do not hold for this package: zero such files in H. **[HIGH]**
2. **"Before: 42/48"** — the shipped port-era Debug ctest is 41/48; no shipped log shows 42/48. **[part of HIGH in §2]**
3. **"wafer.json is outside the tree"** (litho_formula explanation) — `game/LithoKraft/assets/worldgen/wafer.json` is in-tree (43,694 bytes); the selftest's candidates are cwd-relative paths plus the `/home/hatch` absolute, so the failure is a working-directory resolution issue. The environmental (non-regression) classification still stands. **[LOW]**
4. **Marker universality** — `controls_panel.cpp` carries a real ledgered edit and no marker. **[LOW]**
5. **settings.json row completeness** — the file gained four persisted keys beyond the ledgered displayMode change. **[LOW]**

### Could not verify (no evidence in the package either way)
1. **Final Debug ctest 44/48** and the exact final failure set — no post-05:44 test log ships. **[HIGH as an evidence gap]**
2. "1236 PASS / 2 FAIL" selftest totals. **[MEDIUM]**
3. Release scenario hash; all Release test state after 05:11.
4. engine_core batch-4 hang and its 3/3 passing reruns (all shipped runs show it passing).
5. Pixel-identity (SHA-256) of the 7 biome PNGs across the renderer batches; all FPS/thread-scaling tables; mesh-rate figures.
6. The contents of `Hitching-Diagnosis-2026-10-15-0807.md`, `ChunkLoad-Sim-Fix-2026-10-15-0841.md`, `Handshake-Chunk-Streaming-2026-10-15-0841.md`, and `Source-Review-FINAL-2026-10-15-0940.md` — referenced as the basis for four fix batches, none shipped. **[MEDIUM]**
7. The `run-batch.ps1` bench edits and their backups (outside tree, not shipped). **[LOW]**
8. Full GCC/Clang build and test pass (disclosed as not run by Cortana; §4's syntax checks are partial evidence only).
9. That the new dev keypair's private half exists and signs (by design, out of package scope).

**Severity tally:** 0 critical · 2 high (missing backup/reverted artifacts as shipped; missing final-run test evidence incl. the 42/48 baseline contradiction) · 4 medium (un-ledgered config.lth; saves/ shipped in-tree; Release segfault run undisclosed in the letter's Tests section; unshipped diagnosis/source-review docs — with the 1236-count gap folded into the §2 high/medium cluster) · 6 low · 5 info.
