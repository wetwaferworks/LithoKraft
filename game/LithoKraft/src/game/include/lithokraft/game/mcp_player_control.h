// cMcpPlayerController / cMcpPlayerView: game-side adapters for the MCP
// player-control seam (litholattice::mcp::IPlayerController / IPlayerView).
//
// The controller translates a game-agnostic ControlFrame into synthetic
// InputEvents using the CURRENT ActionMap (user remaps respected) and
// stages them into the cInputSystem, where they merge with the backend's
// events at the next update() — the exact point keyboard input enters.
// The model therefore plays by the same rules as a keyboard player: the
// same physics in cPlayer::update, the same reach/interact checks in
// Game::handleWorldActions, the same sim-thread marshaling for world edits.
//
// MULTI-PILOT (locked 2026-10-11): shadow input state is per pilot id.
// v1 wires exactly one pilot ("player" = the local player); adding pilot #2
// is registering another id here — no MCP module changes.
//
// Threading (mutexfree pass 2b): submitFrame()/releaseAll()/pilotIds() run on the single MCP owner thread (the
// stdio serve loop). Pilot shadow state is owned by that thread (checked by owner_, no gate); staging into the
// cInputSystem is a lock-free phase handoff; droppedUnbound_ is a relaxed diagnostic counter.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "litholattice/input/input_system.h"
#include "litholattice/mcp/server.h"
#include "lithokraft/game/actions.h"
#include "litholattice/core/single_owner.h"
#include <atomic>

namespace lithokraft {

class cMcpPlayerController : public litholattice::mcp::IPlayerController {
 public:
  /// `pilotIds`: ids to wire (v1: {"player"}). `controllable`: true when the
  /// local player exists to drive (Playing state, not paused, no inventory
  /// open). Called on the MCP thread.
  cMcpPlayerController(litholattice::cInputSystem& input,
                      std::vector<std::string> pilotIds,
                      std::function<bool()> controllable);
  std::vector<std::string> pilotIds() const override;
  bool submitFrame(std::string_view pilot, const ControlFrame& f) override;
  void releaseAll(std::string_view pilot) override;
  /// Diagnostic: synthetic components dropped because the action has no
  /// Key binding in the current map.
  uint64_t droppedUnbound() const;

 private:
  struct PilotState {
    bool heldFwd = false, heldBack = false;
    bool heldLeft = false, heldRight = false;
    bool heldJump = false, heldSneak = false, heldSprint = false;
  };
  // Find the first Key binding for a game action, or Key::Count when unbound.
  litholattice::Key keyFor(Action a) const;
  void setHeld(Action a, bool held, bool& shadow,
               std::vector<litholattice::InputEvent>& out);
  void pressOnce(Action a, std::vector<litholattice::InputEvent>& out);

  litholattice::cInputSystem& input_;
  std::vector<std::string> pilotIds_;
  std::function<bool()> controllable_;
  litholattice::cSingleOwner owner_{"cMcpPlayerController pilot state (MCP owner thread)"};  // mutexfree 2b: was mutex_
  std::map<std::string, PilotState, std::less<>> pilots_;
  std::atomic<uint64_t> droppedUnbound_{0};  // mutexfree 2b: owner writes, any thread may read (diagnostic)
};

/// Game-side IPlayerView adapter: fills the snapshot from the live game.
/// Constructed with accessors so the game owns the data.
class cMcpPlayerView : public litholattice::mcp::IPlayerView {
 public:
  struct GameState {
    double x = 0, y = 0, z = 0;
    double vx = 0, vy = 0, vz = 0;  ///< true velocity (blocks/s), via cGamePlayerAdapter
    float yaw = 0, pitch = 0;
    uint8_t dimension = 1;
    uint8_t health = 64;  ///< effective health stat (IPlayer integration)
    /// Named stats (name, effective value) from the engine stat registry,
    /// via cGamePlayerAdapter::namedStats().
    std::vector<std::pair<std::string, double>> stats;
    std::vector<uint16_t> hotbar;
    int selectedSlot = 0;
  };
  explicit cMcpPlayerView(std::function<GameState()> snapshot);
  Snapshot localPlayer() override;

 private:
  std::function<GameState()> snapshot_;
};

/// Provisional string->uint8 mapping for the MCP snapshot dimension field
/// (1 = Wafer). Goes away when the engine gets canonical numeric ids.
uint8_t mcpDimensionNumber(const std::string& id);

}  // namespace lithokraft
