// cMcpSensesAdapter: game-side MCP pilot-senses adapters (see the header for
// the threading model, the downgrade ladder, and the chunk-loading note).
#include "lithokraft/game/mcp_senses_adapter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>

#include "litholattice/core/math.h"
#include "litholattice/world/chunk.h"  // packPos
#include "litholattice/core/atomic_gate.h"
#include "lithokraft/game/McpDisplayVision.h"  // SCRATCH vulkfb
#include "lithokraft/game/NemoViewHub.h"
#include "litholattice/render/software_simd.h"  // g_depthRemapMode (Nemo View depth bands)

namespace lithokraft {
namespace {

using litholattice::mcp::AudioChunk;
using litholattice::mcp::CameraPose;
using litholattice::mcp::VisionFrame;

constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
constexpr uint32_t kSkyClear = 0xFF94B3DB;  // wafer day sky, from Game::frameParams (0xAARRGGBB)
constexpr int kMinDim = 16, kMaxDim = 512;
constexpr float kNear = 0.05f, kFar = 1000.0f;  // same clip range as the game camera
constexpr float kTapRate = 48000.0f;            // cAudioEngine renders at 48 kHz
// Largest single drain: tier 0 needs 1024 * (48000/8000) tap frames.
constexpr size_t kTapScratchFrames =
    litholattice::mcp::kAudioChunkFrames * size_t(kTapRate / litholattice::mcp::kAudioTierRates[0]);

litholattice::Mat4 translation(float x, float y, float z) {
  litholattice::Mat4 t = litholattice::Mat4::identity();
  t.m[12] = x;
  t.m[13] = y;
  t.m[14] = z;
  return t;
}

}  // namespace

cMcpSensesAdapter::cMcpSensesAdapter(litholattice::cAudioEngine& audio,
                                   std::function<cMcpPlayerView::GameState()> snapshot,
                                   std::function<uint64_t()> tick,
                                   std::shared_ptr<cPilotCameraRig> rig)
    : audio_(audio), snapshot_(std::move(snapshot)), tick_(std::move(tick)), rig_(std::move(rig)) {
  tapScratch_.resize(kTapScratchFrames);
  audio_.setTapEnabled(true);  // arm the mixed-output tap once; the tap hears the pilot's ears
}

cMcpSensesAdapter::~cMcpSensesAdapter() { audio_.setTapEnabled(false); }

void cMcpSensesAdapter::noteMesh(const litholattice::ChunkMesh& m) {
  const int64_t k = litholattice::packPos(m.cx, m.sy, m.cz);
  litholattice::cGateGuard<litholattice::cAtomicGate> lk(mutex_);
  if (m.verts.empty()) {
    meshes_.erase(k);  // mesh removed
    return;
  }
  auto cm = std::make_shared<CachedMesh>();
  cm->cx = m.cx;
  cm->sy = m.sy;
  cm->cz = m.cz;
  // Winding: none needed. The mesher winds quads CCW seen from outside (what
  // GL's glFrontFace(GL_CCW) keeps); the 2026-10-11 isBackface fix made the
  // software renderer cull the same faces, so the old v1<->v3 reversal
  // workaround was removed (it would now double-invert and hide every front
  // face from the agent). Verbatim copy.
  cm->verts = m.verts;  // no reversal: isBackface now matches GL's CCW convention
  meshes_[k] = std::move(cm);
}

void cMcpSensesAdapter::clearMeshes() {
  litholattice::cGateGuard<litholattice::cAtomicGate> lk(mutex_);
  meshes_.clear();
  spriteVerts_.clear();
}

void cMcpSensesAdapter::setSpriteVerts(std::vector<litholattice::PackedVertex> verts,
                                       litholattice::Vec3 origin) {
  litholattice::cGateGuard<litholattice::cAtomicGate> lk(mutex_);
  spriteVerts_ = std::move(verts);
  spriteOrigin_ = origin;
}

void cMcpSensesAdapter::syncCvars(const CvarState& s) {
  litholattice::cGateGuard<litholattice::cAtomicGate> lk(mutex_);
  cvars_ = s;
}

cMcpSensesAdapter::CvarState cMcpSensesAdapter::cvars() const {
  litholattice::cGateGuard<litholattice::cAtomicGate> lk(mutex_);
  return cvars_;
}

std::vector<std::string> cMcpSensesAdapter::pilotIds() const { return {"player"}; }

// SCRATCH vulkfb (2026-11-11): the player's real on-screen frame from the active display renderer.
// Off unless LL_MCP_DISPLAY_VISION=1. Waits (MCP thread) for a frame drawn after this request.
bool cMcpSensesAdapter::captureDisplayFrame(std::string_view pilot, int width, int height, VisionFrame& out) {
  if (!cMcpDisplayVision::enabled() || pilot != "player") return false;
  cMcpDisplayVision& dv = cMcpDisplayVision::instance();
  const uint64_t seq = dv.request();
  cMcpDisplayVision::Frame f;
  if (!dv.waitFrame(seq, 4000, f) || f.width <= 0 || f.height <= 0) return false;
  const int w = std::clamp(width > 0 ? width : std::min(f.width, 640), kMinDim, 1280);
  const int h = std::clamp(height > 0 ? height : std::max(kMinDim, f.height * w / f.width), kMinDim, 720);
  std::vector<uint8_t> rgb(size_t(w) * size_t(h) * 3);
  for (int y = 0; y < h; ++y) {  // box filter (integer source spans)
    const int y0 = y * f.height / h, y1 = std::max(y0 + 1, (y + 1) * f.height / h);
    for (int x = 0; x < w; ++x) {
      const int x0 = x * f.width / w, x1 = std::max(x0 + 1, (x + 1) * f.width / w);
      uint64_t acc[3] = {0, 0, 0}, n = 0;
      for (int yy = y0; yy < y1; ++yy)
        for (int xx = x0; xx < x1; ++xx) {
          const uint8_t* q = &f.rgb[(size_t(yy) * size_t(f.width) + size_t(xx)) * 3];
          acc[0] += q[0]; acc[1] += q[1]; acc[2] += q[2]; ++n;
        }
      uint8_t* d = &rgb[(size_t(y) * size_t(w) + size_t(x)) * 3];
      for (int c = 0; c < 3; ++c) d[c] = uint8_t((acc[c] + n / 2) / n);
    }
  }
  out.width = w;
  out.height = h;
  if (!litholattice::mcp::encodePngRgb(w, h, rgb.data(), out.png)) return false;
  out.tick = f.tick;
  out.source = f.renderer;
  out.srcWidth = f.width;
  out.srcHeight = f.height;
  CameraViewPose res;
  if (rig_ && rig_->resolvedPose(pilot, res)) {
    out.camX = res.position.x; out.camY = res.position.y; out.camZ = res.position.z;
    out.yaw = float(res.yaw * kRadToDeg); out.pitch = float(res.pitch * kRadToDeg);
  }
  return true;
}

cMcpSensesAdapter::Snapshot cMcpSensesAdapter::takeSnapshot() const {
  // Snapshot the shared state under one lock: cvars and a shallow
  // (shared_ptr) copy of the mesh cache. The entries are immutable, so the
  // render sees a consistent world without blocking the game thread.
  Snapshot s;
  litholattice::cGateGuard<litholattice::cAtomicGate> lk(mutex_);
  s.cv = cvars_;
  s.meshes = meshes_;
  s.spriteVerts = spriteVerts_;
  s.spriteOrigin = spriteOrigin_;
  s.objectTable = objectTable_;
  s.textureColors = textureColors_;
  return s;
}

void cMcpSensesAdapter::setObjectTable(std::shared_ptr<const std::vector<uint8_t>> table) {
  litholattice::cGateGuard<litholattice::cAtomicGate> lk(mutex_);
  objectTable_ = std::move(table);
}

void cMcpSensesAdapter::setTextureColors(std::shared_ptr<const std::vector<uint32_t>> colors) {
  litholattice::cGateGuard<litholattice::cAtomicGate> lk(mutex_);
  textureColors_ = std::move(colors);
}

void cMcpSensesAdapter::renderCone(litholattice::cSoftwareRenderer& sw, litholattice::CullProtoState& st,
                                   const CameraViewPose& res, int w, int h, const Snapshot& snap,
                                   bool spritesAsEntities) const {
  const CvarState& cv = snap.cv;
  const double ex = res.position.x, ey = res.position.y, ez = res.position.z;
  const float fovDeg = float(res.fovRad * kRadToDeg);
  const float yawR = res.yaw, pitchR = res.pitch;
  // The engine camera's forward (cPlayer::forward convention, radians).
  const litholattice::Vec3 eye{float(ex), float(ey), float(ez)};
  const litholattice::Vec3 fwd = res.forward;
  const litholattice::Mat4 vp =
      litholattice::mul(litholattice::perspective(float(std::clamp(fovDeg, 10.0f, 120.0f) * kDegToRad),
                                                 float(w) / float(h), kNear, kFar),
                        litholattice::lookDir(eye, fwd));
  sw.setPerPixelShading(false);  // flat = the downgraded path
  sw.resize(uint32_t(w), uint32_t(h));
  sw.clear(kSkyClear);  // beyond loaded chunks: sky, not a crash
  // Culling-cache prototypes (JD 2026-10-11): pass 1 classifies every mesh
  // under the prototype pipeline (per-camera state owned by the caller's
  // thread); pass 2 renders the survivors. Base FOV margin is the
  // foundation; the prototypes only add keep-behavior.
  const double nowSec = std::chrono::duration<double>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
  std::vector<std::shared_ptr<const CachedMesh>> drawList;
  drawList.reserve(snap.meshes.size());
  {
    litholattice::CullProtoConfig cfg;
    cfg.cometTailFrames = cv.cometTailFrames;
    cfg.velocityMargin = cv.velocityMargin;
    cfg.velocityMarginGain = cv.velocityMarginGain;
    cfg.twoTier = cv.twoTier;
    cfg.twoTierOuterDeg = cv.twoTierOuterDeg;
    st.configure(cfg);  // no-op when unchanged (history/velocity survive)
    st.beginFrame(vp, yawR, pitchR, nowSec);
    for (const auto& [k, cm] : snap.meshes) {
      // Sub-chunk world origin (16^3), the same offset the GL backend
      // uploads as its per-mesh uniform; baked into the MVP here.
      const litholattice::Vec3 mn{float(cm->cx * 16), float(cm->sy * 16), float(cm->cz * 16)};
      if (st.decide(vp, mn, {mn.x + 16.0f, mn.y + 16.0f, mn.z + 16.0f}, eye,
                    cv.frustumMarginDeg) == litholattice::CullDecision::Full)
        drawList.push_back(cm);
    }
  }
  for (const auto& cm : drawList) {
    const litholattice::Vec3 mn{float(cm->cx * 16), float(cm->sy * 16), float(cm->cz * 16)};
    const litholattice::Mat4 mvp = litholattice::mul(vp, translation(mn.x, mn.y, mn.z));
    sw.renderMesh(cm->verts.data(), cm->verts.size(), mvp.m);
  }
  // Sprites (flat form fed by Game's cSpriteHost): drawn after the world,
  // world-anchored via the same translation trick with their own origin.
  if (!snap.spriteVerts.empty()) {
    const litholattice::Mat4 mvp =
        litholattice::mul(vp, translation(snap.spriteOrigin.x, snap.spriteOrigin.y, snap.spriteOrigin.z));
    if (spritesAsEntities) sw.setObjectOverride(int(litholattice::nemoview::ObjectId::Entity));
    sw.renderMesh(snap.spriteVerts.data(), snap.spriteVerts.size(), mvp.m);
    if (spritesAsEntities) sw.setObjectOverride(-1);
  }
}

bool cMcpSensesAdapter::captureVisionFrame(std::string_view pilot, int width, int height,
                                          VisionFrame& out) {
  // The pilot's engine camera (cPilotCameraRig) resolves the capture pose on
  // the game thread; an unknown pilot — or one whose camera has not
  // resolved yet (no frame ticked) — fails, as an unknown pilot did before.
  CameraViewPose res;
  if (!rig_ || !rig_->resolvedPose(pilot, res)) return false;
  const Snapshot snap = takeSnapshot();
  const int w = std::clamp(width > 0 ? width : snap.cv.visionWidth, kMinDim, kMaxDim);
  const int h = std::clamp(height > 0 ? height : snap.cv.visionHeight, kMinDim, kMaxDim);
  {
    owner_.check();  // mutexfree 2b: single MCP owner thread (was renderMutex_)
    // mutexfree 2b: protoMutex_ removed (same owner thread as the render pass)
    renderCone(sw_, protoStates_[std::string(pilot)], res, w, h, snap, false);
    // Framebuffer pixels are 0xAARRGGBB; PNG wants RGB bytes.
    const auto& fb = sw_.framebuffer();
    std::vector<uint8_t> rgb(size_t(w) * size_t(h) * 3);
    for (size_t i = 0, n = size_t(w) * size_t(h); i < n; ++i) {
      const uint32_t p = fb.pixels[i];
      rgb[3 * i] = uint8_t((p >> 16) & 0xFF);
      rgb[3 * i + 1] = uint8_t((p >> 8) & 0xFF);
      rgb[3 * i + 2] = uint8_t(p & 0xFF);
    }
    out.width = w;
    out.height = h;
    if (!litholattice::mcp::encodePngRgb(w, h, rgb.data(), out.png)) return false;
  }

  out.tick = tick_ ? tick_() : 0;
  out.camX = res.position.x;
  out.camY = res.position.y;
  out.camZ = res.position.z;
  out.yaw = float(res.yaw * kRadToDeg);
  out.pitch = float(res.pitch * kRadToDeg);
  return true;
}

// Nemo View. Capture size comes from the renderer Grid (not a 4x4 hard-wire): 80x45 and
// 160x90 share a 320x180 source (4x4 vs 2x2 cells); 96x54 is 384x216; 180x80 is 360x160;
// 240x135 is 480x270. The fovea is still the centre 128x48 of the 80-col 320x180 view
// (32x12), so the prompt token budget is unchanged. The object pass is on only for this render.
bool cMcpSensesAdapter::buildAscii(litholattice::cSoftwareRenderer& sw, litholattice::CullProtoState& cull,
                                   const CameraViewPose& res, const litholattice::mcp::AsciiRequest& req,
                                   litholattice::nemoview::AsciiFrame& out) const {
  namespace nv = litholattice::nemoview;
  const Snapshot snap = takeSnapshot();
  nv::Grid grid = nv::kHashGrid;
  if (req.fovea) {
    grid = nv::kHashGrid;  // fovea is always cropped from the 80x45 320x180 capture
  } else if (!nv::parseGrid(std::to_string(req.cols), grid)) {
    return false;
  }
  const int w = grid.captureW;
  const int h = grid.captureH;
  const bool objects = req.objects && snap.objectTable && !snap.objectTable->empty();
  if (objects) sw.setObjectCapture(snap.objectTable->data(), snap.objectTable->size());
  renderCone(sw, cull, res, w, h, snap, objects);
  nv::SourceView src;
  src.width = w;
  src.height = h;
  src.pixels = sw.framebuffer().pixels.data();
  src.depth = sw.depthBuffer().data();
  src.objects = objects ? sw.objectBuffer().data() : nullptr;
  src.faces = (objects && sw.faceBuffer().size() == sw.objectBuffer().size()) ? sw.faceBuffer().data()
                                                                               : nullptr;
  if (objects && snap.textureColors && !snap.textureColors->empty() &&
      sw.layerBuffer().size() == sw.objectBuffer().size()) {
    src.layers = sw.layerBuffer().data();
    src.layerColors = snap.textureColors->data();
    src.layerColorCount = int(snap.textureColors->size());
  }
  src.depthRemapMode = litholattice::swsimd::g_depthRemapMode;
  nv::FrameConfig cfg =
      nv::FrameConfig::forGrid(req.fovea ? nv::kFoveaGrid : grid,
                              req.objects ? nv::ViewMode::Objects : nv::ViewMode::PixelsAndDepth);
  if (req.fovea) {
    src.cropW = nv::kFoveaGrid.captureW;
    src.cropH = nv::kFoveaGrid.captureH;
    src.cropX = (w - src.cropW) / 2;
    src.cropY = (h - src.cropH) / 2;
  }
  const bool ok = nv::build(src, cfg, out);
  if (objects) sw.setObjectCapture(nullptr, 0);
  out.tick = tick_ ? tick_() : 0;
  return ok;
}

bool cMcpSensesAdapter::captureAsciiFrame(std::string_view pilot, const litholattice::mcp::AsciiRequest& req,
                                          litholattice::mcp::AsciiCapture& out) {
  namespace nv = litholattice::nemoview;
  CameraViewPose res;
  if (!rig_ || !rig_->resolvedPose(pilot, res)) return false;
  nv::AsciiFrame f;
  {
    owner_.check();  // the MCP owner thread owns sw_ and protoStates_
    if (!buildAscii(sw_, protoStates_[std::string(pilot)], res, req, f)) return false;
  }
  out.cols = f.cols;
  out.rows = f.rows;
  out.tick = f.tick;
  out.mode = nv::viewModeName(f.mode);
  out.lutSha = nv::depthTableShaPrefix();
  auto wanted = [&](const char* name) {
    const std::string& ch = req.channels;
    if (ch.empty()) return std::string(name) == "glyph";
    if (ch == "all") return true;
    std::string wrapped = ",";
    for (char c : ch) {
      if (c != ' ') wrapped += c;
    }
    wrapped += ",";
    auto has = [&](const char* n) { return wrapped.find(std::string(",") + n + ",") != std::string::npos; };
    if (std::string(name) == "class") return has("class") || has("cls") || has("object");
    if (std::string(name) == "qword") return has("qword") || has("cell");
    if (std::string(name) == "fg") return has("fg") || has("ink");
    return has(name);
  };
  if (wanted("glyph")) out.glyph = nv::glyphRows(f);
  if (wanted("fg")) out.fg = nv::fgRows(f);
  if (wanted("depth")) out.depth = nv::bandRows(f);
  if (wanted("class")) out.cls = nv::objectRows(f);
  if (wanted("tex8")) out.tex8 = nv::tex8Rows(f);
  if (wanted("tex16")) out.tex16 = nv::tex16Rows(f);
  if (wanted("qword")) out.qword = nv::qwordRows(f);
  out.sha256 = nv::shaHex(f);
  out.yaw = float(res.yaw * kRadToDeg);
  out.pitch = float(res.pitch * kRadToDeg);
  cNemoViewHub::instance().publish(f, "Nemo's look");
  return true;
}

bool cMcpSensesAdapter::previewAsciiFrame(litholattice::cSoftwareRenderer& sw, litholattice::CullProtoState& cull,
                                          const litholattice::mcp::AsciiRequest& req,
                                          litholattice::nemoview::AsciiFrame& out, float* yawDeg, float* pitchDeg) {
  CameraViewPose res;
  if (!rig_ || !rig_->resolvedPose("player", res)) return false;
  if (yawDeg) *yawDeg = float(res.yaw * kRadToDeg);
  if (pitchDeg) *pitchDeg = float(res.pitch * kRadToDeg);
  return buildAscii(sw, cull, res, req, out);
}

bool cMcpSensesAdapter::setCameraPose(std::string_view pilot, const CameraPose& pose) {
  if (pose.pitch < -90.0f || pose.pitch > 90.0f) return false;
  if (pose.fov < 10.0f || pose.fov > 120.0f) return false;
  // Fix 2026-10-11 (regression): unbounded coords reach the software rasterizer
  // as inf/NaN (1e308 parses; float() overflows to inf; NaN poisons lookDir /
  // perspective and int(floor(NaN)) is UB). Reject non-finite and absurd
  // magnitudes before they can get there.
  if (!std::isfinite(pose.x) || !std::isfinite(pose.y) || !std::isfinite(pose.z)) return false;
  constexpr double kMaxCameraCoord = 1e6;
  if (std::fabs(pose.x) > kMaxCameraCoord || std::fabs(pose.y) > kMaxCameraCoord ||
      std::fabs(pose.z) > kMaxCameraCoord)
    return false;
  return rig_ && rig_->setFreePose(pilot, pose);  // false for an unknown pilot
}

bool cMcpSensesAdapter::trackPlayer(std::string_view pilot) {
  return rig_ && rig_->trackPlayer(pilot);
}

bool cMcpSensesAdapter::cameraPose(std::string_view pilot, CameraPose& out) const {
  return rig_ && rig_->mirrorPose(pilot, out);
}

bool cMcpSensesAdapter::captureAudioChunk(std::string_view pilot, int tier, AudioChunk& out) {
  if (!rig_ || !rig_->hasPilot(pilot)) return false;  // unknown pilot
  const int t = std::clamp(tier, 0, litholattice::mcp::kAudioTierCount - 1);
  const int rate = litholattice::mcp::kAudioTierRates[t];
  const size_t step = size_t(kTapRate) / size_t(rate);  // 6, 3, 2, 1
  const size_t need = litholattice::mcp::kAudioChunkFrames * step;

  // drainTap() is the tap's single reader; serialize captures.
  owner_.check();  // mutexfree 2b: single MCP owner thread (was audioMutex_)
  const size_t got = audio_.drainTap(tapScratch_.data(), need);
  if (got < need) return false;  // tap has no data yet (or underran)

  // Box-downsample the 48 kHz tap to the tier rate; clamp float -> int16.
  out.sampleRate = rate;
  out.samples.resize(litholattice::mcp::kAudioChunkFrames);
  for (size_t i = 0; i < out.samples.size(); ++i) {
    float sum = 0.0f;
    for (size_t s = 0; s < step; ++s) sum += tapScratch_[i * step + s];
    const float c = std::clamp(sum / float(step), -1.0f, 1.0f);
    out.samples[i] = int16_t(std::lround(c * 32767.0f));
  }
  out.tick = tick_ ? tick_() : 0;
  return true;
}

}  // namespace lithokraft
