#include "components.h"

namespace lithokraft {

const char* facingName(int facing) {
  static constexpr const char* kNames[4] = {"N", "E", "S", "W"};
  return kNames[facing & 3];
}

const cBehaviorCatalog& cBehaviorCatalog::get() {
  static const cBehaviorCatalog catalog;
  return catalog;
}

cBehaviorCatalog::cBehaviorCatalog() {
  const components::Registrar add = [this](std::string_view name, std::unique_ptr<cBlockBehavior> b) {
    this->add(name, std::move(b));
  };
  components::registerPassive(add);
  components::registerTorch(add);  // 2026-11-05 (JD): LED colour property
  components::registerLogic(add);
  components::registerInputs(add);
  components::registerSound(add);
  components::registerCertificateTable(add);
  components::registerEtchor(add);
  components::registerPosts(add);  // 2026-10-25 Posts

  // Bind every block that names a behaviour in the block registry.
  const auto& reg = blockRegistry();
  behaviors_.assign(reg.size(), &plain_);
  components_.assign(reg.size(), nullptr);
  traits_.assign(reg.size(), cCompTraits{});
  for (size_t id = 0; id < reg.size(); ++id) {
    const std::string_view name = reg.def(BlockId(id)).behavior;
    if (name.empty()) continue;
    if (const cBlockBehavior* b = byName(name)) {
      behaviors_[id] = b;
      components_[id] = dynamic_cast<const cCompBehavior*>(b);
    }
  }
  // Archie Phase 4: snapshot constant virtuals into POD traits (once per BlockId).
  for (size_t id = 0; id < reg.size(); ++id) {
    const cCompBehavior* c = components_[id];
    if (!c) continue;
    cCompTraits t{};
    t.rate = c->tickRate();  // A-H2 validation / log stays in cSignalSim::classTickRate
    if (c->queuedOutput()) t.flags = uint8_t(t.flags | cCompTraits::kQueuedOutput);
    const int d0 = c->delayTicks(0, 0, 0);
    const int d1 = c->delayTicks(CellState(0xFFFFu), int64_t(0x1234), uint64_t(7));
    if (d0 == d1 && d0 >= 0 && d0 <= 32767) t.delayFixed = int16_t(d0);
    ComponentMemory m0{}, m1{};
    m1.out = 64;
    m1.prevIn = 1;
    const bool q0 = c->quiescent(0, m0);
    const bool q1 = c->quiescent(CellState(0xFFFFu), m1);
    if (q0 && q1) t.flags = uint8_t(t.flags | cCompTraits::kQuietAlways);
    else if (!q0 && !q1) t.flags = uint8_t(t.flags | cCompTraits::kQuietNever);
    // Archie Phase 4 fix: emitFrontOut/Chan0 only when TRUE for every probed state.
    // Catchor left/right outDir and Redirector split (out + out2) must NOT get kEmitFrontOut.
    m0.out = 42;
    m0.out2 = 17;
    // Must include split Redirector with blueFront != facing, else kEmitFrontOut
    // wrongly sticks and out2 on the blue lane is never seeded (catchor->split selftest).
    static const CellState kEmitProbe[] = {
        CellState(0),                                    // facing N, aux 0 (Catchor centre)
        CellState((1u << 7) | (1u << 9)),                 // facing E, aux 1 (Catchor left)
        CellState((2u << 7) | (2u << 9)),                 // facing S, aux 2 (Catchor right)
        CellState((3u << 7) | (3u << 9)),                 // facing W, aux 3 (Catchor unwired)
        CellState((0u << 7) | (4u << 9)),                 // facing N, aux 4 = split blueFront N (==facing)
        CellState((0u << 7) | ((4u | 1u) << 9)),          // split: facing N, blueFront E (!=)
        CellState((1u << 7) | ((4u | 3u) << 9)),          // split: facing E, blueFront W (!=)
        CellState((2u << 7) | ((4u | 0u) << 9)),          // split: facing S, blueFront N (!=)
        CellState((1u << 7) | (5u << 9) | (1u << 13)),    // split-ish + red-off bit
        CellState((2u << 7) | (6u << 9) | (1u << 14)),    // blue-off bit
        CellState(0xFFFFu),
    };
    bool frontOut = true, chan0 = true;
    for (CellState st : kEmitProbe) {
      for (int dir = 0; dir < 4; ++dir) {
        const int got = c->emitToward(dir, st, m0);
        const int want = (dir == stFacing(st)) ? 42 : 0;
        if (got != want) frontOut = false;
        if (c->emitChannel(dir, st, m0) != 0) chan0 = false;
      }
    }
    if (frontOut) t.flags = uint8_t(t.flags | cCompTraits::kEmitFrontOut);
    if (chan0) t.flags = uint8_t(t.flags | cCompTraits::kEmitChan0);
    traits_[id] = t;
  }
}

void cBehaviorCatalog::add(std::string_view name, std::unique_ptr<cBlockBehavior> behavior) {
  named_.emplace_back(name, std::move(behavior));
}

const cBlockBehavior& cBehaviorCatalog::behavior(BlockId b) const {
  return b < behaviors_.size() ? *behaviors_[b] : plain_;
}

const cBlockBehavior* cBehaviorCatalog::byName(std::string_view name) const {
  for (const auto& [n, b] : named_)
    if (n == name) return b.get();
  return nullptr;
}

}  // namespace lithokraft
