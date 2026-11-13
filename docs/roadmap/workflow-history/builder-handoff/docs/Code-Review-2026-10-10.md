# Code Review: Engine & LithoKraft (2026-10-10)
Full review after today's implementations. Build clean, selftest passing.

## New files (today)

### Rendering
- `software_renderer.h/.cpp` — CPU rasterizer. Clean, well-structured.
- `software_shaders.h` — Per-pixel lighting, normal mapping, bump mapping. Good.
- `greedy_mesher.h/.cpp` — Binary face detection, BFS occlusion, upload budgets, LOD. Interfaces solid; rectangle merging TODO.
- `axis_culling.h/.cpp` — 6-bucket mesh split. Simple, correct.
- `transparency.h` — All 4 methods. Clean.
- `graphics_tiers.h` — Preset mappings. Simple.

### Simulation
- `trace_jit.h` / `trace_jit_basic.cpp` — Interface defined. Basic impl is placeholder (copies pointers, doesn't compile). **Needs real implementation before use.**

### World
- `light_engine.h/.cpp` — BFS propagation. Clean, tested logic.

### Player
- `stat.h` / `ability.h` — Interfaces only. No implementation yet.

## Issues found & fixed

1. **BlockDef test initializers** — Tests used 9-field init, BlockDef now has 12 (added friction, falls, density). Fixed.
2. **PortOR/LithographOR/ChipOR/Fusor** — Registered but crash selftest (no component integration). Disabled for now, code preserved. **Needs proper component wiring before enabling.**
3. **Axis culling field name** — Used `flags`, actual field is `faceAo`. Fixed.

## Documentation

Added WHAT/WHY/HOW headers to:
- `greedy_mesher.cpp`
- `axis_culling.cpp`
- `light_engine.cpp`

All new headers have class/function docs. Existing code has inline comments.

## Build status

✅ Clean build, zero errors
✅ Selftest passing (sim tests)
⚠️ Vulkan test skipped (no GPU on VM, expected)

## Recommendations

1. **Trace JIT** needs a real compiler before lithography works. Currently a stub.
2. **New blocks** (PortOR, etc.) need component integration. Don't enable until wired.
3. **Greedy rectangle merging** is TODO — current code does binary detection but not merging.
4. Consider adding unit tests for `LightEngine` and `UploadBudget`.
