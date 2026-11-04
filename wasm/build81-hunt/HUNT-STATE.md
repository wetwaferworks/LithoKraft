# Build 81 present hunt — state of the evidence (Elfie, 2026-10-23 05:56 EDT)

**The fault (JD's itch test, 2026-10-22):** build 81 (counter 0.0.0.0.0.0.1.17,
Alpha 0.0.3.21) draws NOTHING on itch — uniform black canvas on BOTH the GL and
software pages, exit 0, logs complete. Build 80 on itch flickered but DREW.
Build 81 is sealed as a specimen, NOT shippable.

## What the counts have established (local headless bench, bundle81r lineage)

- Routing is innocent: 4,710–11,197 pixel records marked dirty against the
  surface across runs; mdNotRouted = 0 in every instrumented run.
- Presents fire all session: 42–62 PresentFrame calls per run, timestamped
  through t=92 s, long after Enter. Frame-done starvation is DEAD.
- Binds are textbook (monkey-patch log of every physical bindFramebuffer,
  dbg7): make-current binds the surface once; each present binds
  READ=surface / DRAW=null for the blit, then ApplyBindings restores
  READ+DRAW=surface. 241 binds, one context, nothing else binds. Ever.
- At settle, surface and canvas hold IDENTICAL content: 64/64 blocks equal
  (dbg9), a real scene (bright sky band, dark terrain), viewport
  [0,0,1280,720], scissor off, glError 0 — and ALPHA = 255 in every block of
  both buffers (dbg10). The alpha-compositing theory is DEAD.
- lkSetCanvasSize fired exactly once per run (t≈1.9 s, same-value 1280x720) —
  the per-frame resize theory is DEAD.
- Open anomaly: getParameter(DRAW_FRAMEBUFFER_BINDING) read at MarkDirty
  moments returns null (73/73 across dbg6/dbg7), contradicting the bind log.
  Unresolved; possibly a probe-placement artifact (MarkDirty runs at
  lkGLExecute entry, before the record's own state application).

## Environment gap

Local headless (SwiftShader) does NOT reproduce the itch total-black; every
local probe shows real pixels moving. Remaining deltas vs itch: real-GPU
ANGLE, the itch iframe embed + its canvas sizing, crossOriginIsolated=true
(already matched locally — serve81.js sends COOP/COEP). BOTH pages failing on
itch, including the software page (no GL surface involved), points at a
shared layer: boot / canvas / transport, not only the GL present verb.

## Next steps (in order)

1. SW page instrumented run (io tally — lkioPresent behavior), still owed.
2. A/B census with CORRECTED geometry: crop screenshots to the canvas's
   real getBoundingClientRect (the old band crop may have measured page
   chrome; the Oct 22 conviction relied on the same harness for both sides,
   so the A/B difference stands, but absolute numbers are suspect).
3. If the trail stays cold locally: pair with Cortana on the code (JD's
   instruction 2026-10-23) and aim at a device/itch-shaped test.

Instruments: dbg-proof.js / dbg-proof3.js runners; bundleDBG* trees are
instrumented copies of bundle81r (never ship). Candidate source: lkboot-b81.js.
