// Playtest test world: see test_world.h. 2026-10-16 (JD).
#include "lithokraft/content/test_world.h"
#include <algorithm>
#include <cmath>
#include <map>
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/circuit_scene.h"  // 2026-10-24 Examples section
#include "lithokraft/content/repeator.h"  // WaferBeta tint demo
#include "litholattice/world/block.h"

namespace lithokraft {

namespace {
// 3x5 block font for the row labels (top row first, '#' = block).
const char* glyph(char c) {
  static const std::map<char, const char*> g = {
      {'A', "###"
            "#.#"
            "###"
            "#.#"
            "#.#"},
      {'B', "##."
            "#.#"
            "##."
            "#.#"
            "##."},
      {'C', "###"
            "#.."
            "#.."
            "#.."
            "###"},
      {'D', "##."
            "#.#"
            "#.#"
            "#.#"
            "##."},
      {'F', "###"
            "#.."
            "##."
            "#.."
            "#.."},
      {'U', "#.#"
            "#.#"
            "#.#"
            "#.#"
            "###"},
      {'Y', "#.#"
            "#.#"
            ".#."
            ".#."
            ".#."},
      {'-', "..."
            "..."
            "###"
            "..."
            "..."},
      {'E', "###"
            "#.."
            "##."
            "#.."
            "###"},
      {'G', "###"
            "#.."
            "#.#"
            "#.#"
            "###"},
      {'H', "#.#"
            "#.#"
            "###"
            "#.#"
            "#.#"},
      {'I', "###"
            ".#."
            ".#."
            ".#."
            "###"},
      {'K', "#.#"
            "#.#"
            "##."
            "#.#"
            "#.#"},
      {'L', "#.."
            "#.."
            "#.."
            "#.."
            "###"},
      {'M', "#.#"
            "###"
            "###"
            "#.#"
            "#.#"},
      {'N', "##."
            "#.#"
            "#.#"
            "#.#"
            "#.#"},
      {'O', "###"
            "#.#"
            "#.#"
            "#.#"
            "###"},
      {'P', "###"
            "#.#"
            "###"
            "#.."
            "#.."},
      {'R', "##."
            "#.#"
            "##."
            "#.#"
            "#.#"},
      {'S', "###"
            "#.."
            "###"
            "..#"
            "###"},
      {'T', "###"
            ".#."
            ".#."
            ".#."
            ".#."},
      {'W', "#.#"
            "#.#"
            "###"
            "###"
            "#.#"},
      {'4', "#.#"
            "#.#"
            "###"
            "..#"
            "..#"},
      {'5', "###"
            "#.."
            "###"
            "..#"
            "###"},
      {'6', "###"
            "#.."
            "###"
            "#.#"
            "###"},
      {'7', "###"
            "..#"
            "..#"
            "..#"
            "..#"},
      {'8', "###"
            "#.#"
            "###"
            "#.#"
            "###"},
      {'9', "###"
            "#.#"
            "###"
            "..#"
            "###"},
      {'X', "#.#"
            "#.#"
            ".#."
            "#.#"
            "#.#"},
      {'0', "###"
            "#.#"
            "#.#"
            "#.#"
            "###"},
      {'1', ".#."
            "##."
            ".#."
            ".#."
            "###"},
      {'2', "###"
            "..#"
            "###"
            "#.."
            "###"},
      {'3', "###"
            "..#"
            "###"
            "..#"
            "###"},
  };
  const auto it = g.find(c);
  return it == g.end() ? nullptr : it->second;
}

enum Group { kGWire, kGIo, kGLogic, kGLamp, kGMachine, kGBlock, kGPlant, kGroups };
const char* const kGroupLabel[kGroups] = {"WIRE", "IO", "LOGIC", "LAMP", "MACHINE", "BLOCK", "PLANT"};

Group groupOf(const BlockDef& d) {
  const auto r = SimRole(d.role);
  if (r == SimRole::Wire) return kGWire;
  if (r == SimRole::Lamp || r == SimRole::ColorLamp) return kGLamp;
  if (r == SimRole::Torch || r == SimRole::Switch || r == SimRole::cButton || r == SimRole::Keyboard) return kGIo;
  if (r != SimRole::None) return kGLogic;
  if (d.flags & kAttachable) return kGPlant;
  if (d.flags & (kDirectional | kInteractive)) return kGMachine;
  return kGBlock;
}
}  // namespace

std::vector<BlockId> testWorldKit(int n) {
  const auto& reg = blockRegistry();
  std::vector<BlockId> out;
  auto add = [&](BlockId b) {
    if (int(out.size()) < n && b != kAir && std::find(out.begin(), out.end(), b) == out.end()) out.push_back(b);
  };
  for (int g = kGWire; g <= kGMachine; ++g)
    for (int id = 1; id < int(reg.size()); ++id)
      if (groupOf(reg.def(BlockId(id))) == g) add(BlockId(id));
  add(kWaferSilicon); add(kGlass); add(kCeramic);
  for (int id = 1; id < int(reg.size()); ++id) add(BlockId(id));
  return out;
}

TestWorldLayout buildTestWorld(ChunkMap& w, int sx, int floorY, int sz, int screenSize) {
  TestWorldLayout L;
  const auto& reg = blockRegistry();
  L.floorY = floorY;
  // The player looks +Z (yaw pi/2); its right hand is -X, so rows run from +X toward -X.
  constexpr int kPerRow = 24, kStep = 2, kRowPitch = 9;
  const int right = sx + kPerRow - 1;  // row start (the player's left)
  auto put = [&](int x, int y, int z, BlockId b, CellState st = 0) {
    if (!w.set(x, y, z, b)) return false;
    w.setState(x, y, z, st);
    ++L.placed;
    return true;
  };
  // Group the registry (ids 1 .. size-1: the E palette list), registry order inside a group.
  std::vector<BlockId> groups[kGroups];
  for (int id = 1; id < int(reg.size()); ++id) groups[groupOf(reg.def(BlockId(id)))].push_back(BlockId(id));
  int rowZ = sz + 10;
  for (int g = 0; g < kGroups; ++g)
    for (size_t at = 0, part = 0; at < groups[g].size(); at += kPerRow, ++part) {
      TestWorldRow row;
      row.label = kGroupLabel[g];
      if (groups[g].size() > size_t(kPerRow)) row.label += char('1' + part);
      row.blocks.assign(groups[g].begin() + long(at), groups[g].begin() + long(std::min(groups[g].size(), at + kPerRow)));
      row.z = rowZ;
      rowZ += kRowPitch;
      L.rows.push_back(std::move(row));
    }
  const int screenZ = rowZ + 6;
  const int scale = screenSize >= 64 ? 4 : 2, px = 16 * scale;
  // Flat pad: clear the air above, level the top at floorY, solid under it.
  const int x0 = std::min(sx - px / 2, sx - kPerRow * kStep) - 12, x1 = std::max(sx + px / 2, right) + 12;
  const int z0 = sz - 12, z1 = screenZ + 8, yTop = floorY + std::max(px, 24) + 8;
  for (int z = z0; z <= z1; ++z)
    for (int x = x0; x <= x1; ++x) {
      for (int y = floorY + 1; y <= yTop; ++y) w.set(x, y, z, kAir);
      for (int y = floorY - 3; y <= floorY; ++y) w.set(x, y, z, kWaferSilicon);
    }
  // Rows: a ceramic strip (not powerable, cuts wire) under each row so neighbours never couple
  // through the floor; one block gap between items; a block-font label on the floor in front.
  for (const TestWorldRow& row : L.rows) {
    for (int i = 0; i < int(row.blocks.size()) * kStep + 1; ++i) put(right + 1 - i, floorY, row.z, kCeramic);
    for (size_t i = 0; i < row.blocks.size(); ++i) {
      const BlockId b = row.blocks[i];
      const int x = right - int(i) * kStep;
      const auto& d = reg.def(b);
      CellState st = 0;
      if (d.flags & kAttachable) st = withAttach(0, 1);  // standing on the strip
      // directional blocks keep facing 0 = north (-Z): their fronts face the player
      if (!put(x, floorY + 1, row.z, b, st)) L.missing.push_back(b);
    }
    // label: 3x5 glyphs in ceramic flat on the floor, top of the text toward the row (+Z)
    int cx = right;
    for (char c : row.label) {
      if (const char* gl = glyph(c))
        for (int r = 0; r < 5; ++r)
          for (int k = 0; k < 3; ++k)
            if (gl[r * 3 + k] == '#') put(cx - k, floorY, row.z - 2 - r, kCeramic);
      cx -= 4;
    }
  }
  // Pattern ROM demo screen: colour lamps (64-colour) standing in z = screenZ, a ceramic backing
  // behind, centred on the spawn line, bottom row at floorY + 2. Label "ROM" on the floor.
  L.screen.rom = smileyRom(64);
  L.screen.scale = scale;
  L.screen.z = screenZ;
  L.screen.y0 = floorY + 2;
  L.screen.x0 = sx - px / 2 + 1;
  for (int y = floorY + 1; y < L.screen.y0 + px + 1; ++y)
    for (int x = L.screen.x0 - 1; x <= L.screen.x0 + px; ++x) {
      const bool frame = y == floorY + 1 || y == L.screen.y0 + px || x == L.screen.x0 - 1 || x == L.screen.x0 + px;
      put(x, y, screenZ, frame ? kCeramic : kColorLamp64);
      put(x, y, screenZ + 1, kCeramic);
    }
  paintPatternRom(w, L.screen, 0);
  L.screen.frame = 0;  // frame 0 is up; the player starts its count from tick 0
  {
    int cx = sx + 5;
    for (char c : std::string("ROM")) {
      if (const char* gl = glyph(c))
        for (int r = 0; r < 5; ++r)
          for (int k = 0; k < 3; ++k)
            if (gl[r * 3 + k] == '#') put(cx - k, floorY, screenZ - 2 - r, kCeramic);
      cx -= 4;
    }
  }
  {  // 2026-10-16 (JD leaf pass): crystal leaf grove left of the screen. JD: trees vary, some
     // red-dominant, some blue-dominant (CloudAtlas reddom/bluedom); a few mixed with a/b.
    const auto& r = blockRegistry();
    auto idOf = [&](const char* n) { const auto i = r.findById(n); return i ? *i : kAir; };
    const BlockId red = idOf("lithokraft:crystal_leaves_reddom"), blue = idOf("lithokraft:crystal_leaves_bluedom");
    const BlockId la = idOf("lithokraft:crystal_leaves_a"), lb = idOf("lithokraft:crystal_leaves_b");
    // per tree: x offset, z offset, height, main leaf, accent leaf
    struct Tree { int dx, dz, h; BlockId main, accent; };
    const Tree trees[8] = {{0, 0, 4, red, la},   {-6, 2, 5, blue, lb}, {-12, -1, 4, red, red}, {-18, 1, 5, blue, blue},
                           {-24, 0, 4, red, lb}, {-3, 7, 3, blue, la}, {-9, 8, 4, red, blue}, {-15, 7, 3, blue, red}};
    const int lx0 = L.screen.x0 - 30, lz0 = screenZ - 6;
    for (const Tree& t : trees) {
      if (t.main == kAir) continue;
      const int cx = lx0 + t.dx, cz = lz0 + t.dz;
      for (int y = floorY + 1; y <= floorY + 2; ++y) put(cx - 2, y, cz + 1, kCeramic);  // trunk
      for (int y = floorY + 3; y < floorY + 3 + t.h; ++y)
        for (int dz = 0; dz < 4; ++dz)
          for (int dx = 0; dx < 5; ++dx) {
            const bool edge = (dx == 0 || dx == 4) && (dz == 0 || dz == 3);
            if (edge && (y == floorY + 3 || y == floorY + 2 + t.h)) continue;
            const bool acc = ((dx * 7 + dz * 3 + y * 5) % 9) == 0;  // sparse accent blocks
            put(cx - dx, y, cz - 1 + dz, acc && t.accent != kAir ? t.accent : t.main);
          }
    }
    L.leafDemoX = lx0; L.leafDemoZ = lz0;
  }
  {  // 2026-10-16 WaferBeta: Trace-tint demo behind the spawn. Machines face south (+Z, toward the
     // spawn) with their back port on red / blue Trace (off rows) or a red / blue torch (on rows),
     // so the tint-masked fronts show both channels idle and powered.
    const auto& r = blockRegistry();
    auto idOf = [&](const char* n) { const auto i = r.findById(n); return i ? *i : kAir; };
    const char* names[] = {"lithokraft:signal_transmittor", "lithokraft:signal_receivor", "lithokraft:dish", "lithokraft:overclockor",
                           "lithokraft:portor", "lithokraft:fusor", "lithokraft:chipor", "lithokraft:chipor_viewport",
                           "lithokraft:lithographor", "lithokraft:repeator", "lithokraft:repeator", "lithokraft:repeator", "lithokraft:repeator"};
    for (int row = 0; row < 4; ++row) {
      const bool blue = row >= 2, on = row & 1;
      const int z = sz - 3 - row * 3;
      for (int i = 0; i < 13; ++i) {
        const BlockId b = idOf(names[i]);
        if (b == kAir) continue;
        const int x = sx + 12 - i * 2;
        const CellState st = b == kRepeator ? repeator::encode(0, 2, i - 9, on) : makeState(0, 2, 0, on);
        put(x, floorY + 1, z, b, st);
        if (on) put(x, floorY + 1, z - 1, blue ? kBlueTorch : kRedTorch, withAttach(0, 1));
        else put(x, floorY + 1, z - 1, blue ? kBlueTrace : kRedTrace, 0);
      }
    }
  }
  L.spawnX = float(sx) + 0.5f;
  L.spawnZ = float(sz) + 0.5f;
  L.spawnYaw = 1.5707963f;
  return L;
}

std::vector<TestWorldExample> buildTestWorldExamples(ChunkMap& w, const TestWorldLayout& tw, int sx, int sz, int worldBlocks) {
  // Grid of slots north (+Z) of the test world (its pad ends a few blocks past the ROM screen).
  // Each slot: a flat Wafer pad (air cleared above), a ceramic label on the floor at its south
  // edge, the scene built by its own bench code. Slots are kSlotW apart so nothing couples.
  constexpr int kSlotW = 80, kSlotD = 104, kCols = 6, kClear = 72;
  const int floorY = tw.floorY, y = floorY + 1;
  const int gx0 = sx - 40, gz0 = tw.screen.z + 24;
  struct Def { const char* label; int w, d; };
  // order: memory / display first, then screens, routing, machines, benchmarks (wire torture last: deepest)
  const Def defs[] = {{"DISPLAY", 1, 1},    {"FRAMEBUFFER", 1, 1}, {"FB WALL", 1, 1},  {"WALL PIXEL", 1, 1}, {"PATTERN", 1, 1},
                      {"SAWTOOTH", 1, 1},   {"REDIRECTOR", 1, 1},  {"LITHO", 1, 1},    {"CIRCUITS", 1, 1},  {"WIRE TORTURE", 1, 2},
                      {"CLOCKS", 1, 1},     {"PIXEL DISPLAY", 2, 1}};
  std::vector<TestWorldExample> out;
  int col = 0, row = 0;
  for (const Def& d : defs) {
    if (col >= kCols) { col = 0; ++row; }
    TestWorldExample e;
    e.label = d.label;
    e.x0 = gx0 + col * kSlotW; e.z0 = gz0 + row * kSlotD;
    e.x1 = e.x0 + kSlotW * d.w - 9; e.z1 = e.z0 + kSlotD * d.d - 9;
    ++col;
    if (e.x0 < 1 || e.z0 < 1 || e.x1 >= worldBlocks - 1 || e.z1 >= worldBlocks - 1) {
      e.note = "skipped: slot outside the loaded world";
      out.push_back(std::move(e));
      continue;
    }
    for (int z = e.z0; z <= e.z1; ++z)
      for (int x = e.x0; x <= e.x1; ++x) {
        for (int yy = y; yy <= floorY + kClear; ++yy) w.set(x, yy, z, kAir);
        for (int yy = floorY - 3; yy <= floorY; ++yy) w.set(x, yy, z, kWaferSilicon);
      }
    int lx = e.x0 + 3 + 4 * int(e.label.size()) - 1;  // text reads left to right for a player facing +Z
    for (char c : e.label) {
      if (const char* gl = glyph(c))
        for (int r = 0; r < 5; ++r)
          for (int k = 0; k < 3; ++k)
            if (gl[r * 3 + k] == '#') w.set(lx - k, floorY, e.z0 + 6 - r, kCeramic);
      lx -= 4;
    }
    const int bx = e.x0 + 8, bz = e.z0 + 14;  // scene origin inside the slot
    const std::string_view L = e.label;
    if (L == "DISPLAY") e.blocks = buildDisplayScene(w, bx, y, bz, 32);
    else if (L == "FRAMEBUFFER") e.blocks = buildFramebufferScene(w, bx, y, bz, 64).blocks;
    else if (L == "FB WALL") e.blocks = buildFramebufferWall(w, bx, y, bz, 32).blocks;
    else if (L == "WALL PIXEL") e.blocks = buildWallPixelScene(w, bx + 4, y, bz + 12, 16, 8).blocks;
    else if (L == "PATTERN") e.blocks = buildPatternWall(w, bx + 4, y, bz + 22, 32, "checkers").blocks;
    else if (L == "SAWTOOTH") e.blocks = buildSawtoothWall(w, bx + 4, y, bz + 12, 32, 8, "circles").blocks;
    else if (L == "REDIRECTOR") e.blocks = buildRedirectorScene(w, bx + 2, y, bz + 6, 24);
    else if (L == "LITHO") e.blocks = buildLithoMachine(w, bx + 16, y, bz + 12, false) + buildLithoMachine(w, bx + 40, y, bz + 12, true);
    else if (L == "CIRCUITS") e.blocks = buildCircuitScene(w, bx, y, bz, 48);
    else if (L == "WIRE TORTURE") e.blocks = buildWireTorture(w, bx + 4, y, bz).blocks;
    else if (L == "CLOCKS") {
      // Four self-running Repeator ring clocks (delay 2 / 4 / 8 / 16), each labelled with its
      // measured lap period in ticks ("T"); pulse lamp left, Fast Latch toggle lamp right.
      for (int i = 0; i < 4; ++i) {
        const int cx = bx + 2 + 14 * i, cz = bz + 10;
        e.blocks += buildRingClock(w, cx, y, cz, i);
        const std::string t = std::to_string(kRingClockPeriod[i]) + "T";
        int tx = cx + 1 + 4 * int(t.size()) - 1;
        for (char c : t) {
          if (const char* gl = glyph(c))
            for (int r = 0; r < 5; ++r)
              for (int k = 0; k < 3; ++k)
                if (gl[r * 3 + k] == '#') w.set(tx - k, floorY, cz - 2 - r, kCeramic);
          tx -= 4;
        }
      }
      e.note = "ring clocks, periods measured by the signal selftest";
    } else if (L == "PIXEL DISPLAY") {
      // Small working displays (the display bench scene: ring clock -> Pulsor reads Memor -> row of
      // Color Lamps): a 4-row and an 8-row; the east half of the pad is left blank for bigger ones.
      e.blocks = buildDisplayScene(w, bx, y, bz, 24) + buildDisplayScene(w, bx, y, bz + 30, 40);
      e.note = "4-row + 8-row display (one colour per row); east half blank for wiring";
    }
    e.placed = true;
    if (L == "LITHO") e.note = "left unwired, right wired reference";
    out.push_back(std::move(e));
  }
  return out;
}

}  // namespace lithokraft
