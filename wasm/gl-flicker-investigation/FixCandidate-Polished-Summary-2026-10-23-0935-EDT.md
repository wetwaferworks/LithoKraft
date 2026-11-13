# Flicker-Fix Candidate — POLISHED — Summary — 2026-10-23 0935 EDT

**Package:** `LithoKraft-Itch-FixCandidate-Polished-2026-10-23-0935-EDT.zip` · supersedes the 0835 fix candidate (same bits except the driver polish below).
**Identity (labeled, separate):** shell — none (web package) · in-binary rolling counter still **0.0.0.0.0.0.1.17** · game Alpha **0.0.3.21**. A candidate, not a sealed build; the next seal's identity is JD's lock.
**Driver:** tree `wasm/full/lkboot.js`, hash **c04dc90e…** (JD verified the unpolished 4cd379ce build on itch at ~09:30 EDT — flicker FIXED).

## Code review of the landed fix (registrar, 2026-10-23)
Findings, all checked against the source:
1. Snapshot extents mirror the impls exactly (Uniform vec count×n; matrices count×cols×rows; BufferData/SubData size arg) — no over/under-read vs the original paths.
2. Gen/Create regex cannot catch reserved context ops (their funcIdx has no LK_GL_NAMES entry → tests as '') and matches only pure producers in the GL namespace.
3. Zero-init BufferData (ptr=0) snapshots as "already pure" and keeps the size-form impl path — correct.
4. Snapshot replies post the same value-slot protocol as the generic sync path (tag 0, value 0 — these calls are void).
5. Remaining sync-by-design (documented in the driver header): Tex readers, ShaderSource, VertexAttrib*fv, ClearBuffer*fv, the Get family — none appear in the itch census tops.
6. **One real defect found and polished:** the snapshot call sat OUTSIDE the drain's try/catch — a snapshot that threw (bad extent, exotic arg shape) would have killed the drain and hung the engine. Now guarded: any snapshot failure falls back to the original flush-then-execute path for that record. This is the ONLY change vs the itch-verified driver.

## Re-verification of the polished driver (local bench, SwiftShader)
- GL settled: **0.0% black, 41,032 uniques, mean 59.3** · page errors 0 · texture-array error absent.
- SW settled: **0.0% black, 37,005 uniques, mean 48.3** · page errors 0.
