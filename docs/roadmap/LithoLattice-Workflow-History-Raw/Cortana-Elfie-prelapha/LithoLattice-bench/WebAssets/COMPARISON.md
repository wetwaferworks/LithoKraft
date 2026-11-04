# WebAssets comparison (2026-10-09)

Three web brand kits copied from `web_kit/` (folders only; the kit .zip/.rar files are not included). All share the same layout: site-root favicons + `site.webmanifest`, `favicons/`, `logos/`, `social/` (OG 1200x630, X header 1500x500, hero 1920x1080), each image in PNG + WebP.

## LithoKraft (game) — `WebAssets/lithokraft/`

- **Site:** https://lithokraft.net (owned)
- **Files:** 53 · **Total size:** 4.85 MB (5,083,948 bytes)
- **README notes:** F3 locked icon + mixed-case lockup; includes gameplay showcase shots (blocks in-game, circuit samples, color lamp tiers, hotbar mockup, lamp screen demo). Theme color #151826.

**Intended use**

- Site root (upload as-is): `favicon.ico`, `favicon-32.png`, `apple-touch-icon.png` (180x180), `android-chrome-192x192.png`, `android-chrome-512x512.png`, `site.webmanifest`.
- Squarespace: Settings > Website > Browser icon → `android-chrome-512x512.png` (or `favicon-32.png`); Social sharing image → `social/og-image-1200x630.png`; header logo → a `logos/` dark/light PNG matching the header color.
- X: header → `social/x-header-1500x500.png`; avatar → the 1024 icon in `logos/` (`lithokraft_icon_dark_1024.png`).
- Hero/banner section: `social/hero-1920x1080.webp` (PNG fallback). SVGs for anything that scales; `*_dark*` for dark backgrounds, `*_light*` for light.

**Contents**

- `(site root)` (7): `README.md`, `android-chrome-192x192.png`, `android-chrome-512x512.png`, `apple-touch-icon.png`, `favicon-32.png`, `favicon.ico`, `site.webmanifest`
- `favicons` (4): `favicon-16.png`, `favicon-32.png`, `favicon-48.png`, `favicon.svg`
- `logos` (24): `lithokraft_icon_dark_1024.png`, `lithokraft_icon_dark_1024.webp`, `lithokraft_icon_dark_512.png`, `lithokraft_icon_dark_512.webp`, `lithokraft_icon_dark_full.svg`, `lithokraft_icon_light_1024.png`, `lithokraft_icon_light_1024.webp`, `lithokraft_icon_light_512.png`, `lithokraft_icon_light_512.webp`, `lithokraft_icon_light_full.svg`, `lithokraft_lockup_dark_transparent.png`, `lithokraft_lockup_dark_transparent.svg`, `lithokraft_lockup_dark_transparent.webp`, `lithokraft_lockup_light_transparent.png`, `lithokraft_lockup_light_transparent.svg`, `lithokraft_lockup_light_transparent.webp`, `lithokraft_lockup_mixed_dark.svg`, `lithokraft_lockup_mixed_dark_transparent.png`, `lithokraft_lockup_mixed_dark_transparent.svg`, `lithokraft_lockup_mixed_dark_transparent.webp`, `lithokraft_lockup_mixed_light.svg`, `lithokraft_lockup_mixed_light_transparent.png`, `lithokraft_lockup_mixed_light_transparent.svg`, `lithokraft_lockup_mixed_light_transparent.webp`
- `showcase` (12): `blocks_batch2_ingame.png`, `blocks_batch2_ingame.webp`, `circuit_sample.png`, `circuit_sample.webp`, `circuit_sample_c4.png`, `circuit_sample_c4.webp`, `color_lamp_tiers_v3_sheet.png`, `color_lamp_tiers_v3_sheet.webp`, `hotbar_mockup@2x.png`, `hotbar_mockup@2x.webp`, `lamp_screen_demo.png`, `lamp_screen_demo.webp`
- `social` (6): `hero-1920x1080.png`, `hero-1920x1080.webp`, `og-image-1200x630.png`, `og-image-1200x630.webp`, `x-header-1500x500.png`, `x-header-1500x500.webp`

## LithoLattice (engine) — `WebAssets/litholattice/`

- **Site:** https://litholattice.net (owned)
- **Files:** 65 · **Total size:** 946 KB (968,660 bytes)
- **README notes:** Crystal-hexagon mark, caps/mixed wordmarks, "LithoLattice Engine" lockups and Powered-by badges. No showcase folder. Theme color #141726.

**Intended use**

- Site root (upload as-is): `favicon.ico`, `favicon-32.png`, `apple-touch-icon.png` (180x180), `android-chrome-192x192.png`, `android-chrome-512x512.png`, `site.webmanifest`.
- Squarespace: Settings > Website > Browser icon → `android-chrome-512x512.png` (or `favicon-32.png`); Social sharing image → `social/og-image-1200x630.png`; header logo → a `logos/` dark/light PNG matching the header color.
- X: header → `social/x-header-1500x500.png`; avatar → the 1024 icon in `logos/` (`litholattice_mark_color_dark_512.png` (largest mark; 512 is fine for X)).
- Hero/banner section: `social/hero-1920x1080.webp` (PNG fallback). SVGs for anything that scales; `*_dark*` for dark backgrounds, `*_light*` for light.

**Contents**

- `(site root)` (7): `README.md`, `android-chrome-192x192.png`, `android-chrome-512x512.png`, `apple-touch-icon.png`, `favicon-32.png`, `favicon.ico`, `site.webmanifest`
- `badges` (12): `powered_by_litholattice_caps_dark.png`, `powered_by_litholattice_caps_dark.svg`, `powered_by_litholattice_caps_dark.webp`, `powered_by_litholattice_caps_light.png`, `powered_by_litholattice_caps_light.svg`, `powered_by_litholattice_caps_light.webp`, `powered_by_litholattice_mixed_dark.png`, `powered_by_litholattice_mixed_dark.svg`, `powered_by_litholattice_mixed_dark.webp`, `powered_by_litholattice_mixed_light.png`, `powered_by_litholattice_mixed_light.svg`, `powered_by_litholattice_mixed_light.webp`
- `favicons` (4): `favicon-16.png`, `favicon-32.png`, `favicon-48.png`, `favicon.svg`
- `logos` (36): `litholattice_engine_caps_dark.png`, `litholattice_engine_caps_dark.svg`, `litholattice_engine_caps_dark.webp`, `litholattice_engine_caps_light.png`, `litholattice_engine_caps_light.svg`, `litholattice_engine_caps_light.webp`, `litholattice_engine_mixed_dark.png`, `litholattice_engine_mixed_dark.svg`, `litholattice_engine_mixed_dark.webp`, `litholattice_engine_mixed_light.png`, `litholattice_engine_mixed_light.svg`, `litholattice_engine_mixed_light.webp`, `litholattice_mark_color_dark.svg`, `litholattice_mark_color_dark_512.png`, `litholattice_mark_color_dark_512.webp`, `litholattice_mark_color_light.svg`, `litholattice_mark_color_light_512.png`, `litholattice_mark_color_light_512.webp`, `litholattice_mark_mono_dark.svg`, `litholattice_mark_mono_dark_512.png`, `litholattice_mark_mono_dark_512.webp`, `litholattice_mark_mono_white.svg`, `litholattice_mark_mono_white_512.png`, `litholattice_mark_mono_white_512.webp`, `litholattice_wordmark_caps_dark.png`, `litholattice_wordmark_caps_dark.svg`, `litholattice_wordmark_caps_dark.webp`, `litholattice_wordmark_caps_light.png`, `litholattice_wordmark_caps_light.svg`, `litholattice_wordmark_caps_light.webp`, `litholattice_wordmark_mixed_dark.png`, `litholattice_wordmark_mixed_dark.svg`, `litholattice_wordmark_mixed_dark.webp`, `litholattice_wordmark_mixed_light.png`, `litholattice_wordmark_mixed_light.svg`, `litholattice_wordmark_mixed_light.webp`
- `social` (6): `hero-1920x1080.png`, `hero-1920x1080.webp`, `og-image-1200x630.png`, `og-image-1200x630.webp`, `x-header-1500x500.png`, `x-header-1500x500.webp`

## Wet Wafer Works (studio) — `WebAssets/wetwaferworks/`

- **Site:** https://www.wetwaferworks.com (NOT yet purchased)
- **Files:** 32 · **Total size:** 652 KB (667,529 bytes)
- **README notes:** B1 final v3 die-grid wafer mark, wordmarks (light/dark), palette, "presents" splash in showcase/. Theme color #131726.

**Intended use**

- Site root (upload as-is): `favicon.ico`, `favicon-32.png`, `apple-touch-icon.png` (180x180), `android-chrome-192x192.png`, `android-chrome-512x512.png`, `site.webmanifest`.
- Squarespace: Settings > Website > Browser icon → `android-chrome-512x512.png` (or `favicon-32.png`); Social sharing image → `social/og-image-1200x630.png`; header logo → a `logos/` dark/light PNG matching the header color.
- X: header → `social/x-header-1500x500.png`; avatar → the 1024 icon in `logos/` (`www_B1_final_v3_icon_1024.png`).
- Hero/banner section: `social/hero-1920x1080.webp` (PNG fallback). SVGs for anything that scales; `*_dark*` for dark backgrounds, `*_light*` for light.

**Contents**

- `(site root)` (7): `README.md`, `android-chrome-192x192.png`, `android-chrome-512x512.png`, `apple-touch-icon.png`, `favicon-32.png`, `favicon.ico`, `site.webmanifest`
- `favicons` (4): `favicon-16.png`, `favicon-32.png`, `favicon-48.png`, `favicon.svg`
- `logos` (13): `www_B1_final_v3_icon.svg`, `www_B1_final_v3_icon_1024.png`, `www_B1_final_v3_icon_1024.webp`, `www_B1_final_v3_mark.png`, `www_B1_final_v3_mark.svg`, `www_B1_final_v3_mark.webp`, `www_B1_final_v3_palette.json`, `www_B1_final_v3_palette.png`, `www_B1_final_v3_palette.webp`, `www_B1_final_v3_wordmark_dark.png`, `www_B1_final_v3_wordmark_dark.webp`, `www_B1_final_v3_wordmark_light.png`, `www_B1_final_v3_wordmark_light.webp`
- `showcase` (2): `www_B1_final_v3_splash_1920x1080.png`, `www_B1_final_v3_splash_1920x1080.webp`
- `social` (6): `hero-1920x1080.png`, `hero-1920x1080.webp`, `og-image-1200x630.png`, `og-image-1200x630.webp`, `x-header-1500x500.png`, `x-header-1500x500.webp`

## Recommendation

1. **Launch lithokraft.net first.** The domain and the X handle are already owned, and it's the richest kit (the showcase shots give a teaser page real content). Use `og-image-1200x630.png` as the Squarespace social image, `android-chrome-512x512.png` as the browser icon, `lithokraft_lockup_mixed_dark_transparent.png` as the header logo on a dark theme, and `x-header-1500x500.png` + `lithokraft_icon_dark_1024.png` on X.
2. **litholattice.net second**, as a simple engine page reusing a couple of LithoKraft showcase shots, with Powered-by badges for the footer. Its kit has no 1024 avatar; the 512 color mark works for a Google/X avatar.
3. **wetwaferworks.com last.** Buy the domain first. Until then, the studio kit can supply a "Wet Wafer Works presents" footer/splash on the other two sites. Its icon is the cleanest at favicon size.

Keep `site.webmanifest` + favicons at each site root; Squarespace only needs the browser icon and social image uploaded via its settings.


## Squarespace packs (added 2026-10-09)

The upload-ready Squarespace 7.1 packs are in `<site>\squarespace\`. See UPLOAD.md for where each file goes.

| Site | Contents | Pack size |
|---|---|---|
| lithokraft | desktop/clean/mobile heroes, 3 section backgrounds + seamless tile, header logos (dark/light, 1x/2x), mobile mark, 300px favicon, 1200x1200 + 1200x630 share images, 4 feature cards (1500x1000) | about 3.5 MB |
| litholattice | same set minus feature cards (hex-lattice backgrounds) | about 3.1 MB |
| wetwaferworks | same set minus feature cards (wafer/sheen backgrounds) | about 2.7 MB |

Every image is under 500 KB, following Squarespace's recommendation. **Recommendation:** use these Squarespace packs for the live sites. The earlier site-root kits (favicon.ico, site.webmanifest, og-image) are for custom-coded hosting, because Squarespace won't accept .ico favicons. Launch lithokraft.net first.

