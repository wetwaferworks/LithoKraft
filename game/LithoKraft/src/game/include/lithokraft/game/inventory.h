// LithoKraft inventory: 50 carry slots + 8 gear slots (58 flat slots).
// 50 = 16 hotbar + 2 offhands (L/R) + 32 main (C9 2026-11-12, JD 6:17 AM ET: four rows of EIGHT;
// B9 had 4 rows of 16 = 64). The original design was 42 = 8 + 2 + 32 (101010 in binary).
// C9's flat map is A9's 58-slot map: hotbar 0-15, hands 16-17, main 18-49, gear 50-57.
// Gear (worn, not counted in the carry slots): head, chest, legs, feet, arms, face, heart, mind.
// Max stack: 512 (= tick rate).
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "lithokraft/content/items.h"  // startingItemDef (the Fragment)

namespace lithokraft {

// Cortana 2026-10-15 1510 (GAME, Herominde H-03): per-item stack cap = min(ItemDef::maxStack, kMaxStack);
// ids with no ItemDef (block-backed stacks) keep the global kMaxStack.
inline uint16_t stackCapFor(uint32_t itemId);

constexpr int kMaxStack = 512;       ///< max items per stack (= tick rate)
constexpr int kInvHotbarSlots = 16;  // 2026-10-24 (JD): was 8 (saves with the 8-slot layout are remapped on load)
constexpr int kLegacyInvHotbarSlots = 8;
constexpr int kMainRows = 4;
constexpr int kMainCols = 8;  // C9 2026-11-12 (JD): four rows of EIGHT = 32 (B9 had 16 columns = 64)
constexpr int kMainSlots = kMainRows * kMainCols;  // 32 (64 in B9)
constexpr int kMainFlatBase = kInvHotbarSlots + 2;  // 18: flat index of main slot 0
constexpr int kCarrySlots = kInvHotbarSlots + 2 + kMainSlots;  // 50 (82 in B9, 42 before the 16-slot hotbar)
constexpr int kGearSlots = 8;
constexpr int kTotalSlots = kCarrySlots + kGearSlots;  // 58: every slot, gear included (90 in B9)
/// Saved inventory layouts (player.lat v4+ writes kTotalSlots entries in the canonical flat
/// order, so the block's slot count names the layout it was written with):
///   50 = 8-slot hotbar (saves before 2026-10-24): 8 + 2 + 32 main + 8 gear
///   58 = 16-slot hotbar, 4x8 main (2026-10-24 .. A9, and C9 again): 16 + 2 + 32 main + 8 gear
///   90 = B9 four-row (4x16): 16 + 2 + 64 main + 8 gear (gear at 82..89)
constexpr int kLegacyMainSlots = 32;
constexpr int kLegacyTotalSlots = kLegacyInvHotbarSlots + 2 + kLegacyMainSlots + kGearSlots;  // 50
constexpr int kPreFourRowTotalSlots = kInvHotbarSlots + 2 + kLegacyMainSlots + kGearSlots;   // 58 (= C9's own layout)
constexpr int kB9MainSlots = 64;
constexpr int kB9TotalSlots = kInvHotbarSlots + 2 + kB9MainSlots + kGearSlots;               // 90
static_assert(kTotalSlots == kPreFourRowTotalSlots, "C9: the live layout is A9's 58-slot map again");
/// savedFlatToCurrent result for a B9 main slot past this build's 32 (B9 main 32..63): the loader
/// spills those stacks into free C9 slots (world_save.cpp), never silently drops them.
constexpr int kSavedOverflow = -2;
/// Maps flat index `i` of an inventory block written with `writtenSlots` entries into this
/// build's flat order, section by section (hotbar, hands, main, gear keep their own section and
/// position; gear moves to this build's gear block). 58 (this layout) and any other length are
/// taken as written. B9 (90): main 0..31 keep their place, main 32..63 -> kSavedOverflow.
constexpr int savedFlatToCurrent(int writtenSlots, int i) {
  int hot = 0, mainN = 0;
  if (writtenSlots == kLegacyTotalSlots) { hot = kLegacyInvHotbarSlots; mainN = kLegacyMainSlots; }
  else if (writtenSlots == kB9TotalSlots) { hot = kInvHotbarSlots; mainN = kB9MainSlots; }
  else return i;
  if (i < 0) return i;
  if (i < hot) return i;
  if (i < hot + 2) return kInvHotbarSlots + (i - hot);
  if (i < hot + 2 + mainN) return (i - hot - 2) < kMainSlots ? kMainFlatBase + (i - hot - 2) : kSavedOverflow;
  return kCarrySlots + (i - hot - 2 - mainN);
}
/// The 8-slot-hotbar remap (2026-10-24), kept for callers; now section-aware via savedFlatToCurrent.
constexpr int legacyFlatToCurrent(int i) { return savedFlatToCurrent(kLegacyTotalSlots, i); }
static_assert(savedFlatToCurrent(kPreFourRowTotalSlots, 50) == kCarrySlots, "C9: an A9 58-slot save is this layout (gear 50)");
static_assert(savedFlatToCurrent(kLegacyTotalSlots, 49) == kTotalSlots - 1, "an 8-hotbar save's last gear slot lands on this build's last gear slot");
static_assert(savedFlatToCurrent(kPreFourRowTotalSlots, 49) == kMainFlatBase + 31, "a 58-slot save's main slots keep their positions");
static_assert(savedFlatToCurrent(kB9TotalSlots, 82) == kCarrySlots && savedFlatToCurrent(kB9TotalSlots, 89) == kTotalSlots - 1,
              "C9: a B9 90-slot save's gear (82..89) lands on 50..57");
static_assert(savedFlatToCurrent(kB9TotalSlots, 49) == kMainFlatBase + 31 && savedFlatToCurrent(kB9TotalSlots, 50) == kSavedOverflow &&
              savedFlatToCurrent(kB9TotalSlots, 81) == kSavedOverflow && savedFlatToCurrent(kB9TotalSlots, 17) == 17,
              "C9: a B9 save keeps hotbar / hands / main 0..31; main 32..63 overflow");

/// Gear slot indices.
enum GearSlot : uint8_t {
  kGearHead = 0, kGearChest, kGearLegs, kGearFeet,
  kGearArms, kGearFace, kGearHeart, kGearMind,
};

/// A stack of items. itemId 0 = empty.
///
/// Per-stack data (wiring pass 2026-10-13, B6/B7): a stack may carry a
/// custom name (the Etchor name field: "the typed name becomes the output
/// chip's name") and applied certificate ids (certificate-table signing).
/// Data-bearing stacks are INDIVIDUALS: plain addItem() never merges into
/// them and they only stack with stacks whose data is identical. The data
/// persists in player.lat's v5 stack-data block (game/world_save.cpp);
/// in the game's tagged binary storage the name is simply the stack's
/// "name" field, per JD's LBT field design.
struct ItemStack {
  uint32_t itemId = 0;  ///< BlockId for blocks; item ids for non-blocks
  uint16_t count = 0;   ///< 1..kMaxStack
  std::string customName;                  ///< Etchor name field value; empty = the def name
  std::vector<std::string> certificates;   ///< applied certificate ids (content/certificates.h)

  bool empty() const { return itemId == 0 || count == 0; }
  bool hasData() const { return !customName.empty() || !certificates.empty(); }
  /// Identity for stacking: same item AND same per-stack data.
  bool sameIdentity(const ItemStack& o) const {
    return itemId == o.itemId && customName == o.customName && certificates == o.certificates;
  }
  void clear() { itemId = 0; count = 0; customName.clear(); certificates.clear(); }
};

class Inventory {
 public:
  uint64_t version() const { return version_; }  ///< increments on every mutation (UI polls this)
  /// Adds items, stacking with existing and filling empty slots. Returns leftover count.
  uint16_t addItem(uint32_t itemId, uint16_t count) {
    uint16_t left = count;
    // 1. Stack with existing (hotbar first, then main). Plain adds never
    // merge into a data-bearing stack (a named/certified stack is an
    // individual; merging would silently strip its identity).
    for (auto* s : carrySlots()) {
      if (left == 0) break;
      if (s->itemId == itemId && !s->hasData() && s->count < stackCapFor(itemId)) {  // H-03
        const uint16_t space = uint16_t(stackCapFor(itemId) - s->count);
        const uint16_t take = left < space ? left : space;
        s->count += take;
        left -= take;
      }
    }
    // 2. Fill empty slots.
    for (auto* s : carrySlots()) {
      if (left == 0) break;
      if (s->empty()) {
        s->itemId = itemId;
        const uint16_t cap = stackCapFor(itemId);  // H-03
        s->count = left < cap ? left : cap;
        left -= s->count;
      }
    }
    if (left != count) ++version_;
    return left;
  }

  /// Removes up to `count` of itemId. Returns how many were actually removed.
  uint16_t removeItem(uint32_t itemId, uint16_t count) {
    uint16_t removed = 0;
    for (auto* s : carrySlots()) {
      if (removed >= count) break;
      if (s->itemId == itemId) {
        const uint16_t take = s->count < (count - removed) ? s->count : (count - removed);
        s->count -= take;
        removed += take;
        if (s->count == 0) s->clear();
      }
    }
    if (removed > 0) ++version_;
    return removed;
  }

  /// Total count of itemId across all carry slots.
  uint16_t countOf(uint32_t itemId) const {
    uint16_t total = 0;
    for (const auto& s : hotbar_) if (s.itemId == itemId) total += s.count;
    if (offhandLeft_.itemId == itemId) total += offhandLeft_.count;
    if (offhandRight_.itemId == itemId) total += offhandRight_.count;
    for (const auto& s : main_) if (s.itemId == itemId) total += s.count;
    return total;
  }

  // Accessors.
  std::array<ItemStack, kInvHotbarSlots>& hotbar() { return hotbar_; }
  const std::array<ItemStack, kInvHotbarSlots>& hotbar() const { return hotbar_; }
  ItemStack& offhandLeft() { return offhandLeft_; }
  const ItemStack& offhandLeft() const { return offhandLeft_; }
  ItemStack& offhandRight() { return offhandRight_; }
  const ItemStack& offhandRight() const { return offhandRight_; }
  std::array<ItemStack, kMainSlots>& main() { return main_; }
  const std::array<ItemStack, kMainSlots>& main() const { return main_; }
  std::array<ItemStack, kGearSlots>& gear() { return gear_; }
  const std::array<ItemStack, kGearSlots>& gear() const { return gear_; }

  /// Every slot in the CANONICAL FLAT ORDER (P2b-1): hotbar 0..15, offhand
  /// L, offhand R, main 0..63 (B9 four-row; was 0..31), gear 0..7 — 50 slots. This is the order the
  /// player.lat v4 inventory block is written in; never reorder it.
  std::array<ItemStack*, kTotalSlots> allSlots() {
    std::array<ItemStack*, kTotalSlots> out{};
    size_t i = 0;
    for (auto& s : hotbar_) out[i++] = &s;
    out[i++] = &offhandLeft_;
    out[i++] = &offhandRight_;
    for (auto& s : main_) out[i++] = &s;
    for (auto& s : gear_) out[i++] = &s;
    return out;
  }
  std::array<const ItemStack*, kTotalSlots> allSlots() const {
    std::array<const ItemStack*, kTotalSlots> out{};
    size_t i = 0;
    for (const auto& s : hotbar_) out[i++] = &s;
    out[i++] = &offhandLeft_;
    out[i++] = &offhandRight_;
    for (const auto& s : main_) out[i++] = &s;
    for (const auto& s : gear_) out[i++] = &s;
    return out;
  }
  ItemStack& slotAt(int flatIndex) { return *allSlots()[size_t(flatIndex)]; }
  const ItemStack& slotAt(int flatIndex) const { return *allSlots()[size_t(flatIndex)]; }

  /// 2026-10-31 (JD): one-way assign for the legacy inventory screen: copies the stack at flat slot
  /// `from` into hotbar slot `hot`. The source is never touched (no swap: the hotbar never flows back
  /// into the list). False (nothing changes) when out of range or `from` is that hotbar slot itself.
  bool assignToHotbar(int from, int hot) {
    if (from < 0 || from >= kTotalSlots || hot < 0 || hot >= int(hotbar_.size()) || from == hot) return false;
    hotbar_[size_t(hot)] = slotAt(from);
    return true;
  }

  /// Places a (possibly data-bearing) stack: merges onto an identical
  /// stack (sameIdentity) with room for the WHOLE stack, else takes the
  /// first empty carry slot. All-or-nothing: returns false and places
  /// nothing when it does not fit (etching/signing check this BEFORE
  /// consuming anything, so a full inventory never eats an input).
  bool placeStack(const ItemStack& st) {
    if (st.empty()) return true;
    for (auto* s : carrySlots())
      if (s->sameIdentity(st) && int(s->count) + int(st.count) <= int(stackCapFor(st.itemId))) {
        s->count = uint16_t(s->count + st.count);
        ++version_;
        return true;
      }
    for (auto* s : carrySlots())
      if (s->empty()) {
        *s = st;
        ++version_;
        return true;
      }
    return false;
  }

  /// True when placeStack(st) would succeed (no mutation). Mirrors
  /// placeStack's carry-slot scope exactly (flat indices 0..kCarrySlots-1;
  /// gear slots never receive placed stacks).
  bool canPlaceStack(const ItemStack& st) const {
    if (st.empty()) return true;
    const auto slots = allSlots();
    for (int i = 0; i < kCarrySlots; ++i) {
      const ItemStack* s = slots[size_t(i)];
      if (s->empty()) return true;
      if (s->sameIdentity(st) && int(s->count) + int(st.count) <= int(stackCapFor(st.itemId))) return true;
    }
    return false;
  }

  void clear() {
    for (auto* s : allSlots()) s->clear();
    ++version_;
  }
  bool isEmpty() const {
    for (const auto* s : allSlots())
      if (!s->empty()) return false;
    return true;
  }

 private:
  // All kCarrySlots (82) carry slots in fill order: hotbar, offhands, main.
  std::vector<ItemStack*> carrySlots() {
    std::vector<ItemStack*> out;
    out.reserve(kCarrySlots);
    for (auto& s : hotbar_) out.push_back(&s);
    out.push_back(&offhandLeft_);
    out.push_back(&offhandRight_);
    for (auto& s : main_) out.push_back(&s);
    return out;
  }

  std::array<ItemStack, kInvHotbarSlots> hotbar_{};
  ItemStack offhandLeft_, offhandRight_;
  std::array<ItemStack, kMainSlots> main_{};
  std::array<ItemStack, kGearSlots> gear_{};
  uint64_t version_ = 0;
};

/// C9 2026-11-12 (JD): placing a block takes one from the selected stack in SURVIVAL only.
/// Creative items are infinite: the stack is left as it is (from 1, from a full 512, any count).
/// Returns true when one was taken.
inline bool consumePlacedOne(ItemStack& s, bool creative) {
  if (creative || s.empty()) return false;
  if (s.count > 1) --s.count;
  else s.clear();
  return true;
}

/// Display name of a stack: the Etchor name field value when the stack
/// carries one (it OVERWRITES the chip's def name, per the locked design),
/// else the item's def name.
inline std::string stackDisplayName(const ItemStack& s) {
  if (!s.customName.empty()) return s.customName;
  return itemDisplayName(s.itemId);
}

/// Fresh-spawn grant (lore: the Fragment is the starting item -- available
/// day one, first tick; open inventory -> open Fragment -> read/write
/// immediately). Adds the starting item iff the inventory does not already
/// hold one, so it is idempotent: safe on first spawn, on respawn, and on
/// loading a save whose owner somehow lost it (Easy/Normal soulbound rule).
/// Returns true when the item was granted by this call.
inline bool grantStartingItems(Inventory& inv) {
  const ItemDef& start = startingItemDef();
  if (inv.countOf(start.itemId) > 0) return false;
  return inv.addItem(start.itemId, 1) == 0;
}

inline uint16_t stackCapFor(uint32_t itemId) {  // Cortana 2026-10-15 1510 (H-03)
  const ItemDef* d = findItem(itemId);
  if (!d || d->maxStack == 0) return uint16_t(kMaxStack);
  return d->maxStack < kMaxStack ? d->maxStack : uint16_t(kMaxStack);
}

}  // namespace lithokraft
