// Game-side entities: item drops (first entity type).
// The engine owns storage/ticking (litholattice::cEntityMgr); the game defines
// types and behaviors. Rendering is TODO (v1 items are sim-functional, invisible).
#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include "litholattice/entity/entity.h"
#include "litholattice/entity/physics.h"
#include "lithokraft/game/item_drops.h"

namespace lithokraft {

constexpr uint32_t kEntityItem = 1;         ///< dropped block item (Entity.data = BlockId)
constexpr uint32_t kEntityFallingBlock = 2;  ///< sand-like falling block (Entity.data = BlockId)
constexpr uint32_t kEntityDeceitor = 3;      ///< Deceitor visual mirror (Entity.data = cAiWorld mob id)

/// Item physics (P-1/P-2, 2026-11-17): one call per WORLD tick (dt = 1/TPS), so the path never
/// depends on how ticks group into frames. Order per tick: gravity, liquid buoyancy and drag (or
/// air drag), collision, landing bounce, ground friction. Despawn when the entity's age (world
/// ticks, counted by cEntityMgr) reaches 8 minutes of ticks. Magnet, merge and pickup are done
/// by the game (they need the player), also per tick.
class cItemBehavior final : public litholattice::IEntityBehavior {
 public:
  using SolidFn = std::function<bool(int x, int y, int z)>;
  using FrictionFn = std::function<float(int x, int y, int z)>;
  using LiquidFn = std::function<bool(int x, int y, int z)>;
  cItemBehavior(SolidFn isSolid, FrictionFn friction, LiquidFn liquid = {})
      : isSolid_(std::move(isSolid)), friction_(std::move(friction)), liquid_(std::move(liquid)) {}
  void onTick(litholattice::Entity& e, double dt) override {
    using namespace itemdrop;
    if (!(dt > 0)) return;  // a paused world: nothing moves, nothing ages
    const int tps = int(1.0 / dt + 0.5);
    if (e.age >= ticksFor(kLifeSeconds, tps)) { e.dead = true; return; }  // JD: 8-minute despawn, in ticks
    const bool wet = liquid_ && liquid_(int(std::floor(e.x)), int(std::floor(e.y + kHeight * 0.5)), int(std::floor(e.z)));
    if (!e.onGround || e.vy != 0 || wet) e.vy -= kGravity * dt;
    if (wet) {  // P-2: items float (buoyancy beats gravity), liquid drag on every axis
      e.vy += kBuoyancy * dt;
      const double k = std::max(0.0, 1.0 - kLiquidDragPerSecond * dt);
      e.vx *= k; e.vy *= k; e.vz *= k;
      if (e.vy > kMaxRiseSpeed) e.vy = kMaxRiseSpeed;
    } else {
      const double k = std::max(0.0, 1.0 - kAirDragPerSecond * dt);
      e.vx *= k; e.vz *= k;
    }
    if (e.vy < -kTerminalSpeed) e.vy = -kTerminalSpeed;
    const double vyIn = e.vy;
    litholattice::moveCollide(e, isSolid_, float(kHalfWidth), float(kHeight), dt);  // 0.3 x 0.3 x 0.3 item
    if (e.onGround && vyIn < -kBounceMinSpeed) { e.vy = -vyIn * kBounce; e.onGround = false; }  // a hard landing hops once
    if (e.onGround) {
      // Block friction (1 = ice keeps sliding, 0 = stops), scaled to the tick rate: at 20 TPS a
      // block with friction f keeps f of the speed per tick.
      const float f = friction_(int(std::floor(e.x)), int(std::floor(e.y)) - 1, int(std::floor(e.z)));
      const double k = std::clamp(1.0 - (1.0 - double(f)) * kGroundDragScale * dt, 0.0, 1.0);
      e.vx *= k; e.vz *= k;
    }
  }

 private:
  SolidFn isSolid_;
  FrictionFn friction_;
  LiquidFn liquid_;
};

/// Falling block: gravity + collision; on landing, places the block and despawns.
/// Lifetime (H-ENT-4): a block that never lands — over the Void or an unloaded
/// column, isSolid reads air forever — must not fall (and leak) without bound.
/// Mirrors the item rule in shape: after kMaxFallSeconds of falling, or below
/// the absolute y floor, it despawns WITHOUT placing (there is nowhere lawful
/// to place it). At terminal velocity 24 b/s, 60 s is ~1,370 blocks of fall —
/// far beyond any real dimension height (768 max), so no legitimate landing
/// is affected.
class cFallingBlockBehavior final : public litholattice::IEntityBehavior {
 public:
  using SolidFn = cItemBehavior::SolidFn;
  using PlaceFn = std::function<void(int x, int y, int z, uint32_t blockId)>;
  static constexpr float kMaxFallSeconds = 60.f;
  static constexpr double kYFloor = -1024.0;  // below every dimension's bound
  cFallingBlockBehavior(SolidFn isSolid, PlaceFn place) : isSolid_(std::move(isSolid)), place_(std::move(place)) {}
  void onTick(litholattice::Entity& e, double dt) override {
    fallS_ += float(dt);
    if (fallS_ > kMaxFallSeconds || e.y < kYFloor) { e.dead = true; return; }
    e.vy -= 22.0 * dt;
    if (e.vy < -24.0) e.vy = -24.0;
    litholattice::moveCollide(e, isSolid_, 0.5f, 1.0f, dt);  // full-block size
    if (e.onGround) {
      place_(int(std::floor(e.x)), int(std::floor(e.y)), int(std::floor(e.z)), e.data);
      e.dead = true;
    }
  }
 private:
  float fallS_ = 0.f;
  SolidFn isSolid_;
  PlaceFn place_;
};

/// Deceitor visual mirror: the mob's brain and position live in the
/// engine cAiWorld (game/deceitor.h); this entity is only its body in
/// the renderer's entity pass. Game::updateDeceitors rewrites its
/// position from the cAiWorld mob every frame, so the behavior does
/// nothing (no gravity, no self-motion) — honest placeholder body
/// until a real mob render path exists.
class cDeceitorVisualBehavior final : public litholattice::IEntityBehavior {
 public:
  void onTick(litholattice::Entity&, double) override {}
};

/// Factory for game entity behaviors. The game passes its solidity callback so items
/// collide with the real world (thread-safe via cSimThread::getBlock).
inline std::unique_ptr<litholattice::IEntityBehavior> makeEntityBehavior(
    uint32_t type, cItemBehavior::SolidFn isSolid, cItemBehavior::FrictionFn friction,
    cFallingBlockBehavior::PlaceFn place = {}, cItemBehavior::LiquidFn liquid = {}) {
  if (type == kEntityItem) return std::make_unique<cItemBehavior>(std::move(isSolid), std::move(friction), std::move(liquid));
  if (type == kEntityFallingBlock) return std::make_unique<cFallingBlockBehavior>(std::move(isSolid), std::move(place));
  if (type == kEntityDeceitor) return std::make_unique<cDeceitorVisualBehavior>();
  return nullptr;
}

}  // namespace lithokraft
