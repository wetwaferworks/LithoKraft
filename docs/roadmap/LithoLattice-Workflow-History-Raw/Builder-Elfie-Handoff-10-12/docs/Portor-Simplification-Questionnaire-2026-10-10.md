# Portor Simplification Questionnaire
October 10, 2026. How to keep Portors simple while harmonizing with Trace/Signal.

## The core principle

**A Portor is a wire endpoint, not a processor.** It doesn't compute, filter, or
transform. It says "this chip face connects here." The wire does the talking,
the chip does the thinking. Dumb by design.

## What Portors DON'T need

- No signal processing (the chip's JIT handles logic)
- No color conversion (red stays red, blue stays blue)
- No channel management (that's Signal's job)
- No power (passive connector, like a via)

## Questionnaire

### 1. Wire color: auto-detect or manual?
When a Portor touches a red wire, does it automatically become red? Or does the
player set it with sneak+use?
- **Auto**: simpler UX, less configuration
- **Manual**: more control, can override

### 2. One color per Portor, or dual?
Can a single Portor carry both red and blue (like a via stack)? Or is it one
color per Portor, and you place two for dual?
- **Single**: simpler, matches the 6-bit model (1 bit per face)
- **Dual**: more compact, but breaks the 6-bit = 64 elegance

### 3. Signal (wireless) integration
How does a chip face talk wirelessly? Options:
- **A**: Portor connects to adjacent Transmittor/Receivor (physical adjacency)
- **B**: Portor has a channel setting, talks directly to Signal (no extra block)
- **C**: Chips are Trace-only; wireless needs an external adapter block

### 4. Face conflicts
What if two Portors map to the same chip face?
- **A**: Error — Lithographer refuses to print, tells player to fix
- **B**: First placed wins, second is ignored (with warning)
- **C**: They merge (both wires connect to the same face)

### 5. Portor power
Do Portors need to be powered, or are they passive?
- **Passive** (recommended): they're just connectors, like vias
- **Powered**: they act as repeaters (boost signal through the chip boundary)

### 6. Chip-to-chip
Can a Chip's face connect directly to another Chip's face (face-to-face)?
Or must there always be wire between them?
- **Direct**: more compact builds
- **Wire required**: clearer, matches Trace rules

## Elfie's recommendations

1. **Auto-detect** color (simpler UX)
2. **Single color** per Portor (preserves 6-bit elegance)
3. **Option A** for Signal (physical Transmittor/Receivor adjacency — keeps Portor dumb)
4. **Option A** for conflicts (error out — forces clean design)
5. **Passive** (no power — they're connectors)
6. **Wire required** between chips (clearer signal flow)
