# Handshake-Coupled Meshing: Design and Status (2026-10-15)

**Status: NOT implemented in the Elfie handoff package.** JD set a hard cutoff of 12:40 ET. The focus-bug fix came first, and the feature could not be built *and* verified (Debug ctest, verify-mode with zero mismatches, mesh rate on 1/2/4/8/12 threads, and FPS) before then. The package therefore ships the original mesher path, with nothing half-built. Cortana will implement it after the handoff.

**Credits.** JD came up with the coupling idea: meshing rides the timing handshake. Cortana (Grok Bot) refined it into the stable-copy read, publish-at-phase-boundary, and debug-only checksum design below. It builds on Elfie's TimingHandshakeAuthority and job system.

## Problem
Mesh throughput falls as threads are added: 3,980 sub-chunks/s on 1 thread versus 966/s on 12. Mesher jobs read live chunk data under the sim's block mutex (recursive) and contend with the tick and with each other.

## Design (engine/)
1. **Read the stable copy, lock-free.** In a handshake phase the sim writes one copy (the phase's write set); the other copy is settled. Mesher jobs snapshot the settled copy's sub-chunk plus its 6 neighbours, which is a read-only span, without taking blockMutex_. Readers pin a phase epoch (an atomic counter), so the authority never recycles a copy while a job still holds it.
2. **Publish at the phase boundary.** Finished meshes go to a per-phase staging list. At the handshake phase boundary, the authority's post-phase hook swaps that list into the renderer's mesh queue. The renderer then never sees a mix of meshes from two different world states.
3. **Verify mode** (Debug default, and `--verify-mesh` in Release). Each sub-chunk is meshed from both the stable copy and the locked live path, and the two results are compared with a 64-bit FNV-1a checksum over the vertex bytes. A mismatch is logged with key, phase and both sums, and asserts in Debug. Release meshes once.
4. **Determinism.** Meshing only reads world state and never writes it, so the sim hash `aa82e7afb1eff9d3` cannot move. CI must still check it.

## Test plan, once implemented
- Debug ctest at or above the current baseline.
- `--verify-mesh` soak with 0 mismatches.
- `--perf-headless` mesh rate on 1/2/4/8/12 threads.
- Orbit and stream at 512 TPS in borderless mode.
- Hash checked in both configurations.
