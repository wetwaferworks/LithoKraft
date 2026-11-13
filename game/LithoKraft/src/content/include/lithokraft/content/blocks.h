// LithoKraft's blocks: ids, game flags, sim roles and the registry the game hands to the engine.
// The engine (litholattice::cBlockReg) only stores these rows; everything they mean is here.
#pragma once
#include <cstdint>
#include <vector>
#include "lithokraft/engine.h"
#include "litholattice/world/block.h"

namespace lithokraft {

// Block ids: resolved from the registry at startup (blocks.cpp), keyed by string id
// ("lithokraft:pulsor"). Numeric ids are auto-assigned in registration order -- vanilla
// registration is append-only, so save ids stay stable. Never hardcode a number here;
// to add a block, append one r.add() row in blocks.cpp and one extern const below.
extern const BlockId kWaferSilicon;
extern const BlockId kRedTrace;
extern const BlockId kBlueTrace;
extern const BlockId kRedTorch;   // outputs only into red trace
extern const BlockId kLamp;
extern const BlockId kGrass;
extern const BlockId kDirt;
extern const BlockId kStone;
extern const BlockId kBlueTorch;  // outputs only into blue trace
// Preview-build components (art batch 2). Directional ones are aimed on placement.
extern const BlockId kRepeator;
extern const BlockId kThresholdor;
extern const BlockId kRedirector;
extern const BlockId kPulsor;
extern const BlockId kMemor;
extern const BlockId kLatchor;
extern const BlockId kDividor;  ///< 2026-11-05 (JD): Dividor, front = floor(back / 2)
extern const BlockId kDialor;  ///< 2026-11-11 (JD): Dialor, independent 1..64 source, back inhibit
extern const BlockId kSwivelor;  ///< B9 2026-11-12 (JD): Swivelor (revised), row B gear ring + resin mount; no control port (wells OPEN)
extern const BlockId kPushor;  ///< B9 2026-11-12 (JD): Pushor, row B honeycomb face; power + quasi-connectivity only, extension distance OPEN
extern const BlockId kBitor;  ///< B9 2026-11-12 (JD): Bitor, row B op readout; control 0 = AND (16/32/64 unconfirmed)
extern const BlockId kWatchor;  ///< B9 2026-11-12 (JD): Watchor, row B eye cube; fires 64 out the back for one tick on a change ahead
extern const BlockId kFusorSum;  ///< PlayN (JD): Fusor adder (old Fusor id = kFusor = Breakor)
extern const BlockId kRedirectorSelect;  ///< 2026-11-04 (JD t4302u): Redirector (selected input -> front); kRedirector = Multiplexor
extern const BlockId kRandor;  ///< 2026-10-31 (JD): random range block
extern const BlockId kSwitch;
extern const BlockId kButton;
extern const BlockId kKeyboard;
extern const BlockId kColorLamp16;
extern const BlockId kColorLamp32;
extern const BlockId kColorLamp64;
extern const BlockId kSiliconSlab;
extern const BlockId kStoneSlab;
extern const BlockId kDirtSlab;
extern const BlockId kToneBlock;
extern const BlockId kRedVia;   // hollow vertical wire tube: links only straight up/down, lossless
extern const BlockId kBlueVia;
// 2026-10-25 (JD): side-lying Vias, open ends along X or Z (appended ids; old saves keep vertical vias).
extern const BlockId kRedViaX;
extern const BlockId kRedViaZ;
extern const BlockId kBlueViaX;
extern const BlockId kBlueViaZ;
extern const BlockId kGlass;    // non-conductive, see-through; does NOT cut wire stairs (JD 2026-11-08: transparent never cuts; Trace stairs through glass)
extern const BlockId kCatchor;
extern const BlockId kFrostedGlass;  // like glass, diffused look
extern const BlockId kCeramic;
extern const BlockId kSignalTransmittor;  ///< wireless signal source (trace -> signal)
extern const BlockId kSignalReceivor;     ///< wireless signal sink (signal -> trace)
extern const BlockId kDish;               ///< long-range receivor; hears channel 0 (the Daemon)
extern const BlockId kOverclockor;
extern const BlockId kPortor;
extern const BlockId kFusor;  ///< cFusor block (G-L2: this line previously carried garbled
                              ///< copy-paste fragments of five other blocks' doc comments)
// Wafer biome blocks (2026-10-11, procgen pass). Texture keys reuse existing art
// until CloudAtlas does the biome art pass; geometry and placement come first.
extern const BlockId kTraceTrunk;       ///< trace-tree trunk (via-like)
extern const BlockId kTraceBranch;      ///< trace-tree branch
extern const BlockId kTraceTipRed;      ///< glowing red branch tip (emissive via appearance)
extern const BlockId kTraceTipBlue;     ///< glowing blue branch tip (emissive via appearance)
extern const BlockId kCapacitorCan;     ///< capacitor-grove trunk (squat cylinder read)
extern const BlockId kResonatorCrystal; ///< resonator spire crystal
extern const BlockId kDeadSilicon;      ///< dead-zone surface: grey, unpowered
extern const BlockId kFossilTrace;      ///< fossilized dead trace (underground ruins)
extern const BlockId kBrokenVia;        ///< broken via stub (underground ruins)
// Biome surface blocks (2026-10-11, texture stub pass) — per-biome ground palettes.
extern const BlockId kScribeSilicon;    ///< dicing plains surface (die-grid scribe lines)
extern const BlockId kDenseSilicon;     ///< capacitor grove ground (dark dense metallic)
extern const BlockId kFleckedSilicon;   ///< resonator spire ground (quartz-flecked)
// Structure blocks (2026-10-11, procgen structures pass).
extern const BlockId kDeadMemoryCell;   ///< dead memory-bank cell: inert (no sim role), memor-look stub texture
// Chipor (2026-10-11, workstream 4): the printed chip. kChipor is the plain solid
// chip; kChiporViewport is the trophy-case variant with a transparent top - peer
// inside and see the miniaturized circuit (see content/chipor_viewport.h).
extern const BlockId kChipor;           ///< Chipor: solid printed chip, inert (no sim role)
extern const BlockId kLithographor;     ///< 2026-10-16 WaferBeta: cuboid machine, appended at the registry end
extern const BlockId kChiporViewport;   ///< Chipor viewport: transparent top, opaque body, static interior

// Silica (2026-10-11, workstream 7): the Wafer's grass equivalent -- silicon
// fern ground cover -- plus tilled silicon farmland. Ids follow the Chipor rows.
extern const BlockId kSilicaGrass;      ///< silicon fern ground cover ("silica grass")
extern const BlockId kSilicaTilledSoil; ///< tilled silicon farmland (plantable)

// Structure block (2026-10-12): JD's in-game design tool (Minecraft's structure
// block, renamed). Modes in the cell-state value bits: 0=CORNER, 1=SAVE, 2=LOAD.
// CORNERx2 marks a region; SAVE captures it into the cStructureReg under a
// name; LOAD places a named structure. Behavior "structure_block".
extern const BlockId kStructureBlock;   ///< structure block: capture/save/load tool

// Certificate table (2026-10-12): JD's certificate-signing station (the
// enchanting-table analogue). Signing will spend the "static" stat and apply
// "Certificate of X" effects to items. THIS PASS IS THE BLOCK ONLY: the
// signing mechanic binds later through the "certificate_table" behavior
// (components/certificate_table.cpp). Behavior "certificate_table".
extern const BlockId kCertificateTable; ///< certificate table: signing station (mechanic wired game-side, B7)
extern const BlockId kEtchor;           ///< Etchor: the etch station (block + role; flow in game/etching.cpp, B6)
extern const BlockId kProcgenGrassTuft;   ///< procgen plant crosses (B4): texture-index order 0..6
extern const BlockId kProcgenFern;
extern const BlockId kProcgenDryScrub;
extern const BlockId kProcgenFormulaFlower;
extern const BlockId kProcgenFormulaStar;
extern const BlockId kProcgenFormulaLeaf;  ///< placed by wafer.json groundcover in DicingPlains
extern const BlockId kProcgenFormulaBlob;  ///< placed by wafer.json groundcover in ResonatorSpires
extern const BlockId kFormulaShrub311;     ///< seeded Litho Formula shrubs (JD directive 2026-10-13):
extern const BlockId kFormulaShrub314;     ///< generateShrubSprite seeds 311 / 314, planted at Wafer spawn

// JD materials blocks (2026-10-14, New-Blocks-Materials list): the solid
// block forms of the two interconnect metals (the materials themselves are
// items, see items.h), plus three JD-named blocks. Append-only ids 67-73.
extern const BlockId kCopperBlock;    ///< Copper Block
extern const BlockId kAluminumBlock;  ///< Aluminum Block
extern const BlockId kGravityBlock;   ///< Gravity Block (JD-named; function OPEN -- neutral block, no behavior)
extern const BlockId kSubstrate;      ///< Substrate: the Wafer's low block (its bedrock layer) -- semi-transparent and BREAKABLE (JD, 2026-10-14)
// The insulator family (JD, 2026-10-14): three distinct insulator blocks,
// differentiated by type and color -- Insulatnor, Insulator, Insulatxor.
extern const BlockId kInsulatnor;     ///< Insulatnor (insulator, type 1)
extern const BlockId kInsulator;      ///< Insulator (insulator, type 2)
extern const BlockId kInsulatxor;     ///< Insulatxor (insulator, type 3)
// Cortana 2026-10-15 1755: lithography machine blocks (JD approved 16:31), ids 74-81,
// registration + render only (Etchor logic is the next pass).
extern const BlockId kSelector;       ///< Selector (tells the Etchor what to make)
extern const BlockId kConnector;      ///< Connector (9-pad wiring block)
extern const BlockId kLaysor;         ///< Laysor: transparent purple laser crystal (blended translucency)
extern const BlockId kEmitor;         ///< Emitor: light source; beam through the Laysor into the Etchor
extern const BlockId kTimerBuffer;    ///< 2026-10-25 roadmap 7: comparator-clock input buffer (hidden, no art yet)
extern const BlockId kVirtualPost;       ///< 2026-10-25 Posts: Standing Post (2 tall, 8 facings), virtual wood
extern const BlockId kOrganicPost;       ///< Standing Post, organic wood
extern const BlockId kVirtualWallPost;   ///< Wall Post (one panel on the block in front, 4 facings), virtual wood
extern const BlockId kOrganicWallPost;   ///< Wall Post, organic wood
extern const BlockId kInputBuffer;    ///< Input buffer (wafers in on top, out the front)
extern const BlockId kFabor;          ///< Fabor (wafer fab)
extern const BlockId kAutoFabor;      ///< AutoFabor (Trace-controlled Fabor)
extern const BlockId kVior;           ///< Vior (neutral arm run; red/blue are the existing Vias)
// Cortana 2026-10-15 2010 [GAME]: 3D voxel foliage instances (ids 82-86, Shape::Custom).
extern const BlockId kFoliageCattail;       ///< Cattails (2 blocks tall model)
extern const BlockId kFoliageBush;          ///< Berry Bush (engine generateShrubOctree)
extern const BlockId kFoliageFern;          ///< Fern Clump
extern const BlockId kFoliageReeds;         ///< Reed Tuft
extern const BlockId kFoliageCrystalShrub;  ///< Crystal Shrub (Litho Formula voxelizer)
extern const BlockId kFoliageMoss;          ///< Routing Moss (id 87, Cortana 2026-10-15 2110)
extern const BlockId kHighwayRamp;           ///< Highway Ramp (2-block 1:2 slope, highways only; Proposed by JD, Built by Cortana)
extern const BlockId kAluminumRamp;          ///< Aluminum Ramp (slope, corner snapping; Cortana 2026-10-16)

/// Game flags (BlockDef::flags, above the engine's kFirstGameFlag).
constexpr uint32_t kPowerable = uint32_t(kFirstGameFlag) << 0;  // can be powered by wire or a torch below (then turns torches on it off)
constexpr uint32_t kCutsWire = uint32_t(kFirstGameFlag) << 1;   // legacy flag: formerly the climb-cut rule. Since the 2026-11-08 wire job the climb derives its cut from block properties (cutsWireClimb below); this flag remains registry data for its other readers (e.g. game.cpp's flatlab pad test) and no longer decides climbs.
// Nav classification flags (P0, 2026-10-14): read by cAiHost's NavRules
// builder (gameNavRules) to feed the engine cWalkCache. NO block row carries
// either flag yet -- there is no ladder/vine or fluid content in the game --
// so today they classify nothing; they exist so the first climbable/fluid
// block is a data change, not a code change.
constexpr uint32_t kGameClimbable = uint32_t(kFirstGameFlag) << 2;  // mobs may climb it (ladder/vine class)
constexpr uint32_t kGameFluid = uint32_t(kFirstGameFlag) << 3;      // swimmable fluid cell

/// What a block does in the signal sim (stored in BlockDef::role).
enum class SimRole : uint16_t {
  None, Wire, Torch, Lamp, ColorLamp,
  Repeator, Thresholdor, Redirector, Pulsor, Memor, Latchor, Switch, cButton, Keyboard, Tone, Catchor,
  cSignalTransmittor, cSignalReceivor, cDish, cOverclockor, Lithographor, Chipor, cPortor, cFusor,
  Etchor, Randor, cFusorSum, Dividor, Dialor, Watchor, Bitor, Pushor, Swivelor,  // Swivelor + Pushor appended B9 2026-11-12 (JD); Bitor appended B9 2026-11-12 (JD); Watchor appended B9 2026-11-12 (JD); Dialor appended 2026-11-11 (JD); Dividor appended 2026-11-05 (JD); cFusorSum appended PlayN; Randor appended 2026-10-31; Etchor appended 2026-10-13 (wiring pass B6): never insert mid-enum -- values are registry data
};
constexpr SimRole roleOf(const BlockDef& d) { return SimRole(d.role); }
/// Lets content code compare the engine's raw BlockDef::role with a SimRole (found by ADL).
constexpr bool operator==(uint16_t role, SimRole r) { return role == uint16_t(r); }

/// Strongest signal (wire strength, component output).
constexpr int kMaxSignal = 64;

/// Radio channels unlocked by progression (4 -> 16 -> 64). The transmittor / receivor cycle
/// sneak+use through 1..unlocked; channel 0 is the Daemon's and never unlocks.
inline uint8_t& radioChannelsUnlocked() {
  static uint8_t n = 4;
  return n;
}
inline void setRadioChannelsUnlocked(uint8_t n) {
  radioChannelsUnlocked() = n < 1 ? 1 : (n > 64 ? 64 : n);
}
/// Transmittor broadcast power in blocks (link budget: range = power + receivor sensitivity).
constexpr int kTransmittorPower = 256;
/// cDish receivor sensitivity in blocks (link budget: range = power + sensitivity).
constexpr int kDishSensitivity = 1024;
/// Jammer radius in blocks (cOverclockor + jammer item).
constexpr int kJammerRadius = 64;      ///< base (cOverclockor)
constexpr int kJammerRadiusCopper = 32;  ///< Copper cOverclockor
constexpr int kJammerRadiusSilver = 64;  ///< Silver cOverclockor
constexpr int kJammerRadiusViolet = 128; ///< Violet cOverclockor

/// The LithoKraft block registry (built once, thread-safe to read).
const cBlockReg& blockRegistry();
/// 2026-11-04 (JD t4305u): the ONE right-click gate (game.cpp Use and the selftests): a block takes Use
/// only when it is flagged kInteractive. The Thresholdor lacked the flag, so its subtract toggle
/// worked in the selftest (sim.use direct) but never in game.
/// B9 (JD 2026-11-11): the DIALOR's dial is set with Alt + wheel (kCmdAltUse), never a bare click,
/// so a bare right click does not reach it (its kInteractive use-cycle flag and use() stay as ruled).
inline bool playerUsable(const cBlockReg& reg, BlockId b) {
  return reg.has(b, kInteractive) && roleOf(reg.def(b)) != SimRole::Dialor;
}
/// Mutable registry handle for the seal/hot-swap system. Call single-threaded:
/// seal() at boot after mods load, stageBlockUpdate()/refreshStaged() via the
/// debug hot-swap path (cSimThread::runBetweenTicks). Never while sealed.
cBlockReg& mutableBlockRegistry();
/// Mod API: register a new block, returns its numeric id. Call single-threaded during mod
/// init, before any world loads. Vanilla ids are dense from 1; mod blocks append after.
/// Namespace contract (F-M4): `def.id` must be a well-formed `<namespace>:<path>`
/// id in the registering mod's OWN manifest namespace (namespace
/// [a-z0-9_.-]{1,64}, path [a-z0-9_./-]{1,128}); malformed ids are refused
/// (logged, kAir returned). See blocks.cpp for the full ownership contract
/// and its current enforcement limits.
BlockId registerBlock(BlockDef def);
inline BlockDef blockDef(BlockId b) { return blockRegistry().def(b); }
inline SimRole roleOf(BlockId b) { return roleOf(blockDef(b)); }
inline bool isVia(BlockId b) { return b == kRedVia || b == kBlueVia || b == kRedViaX || b == kRedViaZ || b == kBlueViaX || b == kBlueViaZ; }
/// Via axis: -1 not a via, 0 vertical (Y), 1 side-lying along X, 2 side-lying along Z.
inline int viaAxis(BlockId b) {
  if (b == kRedVia || b == kBlueVia) return 0;
  if (b == kRedViaX || b == kBlueViaX) return 1;
  if (b == kRedViaZ || b == kBlueViaZ) return 2;
  return -1;
}
inline bool isSideVia(BlockId b) { return viaAxis(b) > 0; }
/// 2026-10-25: blocks kept out of the creative palette, test-world kit and block grid (placement
/// variants, or no art yet). Still placeable by id / command and in scenes.
inline bool isPaletteHidden(BlockId b) { return isSideVia(b) || b == kTimerBuffer; }
/// 2026-10-31 (JD): the creative palette list, built from the block registry in registry order (new blocks
/// append themselves). Skips air; internal / placement-variant blocks only when `includeHidden` is false.
std::vector<BlockId> paletteBlocks(bool includeHidden = false);
/// 2026-10-31 (JD): the creative palette grouped for display (Trace, lamps/LEDs, building, other);
/// LL_PALETTE_GROUPS=0 = registry order. `rank` = curated order inside the Trace family.
int paletteFamily(BlockId b, int* rank = nullptr);
std::vector<BlockId> paletteBlocksGrouped();
/// The vertical (item) form of any via: side vias drop as their base colour.
inline BlockId viaBase(BlockId b) { return (b == kRedViaX || b == kRedViaZ) ? kRedVia : (b == kBlueViaX || b == kBlueViaZ) ? kBlueVia : b; }
/// The side-lying variant of a vertical via for axis 1 (X) or 2 (Z); other blocks unchanged.
inline BlockId viaSide(BlockId b, int axis) {
  if (b == kRedVia) return axis == 1 ? kRedViaX : kRedViaZ;
  if (b == kBlueVia) return axis == 1 ? kBlueViaX : kBlueViaZ;
  return b;
}
/// Any wire-network cell (wire or via). Vias only talk vertically to their own colour, so code that
/// reads "power from an adjacent wire" uses isFlatWire.
inline bool isWire(BlockId b) { return roleOf(b) == SimRole::Wire; }
inline bool isFlatWire(BlockId b) { return isWire(b) && !isVia(b); }
/// Whether a block can hold flat Trace on top of it (or under it, as a ceiling).
/// JD's wire rulings, 2026-11-08: ANY solid block supports, deliberately broader
/// than Minecraft: bottom slabs support (a slab is solid; Trace also climbs up
/// and down slab steps, symmetrically), and crystal leaves support (a deliberate
/// difference from Minecraft, JD's explicit choice). Non-solid blocks never do.
inline bool canSupportWire(BlockId b) { return (blockDef(b).flags & kSolid) != 0; }
/// Trace (flat wire, not vias) is a PARTIAL BLOCK THAT REQUIRES SUPPORT (JD 2026-10-24): it must lie on a
/// solid block or hang under one (a ceiling); a wall beside it is no support. Placement is refused without
/// support, and losing both supports pops it (cSimThread::popAttached -> stillSupported). Vias need none.
template <class Map> bool wireSupported(const Map& w, int x, int y, int z) {
  return canSupportWire(w.get(x, y - 1, z)) || canSupportWire(w.get(x, y + 1, z));
}
/// The insulating blocks (JD, 2026-10-14): Ceramic plus the three Insulat* blocks.
/// In this def system insulating = solid, opaque, never kPowerable; the explicit
/// list exists so the climb-cut rule below can name them even if a future
/// insulating block is not solid + opaque.
inline bool isInsulatingBlock(BlockId b) {
  return b == kCeramic || b == kInsulatnor || b == kInsulator || b == kInsulatxor;
}
/// JD's climb-cut rule (2026-11-08, wire plan decision 2): the block in the cut
/// cell (above the lower wire of a climb, or the step edge a descent passes)
/// cuts the climb iff it is solid AND opaque, or it is an insulating block.
/// Transparent blocks never cut (glass and frosted glass let Trace stair
/// through them) and neither do non-solid blocks. Derived from block
/// properties, NOT from the legacy kCutsWire flag: the flag and this rule
/// disagree exactly on the transparent flag-carriers (glass, frosted glass,
/// substrate, the Repeator, Switch, Button, Lithographor, the Trace Bulbs),
/// which the flag cut and this rule does not. JD reviews the per-block census
/// (Trace Wire Run Report, 2026-11-08) and adds special cases by name.
inline bool cutsWireClimb(BlockId b) {
  const BlockDef d = blockDef(b);
  return ((d.flags & kSolid) && (d.flags & kOpaque)) || isInsulatingBlock(b);
}
/// Whether the step block beside a climbing wire presents a full sturdy face
/// to it, so the climb's wall piece (the Straight piece running up the side)
/// is drawn on it. Vanilla's rule, adapted: full solid + opaque cubes only.
/// Glass, frosted glass, substrate, slabs and ramps take the floor strip
/// only, no wall. Render-only; the sim link itself does not consult this.
inline bool wireStepSturdy(BlockId b) {
  const BlockDef d = blockDef(b);
  return (d.flags & kSolid) && (d.flags & kOpaque) && d.shape == Shape::Cube;
}
/// The height (in 1/16 block units) of the top surface of the support under a
/// flat wire cell: where the wire's floor piece SEATS (JD ruling, next-build-2
/// run 2026-11-09, D2.3: the wire rests on its support's actual top surface).
/// Derived from the support's shape, the same way support itself is derived
/// (canSupportWire above): a bottom slab's top is at half height (8), a full
/// block's at 16. Any other shape that can hold a wire seats at 16 today...
/// the full cell top is the only other top surface the mesher draws for a
/// supporting shape. Render-only; the sim never consults this.
/// FRAME WARNING (wire float hunt, 2026-11-09): the value is measured in
/// the SUPPORT's own cell frame. The wire's cell floor is the support
/// cell's top, so a caller drawing in the wire's frame converts first
/// (seat = value - 16 for a solid support; 0 = not solid, no surface).
/// Using the raw value as a wire-frame offset floated every wire piece
/// exactly one full block in build 113.
inline int wireSeatHeight(BlockId support) {
  const BlockDef d = blockDef(support);
  if (!(d.flags & kSolid)) return 0;
  return d.shape == Shape::Slab ? 8 : 16;
}
inline bool isComponent(BlockId b) { return roleOf(b) != SimRole::None; }
/// Wire channel of a wire / torch (1 = red, 2 = blue; channels never connect).
inline int wireChannel(const BlockDef& d) { return d.channel; }
/// Colour a neighbour passes to components: cBehaviorCatalog::colour(def) (cWire / cTorch).

}  // namespace lithokraft
