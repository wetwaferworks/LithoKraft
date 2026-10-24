// Certificate table behaviour: the seam for JD's certificate-signing mechanic
// (the enchanting-table analogue: signing spends the "static" stat and applies
// "Certificate of X" effects to items). THIS PASS IS THE BLOCK ONLY -- the
// behaviour is describe-only and signing does not exist yet.
//
// Why a plain cBlockBehavior and not a cCompBehavior: the standard use path
// (Game -> IWorldSimulation::use -> SignalSim::use) only reaches blocks that
// sit in SignalSim's component map, and that map is fed inconsistently for a
// SimRole::None block -- onBlockChanged (placement) admits any block whose
// behaviour is a cCompBehavior, but scan() (world load) pre-filters to
// SimRole != None. A cCompBehavior here would make right-click work when
// the table is placed yet silently stop after a reload: a broken seam.
// As a describe-only cBlockBehavior, BehaviorCatalog::component() stays
// nullptr, SignalSim::use returns false (the framework's not-usable result)
// uniformly, and the look-at readout resolves through the standard path --
// exactly the structure block's current behaviour, but with the name anchored
// on a real class instead of dangling.
//
// Where the mechanic binds: when signing lands, this class grows a SimRole
// and a cCompBehavior::use() (or the game intercepts use for this block),
// and use opens signing through a game-side entry point (working name:
// openSigning) that reads/spends the player adapter's "static" stat
// (game/player_adapter) and applies certificates to the held item. The block
// id, the behaviour name and this class name stay stable across that change;
// no save data depends on which side of the seam a build is on.
#include "components.h"

namespace lithokraft::components {
namespace {

class cCertificateTableBehavior final : public cBlockBehavior {
 public:
  // Readout is the plain block name for now -- the signing pass extends it
  // (e.g. with the player's static balance). No fake state is shown.
  std::string describe(const BlockDef& d, CellState /*s*/) const override { return std::string(d.name); }
};

}  // namespace

void registerCertificateTable(const Registrar& add) {
  add("certificate_table", std::make_unique<cCertificateTableBehavior>());
}

}  // namespace lithokraft::components
