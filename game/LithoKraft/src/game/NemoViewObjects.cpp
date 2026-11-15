// Nemo View object table (see NemoViewObjects.h).
#include "lithokraft/game/NemoViewObjects.h"

#include <utility>

#include "litholattice/render/nemo_view.h"
#include "lithokraft/content/blocks.h"

namespace lithokraft {

std::shared_ptr<const std::vector<uint8_t>> buildNemoObjectTable(const litholattice::BlockMaterials& mats) {
  using litholattice::nemoview::ObjectId;
  const std::pair<litholattice::BlockId, ObjectId> rules[] = {
      {kStone, ObjectId::Wall},          {kStoneSlab, ObjectId::Wall},
      {kFrostedGlass, ObjectId::Door},   {kGlass, ObjectId::Translucent},
      {kLamp, ObjectId::Light},          {kRedTorch, ObjectId::Light},
      {kBlueTorch, ObjectId::Light},     {kColorLamp16, ObjectId::Light},
      {kColorLamp32, ObjectId::Light},   {kColorLamp64, ObjectId::Light},
      {kAluminumBlock, ObjectId::Floor}, {kCopperBlock, ObjectId::Floor},
      {kGrass, ObjectId::Floor},         {kDirt, ObjectId::Floor},
      {kWaferSilicon, ObjectId::Floor},  {kSiliconSlab, ObjectId::Floor},
      {kDirtSlab, ObjectId::Floor},
  };
  std::vector<uint8_t> table;
  std::vector<uint8_t> set;
  for (const auto& [block, object] : rules) {
    for (size_t f = 0; f < litholattice::kFaceSlotCount; ++f)
      for (bool active : {false, true}) {
        const uint32_t layer = mats.face(block, litholattice::FaceSlot(f), active).layer;
        if (layer >= table.size()) {
          table.resize(layer + 1, uint8_t(ObjectId::Other));
          set.resize(layer + 1, 0);
        }
        if (!set[layer]) {
          table[layer] = uint8_t(object);
          set[layer] = 1;
        }
      }
  }
  if (table.size() < 4096) table.resize(4096, uint8_t(ObjectId::Other));  // layers past the rules read as Other
  return std::make_shared<const std::vector<uint8_t>>(std::move(table));
}

}  // namespace lithokraft
