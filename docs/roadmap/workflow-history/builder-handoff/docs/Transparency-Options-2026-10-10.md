# Transparency & Color Blending Options for LithoLattice
October 10, 2026. Tinted glass, water, fluids, particles.

## Implementation status (2026-10-10)

All four methods implemented in `transparency.h`:
- ✅ **Screen-door** (`kShadeGlass`) — 4x4 Bayer dither, `screenDoorTest()`. No sorting.
- ✅ **Alpha blending** (`kShadeAlphaBlend`) — `sortTransparent()` back-to-front.
- ✅ **Weighted Blended OIT** (`kShadeWeightedOIT`) — `oitWeight()` from McGuire & Bavoil. No sorting.
- ✅ **Additive** (`kShadeAdditive`) — src_alpha, 1. No sorting. For particles/glow.

Engine supports all four. LithoKraft defaults per my recommendations below.

## The techniques

### 1. Screen-door / Dithered (what we use for glass)
- 4x4 Bayer matrix: pixel is either fully opaque or fully transparent.
- **Pros**: No sorting, no blending, works with depth buffer. Fast.
- **Cons**: Slight dither pattern visible up close. Colors don't truly blend.
- **Best for**: Glass, ice, basic transparency.

### 2. Alpha blending with depth sorting
- Standard: `src_alpha, 1-src_alpha`. Sort back-to-front.
- **Pros**: True color blending. Tinted glass stacks correctly.
- **Cons**: Sorting is expensive. Breaks with intersecting geometry.
- **Best for**: Water, tinted glass (when you want real color mixing).

### 3. Weighted Blended OIT (Order-Independent Transparency)
- No sorting. Each fragment contributes weighted by depth.
- Weight = `alpha * exp(-depth * k)` or similar.
- **Pros**: No sorting, handles intersecting geometry. Single pass.
- **Cons**: Not perfectly correct, but looks good. Needs two render targets.
- **Best for**: Particles, water, complex transparent scenes.

### 4. Alpha testing / Cutout
- Binary: pixel is opaque or discarded based on alpha threshold.
- **Pros**: No sorting, works with depth buffer. Very fast.
- **Cons**: Hard edges, no semi-transparency.
- **Best for**: Leaves, sprites, grates.

### 5. Additive blending (for particles/glow)
- `src_alpha, 1` — colors add up, no sorting needed.
- **Pros**: No sorting, looks great for fire, magic, lights.
- **Cons**: Can't do dark transparency (smoke needs normal blending).
- **Best for**: Particles, glow effects, Signal visualizations.

## Tinted glass color blending

When you stack red glass + blue glass, what color do you see?
- **Real physics**: Colors multiply (red * blue = dark).
- **Alpha blending**: Does this correctly IF sorted back-to-front.
- **Screen-door**: Doesn't blend — you see dither patterns overlapping.
- **Weighted OIT**: Approximates the blend without sorting.

**Recommendation**: Use alpha blending with sorting for tinted glass
(when color accuracy matters), screen-door for plain glass (performance).

## Water & fluids

- **Surface**: Alpha-blended, animated texture. Depth-based color (shallow = light blue, deep = dark).
- **Underwater**: Fog color shift, caustics (optional).
- **Our fluid levels 1-16**: Each level could adjust opacity (deeper = more opaque).

## Particles

- **Additive** for fire, sparks, magic, Signal effects (no sorting).
- **Alpha-blended** for smoke, dust (needs sorting OR use weighted OIT).
- **Recommendation**: Additive by default, it's the fastest and looks best for most effects.

## Proposed system

| Material | Technique | Sorting? |
|----------|-----------|----------|
| Plain glass | Screen-door (kShadeGlass) | No |
| Tinted glass | Alpha blend | Yes (back-to-front) |
| Water surface | Alpha blend + depth tint | Yes |
| Leaves/sprites | Cutout (kShadeCutout) | No |
| Particles (fire/glow) | Additive | No |
| Particles (smoke) | Weighted OIT or sorted alpha | Depends |

This gives us the best of all worlds: fast where we can be, correct where it matters.
