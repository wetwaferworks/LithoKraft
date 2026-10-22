# PACKAGE NOTES — Wet Wafer Works full source snapshot (company network)

Packaged by Elfie (registrar) at JD's request, 2026-10-22 04:15 EDT, for the first
commit on the company network (Tailscale) — no more passing zip files back and forth.

## What this is
A source snapshot of the current trees, for shared reading, analysis, and claimed
editing under CLAIMS.md (included at the package root). It is NOT a build:
no build counter tick was taken. The sealed game state it corresponds to is
Build 80 — shell v1.8 / versionCode 9, rolling counter 0.0.0.0.0.0.1.16,
game Alpha 0.0.3.20, scenario hashes cities ON cb32c824f8b3ea98 / OFF e015e51cf1559089.

## Contents
- engine/            LithoLattice engine source (receive tree state, 2026-10-21)
- game/              LithoKraft game source
- host/lithocef/     the CEF layer host (ILithoCEF seam) — private, proprietary
- wasm/              the thin web layer (lkboot.js / lkasm.js), pages, pack/serve tools
- android-shell/     the Android shell app source (the mobile host)
- docs/, Lore/, tools/, designers-editors/, top-level build/setup/manifest docs

## Excluded on purpose
- Build outputs and build trees (build-linux*, build-wasm*, shell build/, .gradle)
- _depsrc/ and _incoming/ (third-party dependency sources and the drop folder)
- Nested .zip files (older packages do not ride inside this one)
- Key material: the shell debug keystore, the localhost .p12 in android-shell/assets, and android-shell/cert/ (the Leg A
  issuance kit) are NOT in this package. JD holds those; add them by hand if
  the repo ever needs them.
