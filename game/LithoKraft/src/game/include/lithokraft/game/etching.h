// The Etchor etch flow (wiring pass 2026-10-13, Team B item B6).
//
// Locked design (MEMORY 2026-10-11 + Glossary v2): the Etchor spits out
// the Chipor; its name field overwrites the chip's name -- wafer in,
// Chipor out, bearing the player name-field value; the name is a field
// in the game's tagged binary storage (player.lat v5 "name" field).
// Glossary v2 refines: the Etchor's ONLY UI element is the name field --
// the Selectors say WHAT to make, the name field says WHAT TO CALL IT.
// The Selector/Connector/Laysor machine is not built yet; this flow is
// the inventory core it will drive: one blank wafer + a name -> one
// named Chipor stack.
#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include "lithokraft/game/inventory.h"

namespace lithokraft {

/// Max name-field length in bytes. Longer names are REFUSED (never
/// silently truncated -- a chip's name is its identity).
constexpr size_t kEtchNameMaxBytes = 64;

/// The etch input item: one blank wafer per etch. Today the blank is the
/// Wafer Silicon block-item (lithokraft:silicon), the wafer material --
/// the fab line's AutoFabor-stamped blank-wafer item does not exist yet;
/// this is the conservative stand-in, flagged for JD in the wiring report.
uint32_t etchInputItemId();
/// The etch output item: the Chipor (its block-backed item id).
uint32_t etchedChipItemId();

/// Etch one chip: consume one blank wafer from `inv`, produce one Chipor
/// stack whose name field is `nameField` VERBATIM (it overwrites the
/// "Chipor" def name; an empty name field yields a plain unnamed chip).
/// The output stacks with an identical named stack when one has room.
/// Atomic: on failure (no wafer, no room, name over the cap or carrying
/// control characters) the inventory is untouched and `err` says why.
bool etchChip(Inventory& inv, std::string_view nameField, std::string* err);

}  // namespace lithokraft
