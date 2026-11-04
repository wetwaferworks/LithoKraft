# LithoLattice web brand kit
Engine brand (crystal-hexagon mark, wordmarks, Powered-by badges). Site: https://litholattice.net

## Files
- `android-chrome-192x192.png` (12 KB)
- `android-chrome-512x512.png` (37 KB)
- `apple-touch-icon.png` (10 KB)
- `badges/powered_by_litholattice_caps_dark.png` (26 KB)
- `badges/powered_by_litholattice_caps_dark.svg` (14 KB)
- `badges/powered_by_litholattice_caps_dark.webp` (14 KB)
- `badges/powered_by_litholattice_caps_light.png` (26 KB)
- `badges/powered_by_litholattice_caps_light.svg` (14 KB)
- `badges/powered_by_litholattice_caps_light.webp` (14 KB)
- `badges/powered_by_litholattice_mixed_dark.png` (25 KB)
- `badges/powered_by_litholattice_mixed_dark.svg` (18 KB)
- `badges/powered_by_litholattice_mixed_dark.webp` (14 KB)
- `badges/powered_by_litholattice_mixed_light.png` (26 KB)
- `badges/powered_by_litholattice_mixed_light.svg` (18 KB)
- `badges/powered_by_litholattice_mixed_light.webp` (14 KB)
- `favicon-32.png` (1 KB)
- `favicon.ico` (4 KB)
- `favicons/favicon-16.png` (0 KB)
- `favicons/favicon-32.png` (1 KB)
- `favicons/favicon-48.png` (2 KB)
- `favicons/favicon.svg` (1 KB)
- `logos/litholattice_engine_caps_dark.png` (23 KB)
- `logos/litholattice_engine_caps_dark.svg` (8 KB)
- `logos/litholattice_engine_caps_dark.webp` (12 KB)
- `logos/litholattice_engine_caps_light.png` (23 KB)
- `logos/litholattice_engine_caps_light.svg` (8 KB)
- `logos/litholattice_engine_caps_light.webp` (13 KB)
- `logos/litholattice_engine_mixed_dark.png` (23 KB)
- `logos/litholattice_engine_mixed_dark.svg` (13 KB)
- `logos/litholattice_engine_mixed_dark.webp` (12 KB)
- `logos/litholattice_engine_mixed_light.png` (23 KB)
- `logos/litholattice_engine_mixed_light.svg` (13 KB)
- `logos/litholattice_engine_mixed_light.webp` (12 KB)
- `logos/litholattice_mark_color_dark.svg` (1 KB)
- `logos/litholattice_mark_color_dark_512.png` (22 KB)
- `logos/litholattice_mark_color_dark_512.webp` (11 KB)
- `logos/litholattice_mark_color_light.svg` (1 KB)
- `logos/litholattice_mark_color_light_512.png` (23 KB)
- `logos/litholattice_mark_color_light_512.webp` (12 KB)
- `logos/litholattice_mark_mono_dark.svg` (1 KB)
- `logos/litholattice_mark_mono_dark_512.png` (20 KB)
- `logos/litholattice_mark_mono_dark_512.webp` (8 KB)
- `logos/litholattice_mark_mono_white.svg` (1 KB)
- `logos/litholattice_mark_mono_white_512.png` (20 KB)
- `logos/litholattice_mark_mono_white_512.webp` (8 KB)
- `logos/litholattice_wordmark_caps_dark.png` (21 KB)
- `logos/litholattice_wordmark_caps_dark.svg` (6 KB)
- `logos/litholattice_wordmark_caps_dark.webp` (11 KB)
- `logos/litholattice_wordmark_caps_light.png` (21 KB)
- `logos/litholattice_wordmark_caps_light.svg` (6 KB)
- `logos/litholattice_wordmark_caps_light.webp` (11 KB)
- `logos/litholattice_wordmark_mixed_dark.png` (22 KB)
- `logos/litholattice_wordmark_mixed_dark.svg` (8 KB)
- `logos/litholattice_wordmark_mixed_dark.webp` (11 KB)
- `logos/litholattice_wordmark_mixed_light.png` (22 KB)
- `logos/litholattice_wordmark_mixed_light.svg` (8 KB)
- `logos/litholattice_wordmark_mixed_light.webp` (11 KB)
- `site.webmanifest` (0 KB)
- `social/hero-1920x1080.png` (52 KB)
- `social/hero-1920x1080.webp` (16 KB)
- `social/og-image-1200x630.png` (34 KB)
- `social/og-image-1200x630.webp` (10 KB)
- `social/x-header-1500x500.png` (32 KB)
- `social/x-header-1500x500.webp` (9 KB)

## Recommended uses
- **Site root** (upload as-is): `favicon.ico`, `favicon-32.png`, `apple-touch-icon.png` (180x180), `android-chrome-192x192.png`, `android-chrome-512x512.png`, `site.webmanifest`.
- **Squarespace**: Settings > Website > Browser icon -> `android-chrome-512x512.png` (or `favicon-32.png`); Social sharing image -> `social/og-image-1200x630.png`; header logo -> a `logos/` dark/light PNG matching your header color.
- **X profile**: header `social/x-header-1500x500.png`; avatar = the 1024 icon in `logos/`.
- **Hero/banner section**: `social/hero-1920x1080.webp` (PNG fallback).
- **Logos**: SVG for anything that scales; `*_dark*` = for dark backgrounds, `*_light*` = for light backgrounds. WebP copies are for web pages; PNG for tools that don't take WebP.
- `favicons/favicon.svg` is a modern vector favicon (optional).

## HTML <head>
```html
<link rel="icon" href="/favicon.ico" sizes="any">
<link rel="icon" type="image/png" sizes="32x32" href="/favicon-32.png">
<link rel="icon" type="image/svg+xml" href="/favicons/favicon.svg">
<link rel="apple-touch-icon" href="/apple-touch-icon.png">
<link rel="manifest" href="/site.webmanifest">
<meta name="theme-color" content="#141726">
<meta property="og:title" content="LithoLattice">
<meta property="og:url" content="https://litholattice.net">
<meta property="og:image" content="https://litholattice.net/social/og-image-1200x630.png">
<meta property="og:image:width" content="1200">
<meta property="og:image:height" content="630">
<meta name="twitter:card" content="summary_large_image">
<meta name="twitter:image" content="https://litholattice.net/social/og-image-1200x630.png">
```
