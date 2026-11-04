# IDemoRecord — Demo Recording on the Control-Serialization Layer

**Date:** 2026-10-13 · **Workstream:** JD's directive of 2026-10-13 12:46 EDT ("the whole demo system"), built in the experiment tree first per JD's 14:24 EDT build-in-experiment-first call
**Tree:** experiment copy `~/workspace/experiments/control-serialization/` (production untouched — verified byte-identical against `~/workspace/fix-program-2026-10-13/manifest-final.md5` at the end of the run, §6)
**Status:** complete in the experiment tree; all demo/control gates green (§6 gives the full, unedited account, including the documented load-flake family). Production port is a single combined decision — the manifest is §8.

This report covers **IDemoRecord itself**: the interface, file lifecycle, and analysis surface that §9 of the control-serialization report (`Control-Serialization-2026-10-13.md`) specified as the next build on top of that layer. The layer underneath — the serialized control stream, its PRE/POST coupling to the Timing Handshake, and the LLCS v1 stream format — is unchanged by this work and is not re-described here except where the demo format deliberately departs from it (§2).

---

## 1. Interface as built

### 1.1 Surface (`engine/src/sim/include/litholattice/sim/idemo_record.h`, new; implementation `engine/src/sim/demo_record.cpp`, new)

- **`IDemoRecord`** — the abstract face JD named: `session()` (the `DemoSessionInfo` the record belongs to) and `lastError()` (the most recent `DemoError`). Both concrete classes implement it.
- **`DemoRecorder`** — `begin(ControlStream&, const DemoSessionInfo&)`, `pump()`, `finish(finalWorldHash, totalTicks)`, `abort()`, plus `recording()`, `eventsRecorded()`, `bytes()` (the in-memory file image), and `saveToFile(path)`.
- **`DemoPlayer`** — `load(data, n)` / `loadFile(path)`, `parsed()` (the full `DemoParseResult`), and `submitInto(ControlStream&)`.
- **Free functions** — `parseDemo(data, n)` (the parser both player paths use), `summarizeDemo(parse)`, and `demoEventsInTickRange(parse, lo, hi)` (§4).
- **`DemoError`** — distinct codes, never collapsed into a bool at the parse layer: `None`, `Io`, `EmptyFile`, `BadMagic`, `UnsupportedVersion`, `SegmentBadMagic`, `SegmentUnsupportedVersion`, `SegmentChecksum`, `SegmentMalformed`, `HeaderMismatch`, `TrailerBadMagic`, `TrailerChecksum`, `TrailerCountMismatch`, `State` (API misuse). `demoErrorName()` renders each for logs and the CLI.

### 1.2 The passive-tap rule

A recorder is **passive by construction**: the only stream operations it ever performs are `setRecording(true/false)` and `takeRecord()` on the tap the control layer already provided. It never submits, never extracts, never stamps. `begin()` attaches the tap and writes the preamble; each `pump()` drains whatever the tap has accumulated since the last pump and appends exactly one framed segment (§2); `finish()` does a final pump, then appends the trailer and detaches; `abort()` detaches *without* a trailer — the deliberate crash image. Because the tap is a copy taken at application time, recording cannot perturb the session it records; §3 proves that at the world-hash level, not just the stream level, and the selftest proves attach/detach alone leaves the extracted stream identical (§7).

### 1.3 Replay: the `submitInto` path

`DemoPlayer::submitInto(stream)` is one call: `stream.submitStamped(parsed_.events)` — every recovered event, stamps (seq, tick) intact, into a fresh stream over the same initial world. It refuses (`State`, returns false) if no successful load preceded it, where "successful" means the parse came back `complete` **or** `truncated` — a torn record replays its recovered prefix, which is the point of recovering it. From there replay is the control layer's existing contract: the sim extracts each event at its stamp tick in `(tick, seq)` order, and the world evolves bit-identically (§3).

### 1.4 Session identity

`DemoSessionInfo` pins everything a replay must match: **world seed**, **mode** (0 legacy / 1 handshake), **tick rate** (the single gear), **handshake phases per cycle**, and both format versions — **stream format version** (LLCS, = 1) and **demo format version** (= 1). The seed/mode/rate/phases ride in every segment's LLCS header (§2), and the parser enforces agreement across segments (`HeaderMismatch`, §5). The recorded-session arm in §3 used **worldSeed = 0**: the scenario-world builder exposes no seed constant, and the Wafer worldgen it builds is pinned to seed 0 — so 0 is the value the builder effectively uses, documented in the test code where the session is constructed.

---

## 2. Format — the LLDM container (v1, little-endian)

```
[ 8-byte preamble ][ segment frame ] × N [ 40-byte trailer — clean finish only ]

Preamble:  'LLDM' · u16 demo format version (= 1) · u16 reserved
Frame:     u32 payloadLen · payloadLen bytes = ONE complete LLCS v1 stream
           (writeControlStream: session header with declaredEvents = this
            segment's event count, declaredTicks = 0)
Trailer:   'LLDT' · u16 demo format version · u16 reserved ·
           u64 finalWorldHash · u64 totalEvents (all segments) ·
           u64 totalTicks ·
           u64 checksum = DeterministicScheduler::hashBytes over ALL
               bytes PRECEDING THE TRAILER, stored at trailer+32
               (i.e. file offset size−8)
```

### Deltas from the LLCS format of the control report (§9 there), with reasons

1. **Framed segments instead of one monolithic stream.** A demo is an LLDM preamble plus a sequence of independently framed, independently checksummed LLCS streams, one per recorder `pump()`. That framing is what makes the file **incrementally appendable** — a running session lands a complete, valid segment every pump, so a crash loses at most the events not yet pumped — and it is what makes **torn-tail recovery** (§5) possible: the parser can trust every whole frame before the cut and salvage the partial one. LLCS alone, written once at close, would lose the whole session to any crash.
2. **A demo preamble exists at all.** `LLDM` + the demo format version identify the container independently of the segment payload version, so the container can evolve without touching LLCS, and a non-demo file is refused at byte 0 (`BadMagic`) rather than deep inside a segment parse.
3. **The trailer carries the outcome, not just integrity.** Where the LLCS trailer is only a checksum, the LLDT trailer also carries the **final world hash** (the session's outcome, checkable against any replay without re-running it — §4's verify path and §3's proofs both read it), the **total event count** (cross-checked against the segments' declared counts *and* the recovered event count — any disagreement is `TrailerCountMismatch`), and **total ticks**.
4. **Trailer checksum scope — stated explicitly because it differs in presentation from LLCS.** The demo trailer checksum is `hashBytes` (the same FNV-1a family, via `DeterministicScheduler::hashBytes`) over **all file bytes preceding the trailer** — preamble and every frame, nothing of the trailer itself — and is stored in the trailer's last field, at trailer+32. This was the one friction point found and fixed during the game-side lane: an early cut hashed a wrong scope (the whole buffer including the trailer region), and every clean record failed its own trailer check until the scope was pinned to *bytes preceding the trailer* (`size − 40` hashed, stored at `size − 8`). The working rule is the one in the box above; the round-trip and corruption gates (§5–§7) pin it.
5. **Per-segment header bookkeeping.** Each segment's LLCS header repeats the session identity and declares **that segment's** event count, with `declaredTicks = 0` (ticks are a whole-session figure; they live in the trailer). Segment counts are what the trailer's total is cross-checked against.

**Two judgment calls, recorded as made:**

- **(a) Trailer format-version mismatch → `UnsupportedVersion`.** There is no trailer-specific version error code, and inventing one mid-build would have split the version story; a trailer whose version field is not 1 is reported with the same `UnsupportedVersion` as a preamble version mismatch. The detail string says which one fired ("unsupported trailer format version").
- **(b) Salvaged segments are accounted differently — deliberately.** Events recovered by record-by-record salvage of a torn final frame (§5) are returned in `events`, but the segment they came from does **not** increment `segmentsRecovered` and its declared count does **not** enter the sum the trailer total is checked against. Rationale: `segmentsRecovered` and the count sum are statements about *byte-complete, checksum-valid* segments; crediting a salvaged frame there would let a torn record masquerade as a whole one in exactly the fields a verifier reads first. Salvage widens what you can *replay*; it does not widen what you can *certify*.

---

## 3. The demo guarantee — proofs with hashes

Harness (`game/LithoKraft/src/app/demo_record_test.cpp`, `--demo-test`): the control crew's §3 scripted session — the fixture `PlayerChunk(4,4)` at tick 0, `Ray`→`Break` at tick 100, `Use` (repeater) at tick 120, `Ray`→`Place` at tick 150, 6 events in all — driven through the **real SimThread** (paused + stepped, 512 TPS, 2,048 world ticks) on the circuit-scene world, recorded with a `DemoRecorder` attached before `start()` and pumped per 512-tick chunk, then replayed from the saved file (`build-linux/demo_record_proof.lldemo`, 398 bytes) through `DemoPlayer::submitInto` into fresh SimThreads. Fresh numbers from this run (§6):

| Arm | Hash | Applied | Dropped | Violations |
|---|---|---|---|---|
| Recorded session (legacy, recorder attached) | `5041cf1795c33a8c` | 6 | 0 | 0 |
| Same script, **no recorder** (legacy) | `5041cf1795c33a8c` | 6 | 0 | 0 |
| **Replay** of the record (legacy) | `5041cf1795c33a8c` | 6 | 0 | 0 |
| **Replay** of the record (handshake, 2 workers) | `5041cf1795c33a8c` | 6 | 0 | 0 |
| No-input baseline (control report §3, contrast) | `2a2d19a416a1826e` | 1 (fixture) | 0 | 0 |

The four equal hashes are the guarantee, in its three parts:

- **Recording is non-perturbing.** Recorder attached vs not: bit-identical world.
- **Replay is faithful.** A fresh SimThread fed *only* from the file reproduces the recorded world bit-identically — and the record's own trailer carries that hash, so the check is file-vs-world, not run-vs-run memory.
- **Replay is mode-independent.** The session was recorded in legacy mode and replays bit-identically under the Timing Handshake (2 workers) — the demo is a record of the *stream*, and the stream's contract (same stream ⇒ same world, across modes) is inherited whole.

Accounting, asserted in every arm: `applied == 6 == events recorded`, `dropped == 0`, `orderViolations == 0` — nothing lost, nothing reordered, nothing applied twice. And the sanity contrast holds: the recorded hash differs from the no-input baseline, so the recorded inputs verifiably changed the world the record reproduces.

---

## 4. Analysis surface

Two CLI verbs wired in `main.cpp`, plus the engine-side queries they are built on. All output below is captured fresh from this run (§6) against `build-linux/demo_record_proof.lldemo`.

### `--demo-verify FILE` — the one-line verdict

Exit codes are part of the contract: **0 = complete, 2 = truncated-but-recovered, 1 = hard error** (including unreadable file).

```
$ LithoKraft --demo-verify demo_record_proof.lldemo
demo-verify: COMPLETE (6 events, 1 segments)            → exit 0

$ LithoKraft --demo-verify <same file, trailer cut off>
demo-verify: PARTIAL (truncated, 6 events recovered)    → exit 2

$ LithoKraft --demo-verify <same file, first byte smashed>
demo-verify: INVALID: BadMagic demo: bad magic          → exit 1

$ LithoKraft --demo-verify <empty file>
demo-verify: INVALID: EmptyFile demo: empty file        → exit 1
```

### `--demo-dump FILE [--ticks LO HI]` — the anatomy

Session line (seed/mode/tickRate/phases/both format versions), status line, per-kind and per-source counts, tick span, trailer line — then, with `--ticks`, the events in the inclusive range in the control layer's one-line text form:

```
session: seed=0000000000000000 mode=0 tickRate=512 phases=4 streamVersion=1 demoVersion=1
status: COMPLETE (6 events recovered, 1 segments)
counts: total=6
  kind edit         3
  kind ray          2
  kind player-chunk 1
  kind locomotion   0
  kind control-frame 0
  source local      0
  source mcp-pilot  0
  source script     0
  source replay     0
  source test       6
ticks: first=0 last=150
trailer: hash=5041cf1795c33a8c events=6 ticks=2048
```

```
$ LithoKraft --demo-dump demo_record_proof.lldemo --ticks 100 150
  … (same header block as above) …
events in ticks [100, 150]: 5
  seq=1 tick=100 src=test#0 kind=ray o=(6.50,266.00,5.50) d=(0.000,-1.000,0.000)
  seq=2 tick=100 src=test#0 kind=edit edit=break block=0 facing=0
  seq=3 tick=120 src=test#0 kind=edit edit=use cell=(8,263,65)
  seq=4 tick=150 src=test#0 kind=ray o=(6.50,266.00,5.50) d=(0.000,-1.000,0.000)
  seq=5 tick=150 src=test#0 kind=edit edit=place block=2 facing=0
```

(The `--ticks 100 150` listing returns 5 of 6 events — the tick-0 fixture chunk falls outside the range, as it should. `--demo-dump` exits 0 when the parse is complete *or* truncated, 1 on a hard error.)

### Engine-side queries

`summarizeDemo(parse)` returns `DemoSummary{ totalEvents, byKind[6], bySource[5], firstTick, lastTick, hasEvents }` — the dump's counts and span are this struct, printed. `demoEventsInTickRange(parse, lo, hi)` returns the events with `lo ≤ tick ≤ hi` in application order — the dump's listing is this vector, rendered with `controlEventToText`. Both operate on the parse result only; neither touches a stream, so analysis can run on a record nobody intends to replay.

---

## 5. Robustness — the corruption / recovery matrix

Doctrine (from the header, and the tree's world-permanence stance applied to records): **fail closed on corruption, recover on a torn tail.** A crash mid-session or mid-write is not corruption — it yields `truncated = true` with every whole event recovered, including a record-by-record salvage of a final frame cut mid-write (the salvage walk trusts only the torn segment's header magic/version and records that are byte-whole and field-valid, and stops at the first incomplete or invalid one). Byte-complete segments get no such charity: an LLCS failure inside a whole frame is a hard error, classified by inspecting the segment *before* trusting its checksum — magic first, then version, then checksum, then malformed. Earlier valid segments are kept in the result for forensics even when a later segment hard-fails.

Lane 2's eight cases (all in the selftest, §7; outcomes fresh from this run):

| # | Damage | Outcome |
|---|---|---|
| i | File magic flipped (byte 0) | `BadMagic`, not complete |
| ii | Demo version bumped (preamble) | `UnsupportedVersion`, not complete |
| iii | One payload byte flipped inside a segment | **`SegmentChecksum`** — hard error, *not* truncated |
| iv | File cut mid-record in the final segment | `truncated`, first segment's 4 events recovered by salvage |
| v | Trailer removed entirely (last 40 bytes) | `truncated`, **all** events recovered |
| vi | Trailer `totalEvents` tampered, trailer checksum honestly recomputed | `TrailerCountMismatch` |
| vii | Trailer checksum broken (last stored byte) | `TrailerChecksum` |
| viii | Segment LLCS magic smashed ('L'→'X') | Hard segment error — **`SegmentBadMagic`** in this run (magic is inspected before the checksum, per §2's classification order) |

Lane 1's smoke extras, on top of the matrix:

| Case | Outcome |
|---|---|
| Empty file | `EmptyFile` hard error — independently re-verified in this run via `--demo-verify` on a zero-byte file (exit 1, §4) |
| `begin()` twice on one recorder | `State` error, second begin refused |
| `abort()` mid-session (detach, no trailer) | The bytes written so far parse as a truncated demo — the crash image by design; the same detach path is exercised in the selftest's non-perturbation checks |

Parser edges the matrix implies and the code handles distinctly, for the record: a preamble torn after the magic (file of 4–7 bytes) is `truncated`, not `BadMagic`; a trailer-length tail that begins a torn frame is salvaged, not misread as a bad trailer; a `LLDT` magic appearing anywhere except the final 40 bytes is `TrailerBadMagic`; a segment whose header disagrees with the first segment's session is `HeaderMismatch`. `DemoPlayer::submitInto` before a successful load, `pump()`/`finish()` on an unbegun recorder, and `loadFile` on an unreadable path all fail with `State` / `Io` rather than touching a stream.

---

## 6. Gates (experiment tree, Release, 2-core VM — fresh run for this report)

| Gate | Result |
|---|---|
| Build | `cmake --build build-linux -j2` exit **0**. Warning count: **0** — the first invocation found the tree up to date, so the demo/control objects (`demo_record.cpp`, `control_stream.cpp`, `demo_record_test.cpp`) were additionally force-recompiled and relinked: exit 0, **0 warnings**, none in any IDemoRecord/control file |
| Game selftest, legacy | **1009 PASS / 0 FAIL** — 978 standing + **31 demo** (§7), first run |
| Game selftest, handshake | **1009 PASS / 0 FAIL** on the final solo re-run — see the honest account below the table |
| `--demo-test` | **DEMO TESTS: OK** — the 31 selftest checks plus the **12 demo-proof checks**; all four §3 hashes `5041cf1795c33a8c`, baseline contrast held |
| `--handshake-test` | **OK** |
| `--handshake-scenario 2048` legacy + handshake | `faec0f02700d98c6` both — unchanged |
| `--demo-dump` / `--demo-verify` | §4 output captured fresh; verify exit codes 0 / 2 / 1 all exercised |
| ctest (serial) | **46/48** — see the ctest account below; solo re-verification: `engine_audio` **PASS**; no demo/control check failed in any run |
| Perf | §6 note below — 512 TPS held legacy; handshake degraded **only under observed cross-crew compile contention** |
| Production tree | **Untouched** — `md5sum -c` against the fix-program final manifest: 1618 files, **0 mismatches** |

**Handshake selftest account (unedited).** Legacy passed clean first try. Handshake mode then failed the *same single check* on three consecutive runs — `streaming: worker-thread decode, double-buffered handoff, 440 Hz intact, clean EOS` (1008 PASS / 1 FAIL each; the middle run additionally tripped two `game ui: script bindings` checks whose log line shows the cause in plain text: a script handler throttled for exceeding its CPU quota under load, 2289 µs against a 1024 µs/tick budget — a timing verdict, not a logic one). A fourth run, solo after the ctest pass, scored **1009 PASS / 0 FAIL**. This is the documented audio-streaming load-flake family the control report §6 recorded for this tree, plus one appearance of its script-quota cousin; in **no** run did any demo or control check fail (31/31 demo and 20/20 control lines were `[PASS]` in every log, including inside ctest).

**ctest account.** Serial run in `build-linux`: **46/48**. The two failures are one check wearing two hats: `engine_audio` (test 22) and `lithokraft_selftest` (test 40) both failed *only* on the embedded `streaming: worker-thread decode … 440 Hz` line — the same flake family as above, under the load of the ctest run itself plus a neighbouring crew's build (§perf note). `engine_audio` re-run solo immediately after: **PASS** (10.10 s). The selftest binary run solo the same hour scored 1009/0 in both modes (legacy first try; handshake per the account above). **`engine_systerian` passed in place**: it resolves master SVGs by a path relative to the tree location (`…/../../../../user/files/…`) that assumes the tree sits at `~/workspace/<name>`, and this experiment tree sits one level deeper — the shim the control crew left for exactly this, `~/workspace/experiments/user` → symlink to `~/workspace/user`, was checked and is still present, which is why no systerian failure appeared this time. It is a locational property of the experiment copy, not of the code; production needs no shim. No demo or control check failed in the ctest run (31 demo `[PASS]`, 20 control `[PASS]` lines in its selftest output).

**Perf note (spot-check, §8-comparison).** `--perf-headless`, default circuits scene (5,531 components), 512 TPS target. Legacy: **512 achieved**, tick avg 0.746 ms, p99 2.925 ms (control run: 512.0 held, avg 0.550 ms on its quieter second run). Handshake: **300 achieved** (avg 2.944 ms, p99 22.300 ms) on the first run and **399 achieved** (avg 2.438 ms, p99 25.123 ms) on a re-run — *both taken while the worldgen-completeness crew was compiling with `-j2` on this same 2-core VM* (`cc1plus` observed live throughout the perf window; load average 2.7–4.2 on 2 cores, i.e. the box was oversubscribed before the benchmark's own threads started). The control report's §8 comparison stands unchallenged by this: on its runs, under the same *kind* of contention, 512.0 held in both modes, and nothing in this lane adds per-tick work to the perf scenes (no recorder is attached there; IDemoRecord code runs only when a session is recorded or a file is parsed). The honest statement is narrower than a regression claim and narrower than a parity claim: **legacy held 512 under load today; the handshake number today is a contention measurement, not a demo-layer measurement, and a quiet-box handshake re-run is still owed if JD wants the §8 table extended.**

---

## 7. Added selftest checks (the 31, all `[PASS] demo: …` in `--selftest`)

**Round trip — 15.** Recorder begin turns the stream recording tap on · recorder session echoes the session info (seed, mode, tick rate, phases) · events extract at their stamp ticks (4 of 7 by tick 122) · the remaining events extract by tick 125 · the recorder captured every applied event · recorder finish writes the trailer · recorder reports no error after a clean finish · the record starts with the LLDM preamble and carries a trailer · parseDemo of the round trip is complete, not truncated, no error · parsed session round-trips (seed, mode, tick rate, phases, stream + demo versions) · trailer fields exact (final hash, total events, total ticks) · every event round-trips field-for-field (all five kinds) · at least two segments were framed and recovered · DemoPlayer loads the round-trip bytes (complete) · DemoPlayer `submitInto` reproduces the exact stamped stream on a fresh stream.

**Non-perturbation — 2.** Attach/detach of a recorder leaves the extracted stream identical (recording off) · recorder attach/detach leaves `dropped == 0` and `orderViolations == 0`.

**Fail-closed / recovery matrix — 8.** Flipped file magic → `BadMagic` · bumped demo format version → `UnsupportedVersion` · flipped payload byte inside a segment → `SegmentChecksum` (hard error, not truncated) · cut mid-record in the final segment → truncated, prefix events recovered · trailer removed → truncated with ALL events recovered · trailer `totalEvents` tampered (checksum recomputed) → `TrailerCountMismatch` · broken trailer checksum → `TrailerChecksum` · smashed segment LLCS magic → hard segment error (`SegmentBadMagic` or `SegmentChecksum`; this run: `SegmentBadMagic`).

**Analysis — 6.** summarizeDemo total matches the script size · byKind exact (3 edits, 1 ray, 1 chunk, 1 locomotion, 1 frame) · bySource exact (Local 3, McpPilot 3, Script 1) · tick span exact (100..125) · demoEventsInTickRange interior range returns exactly its events (seq 1..4) · demoEventsInTickRange outside all ticks returns empty.

**Plus, `--demo-test`-only — the 12 demo-proof checks** (not in `--selftest`): recorded session stepped all 2048 ticks and stopped cleanly · recorder finished and captured every scripted event · recorded hash matches the control crew's scripted hash `5041cf1795c33a8c` · recorded session applied every event exactly once, none dropped/misordered · the recorded bytes parse complete with the session hash in the trailer · recorder saved `./demo_record_proof.lldemo` · non-perturbation — no-recorder hash identical to the recorded hash · DemoPlayer loads the saved record (complete, trailer hash matches) · legacy replay reproduces the recorded world bit-identically · replay applied every event exactly once (`dropped == 0`, `orderViolations == 0`) · handshake replay (2 workers) bit-identical to the recorded legacy session · recorded hash differs from the no-input baseline `2a2d19a416a1826e`.

---

## 8. Combined production-port manifest — ONE list for JD's single port decision

The control-serialization layer and IDemoRecord port **together, in one decision**: the demo system is meaningless without the stream beneath it, and four of IDemoRecord's modified files are the *same files* the control port already touches — additive hunks in the same files, not competing edits (stated plainly: **`engine/src/sim/CMakeLists.txt`, the game app `CMakeLists.txt`, `main.cpp`, and `selftest.cpp` appear in both lanes' modified sets; port them once, with both lanes' hunks**). Per-file internals for the control files are in §10 of the control report and are not re-listed here; every filename from both lanes *is* listed, so this page is complete on its own.

**Deduplicated totals: 10 new files + 8 modified files = 18 files.**

**New (10):**

Control-serialization (6 — details in control report §10):
- `engine/src/sim/include/litholattice/sim/control_stream.h`
- `engine/src/sim/control_stream.cpp`
- `game/LithoKraft/src/app/control_stream_test.h`
- `game/LithoKraft/src/app/control_stream_test.cpp`
- `game/LithoKraft/src/app/control_latency.h`
- `game/LithoKraft/src/app/control_latency.cpp`

IDemoRecord (4):
- `engine/src/sim/include/litholattice/sim/idemo_record.h` — `IDemoRecord` / `DemoRecorder` / `DemoPlayer`, the LLDM format contract, `DemoError`, `parseDemo`, the analysis queries (§1–§2).
- `engine/src/sim/demo_record.cpp` — recorder/player implementation, the fail-closed parser with torn-tail salvage, trailer build/verify (§2, §5).
- `game/LithoKraft/src/app/demo_record_test.h` — declarations for `runDemoTests` / `runDemoSelfTest` / `runDemoDump` / `runDemoVerify`.
- `game/LithoKraft/src/app/demo_record_test.cpp` — the 31 selftest checks, the 12 `--demo-test` SimThread proofs, and the `--demo-dump` / `--demo-verify` CLI (§3, §4, §7).

**Modified (8):**

Control-serialization (all 8; the last four also carry IDemoRecord hunks — see the overlap note above; control-hunk details in control report §10):
- `engine/src/sim/include/litholattice/sim/sim_thread.h`
- `engine/src/sim/sim_thread.cpp`
- `game/LithoKraft/src/app/handshake_test.h`
- `game/LithoKraft/src/app/handshake_test.cpp`
- `engine/src/sim/CMakeLists.txt` — *also IDemoRecord:* `demo_record.cpp` added to `litholattice_sim`, beside `control_stream.cpp`.
- `game/LithoKraft/src/app/CMakeLists.txt` — *also IDemoRecord:* `demo_record_test.cpp` joins the `lithokraft` target, beside the two control harness files.
- `game/LithoKraft/src/app/main.cpp` — *also IDemoRecord:* `--demo-test`, `--demo-dump FILE [--ticks LO HI]`, `--demo-verify FILE` flags and dispatch, beside the control flags.
- `game/LithoKraft/src/app/selftest.cpp` — *also IDemoRecord:* `fails += runDemoSelfTest();` after `runControlSelfTest()`, in the same section pattern.

After the copy: rebuild production `build-linux` and re-run §6's gates there (selftest counts become 1009; `--demo-test` OK; scenario stays `faec0f…`; the proof demo's four hashes stay `5041cf1795c33a8c`).

---

## 9. Deliberately not here

- **The production port itself** — JD's call, as a single combined decision on §8's manifest. Nothing in `~/workspace/litholattice` was modified by any lane of this workstream (§6 verifies it).
- **Playback UI** — no in-game player, transport controls, or timeline. The record is replayable *programmatically* (`submitInto` into a stepped SimThread, §3) and inspectable from the CLI (§4); a watch-it-back experience is a presentation build on top of those, not part of the record system's contract.
- **Per-pilot `ControlFrame` / `Locomotion` game-side taps** — still the documented gap carried over from control report §9.3: the kinds, the format, and this record system all carry these events (the selftest's round trip proves all five kinds field-for-field), but no game-side producer taps them where the game consumes them yet, so a live session record today contains the sim-boundary kinds (Edit / Ray / PlayerChunk) completely and the game-side kinds only when something submits them. Nothing sim-affecting is missing from a record; the attribution/completeness wiring for pilots is future work, unchanged in scope from the control report.
- **Demo compression** — segments are stored as raw LLCS bytes. The framing (§2) is compression-friendly (independent frames, one codec per segment when wanted), but no codec is selected, and none is needed at proof scale: the 2,048-tick scripted session's record is 398 bytes.
