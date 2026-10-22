// Boot-log provenance lines for unofficial mods (JD, 2026-10-18; surfacing
// default-ON).
//
// Pure text utility, INFORMATIONAL only: Warning(kind) labels where a mod
// came from, in JD's register — provenance stated plainly, zero scolding,
// and no trust implied beyond what the label itself says. It has no side
// effects, no state, and it never gates anything: app/main.cpp's scanMods
// prints the line through logWarn for every mod that loads without a
// Verified signature (elevate + flag, never block loading).
//
// Provenance of THIS file: created by Crew ODOMETER in the 2026-10-18 shake.
// It did not exist in the received 0.0.1.61 tree (verified by filename and
// content search); it was authored there to JD's contract, then wired.
//
// Which kinds the boot log can assign today: the mod.json schema carries no
// provenance field, so scanMods can only truthfully label a loaded unsigned
// mod ModKind::Community — to the game, an unsigned mod is community-supplied
// and studio-unverified. SelfMade and AiAssisted ship in the utility for the
// day the schema grows a provenance field; nothing at boot invents a label
// it cannot know.
#pragma once
#include <cstdint>
#include <string>

namespace lithokraft {
namespace modWarnings {

/// Where an unofficial mod came from — the provenance the boot log labels.
enum class ModKind : std::uint8_t {
  Community,   ///< made out in the community; not verified by the studio
  SelfMade,    ///< the player's own work; their responsibility alone
  AiAssisted,  ///< written with AI help; no human wrote all of it
};

/// The boot-log line for an unofficial mod kind. Pure: same kind in, same
/// text out, always. Informational — a label, not a scolding.
inline std::string Warning(ModKind kind) {
  switch (kind) {
    case ModKind::Community:
      return "Community mod. Many hands made this, and the studio's were not among them. "
             "It has not been verified by Wet Wafer Works. If it breaks, you get to keep both pieces.";
    case ModKind::SelfMade:
      return "Self-made mod. Your own work, start to finish, so the credit and the blame are both yours. "
             "No warning implied beyond the label. If it breaks, you already own both pieces.";
    case ModKind::AiAssisted:
      return "AI-assisted mod. No human wrote all of this. It is no more trustworthy than the human who prompted it. "
             "If it breaks, you get to keep both pieces.";
  }
  return "Unofficial mod. Not verified by Wet Wafer Works.";
}

}  // namespace modWarnings
}  // namespace lithokraft
