# LithoLattice — Workstation Package
October 10, 2026. Built by Elfie. For Cortana / workstation testing.

## What's in this package

Full source tree (`engine/src`, `game/`) with all of today's work. Based on the
2026-10-10 handoff (branch `bench/high-tps`, commit `100ecf3`).

## Change log (today)

### Wireless Signal (new)
- Signal Transmittor (`lithokraft:signal_transmittor`) — input/beacon modes, first-touch color bind, sneak+use channel cycle.
- Signal Receivor (`lithokraft:signal_receivor`) — outputs received signal to bound trace color.
- Dish (`lithokraft:dish`) — long-range receivor, 1024 sensitivity.
- Overclockor (`lithokraft:overclockor`) — beacon equivalent; jammer mode (use toggles item, sneak+use cycles jam channel). Channel-selective: blocks all channels except its own within 64 blocks.
- RadioRegistry (link budget: range = power 256 + sensitivity), JammerRegistry, 1-tick latency, 4→16→64 channel gating.
- Save migration SGNL v1→v2 fixed (channel/beacon before key count) + tests.

### Entities & Physics (new)
- Engine `litholattice::entity`: EntityManager, IEntityBehavior, `moveCollide` (AABB-vs-voxel).
- Game ItemBehavior: gravity, real collision (via SimThread::getBlock), block friction, magnet, pickup, 8-min despawn.
- Popped trace spawns item-drop entities (via SimEvent).
- BlockDef.friction added (default 0.8; 1.0 = ice).

### GPU Trace Optimization (Steps 1 & 2, from Cortana's brief)
- **Step 2 (done, tested)**: net marks bit-packed (32 per uint32), CTZ iteration. Incremental-vs-full-pass gate passes.
- **Step 1 (CPU side done)**: TraceStrengthBuffer — slot per trace cell, 4 strengths per uint32, dirty-word tracking. GL SSBO upload + shader change needed (see below).

### Standards
- SimRole::SignalTx/Rx → SignalTransmittor/SignalReceivor (full -OR).
- All new code follows `litholattice::`, `I`-prefix, `k`-prefix, Trace/Signal conventions.

## Build (workstation, Linux)

```bash
cd litholattice
# Symlink expected by CMake (game inside engine tree):
ln -s ../game/LithoKraft engine/LithoKraft
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
# Tests:
./LithoKraft --selftest
```

Requires: CMake 3.28+, GCC 13+, X11 dev packages (`libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`).

## What to measure (Step 1 validation)

1. Wire the GL SSBO: upload `TraceStrengthBuffer::packedWord(i)` for each dirty word index from `dirtyWordIndices()`, then `uploadAck()`.
2. Shader: vertex `value` becomes the slot index; strength = `ssbo[slot>>2] >> ((slot&3)*8) & 0xFF`.
3. Measure upload bytes/frame on wall-pixel and pattern-wall 64. Baseline (CPU): ~0.5–1.7 MB/frame. Target: only changed words.

## Known limitations

- Vulkan init fails without a GPU (expected in headless VM; works on real hardware).
- Item rendering not yet implemented (entities are sim-functional, invisible).
- 42-slot inventory data model not yet built.
- Overclockor tick accelerator stubbed (tiers copper/silver/violet planned).
