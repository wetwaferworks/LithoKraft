# Lithography Port System (Portor)
October 10, 2026. How 16x16 areas become chips with 6-face I/O.

## The concept

1. Player builds a Trace contraption in a 16x16 area.
2. Player places **Portor** blocks to define what each face of the chip does.
3. Lithographer prints the area → single **Chip** block.
4. The Chip has up to 6 faces, each mapped from a Portor.

## Portor block

- Placed in the 16x16 build area (not on the chip itself).
- Each Portor defines one face: direction, I/O type, color.
- **Direction**: which chip face it maps to (top, bottom, front, back, left, right).
- **I/O**: input or output (sneak+use to toggle, like the Catchor).
- **Color**: red or blue Trace (sneak+use to cycle, like Signal channels).

## 6 faces = 6 bits = 64

Each face is 1 bit (input=0, output=1, or unconnected). 6 faces = 6 bits = 64
possible I/O configurations. The theme holds.

- 0 faces connected: `000000` = 0 (chip does nothing externally)
- All 6 output: `111111` = 63
- Mixed: e.g. 2 in, 4 out = `001111` = 15

## Example

Build an AND gate in 16x16:
- Place 2 Portors (set to input, red) → maps to chip's left and right faces
- Place 1 Portor (set to output, red) → maps to chip's front face
- Lithographer prints → Chip with 2 inputs, 1 output
- Place Chip, connect red Trace to the three faces → working AND gate in 1 block
