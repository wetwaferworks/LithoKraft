# ASCII Renderer Roadmap

## ASCII renderer: north-star reference (ASCII CRAFT)

**Proposed by JD, breakdown by Cortana, 2026-10-23**

**Source:** https://x.com/birdabo/status/2113310460384792822, posted by @birdabo on 2026-10-22. It shows "ASCII CRAFT: a voxel sandbox", said to be playable in the browser and 158 KB. No creator or repo is visible, and JD found no repo. Everything below comes from watching the video, not from reading any code.

### What the video shows

- A dense character grid, likely hundreds of columns wide.
- A density ramp from `@ % #` down to `+ : . -` and space.
- Two colors per cell: a foreground (character) color and a background color. Together they dither textures.
- Top faces brighter than side faces.
- Distance fog that thins characters toward `.` and blends into the sky.
- A torch point light with smooth falloff, and a day/night cycle with stars.
- An underwater tint with bubbles, plus rain.
- A thin white wireframe around the targeted block.
- A hotbar, an inventory overlay, a pause menu with an ASCII-art title, and a debug HUD showing FPS over 300 and "Renderer: Direct3D 11". On Windows that fits WebGL running through Chrome's ANGLE layer.
- No CRT effect. The post suggests adding one.

### 1. Techniques to borrow

- **Luminance to glyph ramp.** Pick each cell's character by brightness, using a fixed ramp (`@%#*+=-:. `).
- **Two colors per cell.** Foreground color carries detail and background color carries the base tone. That's what makes the textures read without real pixels.
- **Per-face shading.** Fixed brightness per face direction (top brightest, sides darker, bottom darkest). It's cheap and makes the blocks read clearly.
- **Fog as glyph thinning.** Push distant cells down the ramp toward `.` and blend their colors to the sky color.
- **Smooth point lights.** The torch light falls off smoothly and is added before the glyph is picked, so the light shows up as denser characters.
- **Sky and weather as glyph layers.** Stars, rain streaks and bubbles are drawn as characters over the scene.
- **Crisp overlays.** The block outline and the UI are drawn at full cell precision on top of the ASCII pass, so they stay readable.

### 2. Likely architecture

1. Render the 3D voxel scene as normal, but at low resolution (one pixel per character cell, or a small block per cell) into an offscreen target. Write color plus depth or fog.
2. Run a fullscreen GPU pass over the character grid. For each cell:
   - Read the low-res color and compute luminance.
   - Map luminance (after lighting and fog) to a glyph index on the ramp.
   - Look up that glyph in a font atlas texture (a monospace bitmap font).
   - Output the foreground color where the glyph has ink and the background color everywhere else.
3. Draw the UI (hotbar, inventory, menus, HUD) as a character grid on top, either in the same atlas pass or as a second overlay pass.
4. Optional last pass: a CRT filter (scanlines, slight curvature, bloom, vignette).

This keeps the game logic and meshing unchanged. Only the final look changes.

### 3. Phased plan

- **Phase 1: Prototype pass.** Offscreen low-res target, font atlas, luminance-to-glyph shader, two colors per cell. Toggle it on and off with a debug key. Goal: the current world renders as ASCII at a playable FPS.
- **Phase 2: Lighting and fog.** Per-face brightness, distance fog that thins glyphs and blends into the sky, smooth torch falloff, day/night with stars. Later: underwater tint, bubbles, rain.
- **Phase 3: UI.** Block wireframe, hotbar, inventory, pause menu with an ASCII-art title, debug HUD, all on the same character grid.
- **Phase 4 (optional): CRT filter.** Scanlines, curvature, bloom. It's a single post pass with an on/off setting.

### 4. Fit with LithoKraft and the web build

- It fits as a separate render mode on top of the existing renderer. It doesn't replace it.
- The browser version depends on the LithoKraft web build. **The web build is Elfie's lane, and this work waits until it lands.**
- The prototype could be tried in the desktop build first, as long as it doesn't step on Elfie's work.
- Keep it free-tools friendly: a bitmap font atlas and plain shaders, with no paid assets.

### 5. Open questions for JD

1. Should ASCII be a toggle mode, a separate game mode, or the main look for the web build?
2. What cell size and column count should we target (for example 1080p at 8x16 cells is about 240 columns)?
3. Should we use a fixed glyph ramp or pick glyphs by shape (edges as `/ \ | -`)?
4. Should it use a full color palette or a limited retro palette?
5. Should the CRT filter be on by default, off by default, or left out?
6. Is there a size budget for the web build that this needs to fit?

### 6. Note on the size claim

The 158 KB size comes from the post and is **unverified**. With no repo or build to inspect, we can't confirm what it includes (for example compression, or assets loaded later). Don't use it as a target until it's checked.