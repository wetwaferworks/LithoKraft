# Time Serialization — Workbench Start Sheet
Prepared 2026-10-13 (Elfie, research pass). Project content only.
Status: NOT started. JD seeded this pass 2026-10-13 04:56 EDT —
"serialize for the whole algorithm" — queued behind the full code
review. The review and its fix program are now complete, so this
is the next item in JD's own queue.

## Why this is the long pole (from the in-game sweep report)

- Default scene is components-dominated: of ~1,090 ms phase-ms per
  measurement window, ~1,010–1,030 ms is `comps`, thread-affine in
  v1. The handshake's parallel kernels there (wires ~45 ms, lamps
  ~1.5 ms) are only ~4% of the tick.
- Sweep §8.2: parallelising `tickComponents` (per-component input
  snapshots + deferred apply) is where the default scene's ~95%
  affine time lives; it is the correctness-critical core and was
  deliberately left affine in v1.
- JD's framing (04:56 EDT): apply the serialization discipline to
  the WHOLE algorithm — an end-to-end serialized timeline — not
  just the currently-parallel kernels.

## The one scoping call for JD at kickoff

Do these two readings converge in one design — components brought
under the handshake's publication/serialization discipline
(snapshot inputs at phase open, deferred apply in commit order) —
or does JD want literal end-to-end serialization first as a
correctness reference implementation, with parallelisation after?
Elfie's reading: the sweep mechanism IS the whole-algorithm
serialization, applied to the one phase still outside it. JD locks.

## Gates (current post-fix baselines — do not use sweep-era ones)

- Determinism scenario hash faec0f02700d98c6 must stay bit-identical
  at every step (it held at every pack gate of the fix program).
- Selftest baseline is now 958 PASS / 0 FAIL (both modes) — the
  sweep-era figure was 891/6; the fix program closed those 6.
- ctest baseline is now 48/48 (sweep-era: 42–43/46).
- Timing Handshake stays DEFAULT OFF until JD flips it.

## Bundled small follow-up (flagged in sweep §8.3, deferred from merge)

- C2 offender accounting: judge repeat-offender flags against the
  CYCLE budget, not the single-phase budget (per-phase budget of
  master-step ÷ P guarantees false offenders at 512 TPS in Debug).
- Empty-name diagnostic defect: offender ids 2/3 logged as '' —
  fix in the same pass so the offender log actually names the
  component. Merge report confirms both were intentionally held
  for this optimization pass.

## Honest limits

- Perf win is NOT measurable on this 2-core VM (sweep §6.2): the
  win condition is heavy per-fire work × ≥2 workers × Release on
  JD's real hardware. This pass is a correctness/architecture pass
  first; perf claims wait for his machine.
- Interactive smoke (windowed game, handshake on, dimension reload)
  remains code-reviewed only — no display on the VM.

Sources: Timing-Handshake-InGame-Sweep-2026-10-13.md §§6.2, 7-C2,
8.2–8.3; Timing-Handshake-Merge-2026-10-13.md; Code-Review-Fix-
Program-2026-10-13.md (baselines + deferred list); JD directive
2026-10-13 04:56 EDT (memory 2026-10-13.md).
