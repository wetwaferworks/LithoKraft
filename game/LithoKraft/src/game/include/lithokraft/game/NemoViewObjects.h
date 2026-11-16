#pragma once
// Nemo View object table: texture layer -> object id, built from the loaded block materials.
// Stone is a wall, frosted glass a door, lamps and torches lights, plain ground blocks floor;
// everything else is Other (no character of its own). Where two blocks share a layer, the
// first rule in the list wins, so the table never depends on load order.
#include <cstdint>
#include <memory>
#include <vector>

#include "litholattice/assets/texture_registry.h"
#include "litholattice/render/block_materials.h"

namespace lithokraft {

std::shared_ptr<const std::vector<uint8_t>> buildNemoObjectTable(const litholattice::BlockMaterials& mats);
/// Per-layer average colour from a fixed 4x4 sample grid, built at load. Display only.
std::shared_ptr<const std::vector<uint32_t>> buildNemoTextureColors(const litholattice::cTextureReg& textures);

}  // namespace lithokraft
