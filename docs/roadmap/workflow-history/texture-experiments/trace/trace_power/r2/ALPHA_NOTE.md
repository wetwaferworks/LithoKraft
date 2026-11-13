# Trace "black box" bug: alpha note for the engine (2026-10-15, CloudAtlas)

## What the files actually contain (full per-file list in `alpha_audit.csv`, 555 files)
- **Our refresh128 trace pieces, torches, tinted trace_power files and trace icons:** all RGBA (no palette mode, no RGB-only files, no tRNS). Every transparent pixel is a true alpha 0, and there is no partial alpha anywhere (hard 0 or 255 cutout). **But the RGB under alpha 0 is black (0,0,0).** Any bilinear filter or mipmap that averages across the edge pulls black into the trace, which gives a dark halo or box. 408 such files are fixed here.
- **Vias (blocks):** opaque full faces by design (72 files). They have no transparency, so there's nothing to fix.
- **Elfie's alpha trace pieces and torches (pack_32 and pack_64, 16 each):** RGBA with black RGB under alpha 0, **plus 50–650 partial-alpha pixels each** (anti-aliased edges) and dark opaque fringe pixels. Those partial pixels with dark RGB are exactly what shows as a dark rim under alpha blending, and as a box under alpha test with a low threshold. Elfie's files are reference only and were not modified.
- **Intentional dark edges (not bugs):** the torch heads (blocks) and the trace, torch and trace_tip icons carry the deep #131726 pixel outline on purpose, which is the Minecraft item look. The trace block pieces themselves have no dark fringe.

## Likely engine causes (in order)
1. **Alpha test or cutout is off for the trace pass**, so alpha-0 pixels render with their RGB (black). That gives a black square tile around the dust. This is the most likely cause of a "box".
2. **Blending is disabled,** or the trace is drawn in the opaque pass. Same symptom.
3. **Mipmap or bilinear black bleed:** black RGB under alpha 0 gets averaged into the edges at a distance. That gives a dark outline that grows with distance.
4. **A premultiplied-alpha mismatch:** straight-alpha PNGs treated as premultiplied (or the reverse), or the mip generator not premultiplying before it downsamples. That gives dark or bright fringes.
5. **The texture is loaded or uploaded as RGB** (GL_RGB, VK_FORMAT_R8G8B8, or the alpha channel dropped by the loader or atlas packer). That gives a fully opaque black background.

## Recommended fix
- **Render Trace as an alpha cutout:** `if (texel.a < 0.5) discard;` in the trace, torch and overlay pass, with depth write on and no blending. Pixel-art trace is hard-edged, so cutout is correct and sort-free.
- **Use edge-extended RGB:** the files in this r2 folder copy the nearest opaque colour into every alpha-0 pixel and keep alpha unchanged, so filtering and mips sample trace colour, not black. Use them in place of the r1 files.
- **Upload as RGBA8 (sRGB).** Make sure the atlas packer keeps alpha and adds a 2–4 px edge-extended gutter per tile.
- **Mips:** with cutout, either generate the mips from the edge-extended source, or use alpha-to-coverage (or keep the alpha-scaled cutout threshold) so thin trace doesn't vanish at a distance.
- **Tint:** the shader multiplies the gray mask by the ramp LUT (see `../SHADER_NOTE.md`). Index 0 is the off tint, unchanged in r2.

## Index 0 (off)
JD confirmed off should be a dim, slightly desaturated tint that's still clearly red or blue. The r1 values already meet that, so they are **unchanged**: red #642723 and blue #18415B. The LUTs in `../palettes/` stay valid.
