# Portor Questionnaire v2: The 4-Face Hypothetical
October 10, 2026. Refined around JD's scenario.

## The scenario

A chip with:
- **Face A**: red Trace IN
- **Face B**: blue Trace IN
- **Face C**: red Trace OUT
- **Face D**: blue Trace OUT
- **Top**: unused
- **Bottom**: unused

The inside is a black box. We only care about the faces.

## The bit model

6 faces, each connected (1) or not (0):
```
Face:  A B C D Top Bottom
Bit:   1 1 1 1  0    0     = 0b111100 = 60
```

For each connected face, two properties:
- **Direction**: input or output
- **Color**: red or blue

So Face A = `1` (connected) + input + red. Face C = `1` (connected) + output + red.

## Refined questions

### 1. How does the player assign faces in the 16x16 area?
The player places Portors in the build area. But how does a Portor know which
chip face it maps to?
- **A**: By position — Portor on the north edge → north face, etc.
- **B**: By direction — Portor has a facing (like a repeater), points at the edge
- **C**: By order — first Portor placed = face 1, second = face 2, etc.

### 2. How is I/O direction set on a Portor?
- **A**: Sneak+use toggles input/output (like Catchor match value)
- **B**: Auto-detect — if wire brings signal IN, it's input; if chip drives it, output
- **C**: Two Portor types — Portor-In and Portor-Out (separate blocks)

### 3. How is color set?
- **A**: Auto-detect from adjacent wire (touch red → red)
- **B**: Sneak+use cycles red/blue (manual)
- **C**: Two Portor types — Red Portor and Blue Portor

### 4. What does the printed chip look like?
How does the player know which face is which?
- **A**: Chip texture shows colored dots on active faces (red dot = red I/O)
- **B**: Chip is blank, player uses a "probe" tool to inspect
- **C**: Chip shows arrows (in/out) + colors on each face

### 5. In the hypothetical: what if the player swaps red and blue inputs?
Face A was red IN, now they connect blue instead. Does the chip care?
- **A**: No — the chip is color-agnostic internally, faces just pass through
- **B**: Yes — the chip was compiled for red on Face A, needs re-print for blue

### 6. Top and bottom faces
In the hypothetical they're unused. But can they be used?
- **A**: Yes — all 6 faces available (full 64 configurations)
- **B**: No — top/bottom reserved (e.g. for power, or future use), only 4 side faces
- **C**: Top/bottom are special (e.g. clock input, ground)

## Elfie's recommendations

1. **B** (Portor facing determines chip face — most intuitive)
2. **A** (sneak+use toggles — consistent with Catchor/Signal UX)
3. **A** (auto-detect — less configuration)
4. **C** (arrows + colors on chip faces — self-documenting)
5. **A** (color-agnostic — simpler, more flexible)
6. **A** (all 6 faces — full 64, the theme demands it)
