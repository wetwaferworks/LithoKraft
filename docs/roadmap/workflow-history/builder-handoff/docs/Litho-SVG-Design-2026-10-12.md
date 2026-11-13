# Litho SVG — Design (2026-10-12)

**Goal (JD):** SVG as a first-class texture source in the LithoLattice engine, for alpha.
Named **Litho SVG**. Any texture in the game (blocks, items, UI, particles) may be authored
as `.svg`; the engine rasterizes it into the texture pipeline.

## Rasterizer: NanoSVG

- Single-header, public-domain (zlib) C library: `nanosvg.h` + `nanosvgrast.h`.
- Vendored under `engine/src/assets/svg/vendor/`, following the existing `stb_image_impl.c`
  pattern (keep the license header intact).
- Rationale: tiny, zero dependencies, handles our subset (paths, circles, rects, transforms,
  flat fills) — everything our Cistercian glyphs and game UI need.
- **Documented limits:** no SMIL animation, no filters, no `<text>` layout (convert text to
  paths before shipping), no external references.

## Integration

- New `Image::decodeSvg(bytes, width, height, err)` → RGBA8 `Image`, alongside `decode`.
- `Image::load(path, …)` and `loadImage(stack, rel, …)` detect `.svg` by extension and route
  to the SVG decoder. The raster target size = the pack's tile size (32/64/…), so an SVG
  authored once ships crisply at every pack resolution — the whole point of vector textures.
- Manifests: anywhere a texture path is accepted, `.svg` is accepted. No manifest format
  change — it's extension-driven.
- Caching: rasterize once per (path, tile size) per pack load (existing pack-load caching);
  a small raster cache keyed the same way for UI reuse.
- Failure semantics unchanged: undecodable SVG → nullopt + err → the magenta missing-texture
  path, exactly like a bad PNG.

## Tests (required)

- Decode a real Cistercian glyph SVG (`cistercian_1.svg`) at 64×64: correct dimensions,
  non-zero non-transparent pixel count, spot-check a known stroke pixel ≈ violet and a
  corner pixel = transparent.
- Malformed SVG → nullopt + err message. Missing file → existing missing-texture behavior.
- Round-trip: rasterize at 32 and 64 — both succeed, different pixel buffers.

## Non-goals

- Animated SVG, filters, text layout, scripting. Sim core, trace JIT, netcode, save format
  untouched. The Infinite Registry and Litho Formula crews own their modules — no overlap.
