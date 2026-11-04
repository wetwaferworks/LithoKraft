# Trace power: shader note (2026-10-15, CloudAtlas)
- **The block texture stays a gray luminance mask.** These are the existing `red_trace*` and `blue_trace*` pieces (body gray #B4B4BE is about 0.70 luminance). Don't swap in pre-tinted art at runtime.
- **The shader samples the ramp by strength:** `col = texelFetch(rampLUT, ivec2(strength, 0), 0).rgb`, with strength from 0 to 64.
  The LUT is `palettes/<red|blue>_trace_ramp.png`, 65x1 and sRGB. Use nearest sampling with no filtering or mips, and decode to linear if you light in linear space.
- **Final colour:** `lum = mask.r / 0.70`. If lum ≤ 1, then `rgb = col * lum`. Otherwise `rgb = mix(col, white, min(1, (lum - 1) * 2.5) * (strength / 64)^2)`.
  That gives a near-white hot core only near strength 64. Alpha comes from the mask.
- **Off (strength 0) still uses index 0's tint,** a dim, desaturated red or blue. Never fall back to gray or skip the lookup when unpowered.
- **How the ramp is built:** OKLab with the hue locked to the top colour, so red never drifts orange. Lightness is linear from 1 to 64, and chroma rises with it. Index 64 is exactly #FF514B or #2FB4FF.
- **Emissive (if used):** scale by strength / 64. At 0 the emission is zero, but the tint is still visible.
- **Fallback art:** `pack_*/textures/blocks/trace_power/*_sNN.png` are pre-tinted at levels 0, 1, 16, 32, 48 and 64, for preview or for renderers that can't do the lookup.
- **Masks:** the dot and ring masks (`*_trace_dot*_mask`) for the old shapes still need to match the new 128 pieces. That's still open from the Wafer refresh.
