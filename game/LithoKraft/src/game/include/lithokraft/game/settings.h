// Game settings: the player's preferences, persisted to config/settings.json (next to
// config/input.json). Every field maps to a real control, applied live or at boot:
//   - master volume + mute: cAudioEngine (live, via masterGain commands / setEnabled)
//   - HUD scale: the UI theme scale (live; 0 = auto from the window height)
//   - display mode + vsync: the window (display mode live; vsync at boot)
//   - tick rate + EUV domain: the sim (live, via setTickRate / toggleDomain)
//   - tick radius: the sim (boot-only: SimConfig::radiusChunks)
//   - mesher: chunk mesher (boot-only: PackedVertex or BinaryQuad+PBR, world (re)load)
//   - PBR transparency tier: the PBR path's tier (live via setPbrTier; Video page dropdown)
#pragma once
#include <string>
#include "litholattice/render/pbr_transparency_tiers.h"  // PbrTransparencyTier for pbrTier

namespace lithokraft {

/// Display modes in the settings order (names must match Window::parseDisplayMode).
constexpr const char* kSettingsDisplayModes[] = {"Windowed", "Maximized", "Borderless", "Fullscreen"};
constexpr int kSettingsDisplayModeCount = 4;
/// Tick rates offered in the settings UI (kTickModes plus the 512 stress mode).
constexpr int kSettingsTpsModes[] = {32, 64, 128, 256, 512};
constexpr int kSettingsTpsModeCount = 5;
/// Mesher options in the settings order (BinaryQuad/PackedVertex coexistence, JD 2026-10-11).
constexpr const char* kSettingsMesherNames[] = {"PackedVertex", "BinaryQuad"};
constexpr int kSettingsMesherCount = 2;
/// PBR transparency tier names in PbrTransparencyTier order (must match
/// pbrTransparencyTierName; the Video-page dropdown indexes GameSettings::pbrTier
/// directly, so the order here is the enum order).
constexpr const char* kSettingsPbrTierNames[] = {"Simple", "ScreenDoor", "PbrReflections",
                                                 "TrueRefraction"};
constexpr int kSettingsPbrTierCount = 4;
static_assert(kSettingsPbrTierCount == int(litholattice::PbrTransparencyTier::TrueRefraction) + 1,
              "PBR tier dropdown must cover every PbrTransparencyTier in enum order");

/// Nearest valid tick rate to `v` (one of kSettingsTpsModes; ties round down).
/// Used by settings.json load() and the sim.tickrate cvar setter so the pow2
/// invariant holds on every path, not just at boot.
int nearestTps(int v);

struct GameSettings {
  float masterVolume = 0.6f;  ///< 0..1; matches the cAudioEngine default gain
  bool muted = false;
  int hudScale = 0;       ///< 0 = auto, else a fixed GUI scale (1-4)
  int displayMode = 2;    ///< index into kSettingsDisplayModes (Cortana 2026-10-15 1153: default Borderless per JD, was 0 Windowed)
  /// Renderer backend at startup: "software" (shipped default), "gl33" or "vulkan" (JD 2026-10-24).
  /// Settings > Graphics > Renderer saves the world and relaunches with it; --renderer still wins.
  std::string renderer = "software";
  bool vsync = true;      ///< 2026-10-24 (JD): default ON. boot-only: the window is created with it
  int tps = 32;           ///< one of kSettingsTpsModes (kDefaultTps)
  int tickRadius = 8;     ///< simulation distance, 0..512 chunks; boot-only. F56-10 (JD 2026-10-17): default 8; 16/32 are stress-test options; env LL_SIM_TICK_RADIUS (was 16 since F55-5)
  bool euv = true;        ///< the lithographed EUV-tier sim domain
  int mesher = 0;         ///< index into kSettingsMesherNames; boot-only (world (re)load):
                          ///< 0 = PackedVertex (classic chunk programs), 1 = BinaryQuad
                          ///< (chunk_pbr programs; falls back to 0 if the backend lacks it)
  int pbrTier = 1;        ///< PbrTransparencyTier as int (0..3); live via setPbrTier.
                          ///< Default ScreenDoor (full PBR, no sorting); the Video
                          ///< page "PBR transparency (live)" dropdown drives it.
  int ambientMode = 0;    ///< flood-fill ambient mode (JD 2026-10-11): 0 = Fraction
                          ///< (ambient = skyLevel/64, cheap; default), 1 = NormalizedSun
                          ///< (direction-aware hemisphere via the sun vector). Live.
  float frustumMarginDeg = 5.f;  ///< FOV culling margin in degrees, 0..15 (JD 2026-10-11):
                          ///< chunks inside FOV +/- margin render; the rest cull.
                          ///< Shock absorber against "frustum occlusion tearing". Live.
  // Culling-cache prototypes (JD 2026-10-11; all default OFF — JD A/B tests
  // them on his workstation). See render/cull_prototypes.h. Live.
  int cometTailFrames = 0;       ///< render.comet_tail_frames: temporal frustum
                          ///< union depth, 0..64 (0 = off). Remembers where the
                          ///< camera looked, not just where it is.
  bool velocityMargin = false;   ///< render.velocity_margin: stretch the margin
                          ///< asymmetrically in the direction of rotation.
  float velocityMarginGain = 1.f;  ///< render.velocity_margin_gain: extra margin
                          ///< degrees per rad/s of angular velocity, 0..10.
  bool twoTier = false;   ///< render.two_tier: outer tier keeps meshes but
                          ///< skips draw submission (mesh vs. submit split).
  float twoTierOuterDeg = 10.f;  ///< render.two_tier_outer_deg: outer tier
                          ///< extends this far beyond the inner margin, 0..30.
  bool timingHandshake = true;   ///< sim.timing_handshake (default ON since
                          ///< 2026-10-14, JD's promotion): pull-tick
                          ///< phase scheduling for the circuit domains. Applies
                          ///< at world (re)load — registration is a cSimThread
                          ///< construction-time act, never live-swapped.
  int renderThreads = 0;        ///< Cortana 2026-10-15 1156: software raster threads (0 = auto: pow2 <= logical cores - 2, clamped 2..16 [JD 14:41, Cortana 1450]; N = override, powers of two 1..64)
  int handshakePhases = 4;       ///< sim.handshake_phases: phases per cycle, 1..16
                          ///< (the Signal cycle uses the first four slots).
  int floorPulse = 0;  ///< 2026-10-24 (JD): Graphics 'Floor pulse' 0 Off / 1 Slow / 2 Normal (render only, live; missing key = Off).
  bool groundPulse = true;  ///< render.ground_pulse: the Wafer's energized
                          ///< ground pulse (JD 2026-10-11). Live toggle, no
                          ///< remesh; default on.
  // 2026-10-16 (JD leaf pass): render.leaves -1 auto / 0 fast / 1 fancy. Auto = fast on
  // Chromebook-class machines (<= 4 logical cores), else fancy. Changing it remeshes.
  int leaves = -1;
  // 2026-10-18 (Crew Canopy, F-B): render.foliage_density 0 full (default; today's
  // meshing, bit-identical) / 1 sparse (far leaf cubes thinned by the mesher's
  // density model; fancy leaves only). Changing it remeshes.
  int foliageDensity = 0;
  // 2026-10-16 crystal twinkle: render.twinkle -1 auto / 0 off / 1 static / 2 on. Auto =
  // static when leaves resolve to fast (the Chromebook tier), else on. Live, no remesh.
  int twinkle = -1;
  // Cortana 2026-10-16: render.bloom -1 auto / 0 off / 1 on (software renderer post pass).
  // Auto = on when > 4 logical cores, off on the Chromebook tier. Live, no remesh.
  // Default OFF: the single-threaded pass costs ~11 ms/frame at 1080p (bench 2026-10-16).
  int bloom = 0;
  bool ao = true;       ///< Cortana 2026-10-16: render.ao - software corner AO (live)
  bool shimmer = true;  ///< Cortana 2026-10-16: render.shimmer - powered-Trace lithography shimmer (live)
  bool mipmaps = true;  ///< Cortana 2026-10-16: render.mipmaps - software distance texture LOD (BUG-0005, live)
  bool streaming = true;  ///< graphics.streaming (JD 2026-10-16): procgen columns stream in ahead of the player; boot-only
  /// JD Vulkan push (Cortana 2026-10-16): Vulkan effects, FrameParams::kVkFx* bits (1 normal maps,
  /// 2 per-pixel lighting, 4 bloom, 8 pulsing glow, 16 particles, 32 distance fog, 64 blended bulbs).
  /// Default all on; GL33 / software ignore them. Live, render-only.
  int vkFx = 127;
  float vkBloomStrength = 1.f;
  float vkHeartbeatGlow = 1.75f;
  bool vkFarLod = true;  ///< 0.0.1.54 [C54-1] (Proposed by JD, Built by Cortana): far LOD for the 33-64 ring (render-only, live)  ///< 0.0.1.53 [C53-3] (Proposed by JD, Built by Cortana): wafer heartbeat bloom gain (0..4)
  int streamRadius = 16;  ///< 2026-10-24 (JD): default 16. render / stream distance in chunks, 0 = auto (Vulkan 64, others 32), max 64; boot-only
  bool bulbShellSolid = false;  ///< 2026-10-16 1551 (JD): render.bulb_shell - Trace Bulb shell Ghostly (false, default) / Solid (true); render-only, saved
  bool emissiveLight = false;  ///< Cortana 2026-10-16: render.emissive_light - software local light from emitters (live). Default OFF: ~2x frame cost on the Trace-dense test world.
  int texRes = 0;       ///< Cortana 2026-10-16: render.tex_res 0 auto / 16 / 32 / 64 (software; next texture load or F9)
  bool bridging = true;   ///< game.bridging (Cortana 2026-10-15 2215, JD D9): walk-bridging placement
  bool groundTraces = true;  ///< render.ground_traces: the Wafer's ground
                          ///< trace lines + packets (JD 2026-10-11; landed
                          ///< end-to-end 2026-10-13, P1-RENDER H1). Live
                          ///< toggle, no remesh; default on, independent
                          ///< of groundPulse.
  // Cortana 2026-10-15 1610 (ENGINE): fpsCap -1 = auto (60 if the machine renders <=14 ms, else 30; the new default), 0 = off, N = cap.
  int swapImages = 3;      ///< 2026-10-24 (JD): render.swap_images 2 (double) / 3 (triple, default); Vulkan, boot-only
  int fpsCap = 30;         ///< 2026-10-24 (JD): default 30. render.fps_cap: frame-rate cap in FPS for the
  bool fpsCounter = false;  ///< Cortana 2026-10-15 1610; F56-4 (2026-10-17): perf readout on every backend (FPS, avg/p99 ms, update ms, GPU ms; off by default)
  bool roundSimArea = true;  ///< F56-7 (Proposed by JD, Built by Cortana): round sim area (disc r^2+r); boot-only; env LL_SIM_ROUND_AREA
  bool diamondSimArea = true;  ///< diamond (Manhattan) sim area (JD 2026-10-23); boot-only; env LL_SIM_AREA=diamond. DEFAULT since fix run 3 (JD t2262u)
  bool diamondStreamArea = true;  ///< diamond (Manhattan) stream footprint (JD 2026-10-23); boot-only; env LL_STREAM_AREA=diamond
                          ///< main loop's cFramePacer (D-M3; game-side wiring
                          ///< 2026-10-13). 0 = OFF (default: vsync-only
                          ///< pacing, JD perf doctrine 2026-10-12), else
                          ///< 1..1000. Live: the pacer is reconfigured the
                          ///< frame the settings value changes.

  bool modelDither = false;  // Cortana 2026-10-15 1215: JD: near-camera dither OFF by default (code kept; render.model_dither 1 re-enables)  ///< render.model_dither: near-camera screen-door
                            ///< fade (JD 2026-10-13). Live, no remesh;
                            ///< default on (an engine staple).
  float modelDitherNear = 2.f;  ///< render.model_dither_near: view depth
                                ///< (blocks) at/below which geometry is
                                ///< fully faded out.
  float modelDitherFar = 8.f;   ///< render.model_dither_far: view depth
                                ///< (blocks) at/above which geometry is
                                ///< rendered untouched.

  int texFilter = 0;   ///< PROVISIONAL (JD 2026-10-24) render.tex_filter: 0 sharp, 1 bilinear, 2 trilinear (GPU backends)
  int anisotropy = 8;  ///< PROVISIONAL (JD 2026-10-24) render.anisotropy: 1 (off), 2, 4, 8, 16; clamped to the device max
  std::string aaMode = "off";  ///< render.aa: software-renderer AA ladder
                               ///< ("off" | "fxaa" | "cmaa" | "ssaa2x"),
                               ///< JD 2026-10-13. Default off — JD reviews
                               ///< the capture sets and picks the default.
  bool asciiMode = false;      ///< render.ascii: ASCII output pipeline
                               ///< (experiment tier, JD 2026-10-13).
  int asciiCols = 160;         ///< render.ascii_cols: ASCII glyph columns,
                               ///< clamped 40..400.
  std::string asciiSet = "ascii";  ///< render.ascii_set: glyph set for the
                               ///< ASCII pipeline — "ascii" | "systerian"
                               ///< (the cSysterian renderer, JD 2026-10-13).
  int asciiValueSpan = 255;    ///< render.ascii_value_span: luminance ->
                               ///< level span (JD amendment 2026-10-13:
                               ///< full 0-255 range by default, a deliberate
                               ///< exception to the 64-scale conventions).

  /// Reads `path` (missing or broken -> defaults, never throws). Unknown keys are ignored;
  /// out-of-range values clamp to the nearest valid one.
  static GameSettings load(const std::string& path);
  /// Writes the settings as JSON, creating parent directories. False when unwritable.
  bool save(const std::string& path) const;
};

}  // namespace lithokraft
