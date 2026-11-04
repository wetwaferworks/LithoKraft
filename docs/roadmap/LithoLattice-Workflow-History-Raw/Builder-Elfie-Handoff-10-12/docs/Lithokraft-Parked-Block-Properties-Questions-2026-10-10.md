# Block Properties Questions — RESOLVED from code/docs (2026-10-10)
Parked, then answered the same morning from the repo + Trace brief. Remaining design-only items at bottom.

## Answered from code
1. **Support pop-off:** unsupported flat trace is set to air (breaks) — `SimThread::popAttached` (sim_thread.cpp). Design doc says "pops off." SUB-QUESTION for builder: no item drop in the sim path — intended, or should it drop the trace item?
2. **CellState budget:** all 16 bits used — value(7) + facing(2) + aux(3) + on(1) + attach(3) = 16. No spare bits. (My "14 of 16" count was wrong: facing is 2 bits.)
8. **Through-block repeaters:** intended = repeater hard-powers the solid block it faces; trace on the far face picks it up. One-way, delayed crossing. Bug in testing, being fixed.

## Answered from the Trace brief
2. **Staircasing:** allowed up/down blocks. A block placed over the lower step cuts the stair (slabs don't).
9. **Ceramic vs glass:** glass/frosted carry trace on top, are never powered, cut stairs. Ceramic is an opaque insulator, never powered. Difference = transparency + trace-on-top vs full insulation.

## Still design-only (idea list, not implemented)
3. **Weak-only** — mechanic undefined. Needs JD's call.
4. **Color-filtered** — "color-filtered glass" is idea-list. Needs JD's call.
5. **Attenuating** — idea-list. Needs JD's call.
6. **Wire-transparent** — not in code or docs. Best reading: render-only (trace visible through the block), no connectivity effect. Needs confirmation.
