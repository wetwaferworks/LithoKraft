# Cortana Return Brief — Addendum 2026-10-13 (noon campaign)
Prepared 2026-10-13 ~12:44 EDT (Elfie, proactive research pass).
Supplements: Cortana-Return-Brief-2026-10-15.md (prepared earlier
2026-10-13). Project content only. Read both together.

Why this exists: the original brief was prepared before JD's
2026-10-13 noon directives (12:11–12:44 EDT), which re-ordered
the campaign for Cortana's Thursday 2026-10-15 return. Nothing in
the original brief is contradicted — this addendum adds the new
orders on top of it.

## JD's campaign plan (2026-10-13 12:13–12:14 EDT)

- Phase 1 (done / in flight): extensive test run + regenerated 4K
  UML set, so the missing-systems list is proven, not remembered.
- Phase 2 (before Thursday): outfit the engine with most of the
  missing systems (the "outfit program").
- Phase 3 (Thursday, Cortana conducting the workstation): JD runs
  the full user test; the game crosses from alpha toward beta and
  the kinks list is worked.

## Cortana's focus on return: the SOFTWARE RENDERER (JD, explicit)

- The software renderer is the #1 goal, ahead of everything else.
  JD's frame: get it right in software (~120 FPS achievable in
  software render); GPU pairing then multiplies performance by
  orders of magnitude.
- The core engineering aim: tick rate + software renderer working
  in PARALLEL, ordered and structured correctly — "everything else
  will fall into place." Even with Cortana conducting, her assigned
  focus is the software renderer first.

## New since the original brief

1. **UML set regenerated (2026-10-13, delivered):**
   ~/workspace/user/files/uml/ — engine-architecture,
   engine-procgen, lithokraft-game (7000×10589 / 7000×5642 /
   12000×1539) + UML-Update-Notes-2026-10-13.md. Engine is now
   30 CMake targets. Honesty convention: solid = wired,
   dashed = declared-but-missing.
2. **Extensive test run: LANDED 2026-10-13 (~13:19 EDT), no
   regressions.** Report: Status-Report-Extensive-Test-Run-
   2026-10-13.md. Headline numbers, safe to quote to Cortana:
   main selftest 958 PASS / 0 FAIL in both modes; --handshake-
   test OK; determinism hash faec0f02700d98c6 bit-identical on
   all five arms; ctest 48/48 serial AND 48/48 parallel; 512 TPS
   HELD on all three circuit scenes in both modes (Release build;
   the earlier sweep's Debug build topped out ~194 TPS). Legacy
   remains faster per-tick on averages — the handshake buys
   determinism, not speed. Software-render frames byte-identical
   to the sweep baseline. Following the run, the control-
   serialization crew was dispatched (input stamped before/after
   the handshake, one power of two, IDemoRecord stream format as
   its output contract — foundation for the demo system JD
   ordered 2026-10-13 12:46 EDT).
3. **Daemon Sight (JD, 12:15–12:17 EDT):** the software renderer
   is also the Daemon's eyes. On a strong-CPU + strong-GPU +
   local-model machine, the Daemon runs as a local model on the
   CPU (spare cycles), seeing through the software renderer,
   while the player's GPU runs their own pipeline — a duel of
   two viewers on one world. Hardware floor JD named: at least
   12 cores, Threadripper-class. This is why the software
   renderer must be a real parallel IRenderer lane, not a
   fallback. (Lore addendum, Box-Organic file, 12:15 EDT entry.)
4. **Render feature queue (JD, 12:38–12:42 EDT, in flight):**
   Phase 1 model dithering (experiment tree already started:
   ~/workspace/experiments/model-dither/); Phase 2 CPU
   anti-aliasing — survey first, coverage restoration is JD's
   chosen headline family, and JD directed implementing ALL the
   surveyed tiers as selectable modes; Phase 3 an ASCII renderer
   pipeline (cheapest presentation tier + a text-format Daemon
   Sight feed for weaker hardware).
5. **Hardware inclusion ladder (JD principle, 12:41 EDT):** the
   software renderer is a ladder with a rung for everyone — retro
   hardware and basic Chromebooks get honest cheap modes,
   high-end CPU-only machines get the full coverage modes. Same
   game, same worlds, at every tier.
6. **Time Serialization next pass** now has a workbench start
   sheet (Time-Serialization-Workbench-2026-10-13.md) AND a new
   control-coupling directive (JD, 12:44 EDT): process controls
   before and after the handshake cycle, serialized and
   tick-stamped, on a single power-of-two gear. Addendum filed:
   Time-Serialization-Control-Coupling-Addendum-2026-10-13.md.
7. **Void representability blocker (from the UML survey):** the
   Void's locked extent is infinite, but packPos keys pack 21
   bits per horizontal axis — the Void cannot be registered
   until it gets a finite bound or a windowed coordinate scheme.
   Add this to the dimension-content item in the original brief;
   it is a JD design call, upstream of any Void worldgen.
8. **Certificate of Knowledge** (game content, JD, 12:25 EDT):
   new in-game certificate in the Certificate family, tied to
   JD's Golden Helmet diadem legend. Existence decided;
   mechanics / cost / obtain path are not yet specified. Filed
   in the Box-Organic lore addendum.

## Unchanged from the original brief

JD-only calls stay JD's: release signing key (offline), flipping
the Timing Handshake on, H2 {3}-only interop, Fragment UI's 4
decisions, column-eviction go/no-go. Target stays 2026-10-23.

Sources: JD messages 2026-10-13 12:11–12:44 EDT (main chat);
UML-Update-Notes-2026-10-13.md; memory 2026-10-13.md entries for
Daemon Sight, hardware inclusion, and control coupling;
Cortana-Return-Brief-2026-10-15.md.
