# Voxel Compute — Wire strength readout: don't ship 65 textures

Design input for build week (feeds kickoff checklist item 4). Sparked by your Grok thread this morning — hand it to Grok if the 65-textures question is still open.

## Recommendation

One grayscale wire base texture per color (red/blue) + strength as a shader-side parameter. Never pre-render per-strength variants.

## Why

1. **No texture explosion.** 65 strength states × wire colors × mips × resolutions = an authoring and atlas burden where every art tweak regenerates everything. One base + math costs one per-wire parameter.
2. **Heat composes for free.** Heat v1 (kickoff item 5) also needs to visualize temperature on wires. Strength and heat can ride the same brightness/overlay channel with different ramps — one code path, two visualizations. Pre-baked textures multiply again the day heat lands.
3. **Vulkan and GL share the path.** Strength as a per-voxel/per-instance parameter works identically on both backends — exactly the thin-renderer-interface risk that kickoff item 2 retires in week one. Bake the strength channel into the wire shader params on day one.
4. **Colorblind-safe.** Red vs blue wire identity stays in hue; strength must stay in luminance/fill — encode strength as fill fraction + brightness, not hue. Also reads better in screenshots.

## Suggested spec (hand to Grok)

- Wire voxel carries `strength: u8` (0–64) + `color: u8` (red/blue) in instance/vertex data.
- Fragment shader: `lit = strength / 64.0`. Base texture grayscale; multiply brightness by a ramp; overlay a glow strip whose length along the wire axis equals `lit`. Tick marks every 16 (mini VU meter) — optional, test readability.
- Keep a numeric readout on hover for debugging; the wire visual is for play, the number is for debug.

## Two honest caveats

- **Readability at the smallest voxel resolution is the real acceptance criterion.** Brightness-only ramps wash out at low res; the fill-fraction strip survives it. Test both, keep the strip.
- **Don't share one strip for heat and strength.** If both overlays are on, give them separate strips or layered bands — overlapping channels go muddy.

## Open perf question for build week

Per-voxel attribute vs. a compact strength texture/buffer for very large builds — let the vertical slice benchmark decide, don't pre-optimize.

---

Grounded in: Voxel-Compute-Build-Week-Kickoff.md (items 2, 4, 5); JD's 2026-10-09 decisions (red/blue wire, mandatory visual strength readout on the wire, Vulkan + OpenGL renderers, heat model v1). Prepared by Elfie, Oct 9, 2026.
