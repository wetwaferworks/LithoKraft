# 42-Slot Inventory Data Model — Draft for Review (2026-10-10)

**Status:** proposal, not locked. Encodes every inventory decision already made,
keeps the 3 open questions open (with proposed defaults JD can accept or kill).
Follows the locked conventions: `litholattice::` namespace, I-prefix interfaces,
`-OR` component names, Doxygen comments, 1-based indexing (0 = invalid/off).
Engine owns storage + serialization; game owns item types and behavior —
same split as `IEntityBehavior` (`engine/src/entity/include/litholattice/entity/entity.h`).

## Slot scheme (1-based, 0 = invalid)

**Carry — 42 slots (the meme number; 42 = 0b101010):**

| Space    | Slots | Layout |
|----------|-------|--------|
| Hotbar   | 1–8   | 8 = 2³ |
| Offhands | 9–10  | 9 = left, 10 = right |
| Main     | 11–42 | 4 rows × 8, row-major: row r, col c → `11 + r*8 + c` |

Check: 11 + 3×8 + 7 = 42 ✓ · 8 + 2 + 32 = 42 ✓

**Gear — 8 worn slots, separate space, NOT part of the 42:**

1 head · 2 chest · 3 legs · 4 feet · 5 arms · 6 face · 7 heart · 8 mind

Carry and gear are separate namespaces — gear slot ids never overlap carry ids,
so "42" stays exact.

## Constants

```cpp
kHotbarSlots = 8, kOffhandSlots = 2, kMainRows = 4, kMainCols = 8,
kCarrySlots = 42, kGearSlots = 8,
kMaxStack = 512          // 2^9; locked to the tick rate
// 42 × 512 = 21,504 max items carried (locked)
```

## Stack struct

```cpp
struct ItemStack {
  uint32_t type = 0;   ///< 0 = empty (1-based rule); game registry id otherwise
  uint16_t count = 0;  ///< 0..512 (fits uint16; 512 = 2^9)
  uint32_t data = 0;   ///< aux payload (durability, charge, NBT-ref index…)
};
```

Per-player carry serializes to ~42 × 10 bytes ≈ 420 bytes. Gear another ~80.

## Interfaces (engine side, proposed)

```cpp
class IInventor {  // carry 1–42
 public:
  virtual ~IInventor() = default;
  /// 1-based slot; false on invalid slot or count > per-type cap.
  virtual bool setSlot(uint32_t slot, ItemStack stack) = 0;
  virtual ItemStack getSlot(uint32_t slot) const = 0;
  /// Merge into existing partials per pickup policy, then empties.
  /// Returns leftover count that didn't fit.
  virtual uint32_t addItem(uint32_t type, uint16_t count, uint32_t preferredSlot = 0) = 0;
  virtual bool moveSlot(uint32_t from, uint32_t to) = 0;
  virtual uint64_t totalCount(uint32_t type) const = 0;
  virtual void clear() = 0;
  virtual std::vector<uint8_t> serialize() const = 0;       // versioned; save-migration hooks
  virtual bool deserialize(const uint8_t* data, size_t len) = 0;
};

class IGear {  // worn 1–8; equip validation is game-side
 public:
  virtual ~IGear() = default;
  virtual bool equip(uint32_t slot, ItemStack stack) = 0;   // calls IEquipValidator
  virtual ItemStack unequip(uint32_t slot) = 0;
  virtual ItemStack getGear(uint32_t slot) const = 0;
  // + serialize/deserialize, same versioning
};

class IPickupPolicy {  // game-side; JD's open question #3
 public:
  virtual ~IPickupPolicy() = default;
  /// Candidate slot order for addItem(), 1-based, first = highest priority.
  virtual std::vector<uint32_t> order(const IInventor& inv, uint32_t type) const = 0;
};
```

## Pickup integration with item entities (already built)

Item entities are `Entity` with `data` = item type, `count` = stack size
(`entity.h`), physics-wired: gravity + voxel collision + per-block `friction`
(`world/block.h`: `friction = 0.8f`, velocity retained per tick; 1 = ice, 0 = stop).
Despawn stays on the entity side at the locked **8 minutes** (480 s; 8 on its
side is ∞). Pickup flow: entity within pickup radius → `Inventor::addItem(type,
count)` → leftover stays on the entity; `addItem` returns 0 → despawn entity.
Friction becomes the transport-line mechanic: low-friction blocks (ice-class)
for fast item flow, normal blocks to slow it — exactly what JD designed it for.

## The 3 open questions (JD's call; proposed defaults below)

1. **Stack sizes.** Locked max is 512 per stack. Proposed: per-type cap,
   `maxStack(type)` defaulting to 512 for placeables, game-tunable down for
   specials (tools, charged items…). The `count` field and all merging code are
   written against the per-type cap, so tuning is a registry edit, not a refactor.
2. **Offhand mechanics.** Proposed: symmetric and sneak-driven (matches the
   approved wireless sneak-cycle UX). Hotbar slot = main hand. `use` activates
   right offhand; `sneak+use` activates left offhand; both offhands apply passive
   effects (torch lights, shield blocks). Alternative: dedicated roles
   (right = shield/tool, left = torch/food) — cheaper to balance, less flexible.
3. **Pickup priority.** Proposed default policy: selected-hotbar partial merge →
   hotbar partials → main partials → hotbar empties → main empties. Offhands
   excluded from auto-pickup unless `preferredSlot` is given (prevents torch
   stacks vanishing into your shield hand). Fully replaceable via `IPickupPolicy`.

## Notes for the builder handoff

- Follows the locked naming/namespace schemes (source of truth for <48h handoff).
- `serialize()`/`deserialize()` versioned from day one — the wireless save
  migration pattern applies; inventory save format must ride the same system.
- AngelScript exposure of `IInventor`/`IGear` (get/set/addItem) is a natural
  modding API later; not needed for the data model to land.
- Row-major main grid (8-wide) matches the power-of-two theme: 8 hotbar, 8
  gear, 64 channels, 256 TX power, 512 stacks/ticks.

**What stays JD's:** the 3 open questions above. Everything else is reviewable
as written — approve, red-pen, or hand to the builder as-is.
