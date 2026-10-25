// Wiring-pass selftests (B6 etch flow, B7 signing flow, player.lat v5
// stack data). Follows the game_ui_selftest convention: one PASS/FAIL
// line per check, return the failure count. See wiring_selftest.h.
#include "litholattice/sim/vehicle_interfaces.h"  // IVehicle / IGlider stubs (2026-10-16)
#include "lithokraft/game/wiring_selftest.h"
#include "lithokraft/game/prayer.h"  // Cortana 2026-10-15 2255
#include "lithokraft/game/world_clock.h"  // Cortana 2026-10-16

#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>

#include "lithokraft/content/blocks.h"
#include "lithokraft/content/certificates.h"
#include "lithokraft/content/component.h"  // cBehaviorCatalog
#include "lithokraft/content/items.h"
#include "lithokraft/game/dimensions.h"
#include "lithokraft/game/etching.h"
#include "lithokraft/content/circuit_scene.h"  // Cortana 2026-10-15 1900: buildLithoMachine
#include "lithokraft/content/signal.h"
#include "lithokraft/game/lithography.h"
#include "lithokraft/game/inventory.h"
#include "lithokraft/game/signing.h"
#include "lithokraft/game/world_save.h"
#include "lithokraft/content/post.h"  // 2026-10-25 Posts
#include "litholattice/procgen/shrubs.h"
#include "litholattice/storage/storage.h"

namespace lithokraft {
namespace {

int g_fails = 0;
void check(bool ok, const std::string& name) {
  std::printf("[%s] wiring: %s\n", ok ? "PASS" : "FAIL", name.c_str());
  if (!ok) ++g_fails;
}

// First flat slot whose stack matches pred, or -1.
template <typename Pred>
int findSlot(const Inventory& inv, Pred pred) {
  const auto slots = inv.allSlots();
  for (size_t i = 0; i < slots.size(); ++i)
    if (pred(*slots[i])) return int(i);
  return -1;
}

}  // namespace

// IVehicle / IGlider stub contract check (no gameplay): a test glider computes thrust/lift
// from pressed controls only; the authoritative tick writes state.
struct TestGlider final : litholattice::IGlider {
  bool onMount(litholattice::VehicleState& s, uint64_t r) override { if (s.riderId) return false; s.riderId = r; return true; }
  void onDismount(litholattice::VehicleState& s) override { s.riderId = 0; }
  void onTick(const litholattice::VehicleTickContext&, const litholattice::VehicleControls& c, litholattice::VehicleState& s) override {
    s.thrust = thrustFor(c, s); s.speed += s.thrust; s.lift = liftFor(c, s); if (s.thrust && s.fuel) --s.fuel;
  }
  int32_t thrustFor(const litholattice::VehicleControls& c, const litholattice::VehicleState&) const override {
    return c.has(litholattice::VehicleControls::kThrottle) ? 256 : 0;
  }
  int32_t liftFor(const litholattice::VehicleControls& c, const litholattice::VehicleState& s) const override {
    return c.has(litholattice::VehicleControls::kPitchUp) ? s.speed / 4 : s.speed / 8;
  }
  int32_t glideRatio(const litholattice::VehicleState&) const override { return 10 * 256; }
};

int runWiringSelfTest() {
  {
    TestGlider g; litholattice::IVehicle& v = g;
    litholattice::VehicleState a, b; a.fuel = b.fuel = 10;
    check(v.onMount(a, 7) && !v.onMount(a, 8) && a.riderId == 7, "Vehicle: mount once, second rider refused");
    litholattice::VehicleControls c; c.bits = litholattice::VehicleControls::kThrottle | litholattice::VehicleControls::kPitchUp;
    litholattice::VehicleTickContext ctx;
    for (int t = 0; t < 4; ++t) { ctx.tick = uint64_t(t); v.onTick(ctx, c, a); v.onTick(ctx, c, b); }
    check(a.speed == b.speed && a.lift == b.lift && a.fuel == b.fuel && a.speed == 1024 && a.fuel == 6,
          "Vehicle: thrust/speed/lift/fuel deterministic from controls");
    check(g.glideRatio(a) == 2560, "Glider: glide ratio hook");
    v.onDismount(a); check(a.riderId == 0, "Vehicle: dismount clears rider");
  }
  std::string err;

  // ---------- B6: Etchor block + role + behaviour seam -------------------
  {
    const cBlockReg& reg = blockRegistry();
    check(reg.def(kEtchor).id == "lithokraft:etchor", "B6: Etchor block registered as lithokraft:etchor");
    check(uint32_t(kEtchor) == uint32_t(kCertificateTable) + 1,
          "B6: Etchor id appends directly after the certificate table (append-only, no renumber)");
    check(roleOf(kEtchor) == SimRole::Etchor, "B6: Etchor carries SimRole::Etchor");
    check(cBehaviorCatalog::get().byName("etchor") != nullptr, "B6: 'etchor' behaviour registered in the catalog");
    check(cBehaviorCatalog::get().describe(kEtchor, 0) == "Etchor", "B6: Etchor readout resolves through its behaviour");
    check(etchInputItemId() == uint32_t(kWaferSilicon), "B6: etch input is the Wafer Silicon blank");
    check(etchedChipItemId() == uint32_t(kChipor), "B6: etch output is the Chipor item (block-backed id)");
  }

  // ---------- B6: the etch flow ------------------------------------------
  {
    Inventory inv;
    inv.addItem(etchInputItemId(), 2);
    check(etchChip(inv, "Alpha", &err), "B6: etch with a wafer + name succeeds");
    check(inv.countOf(etchInputItemId()) == 1, "B6: etch consumed exactly one wafer");
    const int a = findSlot(inv, [](const ItemStack& s) { return s.customName == "Alpha"; });
    check(a >= 0 && inv.slotAt(a).itemId == etchedChipItemId() && inv.slotAt(a).count == 1,
          "B6: output is one Chipor stack bearing the name-field value");
    check(a >= 0 && stackDisplayName(inv.slotAt(a)) == "Alpha",
          "B6: the name field overwrites the chip's display name");
    check(etchChip(inv, "Alpha", &err), "B6: second identical etch succeeds");
    check(a >= 0 && inv.slotAt(a).count == 2, "B6: identical named chips stack together (identity stacking)");
    check(!etchChip(inv, "Beta", &err) && inv.countOf(etchedChipItemId()) == 2,
          "B6: etch with no wafer left fails and changes nothing");

    Inventory inv2;
    inv2.addItem(etchedChipItemId(), 3);  // plain chips
    inv2.addItem(etchInputItemId(), 1);
    check(etchChip(inv2, "Gamma", &err), "B6: etch beside plain chips succeeds");
    const int g = findSlot(inv2, [](const ItemStack& s) { return s.customName == "Gamma"; });
    const int p = findSlot(inv2, [](const ItemStack& s) {
      return s.itemId == etchedChipItemId() && !s.hasData();
    });
    check(g >= 0 && inv2.slotAt(g).count == 1 && p >= 0 && inv2.slotAt(p).count == 3,
          "B6: named output never merges into the plain chip stack");
    inv2.addItem(etchedChipItemId(), 1);
    check(inv2.slotAt(p).count == 4 && inv2.slotAt(g).count == 1,
          "B6: plain chip adds never merge into a named stack");

    Inventory inv3;
    inv3.addItem(etchInputItemId(), 1);
    check(etchChip(inv3, "", &err), "B6: empty name field etches a plain chip");
    check(findSlot(inv3, [](const ItemStack& s) {
            return s.itemId == etchedChipItemId() && !s.hasData();
          }) >= 0,
          "B6: empty-name output carries no stack data");

    Inventory inv4;
    inv4.addItem(etchInputItemId(), 1);
    check(!etchChip(inv4, std::string(kEtchNameMaxBytes + 1, 'x'), &err) &&
              inv4.countOf(etchInputItemId()) == 1,
          "B6: over-cap name refused (not truncated), wafer untouched");
    check(!etchChip(inv4, "bad\nname", &err) && inv4.countOf(etchInputItemId()) == 1,
          "B6: control characters in the name field refused");
  }

  // ---------- B7: the static curve (PROPOSED values, pinned from the doc) --
  {
    check(staticLevelCost(1) == 8 && staticLevelCost(4) == 8 && staticLevelCost(5) == 16 &&
              staticLevelCost(32) == 1024,
          "B7: curve bands are powers of two doubling every 4 levels, base 8");
    check(staticTotalForLevel(0) == 0 && staticTotalForLevel(16) == 480 && staticTotalForLevel(32) == 8160,
          "B7: curve totals match the proposal (level 16 = 480, level 32 = 8160)");
    check(staticLevelForTotal(0) == 0 && staticLevelForTotal(479) == 15 && staticLevelForTotal(480) == 16,
          "B7: level derives from total static");
    check(staticSpendLevels(480, 3) == 288, "B7: spending 3 levels from level 16 lands on the level-13 floor");
    check(staticSpendLevels(100, 99) == -1, "B7: overspend is refused");
    const CertificateDef* clarity = findCertificate("certificate_of_clarity");
    check(clarity && std::string(clarity->name) == "Certificate of Clarity",
          "B7: Certificate of Clarity is registered (DECIDED design)");
    check(findCertificate("certificate_of_knowledge") != nullptr,
          "B7: Certificate of Knowledge is registered (DECIDED 2026-10-13)");
    check(findCertificate("certificate_of_nonsense") == nullptr, "B7: unknown certificate id resolves to nothing");
  }

  // ---------- B7: the signing flow ----------------------------------------
  {
    Inventory inv;
    inv.addItem(etchedChipItemId(), 3);  // flat slot 0
    int32_t total = int32_t(staticTotalForLevel(10));  // exactly level 10 (160)
    check(signStack(inv, 0, "certificate_of_efficiency", total, &err), "B7: signing a stack succeeds");
    check(total == 96, "B7: signing spent 2 levels (level 10 -> 8 floor, enchanting-style)");
    check(inv.slotAt(0).count == 2 && !inv.slotAt(0).hasData(), "B7: the stack splits -- 2 unsigned remain");
    const int s = findSlot(inv, [](const ItemStack& st) { return !st.certificates.empty(); });
    check(s > 0 && inv.slotAt(s).count == 1 &&
              inv.slotAt(s).certificates == std::vector<std::string>{"certificate_of_efficiency"},
          "B7: the signed item is its own stack carrying the certificate id");
    check(s > 0 && !signStack(inv, s, "certificate_of_efficiency", total, &err) && total == 96,
          "B7: duplicate certificate on one item refused, static untouched");
    check(s > 0 && signStack(inv, 0, "certificate_of_knowledge", total, &err) && total == 24,
          "B7: second signing (5 levels from level 8) lands on the level-3 floor");
    int32_t poor = 10;  // level 1
    check(!signStack(inv, 0, "certificate_of_clarity", poor, &err) && poor == 10,
          "B7: insufficient level refused, static untouched");
    check(!signStack(inv, 0, "certificate_of_nonsense", total, &err), "B7: unknown certificate refused");
    check(!signStack(inv, 40, "certificate_of_efficiency", total, &err), "B7: empty slot refused");
  }

  // ---------- player.lat v5: stack data persists; v4 still loads ----------
  {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() /
        ("lithokraft_wiring_selftest_" + std::to_string(std::time(nullptr)));
    fs::remove_all(dir);
    fs::create_directories(dir);
    cWorldSave save(dir);
    PlayerMeta m;
    m.dimension = "wafer";
    ItemStack named;
    named.itemId = etchedChipItemId();
    named.count = 1;
    named.customName = "Alpha";
    named.certificates = {"certificate_of_clarity"};
    m.inventory.slotAt(0) = named;
    ItemStack plain;
    plain.itemId = uint32_t(kStone);
    plain.count = 7;
    m.inventory.slotAt(5) = plain;
    check(save.writePlayer(m, &err), "B6/B7: player.lat v5 writes");
    const auto back = save.readPlayer(&err);
    check(back && back->fileVersion == PlayerMeta::kVersion, "B6/B7: written file reads back at the current version");
    check(back && back->inventory.slotAt(0).customName == "Alpha" &&
              back->inventory.slotAt(0).certificates == std::vector<std::string>{"certificate_of_clarity"},
          "B6/B7: name field + certificate list survive the save round trip");
    check(back && back->inventory.slotAt(5).itemId == uint32_t(kStone) &&
              back->inventory.slotAt(5).count == 7 && !back->inventory.slotAt(5).hasData(),
          "B6/B7: plain stacks round-trip with no data attached");

    // A hand-built v4 player.lat (the pre-v5 layout, no stack-data
    // block) must still load, with plain stacks -- world permanence.
    storage::ByteWriter body;
    body.u16(4);
    auto f32 = [&body](float v) { uint32_t u; std::memcpy(&u, &v, 4); body.u32(u); };
    f32(0.f); f32(0.f); f32(0.f); f32(0.f); f32(0.f);
    const std::string dimName = "wafer";
    body.bytes(storage::Bytes(dimName.begin(), dimName.end()));
    body.u8(0);                       // hot
    body.u8(0);                       // hotbar count
    body.u16(0);                      // stats
    body.u16(0);                      // vitals
    body.u16(1);                      // one inventory slot
    const std::string chipSid = "lithokraft:chipor";
    body.bytes(storage::Bytes(chipSid.begin(), chipSid.end()));
    body.u16(2);
    storage::ByteWriter file;
    file.u32(0x4D504C4C);  // "LLPM" (kPlayerMagic in world_save.cpp)
    file.u32(storage::crc32(body.out.data(), body.out.size()));
    file.bytes(body.out);
    std::string werr;
    check(storage::writeFileAtomic(dir / "player.lat", file.out, &werr),
          "B6/B7: hand-built v4 player.lat writes");
    const auto back4 = save.readPlayer(&err);
    check(back4 && back4->fileVersion == 4 && back4->hasInventory &&
              back4->inventory.slotAt(0).itemId == etchedChipItemId() &&
              back4->inventory.slotAt(0).count == 2 && !back4->inventory.slotAt(0).hasData(),
          "B6/B7: a v4 save still loads after the v5 bump (stacks plain, nothing lost)");
    fs::remove_all(dir);
  }

  // ---- Cortana 2026-10-16: world clock + mealtime prayer + player.lat v7 ----
  {
    namespace wc = worldclock;
    check(wc::kUnitsPerDay == 983040 && wc::kTicksPerDayAtDefaultTps == 61440,
          "world clock: a day is 32 real minutes (61,440 ticks at 32 TPS)");
    {
      bool allRates = true;  // every TPS on the ladder gives exactly 32 real minutes per day
      for (int tps = 32; tps <= 512; tps *= 2) {
        wc::Clock c;
        wc::advance(c, uint64_t(tps) * wc::kRealSecondsPerDay, tps);
        allRates = allRates && c.time == wc::kUnitsPerDay && wc::dayOf(c.time) == 1 && wc::hhmm(c.time) == "00:00";
      }
      check(allRates, "world clock: 32 real minutes per day at every TPS on the 32..512 ladder");
      wc::Clock c;
      wc::advance(c, 9 * 2560, 32);  // 2,560 ticks per in-game hour at 32 TPS
      check(wc::hhmm(c.time) == "09:00" && prayer::windowAt(c.time) == 0, "world clock: 24 h clock reads 09:00 after 9 game hours");
      check(wc::fromLegacyPlayTicks(64).time == 64 * 16, "world clock: a v6 play clock migrates at 32 TPS");
    }
    const uint64_t perTick = wc::unitsPerTick(32);
    auto at = [&](uint32_t day, uint32_t minute) {  // world time at the start of an in-game minute
      return uint64_t(day) * wc::kUnitsPerDay + (uint64_t(minute) * wc::kUnitsPerDay + wc::kGameMinutesPerDay - 1) / wc::kGameMinutesPerDay;
    };
    check(prayer::windowAt(at(0, 9 * 60)) == 0 && prayer::windowAt(at(0, 9 * 60 + 31)) == 0 &&
              prayer::windowAt(at(0, 9 * 60 + 32)) == -1 && prayer::windowAt(at(0, 13 * 60)) == 1 &&
              prayer::windowAt(at(0, 17 * 60 + 31)) == 2 && prayer::windowAt(at(0, 8 * 60 + 59)) == -1 &&
              prayer::windowAt(at(0, 12 * 60)) == -1,
          "prayer: windows are 09:00-09:32, 13:00-13:32, 17:00-17:32");
    // Kneel at 08:00: nothing, however long.
    prayer::State s;
    int32_t got = 0;
    uint64_t t = at(0, 8 * 60);
    for (int i = 0; i < 500; ++i) got += prayer::tick(s, true, 0, t += perTick);
    check(got == 0, "prayer: a kneel outside the mealtime windows grants nothing");
    prayer::tick(s, false, 0, t);
    t = at(0, 9 * 60);
    for (int i = 0; i < 64; ++i) got += prayer::tick(s, true, 0, t += perTick);
    check(got == 0, "prayer: 64 in-window kneel ticks grant nothing (needs > 64)");
    got += prayer::tick(s, true, 0, t += perTick);
    check(got == 1 && s.prayersToday() == 1, "prayer: the 65th in-window tick grants max(1, 2% of 8) = 1 static at level 0");
    for (int i = 0; i < 500; ++i) got += prayer::tick(s, true, 0, t += perTick);
    prayer::tick(s, false, 0, t);
    for (int i = 0; i < 100; ++i) got += prayer::tick(s, true, 0, t += perTick);
    check(got == 1, "prayer: one prayer per window (a second kneel in the same window pays nothing)");
    {  // 2026-10-16: a clock rewound to an earlier day keeps that day spent (no re-pray exploit)
      prayer::State r;
      r.prayerDay = 5;
      int32_t gr = 0;
      uint64_t tr = at(4, 9 * 60);
      for (int i = 0; i < 100; ++i) gr += prayer::tick(r, true, 0, tr += perTick);
      check(gr == 0 && r.prayerDay == 4, "prayer: a clock rewound a day does not reopen the windows");
      prayer::tick(r, false, 0, tr);
      tr = at(5, 9 * 60);
      for (int i = 0; i < 100; ++i) gr += prayer::tick(r, true, 0, tr += perTick);
      check(gr == 1, "prayer: ...and the next real day pays again");
    }
    // A kneel begun at 08:59 only counts from 09:00 in a fresh day's breakfast window.
    prayer::State e;
    int32_t ge = 0;
    uint64_t te = at(1, 9 * 60) - 31 * perTick;
    for (int i = 0; i < 30 + 64; ++i) ge += prayer::tick(e, true, 0, te += perTick);
    check(ge == 0, "prayer: kneel ticks before 09:00 do not count toward the window");
    ge += prayer::tick(e, true, 0, te += perTick);
    check(ge == 1, "prayer: ...and the 65th tick inside the window pays");
    prayer::tick(s, false, 0, t);
    t = at(0, 13 * 60);
    for (int i = 0; i < 70; ++i) got += prayer::tick(s, true, 0, t += perTick);
    prayer::tick(s, false, 0, t);
    t = at(0, 17 * 60 + 10);
    for (int i = 0; i < 70; ++i) got += prayer::tick(s, true, 0, t += perTick);
    check(got == 3 && s.prayersToday() == 3, "prayer: breakfast + lunch + dinner = 3 per day max");
    prayer::tick(s, false, 0, t);
    t = at(1, 9 * 60 + 5);
    for (int i = 0; i < 70; ++i) got += prayer::tick(s, true, 0, t += perTick);
    check(got == 4 && s.prayerDay == 1 && s.prayersToday() == 1, "prayer: a new world day resets the windows");
    check(prayer::reward(staticTotalForLevel(20)) == std::max<int64_t>(1, (staticTotalForLevel(21) - staticTotalForLevel(20)) * 2 / 100),
          "prayer: reward = 2% of the next level's cost");
    prayer::State a, b;
    uint32_t seq = 12345;
    int32_t ga = 0, gb = 0;
    uint64_t ta = 0;
    for (int i = 0; i < 200000; ++i) {
      seq = seq * 1103515245u + 12345u;
      const bool kn = ((seq >> 16) & 255) < 250;
      ta += perTick;
      ga += prayer::tick(a, kn, ga, ta); gb += prayer::tick(b, kn, gb, ta);
    }
    check(ga == gb && ga > 0 && a.windows == b.windows && a.prayerDay == b.prayerDay, "prayer: deterministic for one kneel/clock sequence");

    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / ("lithokraft_prayer_selftest_" + std::to_string(std::time(nullptr)));
    fs::remove_all(dir);
    fs::create_directories(dir);
    cWorldSave save(dir);
    PlayerMeta m;
    m.dimension = "wafer";
    m.prayerDay = 7;
    m.prayerWindows = 5;
    check(save.writePlayer(m, &err), "player.lat v7 writes");
    check(fs::exists(dir / "player.lat") && !fs::exists(dir / "player.dat"), "save: the player file is player.lat");
    const auto back = save.readPlayer(&err);
    check(back && back->fileVersion == 7 && back->prayerDay == 7 && back->prayerWindows == 5 && back->prayersToday == 2,
          "player.lat v7: mealtime prayer block round-trips");
    // Hand-built legacy files: a v6 player.dat (play clock) and a v5 player.dat, in a world that
    // has never written player.lat - both still load.
    auto handBuilt = [](uint16_t ver, bool v6block) {
      storage::ByteWriter body;
      body.u16(ver);
      auto f32 = [&body](float v) { uint32_t u; std::memcpy(&u, &v, 4); body.u32(u); };
      f32(0.f); f32(0.f); f32(0.f); f32(0.f); f32(0.f);
      const std::string dimName = "wafer";
      body.bytes(storage::Bytes(dimName.begin(), dimName.end()));
      body.u8(0); body.u8(0); body.u16(0); body.u16(0); body.u16(0); body.u16(0);
      if (v6block) { body.u32(0x42); body.u32(1); body.u32(3); body.u8(2); }
      storage::ByteWriter file;
      file.u32(0x4D504C4C);
      file.u32(storage::crc32(body.out.data(), body.out.size()));
      file.bytes(body.out);
      return file.out;
    };
    const fs::path ldir = dir / "legacy";
    fs::create_directories(ldir);
    cWorldSave lsave(ldir);
    std::string werr;
    check(storage::writeFileAtomic(ldir / "player.dat", handBuilt(6, true), &werr), "hand-built v6 player.dat writes");
    const auto back6 = lsave.readPlayer(&err);
    check(back6 && back6->fileVersion == 6 && back6->playTicks == 0x1'0000'0042ull && back6->prayerDay == 3,
          "save: a legacy v6 player.dat loads (play clock kept for migration)");
    fs::remove_all(ldir);
    fs::create_directories(ldir);
    check(storage::writeFileAtomic(ldir / "player.dat", handBuilt(5, false), &werr), "hand-built v5 player.dat writes");
    const auto back5 = lsave.readPlayer(&err);
    check(back5 && back5->fileVersion == 5 && back5->playTicks == 0 && back5->prayersToday == 0,
          "save: a legacy v5 player.dat loads (no prayer state)");
    check(lsave.writePlayer(*back5, &err) && fs::exists(ldir / "player.lat"), "save: the next save migrates to player.lat");
    const auto again = lsave.readPlayer(&err);
    check(again && again->fileVersion == 7, "save: player.lat wins over the leftover player.dat");
    // World clock in level.json (v2): round-trips; a v1 file has none.
    lsave.setSeed(99);
    check(lsave.writeWorldMeta(&err), "level.json v1 (no clock) writes");
    std::string cerr;
    check(!cWorldSave::readWorldClock(ldir, &cerr) && cerr.empty(), "world clock: a level.json without a clock reads as absent");
    lsave.setWorldClock(0x2'0000'0001ull, 123456789ull);
    check(lsave.writeWorldMeta(&err), "level.json v2 writes");
    const auto clk = cWorldSave::readWorldClock(ldir, &cerr);
    check(clk && clk->tick == 0x2'0000'0001ull && clk->time == 123456789ull, "world clock: worldTick + worldTime round-trip in level.json");
    fs::remove_all(dir);
  }
  // ---- B4: procgen plant blocks + live worldgen binding ----
  {
    namespace pg = litholattice::procgen;
    const BlockId plantIds[7] = {kProcgenGrassTuft, kProcgenFern, kProcgenDryScrub,
                                 kProcgenFormulaFlower, kProcgenFormulaStar,
                                 kProcgenFormulaLeaf, kProcgenFormulaBlob};
    bool reg = true;
    for (int i = 0; i < 7; ++i) {
      const BlockDef& d = blockDef(plantIds[i]);
      reg = reg && plantIds[i] == kEtchor + 1 + i && d.shape == litholattice::Shape::Cross &&
            std::string(d.tex) == pg::plantTextureNameAny(i);
    }
    check(reg, "B4: seven procgen plant blocks registered (Cross, texture-index "
            "order, ids 58-64 after Etchor)");
    // Formula selection per biome_vegetation.h's rows (super* params copied
    // from the table): DicingPlains (4, .6/.6/.6, 1.8) -> leaf index 5;
    // ResonatorSpires (3, 1.5/1.5/1.5, 1) -> blob index 6. (The vegetation
    // selftest pins shrubParamsForRow itself; this pins the texture choice
    // the groundcover binding relies on.)
    pg::ShrubParams dicingP;
    dicingP.superM = 4.0; dicingP.superN1 = 0.6; dicingP.superN2 = 0.6;
    dicingP.superN3 = 0.6; dicingP.superA = 1.8; dicingP.superB = 1.0;
    pg::ShrubParams spiresP;
    spiresP.superM = 3.0; spiresP.superN1 = 1.5; spiresP.superN2 = 1.5;
    spiresP.superN3 = 1.5; spiresP.superA = 1.0; spiresP.superB = 1.0;
    check(pg::formulaTextureIndexFor(dicingP) == 5 &&
           pg::formulaTextureIndexFor(spiresP) == 6,
       "B4: biome formula rows resolve to leaf (5) / blob (6) plant textures");
    // Placement proof on the shipped wafer.json through the real dimension
    // registry: leaf appears in DicingPlains columns, blob in
    // ResonatorSpires columns, silica_grass no longer appears as worldgen
    // groundcover, and regeneration is cell-identical.
    auto dims = createDimensions();
    const Dimension* wafer = dims.find("wafer");
    check(wafer && wafer->generator,
       "B4: wafer generator loads (groundcover materials resolve fail-closed)");
    if (wafer && wafer->generator) {
      const litholattice::StableId sidLeaf = blockRegistry().stableIdOf(kProcgenFormulaLeaf);
      const litholattice::StableId sidBlob = blockRegistry().stableIdOf(kProcgenFormulaBlob);
      const litholattice::StableId sidSilica = blockRegistry().stableIdOf(kSilicaGrass);
      long leafInDicing = 0, blobInSpires = 0, silicaAnywhere = 0;
      bool sawDicing = false, sawSpires = false;
      for (int cx = -6; cx < 6 && !(leafInDicing > 0 && blobInSpires > 0); ++cx) {
        for (int cz = -6; cz < 6 && !(leafInDicing > 0 && blobInSpires > 0); ++cz) {
          pg::ColumnMaps maps;
          auto col = wafer->generator->generate(cx, cz, &maps);
          if (!col || maps.biome.size() != 256) continue;
          for (int lz = 0; lz < 16; ++lz) {
            for (int lx = 0; lx < 16; ++lx) {
              const uint8_t biome = maps.biome[lz * 16 + lx];
              if (biome == 1) sawDicing = true;
              if (biome == 3) sawSpires = true;
              for (size_t sy = 0; sy < col->subs.size(); ++sy) {
                const auto& sub = col->subs[sy];
                if (!sub) continue;
                for (int ly = 0; ly < 16; ++ly) {
                  const litholattice::StableId b = sub->blocks.at(
                      size_t(litholattice::SubChunk::index(lx, ly, lz)));
                  if (b == sidLeaf && biome == 1) ++leafInDicing;
                  if (b == sidBlob && biome == 3) ++blobInSpires;
                  if (b == sidSilica) ++silicaAnywhere;
                }
              }
            }
          }
        }
      }
      check(sawDicing && sawSpires,
         "B4: scan window covers DicingPlains and ResonatorSpires");
      check(leafInDicing > 0, "B4: formula leaf shrubs generate in DicingPlains");
      check(blobInSpires > 0, "B4: formula blob shrubs generate in ResonatorSpires");
      check(silicaAnywhere == 0,
         "B4: worldgen no longer places silica_grass as groundcover");
      pg::ColumnMaps m1, m2;
      auto c1 = wafer->generator->generate(2, 2, &m1);
      auto c2 = wafer->generator->generate(2, 2, &m2);
      bool same = c1 && c2 && c1->subs.size() == c2->subs.size();
      for (size_t s = 0; same && s < c1->subs.size(); ++s) {
        if (!c1->subs[s] || !c2->subs[s]) { same = !c1->subs[s] && !c2->subs[s]; continue; }
        for (int i = 0; i < litholattice::kSubChunkVolume; ++i)
          if (c1->subs[s]->blocks.at(i) != c2->subs[s]->blocks.at(i)) { same = false; break; }
      }
      check(same, "B4: regenerated chunk is cell-identical (deterministic)");
    }
  }

  {  // Cortana 2026-10-15 1900: lithography machine (mask-and-expose) on the real signal sim.
    ChunkMap w;
    w.setRegistry(&blockRegistry());
    for (int cz = 0; cz < 4; ++cz)
      for (int cx = 0; cx < 6; ++cx) {
        auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz;
        w.insert(std::move(col));
      }
    buildLithoMachine(w, 24, 10, 24, true);   // wired reference copy
    buildLithoMachine(w, 64, 10, 24, false);  // unwired copy
    cSignalSim sim;
    for (int cz = 0; cz < 4; ++cz)
      for (int cx = 0; cx < 6; ++cx) sim.scanColumn(w, cx, cz);
    for (int i = 0; i < 12; ++i) sim.tick(w, 0, 0, 100, [](int, int, int) {});
    const LithoReader rd{[&](int x, int y, int z) { return w.get(x, y, z); },
                         [&](int x, int y, int z) { return w.getState(x, y, z); },
                         [&](int x, int y, int z) { return sim.strongPowered(w, x, y, z); }};
    const LithoMachine m = scanLithoMachine(rd, 24, 14, 24);
    std::printf("  litho wired: %s\n", m.describe().c_str());
    const CellState em = w.getState(21, 16, 24);
    check(stFacing(em) == 1 && (em & 1) != 0 && stFacing(w.getState(27, 16, 24)) == 3 && (w.getState(27, 16, 24) & 1),
          "litho: powered Emitors light (Lamp role) and keep their facing");
    check(m.formed && m.laserLeft && m.laserRight, "litho: wired machine formed, both side lasers hit the top row");
    check(m.top == 0b111 && m.bottom == 0b10101 && m.code() == uint8_t(7 | (21 << 3)),
          "litho: expose bits 111 (middle fed by lit Trace), mask bits 10101 from strongly powered feeders");
    check(m.wafers == 8 && m.exitClear && m.canEtch(), "litho: 8 wafers in the back buffer, front clear, ready to etch");
    const LithoMachine u = scanLithoMachine(rd, 64, 14, 24);
    check(u.formed && !u.laserLeft && !u.laserRight && u.top == 0 && u.bottom == 0 && !u.exposed() && !u.canEtch(),
          "litho: unwired machine formed but dark (no laser, no fed Connectors) - refuses to etch");
    w.set(24, 15, 24, kAir);  // remove the blue Vior
    const LithoMachine b = scanLithoMachine(rd, 24, 14, 24);
    check(!b.formed && !b.canEtch() && b.problem.find("blue") != std::string::npos,
          "litho: a missing canon block unforms the machine with a named reason");
    w.set(24, 15, 24, kBlueVia);
    int ex = 0, ey = 0, ez = 0;
    check(findNearestEtchor(rd, 24, 12, 20, 6, ex, ey, ez) && ex == 24 && ey == 14 && ez == 24 &&
              !findNearestEtchor(rd, 44, 12, 4, 6, ex, ey, ez),
          "litho: nearest-Etchor search finds the machine within 6 blocks only");
    const CellState bs = lithoBufferWithCount(makeState(8, 2, 3, true), 99);
    check(stValue(bs) == kLithoBufferMax && stFacing(bs) == 2 && stAux(bs) == 3 && stOn(bs) &&
              stValue(lithoBufferWithCount(bs, -4)) == 0,
          "litho: back-buffer wafer count clamps 0..64 and keeps facing/aux/on");
  }

  // ---- POSTS (2026-10-25, JD): panel text in saves (PTXT payload) + save compatibility ----
  {
    namespace fs = std::filesystem;
    ChunkMap w;
    w.setRegistry(&blockRegistry());
    { auto col = std::make_unique<ChunkColumn>(); col->cx = 2; col->cz = 3; w.insert(std::move(col)); }
    const int x = 2 * 16 + 4, y = 40, z = 3 * 16 + 9;
    w.set(x, y, z, kVirtualPost); w.setState(x, y, z, withFacing8(0, 5));
    w.set(x, y + 1, z, kVirtualPost); w.setState(x, y + 1, z, withFacing8(post::kUpperBit, 5));
    std::string text = "Line one\n";
    for (int i = 0; i < 400; ++i) post::applyEdit(text, 'A' + i % 26, post::kTypeChar);
    w.setCellText(x, y + 1, z, text);
    // Codec: v1 round-trip, and an untouched column records no PTXT at all.
    storage::cPayloadReg with, without;
    with.setRegistry(&blockRegistry()); without.setRegistry(&blockRegistry());
    with.add(cWorldSave::postTextPayload());
    const ChunkColumn* src = w.column(2, 3);
    const storage::Bytes rec = with.encode(*src);
    std::string err;
    auto back = with.decode(rec, &err);
    const auto it = back ? back->cellText.find(ChunkColumn::cellTextKey(x, y + 1, z)) : decltype(back->cellText)::iterator{};
    check(back && back->cellText.size() == 1 && it != back->cellText.end() && it->second == text && text.size() == post::kMaxChars,
          "POSTS save: PTXT v1 round-trips the panel text (256 chars, new line kept)");
    ChunkColumn plain; plain.cx = 1; plain.cz = 1;
    check(with.encode(plain) == without.encode(plain),
          "POSTS save compat: a column without text encodes byte-identical to a pre-Posts build (PTXT omitted)");
    auto old = without.decode(rec, &err);
    storage::StableRemap ident;  // the save's name table == the live registry (no LLBN file here)
    for (size_t i = 0; i < blockRegistry().size(); ++i) ident.map.push_back(blockRegistry().stableIdOf(BlockId(i)));
    bool oldOk = old && old->cellText.empty() && storage::remapColumn(*old, ident, &err);
    if (oldOk) {
      ChunkMap om;
      om.setRegistry(&blockRegistry());
      om.insert(std::move(old));
        oldOk = om.get(x, y, z) == kVirtualPost && om.get(x, y + 1, z) == kVirtualPost && !om.cellText(x, y + 1, z);
    }
    check(oldOk,
          "POSTS save compat: a build without PTXT skips the unknown payload and still loads the blocks");
    // Full store path: saveColumns -> loadColumn keeps blocks, facing and text.
    const fs::path dir = fs::temp_directory_path() / ("lithokraft_posts_selftest_" + std::to_string(std::time(nullptr)));
    fs::remove_all(dir);
    fs::create_directories(dir);
    {
      cWorldSave save(dir);
      cSignalSim sig;
      check(save.saveColumns("wafer", {src}, sig, true) == 1 && save.lastSaveOk(), "POSTS save: the post column saves");
    }
    {
      cWorldSave load(dir);
      auto lc = load.loadColumn("wafer", 2, 3);
      bool ok = lc.status == cWorldSave::ColumnLoad::Loaded && lc.column;
      if (ok) {
        ChunkMap r;
        r.setRegistry(&blockRegistry());
        r.insert(std::move(lc.column));
        const std::string* t = r.cellText(x, y + 1, z);
        ok = r.get(x, y, z) == kVirtualPost && r.get(x, y + 1, z) == kVirtualPost && stFacing8(r.getState(x, y + 1, z)) == 5 &&
             post::isUpper(r.getState(x, y + 1, z)) && t && *t == text && !r.cellText(x, y, z);
      }
      check(ok, "POSTS save: save -> load keeps the pair, its 8-way facing and the panel text");
    }
    fs::remove_all(dir);
  }

  return g_fails;
}

}  // namespace lithokraft
