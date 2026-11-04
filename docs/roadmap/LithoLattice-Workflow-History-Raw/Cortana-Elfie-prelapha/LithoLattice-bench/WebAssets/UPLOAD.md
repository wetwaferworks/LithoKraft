# UPLOAD.md: Squarespace upload map

Ready-to-upload files are in `WebAssets\<site>\squarespace\` (lithokraft, litholattice, wetwaferworks). The original archives are kept in `WebAssets\_raw\<site>\`. Each table row applies to every site; replace `*` with the site name. Use the .jpg (or .webp) for photos and backgrounds, and the .png for logos and favicons.

## Squarespace 7.1 guidance

| Item | Guidance | Source |
|---|---|---|
| General images / section backgrounds | 2500 px wide is ideal; under 1500 px may look blurry | https://support.squarespace.com/hc/en-us/articles/206542517 |
| File limits | .jpg/.gif/.png/.webp (no animated WebP); 20 MB max; 120 MP max; **500 KB or less recommended**; filenames: letters, numbers, _ and - only; uploads are converted to WebP | same |
| Banners (7.1 = section background images) | 1500–2500 px wide, wider than tall, abstract, **no text baked into the image**; site-wide banners never > 2500 px | https://support.squarespace.com/hc/en-us/articles/205826018 |
| Cropping / mobile | 7.1 section backgrounds "always crop to some degree, especially on mobile"; crop depends on section height + browser width; use the **focal point** to set the center | https://support.squarespace.com/hc/en-us/articles/223016508 |
| Image blocks | upload 1500–2500 px wide | https://support.squarespace.com/hc/en-us/articles/205814528 |
| Site logo | no exact dimensions; upload at a height similar to how it will display; Logo Height + Mobile Logo Max Height sliders; separate Mobile Logo Image supported | https://support.squarespace.com/hc/en-us/articles/205812698 |
| Browser icon (favicon) | shows at 16×16, upload **100–300 px square**, **.png**, **≤ 100 KB**, no .ico, no multi-size files | https://support.squarespace.com/hc/en-us/articles/206542527 |
| Social sharing | site-wide "Social sharing logo" (square works best) + per-page Alternate Social Sharing Image; Facebook min 200×200. 1200×630 (1.91:1) is the Open Graph standard used here | https://support.squarespace.com/hc/en-us/articles/205812778 |


## Where each file goes

| File | Size | Where in Squarespace |
|---|---|---|
| hero/*_hero_desktop_2500x1406.jpg/.webp | 2500×1406 | Top section > Edit Section > Background > Image; focal point center |
| hero/*_hero_desktop_clean_*.jpg | 2500×1406 | Same, when you put the logo/headline as real text on top (recommended by Squarespace) |
| hero/*_hero_mobile_1080x1920.jpg | 1080×1920 | Mobile-only section (CSS) or mobile image block |
| sections/*_section_bg1..3_2500x1406.jpg | 2500×1406 | Other section backgrounds (bg1 subtle, bg2 glow, bg3 faintest, best behind text) |
| sections/*_tile_seamless_*.png | 480 px tile | Custom CSS `background-repeat` only |
| logo/*_header_logo_ondark_1x_h100.png / _2x_h200.png | transparent | Header > Site Title & Logo > Logo Image (use 2x for crispness; set height slider ~50–100 px) |
| logo/*_header_logo_onlight_* | transparent | Use if the header color theme is light |
| logo/*_mark_transparent_400.png | ≤400 px | Mobile Logo Image (icon-only) |
| favicon/*_favicon_300.png | 300×300, <100 KB | Settings > Website > Browser Icon |
| social/*_social_logo_square_1200x1200.jpg | 1200×1200 | Settings > Social Sharing logo (site-wide) |
| social/*_social_alt_1200x630.jpg | 1200×630 | Page Settings > Social > Alternate Social Sharing Image |
| lithokraft/feature_cards/*_1500x1000.jpg | 1500×1000 (3:2) | Image blocks / Cards in a features section |

Preview sheet: `WebAssets\_raw\preview_sheet_squarespace.png`
