# ASCII + Systerian Renderer — Phase 3 Report (2026-10-13)

**Status: COMPLETE and verified** (experiment tier) in
`~/workspace/experiments/model-dither/`. Production tree untouched
(verified against the fix-program manifest: zero diffs). JD's orders:
an ASCII renderer pipeline (12:42 EDT, inspired by ASCII City), then
(14:04 EDT): "throw in some of the Systerian characters, using the map
that we have for the ASCII-to-Systerian map — oh wait, maybe we just
do a SYSTERIAN RENDERER." Both shipped: one glyph-grid post stage, two
glyph sets, Systerian as the headline.

---

## 1. The ASCII→Systerian map — located, not invented

The map JD referred to exists in the production engine, exactly where
his materials said it would be:
**`engine/src/systerian/include/litholattice/systerian/isysterian.h`** —
the engine's certificate-signing script standard. Its ASCII layer, in
the header's own words: *a printable ASCII char (32..126) maps to the
glyph of its own code point ('A' = 65 → glyph 65)* — `valueForAscii()`.

Systerian is a Cistercian-style numeral script: a value 0..9999 is ONE
glyph (vertical stave + digit strokes by decimal place, rivet dots at
every graph vertex — JD's rivet rule). Crucially the codec is
**generative**: `composedGlyph(value)` derives the full stroke
geometry from the 9 unit strokes + place transforms, so real glyph
artwork exists in the tree with no asset files. The Systerian renderer
below draws from that geometry directly. The codec's own ctest
(`engine_systerian`, passing) verifies it against JD's reference
generator masters, including `'A' → 65` and `' ' → 32`.

The ramp mapping this produces (luminance ramp → Systerian value):
`' '→32  '.'→46  ':'→58  '-'→45  '='→61  '+'→43  '*'→42  '#'→35
'%'→37  '@'→64`. Note the darkest cell is *not* blank in Systerian
mode — value 32 is a real glyph (3 tens + 2 units). That is faithful
to the map, and it is what makes the mode read as a script rather than
as ASCII with a costume on.

## 2. What was built

A post stage on the software renderer's finished frame
(`engine/src/render/software_ascii.{h,cpp}`):

- **Grid:** the frame is summarized into cells (~1:2 character
  aspect); each cell's average luminance selects a ramp level, its
  average colour tints the glyph.
- **ASCII set:** text grid of ramp characters + an image visualization
  (hand-built 5×7 bitmap font for the ten ramp glyphs, no external
  font dependency).
- **Systerian set (the SYSTERIAN RENDERER):** the same grid, but each
  cell draws the real Systerian glyph for its mapped value — stave and
  digit strokes plotted from `composedGlyph()` geometry scaled into
  the cell, rivet dots at the graph vertices, tinted by the cell
  colour. Text form: Systerian has no Unicode block, so the `.txt`
  is the **glyph-value grid** — each cell as its two-digit code-point
  value, fixed width, machine-parseable (e.g. a mid-gray cell reads
  `58`, a bright cell `64`). That *is* the faithful text form of the
  script: values are its native encoding.
- **Selector + cvars:** `render.ascii` (bool, default off),
  `render.ascii_cols` (40..400, default 160), `render.ascii_set`
  (`ascii` | `systerian`, default `ascii`), applied live through
  `IRenderer::setAsciiMode` / `setAsciiGlyphSet`. In-game, the stage
  runs after AA and before overlays/HUD, and the raw text grid is
  retained on the renderer (`asciiText()`) for machine consumers.

## 3. Agent-vision economics (the Daemon Sight low-end configuration)

A text grid is the cheapest "vision" a local model can be fed — it
reads text natively, no image encoder, no pixel tokens. Measured on
the 1280×720 spires frame (raw RGB frame = 2,764,800 bytes):

| Grid | ASCII bytes/frame | Systerian bytes/frame | vs raw frame |
|---|---|---|---|
| 80 cols (80×23) | 1,863 | 3,703 | 0.07% / 0.13% |
| 120 cols (120×34) | 4,114 | 8,194 | 0.15% / 0.30% |
| 160 cols (160×45) | 7,245 | 14,445 | 0.26% / 0.52% |
| 200 cols (200×56) | 11,256 | 22,456 | 0.41% / 0.81% |
| 240 cols (240×68) | 16,388 | 32,708 | 0.59% / 1.18% |

At a 10 Hz agent-vision rate, the 120-col ASCII feed is ~41 KB/s
against ~27.6 MB/s of raw frames — roughly **three orders of
magnitude** cheaper, on hardware that couldn't run an image model at
all. Systerian costs exactly 2× the ASCII bytes (two digits per cell)
and buys in-fiction legibility: a Daemon that literally sees the world
in its own signing script. Conversion cost is trivial either way:
**5.54 ms** (ASCII) / **11.13 ms** (Systerian) per 720p frame at 160
cols on the 2-core VM, dominated by the cell-averaging pass.

This is a proposal-level observation backed by measurements, not a
built Daemon feed — the MCP/agent plumbing that would consume
`asciiText()` per tick is future work, and the numbers above are the
budget it would live within.

## 4. Visual evidence (read directly)

Captures: `~/workspace/user/files/render-lab-2026-10-13/`

- `spires_ascii_160.png`: the spires scene is unmistakably itself —
  bright glyph field for sky, the descending terrain bowl in coloured
  `=`,`+`,`#` runs, cyan circuitry and salmon accents in their true
  positions, dark void floor near-blank. Terrain, structures and
  traces are legible as shapes.
- `spires_systerian_160.png`: cell-for-cell the same scene in the
  signing script — a dense field of staved numeral glyphs; silhouette
  and colour regions match the ASCII frame exactly. At 160 cols the
  individual glyphs are small (8 px cells); for glyph appreciation the
  close-up is better:
- `closeup_systerian_160.png` / `closeup_ascii_160.png`: near geometry
  reads clearly in both sets.
- The `.txt` grids sit beside the PNGs (`spires_ascii_160.txt`,
  `spires_systerian_160.txt`, closeups likewise) — the Systerian txt
  is digit pairs throughout, parseable back to ramp levels exactly.

## 5. Verification

New engine checks (in `litholattice_tests render`, all passing):
ASCII grid geometry + byte accounting; ramp mapping (dark cell →
space, bright cell → `@`); image output non-empty for the bright
half; Systerian set name roundtrip; **value-grid mapping exact**
(mid-gray → `58`, white → `64`, per the located map); Systerian image
differs from the ASCII image and its glyph strokes actually draw
(1,015 lit pixels in the test cell field vs the ASCII set's sparser
marks). Standard gates in the final tree: build exit 0; game
selftest 958/0 both modes; `--handshake-test` OK; scenario hash
`faec0f02700d98c6` unchanged; ctest 48/48 effective. Presentational
only — nothing here can touch the sim.

## 6. Production port instructions (Phase 3 file set — independent)

- `engine/src/render/include/litholattice/render/software_ascii.h` (new)
- `engine/src/render/software_ascii.cpp` (new)
- `engine/src/render/CMakeLists.txt` — add `software_ascii.cpp` and
  the `PRIVATE litholattice_systerian` link on `litholattice_render`
- `engine/src/render/include/litholattice/render/renderer.h` —
  `setAsciiMode` + `setAsciiGlyphSet` virtuals only
- `engine/src/render/include/litholattice/render/software_renderer.h`
  + `engine/src/render/software_renderer.cpp` — ASCII state,
  `applyAsciiPass`, renderFrame ASCII stage
- `game/LithoKraft/src/game/include/lithokraft/game/settings.h` —
  `asciiMode` / `asciiCols` / `asciiSet` fields
- `game/LithoKraft/src/game/settings_cvars.cpp` — `render.ascii`,
  `render.ascii_cols`, `render.ascii_set` rows + sync lines
- `game/LithoKraft/src/game/game.cpp` — applies at init + on change
- Shared test wiring (once for all phases): `software_test.h` (new),
  `software_test.cpp`, `engine/tests/CMakeLists.txt`,
  `engine/tests/engine_tests.cpp`

Experiment-tier label stands: JD green-lights the port; the lab in
`biome_capture.cpp` is evidence tooling and does **not** port.
