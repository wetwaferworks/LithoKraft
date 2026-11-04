# Certificate Signing — Glyph Spec (2026-10-12)

**Decision (JD):** LithoKraft's enchantment system is called **certificate signing**, performed
at the **certificate table**. Its script is the **Cistercian numeral system** (13th-century
monastic ciphers, vertical-stave revival form) — adopted as the engine's certificate-signing
standard. Medieval monks meet lithography.

## The glyph set

36 component glyphs: digits 1–9 × four place positions. Any integer 1–9999 is one composed
glyph (single stave + up to four strokes).

**Construction (vertical stave):**
| Place | Position | Transform of unit stroke |
|---|---|---|
| Units | upper right | — |
| Tens | upper left | mirror ←→ |
| Hundreds | lower right | flip ↑↓ |
| Thousands | lower left | mirror + flip |

Digit strokes (units form; others transformed): 1 = top horizontal; 2 = mid horizontal;
3 = diagonal top→down-right; 4 = diagonal mid→up-right; 5 = triangular pennant;
6 = detached vertical dash; 7 = corner (horizontal + down); 8 = mid corner;
9 = rectangular pennant.

**JD's rivet rule:** a small round dot at *every* vertex of the stroke graph — line
junctions, crossings, corners, and free line ends (including stave ends). Dots are
deduplicated by coordinate when glyphs compose.

**Halo outline (locked 2026-10-12):** every stroke and every rivet carries a thin outline
pass beneath it (2 units on the 100×140 grid) — **red** `#FF3B30` halo for dark backgrounds
(`on_dark` variant), **blue** `#2F7CF6` halo for light backgrounds (`on_light` variant):
the trace colors. Pure SVG technique: each element drawn twice, halo pass slightly larger
underneath. Maximum readability and distinctness on any background.

## Files

`~/workspace/user/files/cistercian/` (generator: `generate.py` — fully reproducible):
- `svg/{violet,white}/{on_dark,on_light}/cistercian_<value>.svg` — 144 masters (36 glyphs × 2 line colors × 2 halo variants), transparent, infinitely scalable
- `png/{violet,white}/{on_dark,on_light}/cistercian_<value>.png` — 144 renders, 700×980, transparent background
- `composed/cistercian_{1111,2026,9999}.{svg,png}` — composition examples
- `specimen.{svg,png}` — all 36 on one sheet
- `cistercian-glyphs.zip` — the whole set

Naming: `cistercian_<decimal value>` (1–9, 10–90, 100–900, 1000–9000).
Colorways: violet lines `#A970FF` + orange rivets `#FF6F00` (primary, JD's colors —
locked 2026-10-12); white lines + orange rivets (neutral for tinting).
Recoloring = one parameter in `generate.py`.

## ASCII mapping (the enchantment text layer)

`ascii_map.json` + `ascii_specimen.{svg,png}`. Every printable ASCII char (32–126) maps to
the Cistercian numeral **of its own code point**: `A` (65) → glyph 65, ` ` (32) → glyph 32.
Certificate-table text renders through this map — arcane on sight, decodable by players
once they realize the glyphs *are* the codes (the community-decoder game, like Minecraft's
enchanting table). ASCII only per JD — not Unicode.

## Powers-of-two character map

`powers_of_two.json` + `powers_of_two.{svg,png}`. 2^0 through 2^13 (1 to 8192 — the highest
power of two the 9999 ceiling allows), each rendered as its Cistercian glyph. The
powers-of-two motif is a game-wide signature; this chart is its certificate-signing expression.

## Engine integration (proposed, not yet built)

- Home for the standard: engine-side (JD: "part of the core engine as its certificate
  signing standard"). Proposed: `engine/assets/cert/` + a numeral codec (integer ↔ glyph
  composition, dot-aware) in the engine.
- Reference for the codec architecture (JD 2026-10-12): https://formulae.org/?example=Cistercian_numerals
  — transform-based design (9 unit glyphs + mirror/flip per place). Our `generate.py` already
  follows this architecture; the C++ codec will be our own clean-room implementation of it.
- Zero convention: history defines no zero glyph; our codec defines 0 = bare stave.
- The certificate table (game-side) renders these glyphs as the signing language —
  LithoKraft's answer to the enchanting table.
