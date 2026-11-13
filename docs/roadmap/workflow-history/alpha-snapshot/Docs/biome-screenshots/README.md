# Wafer Biome Screenshots — 2026-10-11

**Seed:** 20261011 (fixed, reproducible)  
**Method:** Headless capture program (`biome_capture`) using the REAL game worldgen (ColumnGenerator + wafer.json pipeline), REAL mesher, and software renderer. No window, no GPU.  
**Render style:** White clay (diffuse-lit, no textures) — shows 3D form; matches what MCP agents see.

## The 5 biomes

### trace_forest.png
**Location:** (-72, -11), 73 blocks from origin  
**What you're seeing:** Dense Trace Forest — 74 tree trunks in a 3x3 chunk region. The trees are 4-7 blocks tall with branches. Rolling terrain underneath. This is the "circuit-trees" biome — via-trunk trees with trace branches.

### dicing_plains.png
**Location:** (2130, 20), 132 blocks from origin  
**What you're seeing:** Dicing Plains — flatter terrain, sparser trees (25 trunks). The vast flat expanse with die-grid scribe lines (subtle at this scale). First-spawn territory.

### capacitor_groves.png
**Location:** (0, 2000), at origin  
**What you're seeing:** Capacitor Groves — 338 capacitor cans in clusters. The cans are squat (2-3 blocks tall), forming dense maze-like clusters. Darker, denser feel than the forest.

### resonator_spires.png
**Location:** (-2000, -60), 60 blocks from origin  
**What you're seeing:** Resonator Spires — tall thin crystal spikes (8-16 blocks). Dramatic verticality. The spires pierce the sky — quartz crystals catching the light.

### dead_zones.png
**Location:** (-147, -1995), 147 blocks from origin  
**What you're seeing:** Dead Zones — FLAT AND EMPTY. Zero features. Grey cracked silicon, no pulse, no life. The absence is the tell. This is the wafer's dead skin.

### wafer_overview_map.png
**What you're seeing:** Top-down colored biome map (2048x2048 blocks, 256x256 px).  
**Color key:**  
- Deep teal = Trace Forest  
- Light silicon grey = Dicing Plains  
- Bronze = Capacitor Groves (dominant, ~50%)  
- Pale quartz = Resonator Spires  
- Dark grey = Dead Zones (rare spots)  
**Note the blending** — no hard borders; biomes fade into each other.

## Technical notes
- Tree features (trace_tree, capacitor_cans, resonator_spire) are implemented but NOT yet wired into wafer.json. For these screenshots, they were injected via in-memory JSON (the tree is untouched). The game needs the JSON wiring.
- The white clay render matches the MCP agent vision path (which also renders without textures).
- Ground pulse (energized ground) is a render-time emissive effect, not visible in static captures.
