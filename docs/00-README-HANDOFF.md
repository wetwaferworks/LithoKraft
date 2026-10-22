# LithoLattice + LithoKraft: Handoff for Elfie (CONFIDENTIAL, JD and Elfie only)
October 10, 2026. From Cortana for JD. Elfie leads implementation discussion until the builder returns (about Oct 12), then the builder does code reviews, deeper implementation, and bug fixes.

## What's in the package
- `LithoLattice-engine-src-*.zip`: the engine (repo root without `LithoKraft/`), namespace `litholattice::`.
- `LithoKraft-game-src-*.zip`: the game, namespace `lithokraft::` (the `LithoKraft/` folder). It depends one way, game to engine.
- `docs/`: this README, ROADMAP, the full design brief, the mod-developer architecture brief, the Trace and components brief, the night update, SDK and architecture docs, and diagrams.
- Source is from branch `bench/high-tps` at commit `100ecf3` (main plus all of last night's Trace and benchmark work). Build: CMake + MSVC, Windows, Vulkan primary, GL fallback. `tools\build.ps1`. Self-tests: run the exe with `--selftest`.

## The heart of it: Trace and the tick system (Moore's law in the engine)
- Trace (red and blue) is the signal system. It's part of LithoLattice's timing engine, and it's what LithoKraft lives or dies on.
- Tick rate is the progression curve: as in-game tech advances, and as real hardware improves, Trace can tick faster. Launch baseline is 512 TPS. The cap is a power-of-2 setting (1024 up to 8192+). This makes it a "forever game" that scales with hardware.
- Proven tonight: 8192 TPS in static scenes, and every test screen (16x16 per-pixel, 32x32 and 64x64 pattern walls, sawtooth, wire-torture) at 60 FPS and 512 TPS on JD's RTX 3050 Ti laptop.
- Proposed OOD (for review, see the architecture brief): small inheritable interfaces IBlock, ITickable, IRandomTickable, ISignalNode, plus TickContext. One shared behavior object per block type and a compact per-block state. Block properties are data flags, not subclasses. A scheduler with active lists per section and tick domain. Graph compile and JIT with per-node and per-island profiling. Mods (C++ and AngelScript) implement the same interfaces.

## Questions for Elfie to drive
1. Lock the tick-interface design (above) or change it?
2. Catchor first-touch auto-connect: keep, or add a reset?
3. Via tier, Pattern ROM source and tier, and which idea-list blocks to promote.
4. Rename code identifiers to -OR names now (Pulsar to Pulsor, Memory Cell to Memor, Thresholder to Thresholdor, wire to Trace)? Block ID changes are cheapest now; saves use a name table.
5. Enthusiast tick tier: lore-gated or a settings toggle?
