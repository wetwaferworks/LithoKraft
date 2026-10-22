#include <cstdlib>
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/items.h"  // registerGameItems (items into the unified registry)
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace lithokraft {

namespace {
// Mutable handle to the registry for mod registration. The public blockRegistry()
// exposes it const; this one is for registerBlock() during single-threaded mod init.
BlockRegistry& mutableRegistry() {
  static BlockRegistry reg = [] {
    BlockRegistry r;
    r.reserve(64);
    constexpr uint32_t kTerrain = kSolid | kOpaque | kPowerable | kCutsWire;
    constexpr uint32_t kComp = kSolid | kOpaque | kCutsWire;          // full-cube component (not powerable)
    constexpr uint32_t kDir = kComp | kDirectional;
    using R = SimRole; using S = Shape;
    // `id` is the block's stable, namespaced save id. It is written out explicitly (not derived from
    // the texture) so art renames never change it; changing one needs a save migration.
    // Density (A5 wiring, 2026-10-13): the projectile pierce currency
    // (engine ProjectileManager: penetration > density pierces, else the
    // projectile stops and despawns). Values are keyed to the registry's
    // material classes below. JD-DECISION (balance review): NO approved
    // density table exists in tree/docs — these are the minimum
    // differentiated class values needed to make pierce behavior real,
    // NOT JD-approved numbers. Classes: non-solid 0 (never consulted:
    // the voxel seam reports solid=false); glass 10; earth 20;
    // trace growth 25-30; silicon 35-60 by grade; stone 40-50;
    // lamps 40; machine/component cubes 45; resonator crystal 55;
    // structure block 60; Chipor 70; ceramic 80.
    auto def = [](std::string_view id, std::string_view name, uint32_t flags, Shape shape, std::string_view tex,
                  std::string_view behavior, SimRole role, int channel, int tier, int density) {
      return BlockDef{name, flags, shape, tex, behavior, uint16_t(role), uint8_t(channel), uint8_t(tier), 0.8f, false,
                      uint8_t(density), id};
    };
    // Order must match the BlockId constants in blocks.h (Air is row 0, added by the registry).
    r.add(def("lithokraft:silicon", "Wafer Silicon", kTerrain, S::Cube, "silicon", "", R::None, 0, 0, 45));
    r.add(def("lithokraft:red_trace", "Red Trace", kTransparent | kFlatPick, S::Custom, "red_trace", "wire", R::Wire, 1, 0, 0));
    r.add(def("lithokraft:blue_trace", "Blue Trace", kTransparent | kFlatPick, S::Custom, "blue_trace", "wire", R::Wire, 2, 0, 0));
    r.add(def("lithokraft:red_torch", "Red LED", kTransparent | kAttachable, S::Cross, "red_torch", "on_off", R::Torch, 1, 0, 0));
    r.add(def("lithokraft:lamp", "Lamp", kTerrain, S::Cube, "lamp", "on_off", R::Lamp, 0, 0, 40));
    r.add(def("lithokraft:grass", "Grass", kTerrain, S::Cube, "grass", "", R::None, 0, 0, 20));
    r.add(def("lithokraft:dirt", "Dirt", kTerrain, S::Cube, "dirt", "", R::None, 0, 0, 20));
    r.add(def("lithokraft:stone", "Stone", kTerrain, S::Cube, "stone", "", R::None, 0, 0, 50));
    r.add(def("lithokraft:blue_torch", "Blue LED", kTransparent | kAttachable, S::Cross, "blue_torch", "on_off", R::Torch, 2, 0, 0));
    // Cortana 2026-10-15 2215 [GAME] (JD D4): low-slab placeholder voxel model with 4 delay buttons
  // (foliage.h repeatorMesh). Shape::Custom; kTransparent and no kOpaque, so neighbours draw the faces
  // the slab no longer covers. Collision and pick stay a full cell for now (block shapes are deferred).
  // 2026-10-16 (JD rename Repeater -> Repeator): numeric identity is the stable id fnv1a64 of the
  // string id, which chunks, saves and the handshake hash store. Keep the PRE-RENAME stable id so
  // worlds, saves and e015e51cf1559089 are untouched; the string id alias below covers name lookups.
  r.addWithStableId(def("lithokraft:repeator", "Repeator", kSolid | kCutsWire | kDirectional | kTransparent | kInteractive, S::Custom, "repeator", "repeator", R::Repeator, 0, 0, 45), litholattice::stableIdFor("block", "lithokraft:repeater"));
    r.add(def("lithokraft:thresholdor", "Thresholdor", kDir, S::Cube, "thresholdor", "thresholdor", R::Thresholdor, 0, 0, 45));
    r.add(def("lithokraft:redirector", "Redirector", kDir | kInteractive, S::Cube, "redirector", "redirector", R::Redirector, 0, 0, 45));
    r.add(def("lithokraft:pulsor", "Pulsor", kDir, S::Cube, "pulsor", "pulsor", R::Pulsor, 0, 0, 45));
    r.add(def("lithokraft:memor", "Memor", kDir, S::Cube, "memor", "memor", R::Memor, 0, 0, 45));
    r.add(def("lithokraft:fast_latch", "Fast Latch", kDir, S::Cube, "fast_latch", "fast_latch", R::FastLatch, 0, 0, 45));
    r.add(def("lithokraft:switch", "Switch", kSolid | kCutsWire | kInteractive | kAttachable, S::Cube, "switch", "switch", R::Switch, 0, 0, 45));
    r.add(def("lithokraft:button", "Button", kSolid | kCutsWire | kInteractive | kAttachable, S::Cube, "button", "button", R::Button, 0, 0, 45));
    r.add(def("lithokraft:keyboard", "Keyboard", kDir | kInteractive, S::Cube, "keyboard", "keyboard", R::Keyboard, 0, 0, 45));
    r.add(def("lithokraft:color_lamp_16", "Color Lamp 16", kComp, S::Cube, "color_lamp_16", "color_lamp", R::ColorLamp, 0, 16, 40));
    r.add(def("lithokraft:color_lamp_32", "Color Lamp 32", kComp, S::Cube, "color_lamp_32", "color_lamp", R::ColorLamp, 0, 32, 40));
    r.add(def("lithokraft:color_lamp_64", "Color Lamp 64", kComp, S::Cube, "color_lamp_64", "color_lamp", R::ColorLamp, 0, 64, 40));
    r.add(def("lithokraft:silicon_slab", "Silicon Slab", kSolid, S::Slab, "silicon_slab", "", R::None, 0, 0, 35));
    r.add(def("lithokraft:stone_slab", "Stone Slab", kSolid, S::Slab, "stone_slab", "", R::None, 0, 0, 40));
    r.add(def("lithokraft:dirt_slab", "Dirt Slab", kSolid, S::Slab, "dirt_slab", "", R::None, 0, 0, 20));
    r.add(def("lithokraft:tone_block", "Tone Block", kDir, S::Cube, "tone_block", "tone", R::Tone, 0, 0, 45));
    // Vias: stackable tubes, red and blue may stand side by side (vias never link sideways).
    r.add(def("lithokraft:red_via", "Red Via", kSolid | kTransparent, S::Cube, "red_via", "wire", R::Wire, 1, 0, 45));
    r.add(def("lithokraft:blue_via", "Blue Via", kSolid | kTransparent, S::Cube, "blue_via", "wire", R::Wire, 2, 0, 45));
    r.add(def("lithokraft:glass", "Glass", kSolid | kTransparent | kCutsWire, S::Cube, "glass", "", R::None, 0, 0, 10));
    r.add(def("lithokraft:catchor", "Catchor", kDir | kInteractive, S::Cube, "catchor", "catchor", R::Catchor, 0, 0, 45));
    r.add(def("lithokraft:frosted_glass", "Frosted Glass", kSolid | kTransparent | kCutsWire, S::Cube, "frosted_glass", "", R::None, 0, 0, 10));
    r.add(def("lithokraft:ceramic", "Ceramic", kSolid | kOpaque | kCutsWire, S::Cube, "ceramic", "", R::None, 0, 0, 80));  // insulator: never powered
    r.add(def("lithokraft:signal_transmittor", "Signal Transmittor", kDir, S::Cube, "signal_transmittor", "signal_transmittor", R::SignalTransmittor, 0, 0, 45));
    r.add(def("lithokraft:signal_receivor", "Signal Receivor", kDir, S::Cube, "signal_receivor", "signal_receivor", R::SignalReceivor, 0, 0, 45));
    r.add(def("lithokraft:dish", "Dish", kDir, S::Cube, "dish", "dish", R::Dish, 0, 0, 45));
    r.add(def("lithokraft:overclockor", "Overclockor", kDir, S::Cube, "overclockor", "overclockor", R::Overclockor, 0, 0, 45));
    // r.add(def("lithokraft:lithographor", "Lithographor", kDir | kInteractive, S::Cube, "lithographor", "lithographor", R::Lithographor, 0, 0, 45));  // DISABLED
    // G-L1 APPEND WARNING: do NOT re-enable the row HERE when Lithographor
    // lands. Registration order assigns the numeric BlockIds and the registry
    // is append-only (see blocks.h): un-commenting in place would renumber
    // Portor and every block after it, breaking existing saves. Append the
    // row at the END of this registration list instead (after the last row),
    // add the kLithographor extern at the end of blocks.h, and resolve it
    // there like the other constants.
    r.add(def("lithokraft:portor", "Portor", kDir | kInteractive, S::Cube, "portor", "portor", R::Portor, 0, 0, 45));
    r.add(def("lithokraft:fusor", "Fusor", kDir | kInteractive, S::Cube, "fusor", "fusor", R::Fusor, 0, 0, 45));
    // Wafer biome blocks (2026-10-11, procgen pass). Texture keys are the biome stub
    // textures (pack_64/32/16); CloudAtlas replaces with final art later.
    const auto kTipFlags = (kTerrain & ~kOpaque) | kShell;  // 1551 (JD): Ghostly by default; render.bulb_shell Solid re-opaques at mesh time
    // Order must match the BlockId constants in blocks.h (append-only).
    r.add(def("lithokraft:trace_trunk", "Trace Trunk", kTerrain, S::Cube, "trace_bark", "", R::None, 0, 0, 30));
    r.add(def("lithokraft:trace_branch", "Trace Branch", kTerrain, S::Cube, "trace_canopy", "", R::None, 0, 0, 25));
    r.add(def("lithokraft:trace_tip_red", "Red Trace Bulb", kTipFlags,  /* 2026-10-16: see-through shell (render-only flag) */ S::Cube, "trace_tip_red", "", R::None, 0, 0, 20));
    r.add(def("lithokraft:trace_tip_blue", "Blue Trace Bulb", kTipFlags, S::Cube, "trace_tip", "", R::None, 0, 0, 20));
    r.add(def("lithokraft:capacitor_can", "Capacitor Can", kTerrain, S::Cube, "capacitor_side", "", R::None, 0, 0, 50));
    r.add(def("lithokraft:resonator_crystal", "Resonator Crystal", kTerrain, S::Cube, "quartz_spire", "", R::None, 0, 0, 55));
    r.add(def("lithokraft:dead_silicon", "Dead Silicon", kTerrain, S::Cube, "wafer_ground_dead", "", R::None, 0, 0, 45));
    r.add(def("lithokraft:fossil_trace", "Fossil Trace", kTerrain, S::Cube, "wafer_ground_trace", "", R::None, 0, 0, 45));
    r.add(def("lithokraft:broken_via", "Broken Via", kTerrain, S::Cube, "wafer_ground_dead", "", R::None, 0, 0, 45));
    r.add(def("lithokraft:scribe_silicon", "Scribe Silicon", kTerrain, S::Cube, "wafer_ground_dicing", "", R::None, 0, 0, 45));
    r.add(def("lithokraft:dense_silicon", "Dense Silicon", kTerrain, S::Cube, "wafer_ground_capacitor", "", R::None, 0, 0, 60));
    r.add(def("lithokraft:flecked_silicon", "Flecked Silicon", kTerrain, S::Cube, "wafer_ground_resonator", "", R::None, 0, 0, 45));
    // Structure blocks (2026-10-11, procgen structures pass). The dead memory cell reuses the
    // "memor" stub texture until the art pass; it is inert (R::None) -- dead banks don't simulate.
    // Order must match the BlockId constants in blocks.h (append-only).
    r.add(def("lithokraft:dead_memory_cell", "Dead Memory Cell", kTerrain, S::Cube, "memor", "", R::None, 0, 0, 50));
    // Chipor (2026-10-11, workstream 4): the plain solid printed chip (opaque cube).
    // Append-only: ids 51/52 per the coordinator reservation (Silica takes 53+).
    // Order must match the BlockId constants in blocks.h.
    r.add(def("lithokraft:chipor", "Chipor", kDir, S::Cube, "chipor", "chipor", R::Chipor, 0, 0, 70));
    // Chipor viewport (2026-10-11, workstream 4): Shape::Custom so the appearance's meshCustom
    // draws the opaque body + transparent top + miniaturized interior (see chipor_viewport.h).
    // kOpaque (not kTransparent) at block level: the body is solid, neighbours cull against it;
    // only the top face renders transparent, via kShadeGlass in meshCustom.
    r.add(def("lithokraft:chipor_viewport", "Chipor Viewport", kDir, S::Custom, "chipor_viewport", "chipor_viewport", R::Chipor, 0, 0, 70));  // 2026-10-16 WaferBeta: own tex
    // Silica (2026-10-11, workstream 7). Order must match the BlockId constants
    // in blocks.h (append-only). The grass is ground cover, not a solid block:
    // Cross sprite like the torches, transparent, attachable (mounts on the soil
    // below), random-ticking for growth/spread. Tilled silicon is ordinary soil.
    r.add(def("lithokraft:silica_grass", "Silica Grass", kTransparent | kAttachable | kRandomTicks, S::Cross, "silica_grass", "", R::None, 0, 0, 0));
    r.add(def("lithokraft:tilled_silicon", "Tilled Silicon", kTerrain, S::Cube, "silica_tilled", "", R::None, 0, 0, 20));
    // Structure block (2026-10-12): JD's design tool. Interactive (right-click cycles
    // CORNER/SAVE/LOAD via the "structure_block" behavior); the mode lives in the
    // cell-state value bits (see procgen/struct_block.h: StructureBlockMode).
    r.add(def("lithokraft:structure_block", "Structure Block", kSolid | kInteractive, S::Cube, "structure_block", "structure_block", R::None, 0, 0, 60));
    // Certificate table (2026-10-12): JD's certificate-signing station (the enchanting-table
    // analogue). BLOCK ONLY this pass: solid + interactive, SimRole::None, mirroring the
    // structure block -- the "certificate_table" behavior is describe-only for now, so
    // right-click resolves not-usable through the standard path. The signing mechanic
    // (spend the "static" stat, apply "Certificate of X" effects) binds later through
    // components/certificate_table.cpp.
    r.add(def("lithokraft:certificate_table", "Certificate Table", kSolid | kInteractive, S::Cube, "certificate_table", "certificate_table", R::None, 0, 0, 50));
    // Etchor (2026-10-13, wiring pass B6): the etch station. Append-only id 57,
    // after the certificate table. Directional + interactive like the other -OR
    // machines; SimRole::Etchor (appended at the enum's end). The "etchor"
    // behaviour is the describe-only seam in components/etchor.cpp; the etch
    // flow itself (wafer blank + name field -> named Chipor) is game-side in
    // game/etching.cpp. Texture key "etchor" has no .ltxr art yet (falls back
    // like any pre-art block); the Selector/Connector/Laysor machine is future
    // work and is NOT faked here.
    r.add(def("lithokraft:etchor", "Etchor", kDir | kInteractive, S::Cube, "etchor", "etchor", R::Etchor, 0, 0, 45));
    // Procgen plant blocks (2026-10-13, wiring pass B4): one Shape::Cross block
    // per texture in the engine's unified plant set (shrubs.h "2D RENDER
    // BINDING": 3 legacy + 4 Litho Formula), append-only ids 58-64 in texture
    // index order. Textures are real pack assets generated from the engine's
    // own pixel data by tools/plant_texture_gen.cpp. The wafer.json
    // groundcover feature places the formula leaf (DicingPlains) and formula
    // blob (ResonatorSpires) blocks per biome_vegetation.h's formula rows;
    // the rest are registered for mods/future biomes (StaticMarsh's flower
    // row goes live when that biome joins wafer.json).
    r.add(def("lithokraft:procgen_grass_tuft", "Grass Tuft", kTransparent | kAttachable, S::Cross, "procgen_grass_tuft", "", R::None, 0, 0, 0));
    r.add(def("lithokraft:procgen_fern", "Fern", kTransparent | kAttachable, S::Cross, "procgen_fern", "", R::None, 0, 0, 0));
    r.add(def("lithokraft:procgen_dry_scrub", "Dry Scrub", kTransparent | kAttachable, S::Cross, "procgen_dry_scrub", "", R::None, 0, 0, 0));
    r.add(def("lithokraft:procgen_formula_flower", "Formula Flower", kTransparent | kAttachable, S::Cross, "procgen_formula_flower", "", R::None, 0, 0, 0));
    r.add(def("lithokraft:procgen_formula_star", "Formula Star", kTransparent | kAttachable, S::Cross, "procgen_formula_star", "", R::None, 0, 0, 0));
    r.add(def("lithokraft:procgen_formula_leaf", "Formula Leaf", kTransparent | kAttachable, S::Cross, "procgen_formula_leaf", "", R::None, 0, 0, 0));
    r.add(def("lithokraft:procgen_formula_blob", "Formula Blob", kTransparent | kAttachable, S::Cross, "procgen_formula_blob", "", R::None, 0, 0, 0));
    // Seeded formula shrubs (2026-10-13, JD directive): two Shape::Cross
    // blocks whose textures are generateShrubSprite sprites grown from
    // seeds 311 / 314 alone (ShrubParams defaults -- one shared round
    // silhouette, seed-painted colours), materialised as pack assets by
    // tools/plant_texture_gen.cpp (kSeededShrubs). Append-only ids 65-66.
    // NOT placed by worldgen features: waferSetup plants exactly one of
    // each flanking the Wafer spawn (see game/dimensions.cpp).
    r.add(def("lithokraft:formula_shrub_311", "Formula Shrub 311", kTransparent | kAttachable, S::Cross, "formula_shrub_311", "", R::None, 0, 0, 0));
    r.add(def("lithokraft:formula_shrub_314", "Formula Shrub 314", kTransparent | kAttachable, S::Cross, "formula_shrub_314", "", R::None, 0, 0, 0));
    // JD materials blocks (2026-10-14, New-Blocks-Materials list).
    // Append-only ids 67-73; order must match blocks.h. Densities use the
    // A5 material classes (see the density note at the top of this
    // function -- class values, balance-review placeholders, NOT
    // JD-approved numbers): metals 55/40, machine class 45, structure
    // class 60, ceramic class 80.
    r.add(def("lithokraft:copper_block", "Copper Block", kTerrain, S::Cube, "copper_block", "", R::None, 0, 0, 55));
    r.add(def("lithokraft:aluminum_block", "Aluminum Block", kTerrain, S::Cube, "aluminum_block", "", R::None, 0, 0, 40));
    // Gravity Block: JD named it; its function is OPEN. Neutral
    // full-cube placeholder (kComp: solid, not powerable), no behavior,
    // no sim role -- no mechanics invented.
    r.add(def("lithokraft:gravity_block", "Gravity Block", kComp, S::Cube, "gravity_block", "", R::None, 0, 0, 45));
    // Substrate (JD, 2026-10-14): the low block type of the Wafer -- its
    // bedrock layer -- but semi-transparent (kTransparent, no kOpaque,
    // terrain flags otherwise) and breakable (an ordinary solid; density
    // 60, far under the 255 unbreakable class). What lies beneath it (the
    // see-through barrier) is dimension/render work, not this row.
    r.add(def("lithokraft:substrate", "Substrate", kSolid | kTransparent | kPowerable | kCutsWire, S::Cube, "substrate", "", R::None, 0, 0, 60));
    // Insulators (JD, 2026-10-14): three distinct blocks -- Insulatnor,
    // Insulator, Insulatxor -- differentiated by type and color (pack
    // art). Insulating in this def system = the ceramic pattern: solid,
    // opaque, cuts wire, and NEVER kPowerable (cannot be powered).
    r.add(def("lithokraft:insulatnor", "Insulatnor", kSolid | kOpaque | kCutsWire, S::Cube, "insulatnor", "", R::None, 0, 0, 80));
    r.add(def("lithokraft:insulator", "Insulator", kSolid | kOpaque | kCutsWire, S::Cube, "insulator", "", R::None, 0, 0, 80));
    r.add(def("lithokraft:insulatxor", "Insulatxor", kSolid | kOpaque | kCutsWire, S::Cube, "insulatxor", "", R::None, 0, 0, 80));
    // Cortana 2026-10-15 1755: lithography machine blocks [GAME] (JD approved 16:31).
    // Append-only ids 74-81, order must match blocks.h. REGISTRATION + RENDER ONLY:
    // no behavior, SimRole::None (the Etchor logic is the next pass). Art: r2/r3
    // lithography packs (face map in REGISTRATION-NOTES.md). Densities use the A5
    // classes: machine 45, crystal 55. The red/blue Vias already exist (not
    // duplicated); the neutral Vior (arm runs) is new. The Etchor (id 57) only
    // gains its per-face art in resources.json.
    r.add(def("lithokraft:selector", "Selector", kDir, S::Cube, "selector", "", R::None, 0, 0, 45));
    r.add(def("lithokraft:connector", "Connector", kComp, S::Cube, "connector", "", R::None, 0, 0, 45));
    // Laysor: transparent purple crystal, BLENDED translucency (kShadeAlphaBlend via
    // the appearance), never cutout; adjacent Laysors cull their shared faces (the
    // mesher's same-id transparent rule). Directional: the beam axis is front/back.
    r.add(def("lithokraft:laysor", "Laysor", kSolid | kTransparent | kDirectional, S::Cube, "laysor", "", R::None, 0, 0, 55));
    // Emitor: the light source; its front aperture shines the beam through Laysors
    // (rendered as light on the crystal and the Etchor laser port, not geometry).
    // Cortana 2026-10-15 1900: Emitor power toggling -- SimRole::Lamp (lit on adjacent power,
    // bit 0); the lamp writer keeps a directional lamp's facing bits (signal.cpp).
    r.add(def("lithokraft:emitor", "Emitor", kDir, S::Cube, "emitor", "", R::Lamp, 0, 0, 45));
    r.add(def("lithokraft:input_buffer", "Input Buffer", kDir, S::Cube, "input_buffer", "", R::None, 0, 0, 45));
    r.add(def("lithokraft:fabor", "Fabor", kDir, S::Cube, "fabor", "", R::None, 0, 0, 45));
    r.add(def("lithokraft:autofabor", "AutoFabor", kDir, S::Cube, "autofabor", "", R::None, 0, 0, 45));
    r.add(def("lithokraft:vior", "Vior", kComp, S::Cube, "vior", "", R::None, 0, 0, 45));
  // Cortana 2026-10-15 2010 [GAME]: 3D voxel foliage (ids 82-86). Shape::Custom: the game's
  // appearance emits the kind's ONE shared voxel model mesh (lithokraft/content/foliage.h),
  // rotated by a position-hash variant -- each placed block is an instance that stores only
  // its id. Non-solid like the cross plants; no sim role, never ticks. Placed by worldgen
  // (wafer.json "foliage_3d" groundcover) and available in the creative palette.
  r.add(def("lithokraft:foliage_cattail", "Cattails", kTransparent | kAttachable, S::Custom, "foliage_cattail", "", R::None, 0, 0, 0));
  r.add(def("lithokraft:foliage_bush", "Berry Bush", kTransparent | kAttachable, S::Custom, "foliage_bush", "", R::None, 0, 0, 0));
  r.add(def("lithokraft:foliage_fern", "Fern Clump", kTransparent | kAttachable, S::Custom, "foliage_fern", "", R::None, 0, 0, 0));
  r.add(def("lithokraft:foliage_reeds", "Reed Tuft", kTransparent | kAttachable, S::Custom, "foliage_reeds", "", R::None, 0, 0, 0));
  r.add(def("lithokraft:foliage_crystal_shrub", "Crystal Shrub", kTransparent | kAttachable, S::Custom, "foliage_crystal_shrub", "", R::None, 0, 0, 0));
  r.add(def("lithokraft:foliage_moss", "Routing Moss", kTransparent | kAttachable, S::Custom, "foliage_moss", "", R::None, 0, 0, 0));  // Cortana 2026-10-15 2110 (id 87)
  r.add(def("lithokraft:crystal_leaves_a", "Crystal Leaves", kSolid | kTransparent | litholattice::kLeaves, S::Cube, "crystal_leaves_a", "", R::None, 0, 0, 10));  // 2026-10-16 JD leaf pass (CloudAtlas Traced art), appended
  r.add(def("lithokraft:crystal_leaves_b", "Crystal Leaves (Sparse)", kSolid | kTransparent | litholattice::kLeaves, S::Cube, "crystal_leaves_b", "", R::None, 0, 0, 10));  // 2026-10-16 JD leaf pass (CloudAtlas Traced art), appended
  r.add(def("lithokraft:crystal_leaves_reddom", "Red Crystal Leaves", kSolid | kTransparent | litholattice::kLeaves, S::Cube, "crystal_leaves_reddom", "", R::None, 0, 0, 10));  // 2026-10-16 JD leaf pass (CloudAtlas Traced art), appended
  r.add(def("lithokraft:crystal_leaves_bluedom", "Blue Crystal Leaves", kSolid | kTransparent | litholattice::kLeaves, S::Cube, "crystal_leaves_bluedom", "", R::None, 0, 0, 10));  // 2026-10-16 JD leaf pass (CloudAtlas Traced art), appended
  // 2026-10-16 (JD WaferBeta): Lithographor re-enabled, appended at the END (stable numeric ids). Cuboid model
  // (body + lens column + cap, from CloudAtlas lithographor.cuboids.json) meshed in appearance.cpp; no sim yet.
  r.add(def("lithokraft:lithographor", "Lithographor", kSolid | kCutsWire | kDirectional | kTransparent | kInteractive, S::Custom, "lithographor", "lithographor", R::Lithographor, 0, 0, 45));
  // Cortana 2026-10-16 (JD): Aluminum Ramp, appended at the END (stable ids). Slope block with
  // Minecraft-stair corner snapping; state in the low value bits (facing 2, shape 2, hanging 1),
  // see procgen/ramp_shape.h. Shares the aluminum_block model/texture. Collision: full cell.
  r.add(def("lithokraft:aluminum_ramp", "Aluminum Ramp", kSolid | kTransparent | kDirectional, S::Custom, "aluminum_block", "", R::None, 0, 0, 45));
  // Proposed by JD, Built by Cortana (2026-10-16): Highway Ramp, appended at the END (id 94). A gentle
  // 1-in-2 slope over a paired 2-block footprint for highways, byways and interchanges only: straight
  // run, 4 facings, no corners, no hanging (never a stair/roof block). State: facing bits 0-1 (high
  // side), bit 2 half (0 low, 1 high); see procgen/ramp_shape.h `highway`. Reuses the Aluminum Ramp art.
  r.add(def("lithokraft:highway_ramp", "Highway Ramp", kSolid | kTransparent | kDirectional, S::Custom, "aluminum_block", "", R::None, 0, 0, 45));
    // Save migration: pre-rename ids remap to the -OR names (2026-10-10).
    r.addAlias("lithokraft:pulsar", "lithokraft:pulsor");
    // 2026-10-16 (JD): the Repeater is now the Repeator; old saves name the old id.
    r.addAlias("lithokraft:repeater", "lithokraft:repeator");
    r.addAlias("lithokraft:memory_cell", "lithokraft:memor");
    r.addAlias("lithokraft:thresholder", "lithokraft:thresholdor");
    r.addAlias("lithokraft:red_wire", "lithokraft:red_trace");
    r.addAlias("lithokraft:blue_wire", "lithokraft:blue_trace");
    return r;
  }();
  return reg;
}
}  // namespace

namespace {
// Items (2026-10-12) join the SAME unified ContentRegistry under the item
// kind (Infinite Registry phase 2) -- one catalog for blocks + items. Done
// here, not inside the build lambda above: registration needs the finished
// registry (the Chipor item's placesBlock), and the extern-const resolution
// below can trigger this accessor during static init, before kChipor itself
// is assigned -- registerGameItems resolves ids through the registry, which
// is complete by the time any accessor returns. Single-threaded mod-init
// contract, like the registry build itself; the seal in main.cpp goes
// through mutableBlockRegistry(), so items are in before the seal.
void ensureItemsRegistered() {
  static bool done = false, inProgress = false;
  if (done || inProgress) return;
  inProgress = true;
  registerGameItems(mutableRegistry().content());
  inProgress = false;
  done = true;
}
}  // namespace

const BlockRegistry& blockRegistry() {
  BlockRegistry& r = mutableRegistry();
  ensureItemsRegistered();
  return r;
}
BlockRegistry& mutableBlockRegistry() {
  BlockRegistry& r = mutableRegistry();
  ensureItemsRegistered();
  return r;
}

// Resolve a block's numeric id from its string id. Fails loudly (not silently) if the
// string id isn't registered -- a missing block is a programming error, not a runtime condition.
static BlockId resolveId(const BlockRegistry& reg, std::string_view sid) {
  auto id = reg.findById(sid);
  if (!id) {
    std::fprintf(stderr, "blockRegistry: string id '%.*s' not registered\n", int(sid.size()), sid.data());
    std::abort();
  }
  return *id;
}

// Named block ids, resolved from the registry at startup. Numeric ids are auto-assigned
// in registration order (vanilla registration is append-only, so save ids stay stable).
// To add a block: append one r.add() row above, then one line here.
const BlockId kWaferSilicon     = resolveId(blockRegistry(), "lithokraft:silicon");
const BlockId kRedTrace         = resolveId(blockRegistry(), "lithokraft:red_trace");
const BlockId kBlueTrace        = resolveId(blockRegistry(), "lithokraft:blue_trace");
const BlockId kRedTorch         = resolveId(blockRegistry(), "lithokraft:red_torch");
const BlockId kLamp             = resolveId(blockRegistry(), "lithokraft:lamp");
const BlockId kGrass            = resolveId(blockRegistry(), "lithokraft:grass");
const BlockId kDirt             = resolveId(blockRegistry(), "lithokraft:dirt");
const BlockId kStone            = resolveId(blockRegistry(), "lithokraft:stone");
const BlockId kBlueTorch        = resolveId(blockRegistry(), "lithokraft:blue_torch");
const BlockId kRepeator         = resolveId(blockRegistry(), "lithokraft:repeator");
const BlockId kThresholdor      = resolveId(blockRegistry(), "lithokraft:thresholdor");
const BlockId kRedirector       = resolveId(blockRegistry(), "lithokraft:redirector");
const BlockId kPulsor           = resolveId(blockRegistry(), "lithokraft:pulsor");
const BlockId kMemor            = resolveId(blockRegistry(), "lithokraft:memor");
const BlockId kFastLatch        = resolveId(blockRegistry(), "lithokraft:fast_latch");
const BlockId kSwitch           = resolveId(blockRegistry(), "lithokraft:switch");
const BlockId kButton           = resolveId(blockRegistry(), "lithokraft:button");
const BlockId kKeyboard         = resolveId(blockRegistry(), "lithokraft:keyboard");
const BlockId kColorLamp16      = resolveId(blockRegistry(), "lithokraft:color_lamp_16");
const BlockId kColorLamp32      = resolveId(blockRegistry(), "lithokraft:color_lamp_32");
const BlockId kColorLamp64      = resolveId(blockRegistry(), "lithokraft:color_lamp_64");
const BlockId kSiliconSlab      = resolveId(blockRegistry(), "lithokraft:silicon_slab");
const BlockId kStoneSlab        = resolveId(blockRegistry(), "lithokraft:stone_slab");
const BlockId kDirtSlab         = resolveId(blockRegistry(), "lithokraft:dirt_slab");
const BlockId kToneBlock        = resolveId(blockRegistry(), "lithokraft:tone_block");
const BlockId kRedVia           = resolveId(blockRegistry(), "lithokraft:red_via");
const BlockId kBlueVia          = resolveId(blockRegistry(), "lithokraft:blue_via");
const BlockId kGlass            = resolveId(blockRegistry(), "lithokraft:glass");
const BlockId kCatchor          = resolveId(blockRegistry(), "lithokraft:catchor");
const BlockId kFrostedGlass     = resolveId(blockRegistry(), "lithokraft:frosted_glass");
const BlockId kCeramic          = resolveId(blockRegistry(), "lithokraft:ceramic");
const BlockId kSignalTransmittor = resolveId(blockRegistry(), "lithokraft:signal_transmittor");
const BlockId kSignalReceivor   = resolveId(blockRegistry(), "lithokraft:signal_receivor");
const BlockId kDish             = resolveId(blockRegistry(), "lithokraft:dish");
const BlockId kOverclockor      = resolveId(blockRegistry(), "lithokraft:overclockor");
const BlockId kPortor           = resolveId(blockRegistry(), "lithokraft:portor");
const BlockId kFusor            = resolveId(blockRegistry(), "lithokraft:fusor");
const BlockId kTraceTrunk       = resolveId(blockRegistry(), "lithokraft:trace_trunk");
const BlockId kTraceBranch      = resolveId(blockRegistry(), "lithokraft:trace_branch");
const BlockId kTraceTipRed      = resolveId(blockRegistry(), "lithokraft:trace_tip_red");
const BlockId kTraceTipBlue     = resolveId(blockRegistry(), "lithokraft:trace_tip_blue");
const BlockId kCapacitorCan     = resolveId(blockRegistry(), "lithokraft:capacitor_can");
const BlockId kResonatorCrystal = resolveId(blockRegistry(), "lithokraft:resonator_crystal");
const BlockId kDeadSilicon      = resolveId(blockRegistry(), "lithokraft:dead_silicon");
const BlockId kFossilTrace      = resolveId(blockRegistry(), "lithokraft:fossil_trace");
const BlockId kBrokenVia        = resolveId(blockRegistry(), "lithokraft:broken_via");
const BlockId kScribeSilicon    = resolveId(blockRegistry(), "lithokraft:scribe_silicon");
const BlockId kDenseSilicon     = resolveId(blockRegistry(), "lithokraft:dense_silicon");
const BlockId kFleckedSilicon   = resolveId(blockRegistry(), "lithokraft:flecked_silicon");
const BlockId kDeadMemoryCell   = resolveId(blockRegistry(), "lithokraft:dead_memory_cell");
const BlockId kChipor           = resolveId(blockRegistry(), "lithokraft:chipor");
const BlockId kLithographor      = resolveId(blockRegistry(), "lithokraft:lithographor");
const BlockId kChiporViewport   = resolveId(blockRegistry(), "lithokraft:chipor_viewport");
const BlockId kSilicaGrass      = resolveId(blockRegistry(), "lithokraft:silica_grass");
const BlockId kSilicaTilledSoil = resolveId(blockRegistry(), "lithokraft:tilled_silicon");
const BlockId kStructureBlock   = resolveId(blockRegistry(), "lithokraft:structure_block");
const BlockId kCertificateTable = resolveId(blockRegistry(), "lithokraft:certificate_table");
const BlockId kEtchor           = resolveId(blockRegistry(), "lithokraft:etchor");
const BlockId kProcgenGrassTuft = resolveId(blockRegistry(), "lithokraft:procgen_grass_tuft");
const BlockId kProcgenFern      = resolveId(blockRegistry(), "lithokraft:procgen_fern");
const BlockId kProcgenDryScrub  = resolveId(blockRegistry(), "lithokraft:procgen_dry_scrub");
const BlockId kProcgenFormulaFlower = resolveId(blockRegistry(), "lithokraft:procgen_formula_flower");
const BlockId kProcgenFormulaStar   = resolveId(blockRegistry(), "lithokraft:procgen_formula_star");
const BlockId kProcgenFormulaLeaf   = resolveId(blockRegistry(), "lithokraft:procgen_formula_leaf");
const BlockId kProcgenFormulaBlob   = resolveId(blockRegistry(), "lithokraft:procgen_formula_blob");
const BlockId kFormulaShrub311      = resolveId(blockRegistry(), "lithokraft:formula_shrub_311");
const BlockId kFormulaShrub314      = resolveId(blockRegistry(), "lithokraft:formula_shrub_314");
const BlockId kCopperBlock          = resolveId(blockRegistry(), "lithokraft:copper_block");
const BlockId kAluminumBlock        = resolveId(blockRegistry(), "lithokraft:aluminum_block");
const BlockId kGravityBlock         = resolveId(blockRegistry(), "lithokraft:gravity_block");
const BlockId kSubstrate            = resolveId(blockRegistry(), "lithokraft:substrate");
const BlockId kInsulatnor           = resolveId(blockRegistry(), "lithokraft:insulatnor");
const BlockId kInsulator            = resolveId(blockRegistry(), "lithokraft:insulator");
const BlockId kInsulatxor           = resolveId(blockRegistry(), "lithokraft:insulatxor");
// Cortana 2026-10-15 1755: lithography machine blocks (ids 74-81).
const BlockId kSelector             = resolveId(blockRegistry(), "lithokraft:selector");
const BlockId kConnector            = resolveId(blockRegistry(), "lithokraft:connector");
const BlockId kLaysor               = resolveId(blockRegistry(), "lithokraft:laysor");
const BlockId kEmitor               = resolveId(blockRegistry(), "lithokraft:emitor");
const BlockId kInputBuffer          = resolveId(blockRegistry(), "lithokraft:input_buffer");
const BlockId kFabor                = resolveId(blockRegistry(), "lithokraft:fabor");
const BlockId kAutoFabor            = resolveId(blockRegistry(), "lithokraft:autofabor");
const BlockId kVior                 = resolveId(blockRegistry(), "lithokraft:vior");
// Cortana 2026-10-15 2010 [GAME]: 3D foliage.
const BlockId kFoliageCattail      = resolveId(blockRegistry(), "lithokraft:foliage_cattail");
const BlockId kFoliageBush         = resolveId(blockRegistry(), "lithokraft:foliage_bush");
const BlockId kFoliageFern         = resolveId(blockRegistry(), "lithokraft:foliage_fern");
const BlockId kFoliageReeds        = resolveId(blockRegistry(), "lithokraft:foliage_reeds");
const BlockId kFoliageCrystalShrub = resolveId(blockRegistry(), "lithokraft:foliage_crystal_shrub");
const BlockId kFoliageMoss          = resolveId(blockRegistry(), "lithokraft:foliage_moss");
const BlockId kAluminumRamp       = resolveId(blockRegistry(), "lithokraft:aluminum_ramp");  // Cortana 2026-10-16
const BlockId kHighwayRamp        = resolveId(blockRegistry(), "lithokraft:highway_ramp");   // Proposed by JD, Built by Cortana

namespace {
// F-M4 namespace format: `<namespace>:<path>` where namespace is
// [a-z0-9_.-]{1,64} and path is [a-z0-9_./-]{1,128} — the same character
// classes the assets mod-manifest parser accepts for mod ids, so an id that
// is valid here is valid there.
bool isValidBlockNamespaceId(std::string_view id) {
  const size_t colon = id.find(':');
  if (colon == std::string_view::npos || colon == 0 || colon > 64) return false;
  const std::string_view path = id.substr(colon + 1);
  if (path.empty() || path.size() > 128) return false;
  const auto nsChar = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
  };
  for (char c : id.substr(0, colon))
    if (!nsChar(c)) return false;
  for (char c : path)
    if (!nsChar(c) && c != '/') return false;
  return true;
}
}  // namespace

BlockId registerBlock(BlockDef def) {
  // Single-threaded mod-init only: the sim reads the registry from multiple threads once worlds load.
  // The registry ENFORCES that window (review F-M3): debug builds assert in
  // ContentRegistry mutators when registration happens off the mod-init
  // thread, and registration past the 16-bit BlockId cap is refused with a
  // log (F-M5) rather than silently aliasing onto air.
  //
  // NAMESPACE OWNERSHIP CONTRACT (review F-M4): a block id MUST be a
  // well-formed `<namespace>:<path>` id (see isValidBlockNamespaceId) whose
  // namespace is the registering mod's own manifest id. Malformed ids are
  // refused here (logged, kAir returned) — previously an empty id silently
  // became `anonymous:<tex|name>` in the registry, and a malformed or
  // foreign id was accepted unchecked. First-wins in the registry is the
  // second line of defence: a well-formed id in ANOTHER mod's namespace can
  // only be registered by whoever runs first, so load order + the manifest
  // gate are what assign ownership today. Per-registrant ownership
  // ENFORCEMENT (refusing a well-formed id outside the caller's own
  // namespace) requires the caller to present its manifest id — that
  // identity does not exist in this signature yet, because no mod loader is
  // wired into the game (see F-L12: mods/mod_loader is staged/unwired, and
  // the only production caller path is first-party init). When the loader is
  // wired, it must pass the manifest id through here and this function must
  // refuse mismatches; until then, squatting is prevented by the fact that
  // unwired mods cannot call this at all.
  if (!isValidBlockNamespaceId(def.id)) {
    std::fprintf(stderr,
                 "registerBlock: refused malformed block id '%s' "
                 "(must be <namespace>:<path>, namespace [a-z0-9_.-]{1,64}, "
                 "path [a-z0-9_./-]{1,128})\n",
                 std::string(def.id).c_str());
    return kAir;
  }
  return mutableRegistry().add(std::move(def));
}

}  // namespace lithokraft
