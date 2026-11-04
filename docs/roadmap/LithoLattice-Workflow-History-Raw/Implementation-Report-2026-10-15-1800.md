# Implementation Report, 2026-10-15 18:00 ET (batch 16:25 to 17:50)

The order JD set was CA-03, then thread-audit F-2/F-4/F-5/F-8, then greedy meshing, then Hi-Z. After that came the self-review and verification. The full rows, numbers and backups are in CHANGES.md (see the section "Batch 16:25 to 17:50 ET").

1. **CA-03.** `PackedVertex{... uint32_t slot:24, layerHi:8}`, still 16 bytes. The layer travels through FaceMaterial, the mesher Look and the software varyings as 16 bits. GL and VK use a new byte-15 attribute. `kMaxLayers` is 2048. Pixels and the golden mesh checksum are unchanged; perf is neutral.
2. **F-2:** a Debug single-reader assert, plus a fix for a real MCP-thread read of `SimThread::stats()`. **F-4:** voice-count accessors. **F-5:** a registry instance id in the StableDefScope key. **F-8:** the seam audit is documented.
3. **Greedy meshing.** Merges are opt-in per `MeshContext::greedy`, which the game sets from `IRenderer::prefersGreedyMeshes()` (software = true). `swTile()` wraps u/v above 1 and is bit-exact in [0,1].
4. **Hi-Z.** Each 8x8 tile keeps a depth upper bound with lazy re-tightening. Rejection is per 8-row band, so drawn rows are bit-identical. Tiles align with the 32-row band strips.
5. **Self-review findings fixed while writing:**
   - Avoided a 196 KB stack temporary when clearing the greedy grid.
   - Kept greedy away from Trace, strip and other per-cell modes, so the Trace-mix pulse counts are unchanged.
   - Disabled Hi-Z after any depth re-fill that raises depth (the SSAA resolve, resize).
   - Killed an orphaned cl.exe left by an interrupted build.

**Verification:**
- Hash `aa82e7afb1eff9d3` in Release and Debug.
- Release and Debug ctest 46/48 (environment-only failures).
- Selftest 1238/0.
- Pixels 7/7 (default).
- Benches are in CHANGES.md.