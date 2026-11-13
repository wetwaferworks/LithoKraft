// Component Test world self-test: see component_test_selftest.h. 2026-11-12 (JD).
#include "lithokraft/game/component_test_selftest.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "litholattice/core/threading.h"
#include "litholattice/world/block.h"
#include "lithokraft/content/block_families.h"
#include "lithokraft/content/block_font.h"
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/component_test_world.h"
#include "lithokraft/content/appearance.h"  // B9 2026-11-12: Via B look
#include "lithokraft/content/items.h"  // B9 2026-11-12: Resin is an item
#include "lithokraft/game/cWorldCatalog.h"
#include "lithokraft/game/cWorldConfigBuilder.h"
#include "lithokraft/game/dimensions.h"

namespace lithokraft {

int runComponentTestSelfTest() {
  int fails = 0;
  auto check = [&](bool ok, const std::string& what) {
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++fails;
  };
  using W = cComponentTestWorld;
  std::printf("Component Test world self-test\n");
  const std::vector<W::Pad> pads = W::pads();
  const auto& reg = blockRegistry();

  {  // 1. Pads: 3 x 3 chunks each, chunk aligned, a clear 1-chunk aisle between neighbours.
    bool aligned = !pads.empty(), apart = true;
    for (const W::Pad& p : pads)
      aligned = aligned && W::kPad == 3 * kChunkSize && p.x0 % kChunkSize == 0 && p.z0 % kChunkSize == 0 &&
                p.cx == p.x0 + 24 && p.cz == p.z0 + 24 && p.x0 >= kChunkSize && p.z0 >= 2 * kChunkSize;
    for (size_t i = 0; i < pads.size(); ++i)
      for (size_t j = i + 1; j < pads.size(); ++j) {
        const W::Pad &a = pads[i], &b = pads[j];
        const int gapX = std::max(a.x0, b.x0) - std::min(a.x0, b.x0) - W::kPad;
        const int gapZ = std::max(a.z0, b.z0) - std::min(a.z0, b.z0) - W::kPad;
        if (gapX < kChunkSize && gapZ < kChunkSize) apart = false;  // overlapping or closer than an aisle
      }
    check(aligned, "component test: every pad is 3 x 3 chunks (48 x 48), chunk aligned, component at its centre");
    check(apart, "component test: pads never overlap; a clear 16-block (1-chunk) aisle between neighbours");
    int cx0, cz0, cx1, cz1;
    W::gridChunks(cx0, cz0, cx1, cz1);
    bool inside = true;
    for (const W::Pad& p : pads)
      inside = inside && (p.x0 >> 4) > cx0 && (p.z0 >> 4) > cz0 && ((p.x0 + W::kPad - 1) >> 4) < cx1 - 1 && ((p.z0 + W::kPad - 1) >> 4) < cz1 - 1;
    check(inside && W::minWorldChunks() >= std::max(cx1, cz1), "component test: worldChunks minimum covers every pad plus a margin aisle (no clipping)");
  }
  {  // 2. Which pads: the live Trace + Lamps/LEDs palette families, JD's spellings, the approved B9 names.
    std::set<std::string> labels, ids;
    bool uniqueIds = true;
    for (const W::Pad& p : pads) { labels.insert(p.label); uniqueIds = uniqueIds && ids.insert(p.id).second && p.id != "spawn"; }
    check(uniqueIds, "component test: pad spawn ids are unique and never 'spawn'");
    bool families = true;
    const cBlockFamilies& fam = cBlockFamilies::get();
    for (BlockFamily f : {BlockFamily::Trace, BlockFamily::Lamp})
      for (BlockId b : fam.members(f))
        if (!isPaletteHidden(b))
          families = families && std::any_of(pads.begin(), pads.end(), [b](const W::Pad& p) { return p.block == b; });
    check(families, "component test: every palette Trace and Lamps/LEDs block has a pad");
    bool keywords = true;
    for (const char* k : {"DIALOR", "REPEATOR", "THRESHOLDOR", "RED VIA", "BLUE VIA", "PULSOR", "MEMOR", "LATCHOR", "DIVIDOR", "RANDOR",
                          "CATCHOR", "OVERCLOCKOR", "PORTOR", "SIGNAL TRANSMITTOR", "SIGNAL RECEIVOR", "DISH", "SWITCH", "ROUTOR", "FUSOR"})
      if (!labels.count(k)) { keywords = false; std::printf("    missing label %s\n", k); }
    const bool muxOn = [] { const char* e = std::getenv("LL_MUX_RENAME"); return !(e && e[0] == '0'); }();
    if (muxOn && !labels.count("MULTIPLEXOR")) keywords = false;
    check(keywords && !labels.count("DIALER"), "component test: JD keyword spellings on the pads (DIALOR, MULTIPLEXOR, REPEATOR, THRESHOLDOR, RED VIA, ...)");
    bool noInvented = true;
    for (const W::Pad& p : pads)
      noInvented = noInvented && p.label.find("MORTOR") == std::string::npos && p.label.find("ELBOW") == std::string::npos &&
                   p.label.find('(') == std::string::npos && !isSideVia(p.block) && p.block != kTimerBuffer;
    check(noInvented, "component test: no Mortor pad, no Via elbow pad, no side-Via (orientation) pads");
    bool approved = true;
    for (const char* a : {"WATCHOR", "BITOR", "PUSHOR", "SWIVELOR", "RESIN"}) {
      const auto it = std::find_if(pads.begin(), pads.end(), [a](const W::Pad& p) { return p.label == a; });
      if (it == pads.end()) { approved = false; continue; }
      const auto id = reg.findById(it->engineId);
      if (it->itemOnly) approved = approved && it->block == kAir;                // Resin: never a block
      else approved = approved && (id ? it->block == *id : it->block == kAir);  // unregistered: centre empty
    }
    check(approved, "component test: approved Watchor / Bitor / Pushor / Swivelor / Resin pads exist; unregistered ids leave the centre empty, Resin is never a block");
    {  // B9 2026-11-12: Watchor is registered (Trace role) -> one WATCHOR pad, now carrying the block.
      const auto isW = [](const W::Pad& p) { return p.label == "WATCHOR"; };
      const auto wit = std::find_if(pads.begin(), pads.end(), isW);
      check(wit != pads.end() && std::count_if(pads.begin(), pads.end(), isW) == 1 && wit->block == kWatchor &&
                wit->engineId == "lithokraft:watchor" && cBlockFamilies::get().familyOf(kWatchor) == BlockFamily::Trace,
            "component test: the WATCHOR pad carries lithokraft:watchor (one pad, Trace family), no longer name-only");
    }
    {  // B9 2026-11-12: Bitor is registered (Trace role) -> one BITOR pad, now carrying the block.
      const auto isB = [](const W::Pad& p) { return p.label == "BITOR"; };
      const auto bit = std::find_if(pads.begin(), pads.end(), isB);
      check(bit != pads.end() && std::count_if(pads.begin(), pads.end(), isB) == 1 && bit->block == kBitor &&
                bit->engineId == "lithokraft:bitor" && cBlockFamilies::get().familyOf(kBitor) == BlockFamily::Trace,
            "component test: the BITOR pad carries lithokraft:bitor (one pad, Trace family), no longer name-only");
    }
    {  // B9 2026-11-12: Pushor is registered (Trace role) -> one PUSHOR pad, now carrying the block.
      const auto isP = [](const W::Pad& p) { return p.label == "PUSHOR"; };
      const auto pit = std::find_if(pads.begin(), pads.end(), isP);
      check(pit != pads.end() && std::count_if(pads.begin(), pads.end(), isP) == 1 && pit->block == kPushor &&
                pit->engineId == "lithokraft:pushor" && cBlockFamilies::get().familyOf(kPushor) == BlockFamily::Trace,
            "component test: the PUSHOR pad carries lithokraft:pushor (one pad, Trace family), no longer name-only");
    }
    {  // B9 2026-11-12: Swivelor is registered (Trace role) -> one SWIVELOR pad, now carrying the block.
      const auto isS = [](const W::Pad& p) { return p.label == "SWIVELOR"; };
      const auto sit = std::find_if(pads.begin(), pads.end(), isS);
      check(sit != pads.end() && std::count_if(pads.begin(), pads.end(), isS) == 1 && sit->block == kSwivelor &&
                sit->engineId == "lithokraft:swivelor" && cBlockFamilies::get().familyOf(kSwivelor) == BlockFamily::Trace,
            "component test: the SWIVELOR pad carries lithokraft:swivelor (one pad, Trace family), no longer name-only");
    }
    {  // B9 2026-11-12: Resin is registered as an ITEM; its pad stays name-only (no block, no ground item).
      const auto isR = [](const W::Pad& p) { return p.label == "RESIN"; };
      const auto rit = std::find_if(pads.begin(), pads.end(), isR);
      check(rit != pads.end() && std::count_if(pads.begin(), pads.end(), isR) == 1 && rit->itemOnly && rit->block == kAir &&
                rit->engineId == "lithokraft:resin" && findItemByStringId(rit->engineId) != nullptr && !reg.findById(rit->engineId),
            "component test: the RESIN pad stays name-only -- lithokraft:resin is an item, not a block");
    }
  }
  {  // 3. Spiral labels: every glyph from the shared 3x5 font, inside the pad, clear of the component, apart.
    bool all = true;
    for (const W::Pad& p : pads) {
      const W::Spiral sp = W::spiral(p.label);
      bool ok = sp.fits && sp.glyphs.size() == p.label.size();
      std::map<std::pair<int, int>, int> owner;
      int sides = 0;
      for (size_t g = 0; g < sp.glyphs.size() && ok; ++g) {
        const W::Glyph& gl = sp.glyphs[g];
        if (gl.ch != ' ') ok = ok && blockFont3x5Labels(gl.ch) && !gl.cells.empty();
        sides = std::max(sides, gl.side + 1);
        if (g > 0) ok = ok && gl.side >= sp.glyphs[g - 1].side;  // the path only moves on (spiral, not a scatter)
        for (const auto& c : gl.cells) {
          const int cheb = std::max(std::abs(c.first), std::abs(c.second));
          ok = ok && cheb >= 2 && cheb <= W::kSpiralLimit;      // 2 clear rings round the component, >= 2 from the edge
          ok = ok && owner.emplace(c, int(g)).second;           // no two glyphs share a cell
        }
      }
      for (const auto& [c, g] : owner)  // separate glyphs never touch (8-neighbourhood): readable
        for (int dz = -1; dz <= 1 && ok; ++dz)
          for (int dx = -1; dx <= 1 && ok; ++dx) {
            const auto it = owner.find({c.first + dx, c.second + dz});
            if (it != owner.end() && it->second != g) ok = false;
          }
      if (p.label.size() > 4) ok = ok && sides >= 2;  // longer names turn at least one corner
      if (!ok) { all = false; std::printf("    spiral FAIL for %s (pitch %d)\n", p.label.c_str(), sp.pitch); }
    }
    check(all, "component test: every name spirals round its component in the 3x5 font, fits the pad, no clipping or overlap");
  }
  {  // 4. Dimension: flat Wafer Silicon everywhere (spawn area, pads and a far chunk), no procgen.
    cDimensionReg dims = createDimensions();
    Dimension* d = dims.find("componenttest");
    check(d && d->generator, "component test: dimension 'componenttest' is registered with a generator (componenttest.json)");
    check(builtinWorldgenSource("componenttest") != nullptr, "component test: componenttest.json is embedded as a built-in generator");
    if (d && d->generator) {
      cJobPool pool(2);
      float sx = 0, sz = 0, yaw = 0;
      W::spawn(sx, sz, yaw);
      const Spawn s = d->loadRadius(pool, W::minWorldChunks(), int(sx) >> 4, int(sz) >> 4, 2);
      int cx0, cz0, cx1, cz1;
      W::gridChunks(cx0, cz0, cx1, cz1);
      d->generateMissing(pool, cx0, cz0, cx1, cz1);
      constexpr int kFar = 4000 >> 4;  // a chunk far outside the pad grid
      d->generateMissing(pool, kFar, kFar, kFar + 1, kFar + 1);
      const int floorY = d->chunks.surfaceY(int(sx), int(sz));
      auto flatSilicon = [&](int x0, int z0, int n) {
        for (int z = z0; z < z0 + n; ++z)
          for (int x = x0; x < x0 + n; ++x)
            if (d->chunks.surfaceY(x, z) != floorY || d->chunks.get(x, floorY, z) != kWaferSilicon ||
                d->chunks.get(x, floorY - 1, z) != kWaferSilicon || d->chunks.get(x, floorY + 1, z) != kAir)
              return false;
        return true;
      };
      check(floorY > 0 && s.y == float(floorY + 1) && flatSilicon(kFar * 16, kFar * 16, 16),
            "component test: a far chunk outside the grid is the same flat Wafer Silicon plain (no hills, foliage or ceramic)");
      check(flatSilicon(cx0 * 16, cz0 * 16, 16), "component test: the spawn-aisle chunk is flat Wafer Silicon before the build");
      check(std::abs(s.x - sx) < 0.01f && std::abs(s.z - sz) < 0.01f, "component test: dimension spawn is the Component Test aisle spawn");
      // 5. Build: components on the ground at the centres, ceramic glyphs flush in the floor, spawn clear.
      const W::Layout L = W().build(d->chunks, floorY);
      check(L.unfit.empty() && L.pads.size() == pads.size(), "component test: every pad built, no label unfit");
      bool comps = true, nameOnly = true, glyphs = true, floor = true;
      int registered = 0;
      for (const W::Pad& p : L.pads) {
        const BlockId at = d->chunks.get(p.cx, floorY + 1, p.cz);
        if (p.block != kAir) {
          ++registered;
          comps = comps && at == p.block && d->chunks.get(p.cx, floorY, p.cz) == kWaferSilicon;
        } else {
          nameOnly = nameOnly && at == kAir;
        }
        std::set<std::pair<int, int>> cells;
        for (const W::Glyph& g : W::spiral(p.label).glyphs)
          for (const auto& c : g.cells) cells.insert(c);
        int ceramic = 0;
        for (int dz = -W::kCentre; dz < W::kPad - W::kCentre; ++dz)
          for (int dx = -W::kCentre; dx < W::kPad - W::kCentre; ++dx) {
            const BlockId f = d->chunks.get(p.cx + dx, floorY, p.cz + dz);
            const bool want = cells.count({dx, dz}) != 0;
            if (want) { glyphs = glyphs && f == kCeramic; ++ceramic; }
            else floor = floor && f == kWaferSilicon;
            if (!(dx == 0 && dz == 0)) floor = floor && d->chunks.get(p.cx + dx, floorY + 1, p.cz + dz) == kAir;
          }
        glyphs = glyphs && ceramic == int(cells.size()) && ceramic > 0;
      }
      check(comps && registered == L.components && registered > 0,
            "component test: each registered component stands on the silicone at its pad centre (floorY + 1)");
      check(nameOnly, "component test: name-only pads (approved, not registered yet) have an empty centre");
      {  // B9 2026-11-12: a fresh build puts the Watchor on its pad centre automatically.
        const auto lw = std::find_if(L.pads.begin(), L.pads.end(), [](const W::Pad& p) { return p.label == "WATCHOR"; });
        check(lw != L.pads.end() && lw->block == kWatchor && d->chunks.get(lw->cx, floorY + 1, lw->cz) == kWatchor,
              "component test: a fresh build places the Watchor at the WATCHOR pad centre (floorY + 1)");
      }
      {  // B9 2026-11-12: a fresh build puts the Bitor on its pad centre automatically.
        const auto lb = std::find_if(L.pads.begin(), L.pads.end(), [](const W::Pad& p) { return p.label == "BITOR"; });
        check(lb != L.pads.end() && lb->block == kBitor && d->chunks.get(lb->cx, floorY + 1, lb->cz) == kBitor,
              "component test: a fresh build places the Bitor at the BITOR pad centre (floorY + 1)");
      }
      {  // B9 2026-11-12: a fresh build puts the Pushor on its pad centre automatically.
        const auto lp = std::find_if(L.pads.begin(), L.pads.end(), [](const W::Pad& p) { return p.label == "PUSHOR"; });
        check(lp != L.pads.end() && lp->block == kPushor && d->chunks.get(lp->cx, floorY + 1, lp->cz) == kPushor,
              "component test: a fresh build places the Pushor at the PUSHOR pad centre (floorY + 1)");
      }
      {  // B9 2026-11-12: a fresh build puts the Swivelor on its pad centre automatically.
        const auto ls = std::find_if(L.pads.begin(), L.pads.end(), [](const W::Pad& p) { return p.label == "SWIVELOR"; });
        check(ls != L.pads.end() && ls->block == kSwivelor && d->chunks.get(ls->cx, floorY + 1, ls->cz) == kSwivelor,
              "component test: a fresh build places the Swivelor at the SWIVELOR pad centre (floorY + 1)");
      }
      {  // B9 2026-11-12: Resin is an item -- a fresh build leaves the RESIN pad centre empty.
        const auto lr = std::find_if(L.pads.begin(), L.pads.end(), [](const W::Pad& p) { return p.label == "RESIN"; });
        check(lr != L.pads.end() && lr->block == kAir && d->chunks.get(lr->cx, floorY + 1, lr->cz) == kAir,
              "component test: a fresh build places no Resin block at the RESIN pad centre (item only)");
      }
      {  // B9 2026-11-12: the RED VIA / BLUE VIA pads still carry their vias, now drawn as the Via B lattice cage.
        bool ok = true;
        for (const char* lab : {"RED VIA", "BLUE VIA"}) {
          const auto lv = std::find_if(L.pads.begin(), L.pads.end(), [lab](const W::Pad& p) { return p.label == lab; });
          const int vy = floorY + 1;
          if (lv == L.pads.end()) { ok = false; continue; }
          const ViaLook vl = viaLook(d->chunks.get(lv->cx, vy, lv->cz), d->chunks.getState(lv->cx, vy, lv->cz),
                                     [&](int dx, int dy, int dz) { return d->chunks.get(lv->cx + dx, vy + dy, lv->cz + dz); },
                                     [&](int dx, int dy, int dz) { return d->chunks.getState(lv->cx + dx, vy + dy, lv->cz + dz); });
          ok = ok && isVia(lv->block) && d->chunks.get(lv->cx, vy, lv->cz) == lv->block && vl.model && !vl.elbow &&
               blockAppearance().shapedRender(reg.def(lv->block));
        }
        check(ok, "component test: a fresh build places the Red / Blue Via at their pad centres, drawn as the Via B lattice cage (straight, no elbow)");
      }
      check(glyphs, "component test: each name is Ceramic glyph cells flush in the floor (floorY), on its spiral");
      check(floor, "component test: the rest of every pad is a clear Wafer Silicon floor");
      const int spx = int(std::floor(L.spawnX)), spz = int(std::floor(L.spawnZ));
      check(d->chunks.get(spx, floorY, spz) == kWaferSilicon && d->chunks.get(spx, floorY + 1, spz) == kAir &&
                d->chunks.get(spx, floorY + 2, spz) == kAir && spz < pads.front().z0,
            "component test: spawn on the north aisle, feet on the silicone, not in a letter, looking at the grid");
    }
  }
  {  // 6. Menu + flag: the world picker lists Component Test; --component-test selects it.
    const cWorldCatalog& cat = cWorldCatalog::instance();
    const auto& e = cat.entries();
    const auto it = std::find_if(e.begin(), e.end(), [](const cWorldCatalog::Entry& x) { return x.id == "component-test"; });
    const bool listed = it != e.end() && it->title == "Component Test" && it->args == std::vector<std::string>{"--component-test"};
    check(listed, "component test: the world picker lists 'Component Test' (component-test, --component-test)");
    if (listed) {
      bool spawns = !it->spawns.empty() && it->spawns.front() == "spawn" && it->spawns.size() == pads.size() + 1;
      for (size_t i = 0; spawns && i < pads.size(); ++i) spawns = it->spawns[i + 1] == pads[i].id;
      check(spawns, "component test: catalog spawns are 'spawn' then one per pad, in grid order");
      check(cat.indexFor("componenttest", "componenttest", false) == int(it - e.begin()),
            "component test: indexFor(componenttest) is the Component Test entry");
    }
    cWorldConfigBuilder::Launch l;
    l.noSave = true;
    l.dataRoot = ".";
    std::string note;
    const GameConfig c = cWorldConfigBuilder::build(l, {"--component-test", "--spawn", "dialor"}, "", &note);
    check(c.benchScene == "componenttest" && c.dimension == "componenttest" && c.startSpawn == "dialor" && c.saveDir.empty(),
          "component test: --component-test selects the componenttest scene + dimension (--spawn passes through; --no-save keeps it unsaved)");
  }
  return fails;
}

}  // namespace lithokraft
