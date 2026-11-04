# Wet Wafer Works web brand kit
Studio brand (B1 final v3). Site: https://www.wetwaferworks.com

## Files
- `android-chrome-192x192.png` (14 KB)
- `android-chrome-512x512.png` (39 KB)
- `apple-touch-icon.png` (9 KB)
- `favicon-32.png` (1 KB)
- `favicon.ico` (5 KB)
- `favicons/favicon-16.png` (0 KB)
- `favicons/favicon-32.png` (1 KB)
- `favicons/favicon-48.png` (3 KB)
- `favicons/favicon.svg` (1 KB)
- `logos/www_B1_final_v3_icon.svg` (1 KB)
- `logos/www_B1_final_v3_icon_1024.png` (49 KB)
- `logos/www_B1_final_v3_icon_1024.webp` (20 KB)
- `logos/www_B1_final_v3_mark.png` (32 KB)
- `logos/www_B1_final_v3_mark.svg` (1 KB)
- `logos/www_B1_final_v3_mark.webp` (15 KB)
- `logos/www_B1_final_v3_palette.json` (0 KB)
- `logos/www_B1_final_v3_palette.png` (12 KB)
- `logos/www_B1_final_v3_palette.webp` (13 KB)
- `logos/www_B1_final_v3_wordmark_dark.png` (35 KB)
- `logos/www_B1_final_v3_wordmark_dark.webp` (14 KB)
- `logos/www_B1_final_v3_wordmark_light.png` (35 KB)
- `logos/www_B1_final_v3_wordmark_light.webp` (15 KB)
- `showcase/www_B1_final_v3_splash_1920x1080.png` (40 KB)
- `showcase/www_B1_final_v3_splash_1920x1080.webp` (26 KB)
- `site.webmanifest` (0 KB)
- `social/hero-1920x1080.png` (90 KB)
- `social/hero-1920x1080.webp` (21 KB)
- `social/og-image-1200x630.png` (55 KB)
- `social/og-image-1200x630.webp` (14 KB)
- `social/x-header-1500x500.png` (59 KB)
- `social/x-header-1500x500.webp` (15 KB)

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
<meta name="theme-color" content="#131726">
<meta property="og:title" content="Wet Wafer Works">
<meta property="og:url" content="https://www.wetwaferworks.com">
<meta property="og:image" content="https://www.wetwaferworks.com/social/og-image-1200x630.png">
<meta property="og:image:width" content="1200">
<meta property="og:image:height" content="630">
<meta name="twitter:card" content="summary_large_image">
<meta name="twitter:image" content="https://www.wetwaferworks.com/social/og-image-1200x630.png">
```
