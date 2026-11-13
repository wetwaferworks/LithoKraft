# LithoKraft Mod-Developer Architecture — Elfie's Review
October 10, 2026. Review of Cortana's "Architecture Brief from a Mod Developer's Seat." All 13 decisions approved by JD.

## The 13 calls
1. **Trace placement:** stays in `lithokraft_content` for v1. Red/blue channels are game content; the engine holds none. `ISignalNode` is the SDK seam.
2. **Renames:** do them now while migration is cheap — Pulsor, Memor, Thresholdor. **"Trace" locked** as the wire name (red trace / blue trace — not redstone).
3. **Block JSON schema:** minimal + extensible (`id/shape/tex/behavior/flags`, power properties, typed `settings`). Design to express the Catchor fully, then stop.
4. **Power properties:** conductive / insulator / cuts-trace / transparent-opaque / solid-half → engine flags. Heat-conductive, weak-only, color-filtered, attenuating → game bits (promotable later).
5. **Config storage:** `ComponentMemory`/`SGNL`, not `CellState` (16 bits spoken for; Catchor needs ~11 more).
6. **Mask roles:** fixed generic set (digits, bar, ring). No mod-shipped shader code. Catchor readout = "digits" role.
7. **Binding UX:** break-and-replace + sneak+use to clear/rebind. Generic `ISignalNode` policy, one interaction language.
8. **Script ticks v1:** command-driven only (main thread → SimCommand queue). No sim-thread tickables in v1.
9. **Boundary before bindings:** DLL-ready IMod boundary first; AngelScript bindings mirror the final interfaces.
10. **Profiler:** per-mod cost visible to players in the F3 overlay (opt-in), never a popup.
11. **Tick domains:** true clock domains with latched boundaries. No per-section multipliers (determinism).
12. **Sample game:** minimum hook coverage — Pulsor, Trace, one random-tickable, Button, Memor, one UI panel. 6–8 blocks, zero spoilers.
13. **Missing docs:** JD/Cortana to supply `SAVE_FORMAT.md` + `CHANGELOG.md` contents — not guessed.

## Other notes
- **Catchor approved** as designed (exact-match 1–64 decoder, first-touch binding, L/C/R output).
- **Tone Block** (strength=pitch SID synth): circuits making music is a YouTube-selling feature.
- **Red/Blue Via** approved: lossless vertical links, hollow insulated tubes.
- Cleanup flag: leftover `vc/` folder in `src/game/include/` from the Voxel Compute name — remove before it fossilizes.
- 283 selftests passing at last count.
