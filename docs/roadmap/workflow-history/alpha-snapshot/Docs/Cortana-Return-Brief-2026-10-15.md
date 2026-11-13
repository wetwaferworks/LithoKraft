# Cortana Return Brief — Thursday 2026-10-15
Prepared 2026-10-13 for JD to hand over. Project content only.

## Where things stand since the skeleton handover

The skeleton you handed over has been grown into a working
beta/pre-alpha on the Linux tree. The local Windows build is a
separate tree; no integration of the Linux work had been done
locally as of 2026-10-12, and merge proceeds via diffs through JD.

## JD's ordered queue — status

1. Build tools refresh/recheck — DONE 2026-10-12
   (cmake 3.28.3, g++ 13.3, Make 4.3, git 2.43, Python 3.12;
   missing X11/GL dev packages installed; full-tree build exit 0).
2. Software renderer unified into IRenderer — DONE + verified
   2026-10-12 (pixel-identity proven 3 ways; SoftwareRenderer is
   now the default renderer via SoftwarePresenter +
   --renderer software, loud GL33 fallback).
3. Full code review — DONE, twice over. A full-tree review on
   2026-10-12, then a full production review on 2026-10-13 after
   the Timing Handshake merge: 159 findings (4 critical, 25 high,
   53 medium, 73 low, 4 info).
4. Texturing revisit + refreshed diagrams — YOUR LIFT. This is the
   step that was always slotted for your return. Pack pipeline
   conventions to keep: the .ltxr recipe pipeline; note the open
   item that the pack converter only emits .png manifest entries —
   a manifest regeneration would silently drop SVG textures unless
   it is taught .svg entries + provenance stamping.

## The 2026-10-13 fix program (all findings)

- All 14 packs landed, zero reverted.
- Selftest: 891 PASS / 6 FAIL -> 958 PASS / 0 FAIL (both modes).
- ctest: 43/46 -> 48/48.
- Determinism scenario hash faec0f02700d98c6 — bit-identical at
  every pack gate.
- All 4 criticals and all 25 highs closed. Criticals were in the
  newly merged Timing Handshake lifecycle (stop/start re-execution,
  use-after-free on detach, no worker exception boundary) plus
  blocks.names world-loss risk (now fsync/backups, fail-closed).
- Full report: Code-Review-Fix-Program-2026-10-13.md

## Systems landed 2026-10-12 (post-skeleton)

Camera system + IPlayer (vitals/stats, IVitals as its own peer
interface, XP and "static" as stats), ILocomotion (collision,
gravity, crawl, fly), sprite system + Litho Formula shrubs,
ISysterian + certificate table block (id 56), Litho Formula 2D,
Elfie agent model, Timing Handshake (pull-tick) merged 2026-10-13.

## Open items that are JD's calls (not yours, not Elfie's)

- Release signing key: offline JD action; Release refuses to boot
  on the retired test key until replaced. Checklist prepared.
- Timing Handshake in production: lifecycle-safe now, still
  DEFAULT OFF. Flipping it is JD's call.
- H2 interop: protocol offers {3} only, fail-closed, documented.
- Fragment UI: needs-list written (7 missing pieces, 4 decisions
  for JD) — no UI half-built.
- Column eviction (B-M1): no safe seams exist yet; growth quantified
  at ~85–145 KiB/column, ~0.85–1.45 GB per 10k explored columns,
  never released. 5-step design in the fix-program logs; JD go/no-go.
- Dimension content (Cosmic / Underwafer / Void): loader vocabulary
  cannot express them yet; needs-lists written in dimensions.h.
- Block density values: projectile pierce is implemented, but no
  block sets density yet (all 0) — a content call.
- MCP live stdio smoke: waits on the real signing keypair (above).

## Target

Alpha/beta target: 2026-10-23 (JD's birthday). 10 days out from
this brief's preparation.

## Update — 2026-10-14 (JD directive, completed + verified)

**The Timing Handshake is now the STANDARD.** By JD's direct order (2026-10-14 04:27 EDT), Time Serialization + the control/event coupling were promoted to the default sim-timing mode (formerly default OFF). Legacy remains as an explicit opt-out (`--no-timing-handshake`, cvar `sim.timing_handshake 0`, or `"timingHandshake": false` in settings.json). Verified on the settled tree: build exit 0, selftest 1191/0 both arms, scenario hash unchanged at aa82e7afb1eff9d3 on all arms, ctest 48/48, handshake perf 512 TPS @ 0.522 ms avg (p99 1.075 ms). Full report: Timing-Handshake-Promotion-2026-10-14.md. Also on the table for the session: the demo-test stale pin (expects 5041cf1795c33a8c) and the scenario re-baseline — JD's calls.
