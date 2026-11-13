# LithoKraft web brand kit
Game brand (F3 locked icon, mixed-case lockup). Site: https://lithokraft.net

## Files
- `android-chrome-192x192.png` (25 KB)
- `android-chrome-512x512.png` (78 KB)
- `apple-touch-icon.png` (20 KB)
- `favicon-32.png` (2 KB)
- `favicon.ico` (7 KB)
- `favicons/favicon-16.png` (0 KB)
- `favicons/favicon-32.png` (2 KB)
- `favicons/favicon-48.png` (4 KB)
- `favicons/favicon.svg` (7 KB)
- `logos/lithokraft_icon_dark_1024.png` (127 KB)
- `logos/lithokraft_icon_dark_1024.webp` (28 KB)
- `logos/lithokraft_icon_dark_512.png` (55 KB)
- `logos/lithokraft_icon_dark_512.webp` (13 KB)
- `logos/lithokraft_icon_dark_full.svg` (7 KB)
- `logos/lithokraft_icon_light_1024.png` (120 KB)
- `logos/lithokraft_icon_light_1024.webp` (30 KB)
- `logos/lithokraft_icon_light_512.png` (52 KB)
- `logos/lithokraft_icon_light_512.webp` (15 KB)
- `logos/lithokraft_icon_light_full.svg` (7 KB)
- `logos/lithokraft_lockup_dark_transparent.png` (143 KB)
- `logos/lithokraft_lockup_dark_transparent.svg` (12 KB)
- `logos/lithokraft_lockup_dark_transparent.webp` (38 KB)
- `logos/lithokraft_lockup_light_transparent.png` (136 KB)
- `logos/lithokraft_lockup_light_transparent.svg` (12 KB)
- `logos/lithokraft_lockup_light_transparent.webp` (39 KB)
- `logos/lithokraft_lockup_mixed_dark.svg` (13 KB)
- `logos/lithokraft_lockup_mixed_dark_transparent.png` (141 KB)
- `logos/lithokraft_lockup_mixed_dark_transparent.svg` (13 KB)
- `logos/lithokraft_lockup_mixed_dark_transparent.webp` (36 KB)
- `logos/lithokraft_lockup_mixed_light.svg` (13 KB)
- `logos/lithokraft_lockup_mixed_light_transparent.png` (135 KB)
- `logos/lithokraft_lockup_mixed_light_transparent.svg` (13 KB)
- `logos/lithokraft_lockup_mixed_light_transparent.webp` (37 KB)
- `showcase/blocks_batch2_ingame.png` (261 KB)
- `showcase/blocks_batch2_ingame.webp` (137 KB)
- `showcase/circuit_sample.png` (93 KB)
- `showcase/circuit_sample.webp` (106 KB)
- `showcase/circuit_sample_c4.png` (116 KB)
- `showcase/circuit_sample_c4.webp` (145 KB)
- `showcase/color_lamp_tiers_v3_sheet.png` (574 KB)
- `showcase/color_lamp_tiers_v3_sheet.webp` (309 KB)
- `showcase/hotbar_mockup@2x.png` (143 KB)
- `showcase/hotbar_mockup@2x.webp` (58 KB)
- `showcase/lamp_screen_demo.png` (426 KB)
- `showcase/lamp_screen_demo.webp` (351 KB)
- `site.webmanifest` (0 KB)
- `social/hero-1920x1080.png` (698 KB)
- `social/hero-1920x1080.webp` (48 KB)
- `social/og-image-1200x630.png` (39 KB)
- `social/og-image-1200x630.webp` (10 KB)
- `social/x-header-1500x500.png` (34 KB)
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
<meta name="theme-color" content="#151826">
<meta property="og:title" content="LithoKraft">
<meta property="og:url" content="https://lithokraft.net">
<meta property="og:image" content="https://lithokraft.net/social/og-image-1200x630.png">
<meta property="og:image:width" content="1200">
<meta property="og:image:height" content="630">
<meta name="twitter:card" content="summary_large_image">
<meta name="twitter:image" content="https://lithokraft.net/social/og-image-1200x630.png">
```
