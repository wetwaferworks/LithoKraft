# Litho SVG — Implementation Report (2026-10-12)

**Goal:** SVG as a first-class texture source in the LithoLattice engine (JD: "Litho SVG",
for alpha). Any texture manifest entry may point at `.svg`; the engine rasterizes it into
the texture pipeline at the pack's tile size. Design doc:
`~/workspace/user/files/Litho-SVG-Design-2026-10-12.md`.

**Status:** Complete. Full build clean, assets selftest green (6 new SVG checks),
`litholattice_tests` default run green ("ENGINE TESTS OK", 0 FAIL), ctest in progress at
time of writing (see §5).

---

## 1. Files changed

**New (vendored):**
- `engine/src/assets/svg/vendor/nanosvg.h`, `nanosvgrast.h` — NanoSVG, Mikko Mononen,
  zlib/libpng license, headers intact. Fetched from the official memononen/nanosvg repo.
- `engine/src/assets/svg/nanosvg_impl.c` — implementation TU (`#define
  NANOSVG_IMPLEMENTATION` / `NANOSVGRAST_IMPLEMENTATION`), following the
  `stb_image_impl.c` pattern (third-party TU, engine warning flags off).
- `engine/src/assets/svg/README.md` — module doc: how it works, raster-size flow, cache
  behavior, and the documented limits (no animation/filters/`<text>` layout/external refs).

**New (fixtures):**
- `engine/src/assets/testdata/cistercian_1.svg` — real Cistercian glyph (violet/orange),
  copied from the certificate-signing set. No runtime reference to user files.

**Modified:**
- `engine/src/assets/include/litholattice/assets/image.h` — `decodeSvg(bytes, w, h, err)`,
  `loadSvg(path, w, h, err)` declarations; class comment updated.
- `engine/src/assets/image.cpp` — `decodeSvg` (NanoSVG parse + rasterize → straight-alpha
  RGBA8; `w/h <= 0` = intrinsic size; fit-and-center, transparent padding), `loadSvg`,
  `Image::load` routes `.svg` (case-insensitive) to the SVG decoder at intrinsic size.
- `engine/src/assets/include/litholattice/assets/resource_loader.h` — `SvgRasterCache`
  struct (fetch/store/clear) + `clearSharedSvgRasterCache()` test hook.
- `engine/src/assets/resource_loader.cpp` — `loadImage` now takes the pack tile size and
  routes `.svg` rels to `Image::loadSvg(path, tileSize, tileSize)`; shared raster cache
  keyed by (absolute path, tile size, file size, mtime) so hot-reloaded files re-rasterize;
  three call sites in `loadResources` pass `m.tileSize`.
- `engine/src/assets/CMakeLists.txt` — `litholattice_nanosvg` static lib, linked PRIVATE
  into `litholattice_assets`.
- `engine/src/assets/assets_selftest.cpp` — 6 new Litho SVG checks (see §4).
- `engine/THIRD_PARTY_LICENSES.md` — NanoSVG entry (license text included).

**Untouched (per constraints):** sim core, trace JIT, netcode, save format, procgen,
subdiv, world, storage, and the Infinite Registry / Litho Formula modules.

## 2. How the raster size flows (pack → decoder)

`loadResources` merges manifests → `m.tileSize` (from the pack's `tile_size`, 1..1024)
→ `loadImage(stack, rel, m.tileSize, …)` → for `.svg`: `SvgRasterCache` lookup, else
`Image::loadSvg(path, tileSize, tileSize)` → `decodeSvg` rasterizes the vector art at
exactly tile×tile → `TextureRegistry::add` size validation passes unchanged (it requires
tile×tile). PNG path is byte-identical to before. No manifest format change:
extension-driven only.

`Image::load(path)` on an `.svg` outside the pack system (UI/tools/tests) rasterizes at
the SVG's intrinsic size (viewBox when no width/height attrs, e.g. 100×140 for the fixture).

## 3. Failure semantics (unchanged)

Malformed SVG → `nullopt` + `err` → the magenta missing-texture checker + a warning, exactly
like a bad PNG. Missing `.svg` file → existing "file not found" path. Documented
non-goals (animation, filters, text layout, external refs) are not attempted by the
rasterizer; NanoSVG ignores what it can't do rather than failing loudly, which matches
the lenient-texture philosophy.

## 4. Test evidence

New checks in `runAssetsSelfTest` (`./tests/litholattice_tests assets`):

```
[PASS] litho svg: cistercian_1.svg decodes at 64x64, violet stroke pixels present, corners transparent
[PASS] litho svg: malformed input -> nullopt + err
[PASS] litho svg: 32 and 64 rasterizations both succeed with different buffers
[PASS] litho svg: Image::load routes .svg by extension at intrinsic size (100x140)
[PASS] litho svg: svg test pack opens
[PASS] litho svg: .svg texture loads through loadImage at pack tile size into the registry; reload identical (cache)
ASSETS SELFTEST OK
```

The wire-up proof builds a throwaway pack (`tile_size: 32`, texture `sigil` →
`textures/sigil.svg`), runs it through `loadResources` → `loadImage` → `TextureRegistry`,
and asserts 32×32 dimensions, non-transparent content, and byte-identical reload (cache).
Full default run: `ENGINE TESTS OK`, 0 FAIL. ctest final: **34/36 passed** (1085 s).
The 2 failures are both pre-existing and unrelated to this work:
- `lithokraft_selftest` — already listed as a known grandfathered failure in the
  morning status; game-side, no SVG involvement, and no `.svg` exists in any pack.
- `lithokraft_silica_selftest` — aborts in `ChunkMap::set` (`engine/src/world/…`,
  "set without setRegistry()"), a world-module test-harness issue in a module this
  task forbids touching; reproduces in isolation, zero SVG/assets code on the path.
(A third transient, `audio` streaming timing, failed once under two concurrent ctest
runs on this VM and passes alone — load flake, unrelated.)

Commands run:
- `cmake --build . --target litholattice_nanosvg litholattice_assets` — clean
- `cmake --build . --target litholattice_tests` — clean
- `./tests/litholattice_tests assets` — ASSETS SELFTEST OK (exit 0)
- `./tests/litholattice_tests` — ENGINE TESTS OK (exit 0)
- `ctest` — in progress

## 5. Fixes found (try / catch / accept)

1. **My own test bug (caught by bisect):** the new SVG test block wrote invalid JSON into
   the throwaway pack (`\"` inside a C++ raw string stays literal), so
   `ResourcePack::open` failed and the test dereferenced the empty optional — heap
   corruption surfacing as a segfault in an unrelated destructor (and, in the default
   run, inside the UI suite). Bisected by disabling the block (crash gone), fixed the
   JSON, and hardened the test to check `open().has_value()` before use. Lesson:
   never dereference a pack open without checking — even in tests.
2. **Private-access compile error:** `SvgRasterCache::Key` helpers first landed as free
   functions (can't name a private nested type) and inside the anonymous namespace
   (wrong scope for member definitions). Restructured: `makeKey` is a private static
   member; definitions live at `litholattice` scope.
3. No bugs found in the pre-existing PNG path, loader, or registry — all existing
   assets checks pass unchanged.

## 6. Left for the builder

- ctest final tally (slow suites: `lithokraft_selftest` ~10 min on this VM).
- The earlier default-run UI segfault (seen once, before the test-JSON fix; full suite
  green since) — worth one more full-suite run on the builder's machine to confirm
  it's gone and not flaky.
- Future: wire an actual `.svg` texture into a real pack manifest (e.g. the Cistercian
  certificate-table UI art) once JD approves; consider `Image::loadSvg` for UI-held
  `SvgRasterCache` instances per the design doc.
