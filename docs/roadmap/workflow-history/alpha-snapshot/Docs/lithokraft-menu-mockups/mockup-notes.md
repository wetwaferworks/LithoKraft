# LithoKraft Launch Menu Mockups — 2026-10-11

Four directions, all rendered pixel-faithful to the engine UI's real look:
flat rects, 1px borders, monospace text, theme colors taken straight from
`ui/theme.cpp` (`Theme::defaults()`). Backdrops are dark window clears with a
faint voxel grid, except mockup 3 where the game renderer shows through.

## New engine code (per JD: don't fake it, build it)

**`ui::Divider`** — the one widget the mockups needed that didn't exist.
- `engine/src/ui/include/litholattice/ui/controls.h` — class declaration
  (horizontal/vertical, thickness, accent-colored, never interactive)
- `engine/src/ui/controls.cpp` — implementation (one accent `fillRect`)
- `engine/src/ui/widget_factory.cpp` — registered as `"divider"`
  (`orientation`, `thickness` keys) for JSON-driven UI
- `engine/src/ui/ui_selftest.cpp` — 5 new checks, all passing
- Build: `litholattice_ui` and `litholattice_tests` clean; only the 2
  grandfathered failures remain (mesher golden hash, block-names identity).

Everything else in the mockups uses existing widgets.

## Mockup 1 — Classic centered (`menu-mockup-1-classic.png`)

Big `Label` (textScale ~4.5) title, `Divider` under the tagline, vertical
`StackPanel` of three `Button`s (Play Creative Testing / Settings / Exit),
version `Label` bottom-left. First button shown in hover state.
- Widgets: Panel(none, backdrop is window clear), Label, Button, Divider
- Pause-menu fit: the same stack becomes Resume / Settings / Exit to Menu.

## Mockup 2 — Sidebar nav (`menu-mockup-2-sidebar.png`)

Left rail `Panel` with title `Label`, `Divider`, nav rows (selected row =
accent edge bar + accent text — plain `Label`s in a `StackPanel`, clickable
via `Button`-style rows), right content `Panel` with world info `Label`s,
`Divider`, and an "Enter World" `Button`. Settings page would use `TabView`
+ `Slider`/`Checkbox`/`Dropdown`/`NumberField` on the right side.
- Widgets: Panel, Label, Button, Divider (TabView/Slider/Checkbox/Dropdown
  for the settings content)
- Pause-menu fit: rail becomes Resume / Settings / Save & Exit; right side
  shows session stats.

## Mockup 3 — Immersive bottom bar (`menu-mockup-3-bottombar.png`)

The 3D renderer draws the voxel scene (night terrain, trees, torches —
stand-in art, not UI); a translucent `Panel` dock along the bottom holds the
title `Label`, version, and a horizontal `StackPanel` of `Button`s
(Play / Settings / Exit), with a `Divider` accent line on the dock's top edge.
The dock's translucency in the mockup is honest: `Panel` bg is 72% black.
- Widgets: Panel, Label, Button, Divider, StackPanel
- Pause-menu fit: same dock, buttons become Resume / Settings / Exit;
  the world stays visible behind — nicest pause of the four.

## Mockup 4 — Compact dialog (`menu-mockup-4-compact.png`)

Centered `Panel` (could be a `Dialog` for modal focus-trapping and Esc),
title `Label`, `Divider`, three `Button`s, keyboard-hint `Label`
("Enter: select, Esc: quit"), version line. Keyboard-first by construction:
`Button` already handles Enter/Space when focused, and `Dialog` traps Tab.
- Widgets: Panel (or Dialog), Label, Button, Divider
- Pause-menu fit: literally the same dialog with different buttons —
  cheapest to implement, most consistent.

## Feasibility notes

- All four are buildable today with the current widget set + `Divider`.
  No other new widgets were needed.
- Settings content (all four): `TabView` pages of `Slider` (volume, render
  distance), `Checkbox`/`Toggle` (vsync, fullscreen), `Dropdown` (graphics
  tier — maps to the PBR transparent tiers), `NumberField` (signal/channel
  values). All exist.
- Title text: engine `Label` with `textScale` gives the big flat titles
  shown. A fancier logo would come from the `Image` widget + game art pack.
- Mockup 3's scene is the only non-UI element; it assumes the menu layer
  composites over the running renderer, which is the standard engine
  behavior (UiRoot over the frame).
- Recommended build order after JD picks: menu root + chosen layout,
  settings `TabView` content, pause-menu variant reusing the same widgets.
