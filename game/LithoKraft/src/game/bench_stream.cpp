#include "lithokraft/game/bench_stream.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <thread>
#include "lithokraft/content/appearance.h"
#include "lithokraft/content/blocks.h"
#include "litholattice/core/log.h"
#include "litholattice/core/time.h"
#include "litholattice/world/mesh_input.h"

namespace lithokraft {
// 0.0.1.53 [C53-1] (Proposed by JD, Built by Cortana): phase timing of cBenchStreamer::update (logged with ringstats).
namespace { double g_ph[5]{}; int g_phFrames = 0; }

// 0.0.1.54 [C54-1] (Proposed by JD, Built by Cortana): far LOD helpers.
namespace {
// 0.0.1.54 [A54-3d] (Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; Built by Cortana): level 0 = full mesh with the foliage kShadeLodNear quads
// (column within 2 chunks: chunk.vert may draw them, it switches at 32 blocks from the sub-chunk
// centre); level 1 = full mesh WITHOUT near quads (3..32 chunks: centre >= 40 blocks away, so
// chunk.vert never draws them); 2 = 2x2x2 (33..48); 3 = 4x4x4 (49+).
constexpr int kLodEdge[3] = {2, 32, 48}, kLodHyst[3] = {1, 2, 2};
int lodBase(int d) { return d <= kLodEdge[0] ? 0 : d <= kLodEdge[1] ? 1 : d <= kLodEdge[2] ? 2 : 3; }
int lodWithHysteresis(int d, int cur) {
  const int base = lodBase(d);
  if (base > cur && cur < 3 && d <= kLodEdge[cur] + kLodHyst[cur]) return cur;  // not far enough past the edge yet
  return base;
}
bool stripNearOn() { static const bool on = [] { const char* e = std::getenv("LL_VK_STRIP_NEAR"); return !(e && e[0] == '0'); }(); return on; }
// Downsampled snapshot: coarse cell (a,b,c) of size f sits at interior cell (a,b,c); the mesher
// meshes the (16/f)^3 corner and the result is scaled by f. Representative block: a Custom/shell
// block (Trace, bulbs: keeps the glow readable at distance) if present, else the most frequent
// non-air block, when >= 1/4 of the cell is non-air. Negative-side padding is solid only if the
// whole fine pad patch is opaque (conservative: never a hole); the positive side stays air, so
// boundary faces are always emitted (hidden by the neighbour, never a hole).
std::shared_ptr<MeshInput> lodInput(const MeshInput& in, int f) {
  auto out = std::make_shared<MeshInput>(in);
  std::fill(std::begin(out->id), std::end(out->id), kAir);
  std::fill(std::begin(out->st), std::end(out->st), CellState{});
  std::fill(std::begin(out->mat), std::end(out->mat), uint8_t(0));
  const cBlockReg& reg = blockRegistry();
  const int n = kChunkSize / f;
  for (int b = 0; b < n; ++b)
    for (int c = 0; c < n; ++c)
      for (int a = 0; a < n; ++a) {
        BlockId ids[64]; int cnt[64]; int k = 0, nonAir = 0, feat = -1, featY = -1;
        int src[64];
        for (int dy = 0; dy < f; ++dy)
          for (int dz = 0; dz < f; ++dz)
            for (int dx = 0; dx < f; ++dx) {
              const int i = meshPadIndex(a * f + dx + 1, b * f + dy + 1, c * f + dz + 1);
              const BlockId id = in.id[i];
              if (id == kAir) continue;
              ++nonAir;
              const auto& def = reg.def(id);
              if ((def.shape == Shape::Custom || (def.flags & kShell)) && dy >= featY) { feat = i; featY = dy; }
              int j = 0;
              while (j < k && ids[j] != id) ++j;
              if (j == k) { ids[k] = id; cnt[k] = 0; src[k] = i; ++k; }
              ++cnt[j];
            }
        int pick = -1;
        if (feat >= 0) pick = feat;
        else if (nonAir * 4 >= f * f * f) {
          int best = 0;
          for (int j = 1; j < k; ++j) if (cnt[j] > cnt[best]) best = j;
          pick = src[best];
        }
        if (pick < 0) continue;
        const int o = meshPadIndex(a + 1, b + 1, c + 1);
        out->id[o] = in.id[pick]; out->st[o] = in.st[pick]; out->mat[o] = in.mat[pick];
      }
  // negative-side padding (x = -1, y = -1, z = -1 faces of the coarse grid)
  for (int axis = 0; axis < 3; ++axis)
    for (int p = 0; p < n; ++p)
      for (int q = 0; q < n; ++q) {
        int first = -1; bool all = true;
        for (int dp = 0; dp < f && all; ++dp)
          for (int dq = 0; dq < f && all; ++dq) {
            const int u = p * f + dp + 1, v = q * f + dq + 1;
            const int i = axis == 0 ? meshPadIndex(0, u, v) : axis == 1 ? meshPadIndex(u, 0, v) : meshPadIndex(u, v, 0);
            if (!reg.has(in.id[i], kOpaque)) all = false;
            else if (first < 0) first = i;
          }
        if (!all || first < 0) continue;
        const int o = axis == 0 ? meshPadIndex(0, p + 1, q + 1) : axis == 1 ? meshPadIndex(p + 1, 0, q + 1) : meshPadIndex(p + 1, q + 1, 0);
        out->id[o] = in.id[first]; out->st[o] = in.st[first]; out->mat[o] = in.mat[first];
      }
  return out;
}
}  // namespace

double MsSamples::avg(const std::vector<double>& v) {
  double s = 0; for (double x : v) s += x;
  return v.empty() ? 0.0 : s / double(v.size());
}
double MsSamples::pct(std::vector<double> v, double q) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  return v[std::min(v.size() - 1, size_t(q * double(v.size() - 1) + 0.5))];
}

cBenchStreamer::cBenchStreamer(IRenderer& renderer, cJobPool& pool, std::shared_ptr<const cColumnGen> gen, int worldChunks,
                             BlockId belowWorld, int radiusChunks)
    : renderer_(renderer), pool_(pool), gen_(std::move(gen)), world_(worldChunks), radius_(radiusChunks), below_(belowWorld) {}

cBenchStreamer::~cBenchStreamer() {
  while (inFlight_->load() > 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  {  // [C53-2] give back prepared-but-unpublished slices
    std::vector<MeshOut> rest;
    meshDone_->drainInto(rest);
    for (MeshOut& m : rest) pendingUploads_.push_back(std::move(m));
    for (const MeshOut& m : pendingUploads_) if (m.prep.ready) renderer_.discardChunkMesh(m.prep);
    pendingUploads_.clear();
  }
  for (const ChunkMesh& t : tombstoneRetry_) renderer_.uploadChunkMesh(t);  // Stage 2: parked tombstones land directly at teardown
  tombstoneRetry_.clear();
  for (const auto& [key, subs] : meshedSubs_)
    for (int sy : subs) {
      ChunkMesh empty; empty.cx = int(int32_t(key >> 32)); empty.cz = int(int32_t(key & 0xffffffffu)); empty.sy = sy;
      renderer_.uploadChunkMesh(empty);
    }
}

cBenchStreamer::Second cBenchStreamer::takeSecond() {
  // JD 2026-10-16: per-ring cost (Chebyshev chunk distance from the camera) for the LOD decision.
  {
    constexpr int kEdge[4] = {-1, 16, 32, 64};
    size_t verts[4] = {}, cols[4] = {}; long long draws[4] = {};
    for (const auto& [key, c] : colStats_) {
      const int d = std::max(std::abs(c.cx - lastPcx_), std::abs(c.cz - lastPcz_));
      int r = d <= 16 ? 0 : d <= 32 ? 1 : 2;
      if (d > 64) r = 3;
      verts[r] += c.verts; draws[r] += c.draws; ++cols[r];
    }
    (void)kEdge;
    const char* names[4] = {"0-16", "17-32", "33-64", ">64"};
    for (int r = 0; r < 4; ++r)
      litholattice::logInfo("ringstats ring=%s cols=%zu draws=%lld verts=%zu tris~%zu meshMB=%.1f", names[r], cols[r], draws[r],
                            verts[r], verts[r] / 2, double(verts[r] * sizeof(PackedVertex)) / (1024.0 * 1024.0));
  }
  if (g_phFrames > 0) {  // [C53-1]
    const double f = double(g_phFrames);
    litholattice::logInfo("stream-split ms/frame: request=%.2f gen+snapshot=%.2f upload=%.2f drop=%.2f (frames %d)",
                          g_ph[0] / f, g_ph[1] / f, g_ph[2] / f, g_ph[3] / f, g_phFrames);
    for (double& v : g_ph) v = 0;
    g_phFrames = 0;
  }
  Second s = sec_;
  s.loaded = int(map_.columns().size());
  sec_ = {};
  genMs.second.clear(); meshMs.second.clear();
  return s;
}

// 0.0.1.54 [C54-1] (Proposed by JD, Built by Cortana): one mesh job (snapshot under the shared lock, optional
// LOD downsample + scale, worker-direct GPU write).
void cBenchStreamer::submitMesh(int cx, int sy, int cz, int lod, uint32_t gen, const std::shared_ptr<const BlockMaterials>& materials) {
  ++*inFlight_;
  pool_.submit([map = &map_, mu = mapMu_, below = below_, rend = &renderer_, materials, out = meshDone_, n = inFlight_, cx, sy, cz, lod, gen] {
    const auto t0 = Clock::now();
    std::shared_ptr<MeshInput> in;
    {
      std::shared_lock lock(*mu);
      if (map->column(cx, cz)) in = snapshotForMeshing(*map, blockRegistry(), cx, sy, cz, 0, 0, below);
    }
    MeshOut o;
    o.lod = lod; o.gen = gen;
    if (!in) {
      o.mesh.cx = cx; o.mesh.sy = sy; o.mesh.cz = cz; o.skipped = true;
    } else {
      const int f = lod <= 1 ? 1 : lod == 2 ? 2 : 4;
      if (f > 1) in = lodInput(*in, f);
      o.mesh = meshSubChunk(*in, MeshContext{blockRegistry(), *materials, &blockAppearance()});
      if (f > 1)
        for (PackedVertex& v : o.mesh.verts) { v.x = uint16_t(v.x * f); v.y = uint16_t(v.y * f); v.z = uint16_t(v.z * f); }
      if (lod >= 1 && stripNearOn()) {  // [A54-3d] near-LOD quads are never drawn this far out
        auto& vs = o.mesh.verts;
        size_t w = 0;
        for (size_t q = 0; q + 3 < vs.size(); q += 4)
          if (!(vs[q].mode & kShadeLodNear)) { if (w != q) for (int k = 0; k < 4; ++k) vs[w + size_t(k)] = vs[q + size_t(k)]; w += 4; }
        vs.resize(w);
      }
      o.verts = o.mesh.verts.size();
      if (rend->prepareChunkMeshAsync(o.mesh, o.prep)) std::vector<PackedVertex>().swap(o.mesh.verts);
    }
    o.ms = msBetween(t0, Clock::now());
    out->push(std::move(o));
    --*n;
  });
}

size_t cBenchStreamer::update(float x, float z, const std::shared_ptr<const BlockMaterials>& materials) {
  // 2026-10-18 (Crew Canopy, F-B): the sparse-leaf distance origin is this streamer's camera.
  litholattice::setFoliageFocus(int(std::floor(x / 16.f)), int(std::floor(z / 16.f)));
  // Design B Stage 2: retry tombstones the upload ring refused last frame
  // before anything new is staged (order: they precede today's uploads).
  if (!tombstoneRetry_.empty()) {
    std::vector<ChunkMesh> keep;
    for (ChunkMesh& t : tombstoneRetry_)
      if (!renderer_.enqueueChunkMesh(std::move(t))) keep.push_back(std::move(t));
    tombstoneRetry_.swap(keep);
  }
  if (lodEnv_ == -1) { const char* e = std::getenv("LL_VK_FAR_LOD"); lodEnv_ = e ? (e[0] != '0' ? 1 : 0) : -2; }  // [C54-1]
  ++frame_;
  const int pcx = int(std::floor(x / 16.f)), pcz = int(std::floor(z / 16.f));
  lastPcx_ = pcx; lastPcz_ = pcz;
  auto tPh = Clock::now();  // [C53-1]
  const auto lap = [&](int i) { const auto t = Clock::now(); g_ph[i] += msBetween(tPh, t); tPh = t; };
  ++g_phFrames;
  // 1) request missing columns in the radius (nearest first is not needed: the camera moves +x).
  constexpr int kMaxInFlight = 64;
  for (int dz = -radius_; dz <= radius_; ++dz)
    for (int dx = -radius_; dx <= radius_; ++dx) {
      if (inFlight_->load() >= kMaxInFlight) break;
      const int cx = pcx + dx, cz = pcz + dz;
      if (inFixedWorld(cx, cz)) continue;
      const uint64_t key = ChunkKey::make(cx, cz);
      if (!requested_.insert(key).second) continue;
      ++*inFlight_;
      pool_.submit([gen = gen_, out = genDone_, n = inFlight_, cx, cz] {
        const auto t0 = Clock::now();
        auto col = gen->generate(cx, cz);
        out->push({std::move(col), msBetween(t0, Clock::now())});
        --*n;
      });
    }
  lap(0);  // [C53-1]
  // 2) generated columns -> map + mesh jobs
  std::vector<GenOut> gens;
  genDone_->drainInto(gens);
  for (GenOut& g : gens) {
    if (!g.col) continue;
    genMs.add(g.ms); ++sec_.generated; ++totalGenerated;
    const int cx = g.col->cx, cz = g.col->cz;
    {
      std::unique_lock lock(*mapMu_);  // [C53-2] exclusive only for the map change
      map_.insert(std::move(g.col));
    }
    const ChunkColumn* col = map_.column(cx, cz);
    for (int sy = 0; sy < kSubChunks; ++sy) {
      if (!col->subs[size_t(sy)]) continue;
      // [C54-1] LOD level at submit time (fresh columns: no hysteresis yet)
      const int d = std::max(std::abs(cx - pcx), std::abs(cz - pcz));
      ColLod& cl = lod_[ChunkKey::make(cx, cz)];
      if (sy == 0 || cl.gen == 0) { cl.lod = (farLodActive() ? lodBase(d) : 0); cl.gen = ++lodGen_; }
      submitMesh(cx, sy, cz, cl.lod, cl.gen, materials);
    }
  }
  // [C54-1b] LOD transitions (every 2nd frame, at most 48 columns per scan): remesh the
  // column's sub-chunks at the new level; the old mesh stays drawn until the new one lands.
  if ((frame_ & 1) == 0) {
    const bool on = farLodActive();
    int budget = 48;
    for (auto& [key, cl] : lod_) {
      if (budget <= 0) break;
      const auto cit = map_.columns().find(key);
      if (cit == map_.columns().end()) continue;
      const ChunkColumn* col = cit->second.get();
      const int d = std::max(std::abs(col->cx - pcx), std::abs(col->cz - pcz));
      const int want = on ? lodWithHysteresis(d, cl.lod) : 0;
      if (want == cl.lod) continue;
      cl.lod = want; cl.gen = ++lodGen_; --budget; ++lodRemeshed;
      for (int sy = 0; sy < kSubChunks; ++sy)
        if (col->subs[size_t(sy)]) submitMesh(col->cx, sy, col->cz, cl.lod, cl.gen, materials);
    }
  }
  lap(1);  // [C53-1]
  // 3) upload finished meshes (columns dropped meanwhile are skipped)
  size_t bytes = 0;
  std::vector<MeshOut> meshes;
  meshDone_->drainInto(meshes);
  // 0.0.1.51 (Cortana): upload at most ~6 MB per frame (the Vulkan staging ring is 8 MB; a
  // bigger burst forced a synchronous copy flush = a main-thread stall). The rest waits.
  for (MeshOut& m : meshes) pendingUploads_.push_back(std::move(m));
  constexpr size_t kUploadBudget = size_t(6) << 20;
  // 0.0.1.53 [C53-2] (Proposed by JD, Built by Cortana): worker-written meshes cost the main thread only a
  // publish; they have their own per-frame publish budget (count), the 6 MB budget is for
  // meshes that still go through the staging ring.
  constexpr int kPublishBudget = 512;
  size_t staged = 0;
  int published = 0;
  while (!pendingUploads_.empty() && staged < kUploadBudget && published < kPublishBudget) {
    MeshOut m = std::move(pendingUploads_.front());
    pendingUploads_.pop_front();
    if (m.skipped) continue;
    meshMs.add(m.ms); ++sec_.meshed; ++totalMeshed;
    const uint64_t key = ChunkKey::make(m.mesh.cx, m.mesh.cz);
    const auto lit = lod_.find(key);  // [C54-1] stale LOD level -> drop
    if (!map_.column(m.mesh.cx, m.mesh.cz) || lit == lod_.end() || lit->second.gen != m.gen) {
      if (m.prep.ready) renderer_.discardChunkMesh(m.prep);
      continue;
    }
    const size_t b = m.verts * sizeof(PackedVertex);
    if (m.prep.ready) {
      renderer_.publishChunkMesh(m.prep);
      ++published;
    } else {
      // Design B Stage 2: stage through the renderer's upload entry point.
      // On the software threeway path that is the lock-free ring (never
      // blocks, never touches the store here); everywhere else it applies
      // immediately, as the direct call did. A full ring requeues the mesh
      // at the front — retried next frame, never dropped — and stops this
      // frame's uploads (a full ring will not drain faster by pushing on).
      if (!renderer_.enqueueChunkMesh(std::move(m.mesh))) { pendingUploads_.push_front(std::move(m)); break; }
      staged += b;
    }
    bytes += b;
    { auto& subs = meshedSubs_[key]; if (std::find(subs.begin(), subs.end(), m.mesh.sy) == subs.end()) subs.push_back(m.mesh.sy); }
    {  // [C54-1] per-sub accounting (a LOD remesh replaces, never adds)
      ColStat& cs = colStats_[key]; cs.cx = m.mesh.cx; cs.cz = m.mesh.cz;
      if (cs.sv.empty()) cs.sv.assign(size_t(kSubChunks), 0u);
      const uint32_t old = cs.sv[size_t(m.mesh.sy)];
      cs.sv[size_t(m.mesh.sy)] = uint32_t(m.verts);
      cs.verts = cs.verts - old + m.verts;
      cs.draws += (m.verts ? 1 : 0) - (old ? 1 : 0);
    }
  }
  lap(2);  // [C53-1]
  // 4) drop columns that fell behind (outside radius + 2)
  std::vector<uint64_t> drop;
  for (const auto& [key, col] : map_.columns())
    if (std::abs(col->cx - pcx) > radius_ + 2 || std::abs(col->cz - pcz) > radius_ + 2) drop.push_back(key);
  for (uint64_t key : drop) {
    const ChunkColumn* col = map_.columns().at(key).get();
    ChunkMesh empty; empty.cx = col->cx; empty.cz = col->cz;
    for (int sy : meshedSubs_[key]) {
      empty.sy = sy;
      // Design B Stage 2: tombstones stage through the same entry point
      // (must-push on the ring); a refusal parks the tombstone in
      // tombstoneRetry_ — retried every update, flushed at destruction.
      ChunkMesh tomb = empty;
      if (!renderer_.enqueueChunkMesh(std::move(tomb))) tombstoneRetry_.push_back(empty);
    }
    meshedSubs_.erase(key);
    colStats_.erase(key);
    lod_.erase(key);  // [C54-1]
    requested_.erase(key);
    {
      std::unique_lock lock(*mapMu_);  // [C53-2]
      map_.columns().erase(key);
    }
    ++sec_.dropped;
  }
  lap(3);  // [C53-1]
  return bytes;
}

}  // namespace lithokraft
