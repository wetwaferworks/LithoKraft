#include <cstdlib>
#include "lithokraft/game/resource_manager.h"
#include "litholattice/render/hud_ui_renderer.h"
#include <iterator>
#include <string>
#include "lithokraft/content/appearance.h"
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/items.h"
#include "litholattice/core/log.h"
#include "litholattice/render/renderer.h"
#include "litholattice/render/texture_array.h"

namespace lithokraft {

cResourceMgr::cResourceMgr(std::vector<std::filesystem::path> packDirs)
    : dirs_(std::move(packDirs)), materials_(std::make_shared<const BlockMaterials>()) {}

// Largest <name>_item.png across the active packs and their sibling packs (pack_16..pack_128).
std::optional<Image> cResourceMgr::bestItemIcon(const std::string& name) const {
  std::vector<std::filesystem::path> roots;
  for (const auto& d : dirs_) {
    roots.push_back(d);
    std::error_code ec;
    if (d.has_parent_path())
      for (const auto& e : std::filesystem::directory_iterator(d.parent_path(), ec))
        if (e.is_directory(ec) && e.path().filename().string().rfind("pack_", 0) == 0) roots.push_back(e.path());
  }
  // 2026-10-25 (JD): CloudAtlas 256x256 item icons (assets/item_icons/256) are the source for every pack.
  for (const auto& d : dirs_) {
    if (!d.has_parent_path()) continue;
    const auto f = d.parent_path().parent_path() / "item_icons" / "256" / (name + "_item.png");
    std::error_code ec;
    if (!std::filesystem::exists(f, ec)) continue;
    std::string err;
    if (auto img = loadTextureFileCached(f, &err)) return img;
  }
  std::optional<Image> best;
  for (const auto& root : roots) {
    const auto f = root / "textures" / "ui" / "icons" / (name + "_item.png");
    std::error_code ec;
    if (!std::filesystem::exists(f, ec)) continue;
    std::string err;
    if (auto img = loadTextureFileCached(f, &err); img && (!best || img->width() > best->width())) best = std::move(img);
  }
  return best;
}

// 2026-10-31 (JD, CloudAtlas icons v3): largest textures/ui/icons/<name>.png across the active
// packs and their sibling packs (pack_16..pack_256). LL_ICON_V3=0 restores the _item-first order.
std::optional<Image> cResourceMgr::bestPlainIcon(const std::string& name) const {
  std::optional<Image> best;
  for (const auto& d : dirs_) {
    std::vector<std::filesystem::path> roots{d};
    std::error_code ec;
    if (d.has_parent_path())
      for (const auto& e : std::filesystem::directory_iterator(d.parent_path(), ec))
        if (e.is_directory(ec) && e.path().filename().string().rfind("pack_", 0) == 0) roots.push_back(e.path());
    for (const auto& root : roots) {
      const auto f = root / "textures" / "ui" / "icons" / (name + ".png");
      if (!std::filesystem::exists(f, ec)) continue;
      std::string err;
      if (auto img = loadTextureFileCached(f, &err); img && (!best || img->width() > best->width())) best = std::move(img);
    }
  }
  return best;
}

void cResourceMgr::reload() {
  invalidateHudIconCache();
  cPackStack stack;
  // 2026-10-16 (JD rename): Repeator art is named repeator_*; packs that still ship only the
  // old repeater_* files keep working through this fallback.
  stack.addFileFallback("repeator", "repeater");
  std::string text;
  for (const auto& dir : dirs_) {
    std::string err;
    if (auto pack = ResourcePack::open(dir, &err)) {
      text += (text.empty() ? "" : " < ") + pack->name();
      stack.push(std::move(*pack));
    } else {
      logError("resources: pack skipped: %s", err.c_str());
    }
  }
  // Hotbar icons. (They used to be flat material swatches: the HUD never read textures/ui/icons.)
  std::vector<Image> icons(blockRegistry().size());
  size_t missingIcons = 0;
  for (size_t b = 1; b < icons.size(); ++b) {
    std::string err;
    // 2026-10-25 (JD, CloudAtlas item icons): <name>_item.png (rendered model) wins over <name>.png;
    // blocks without one (e.g. the Trace Bulb) keep their old icon / swatch fallback.
    // 2026-10-25 (JD, crisp icons): the highest-resolution _item.png shipped in ANY pack
    // (pack_128 normally) is kept at full size; the HUD fits it to the slot's pixel size.
    static const bool v3 = [] { const char* e = std::getenv("LL_ICON_V3"); return !(e && e[0] == '0'); }();
    if (v3) if (auto best = bestPlainIcon(iconName(BlockId(b)))) if (best->width() >= 32) { icons[b] = std::move(*best); continue; }
    if (auto best = bestItemIcon(iconName(BlockId(b)))) { icons[b] = std::move(*best); continue; }
    auto file = stack.find("textures/ui/icons/" + iconName(BlockId(b)) + ".png");
    if (auto img = file ? loadTextureFileCached(*file, &err) : std::nullopt) icons[b] = downsampleIcon(*img, kHudIconPx);  // decode cache (CREW TILE)
    else ++missingIcons;
  }
  if (missingIcons) logError("resources: %zu block icon(s) missing under textures/ui/icons (swatches used)", missingIcons);
  icons_ = std::move(icons);
  // Item icons (non-block items from the item registry: tools, the Fragment,
  // the seed). Same pack path + 16x16 box filter as block icons. Block-backed
  // items (Chipor) keep using icon(BlockId) and are skipped here.
  std::map<uint32_t, Image> itemIcons;
  size_t missingItemIcons = 0;
  for (const ItemDef& def : itemRegistry()) {
    if (def.itemId < kFirstItemId) continue;  // block-backed
    std::string err;
    const auto file = stack.find("textures/ui/icons/" + std::string(def.iconName) + ".png");
    if (auto img = file ? loadTextureFileCached(*file, &err) : std::nullopt) itemIcons[def.itemId] = downsampleIcon(*img, kHudIconPx);  // decode cache (CREW TILE)
    else ++missingItemIcons;
  }
  if (missingItemIcons) logError("resources: %zu item icon(s) missing under textures/ui/icons", missingItemIcons);
  itemIcons_ = std::move(itemIcons);
  ResourceSet next = loadResources(stack);
  std::vector<std::string> warnings = std::move(next.warnings);
  ShaderLibrary shaders = ShaderLibrary::load(stack, kShaderPrograms, &warnings);
  auto materials = std::make_shared<const BlockMaterials>(BlockMaterials::build(blockRegistry(), next.textures, next.models, &warnings, &blockAppearance()));
  std::map<std::string, std::string, std::less<>> worldgen;
  for (const std::string_view id : kWorldgenDimensions) {
    std::string origin;
    if (auto json = stack.readText("worldgen/" + std::string(id) + ".json", &origin)) {
      worldgen.emplace(id, std::move(*json));
      logInfo("  resources: worldgen/%s.json from %s", std::string(id).c_str(), origin.c_str());
    }
  }

  ui::Theme theme = ui::Theme::defaults();
  {
    std::string origin, err;
    if (auto json = stack.readText("ui/theme.json", &origin)) {
      if (ui::Theme::parse(*json, theme, err)) logInfo("  resources: ui/theme.json from %s", origin.c_str());
      else warnings.push_back("ui/theme.json (" + origin + "): " + err + " - using the built-in theme");
    }
  }

  logInfo("resources: %s | %zu textures (%zu missing), %zu animations, %zu palettes, %zu models, %zu/%zu shader programs from packs",
          text.c_str(), next.textures.textures().size(), next.textures.missingNames().size(),
          next.textures.animations().size(), next.textures.palettes().size(), next.models.size(), shaders.size(),
          std::size(kShaderPrograms));
  for (const auto& w : warnings) logInfo("  resources: %s", w.c_str());
  if (loaded_) {
    logInfo("hot reload: %s; shaders %s; models %zu -> %zu", diffRegistries(resources_.textures, next.textures).summary().c_str(),
            shaders == shaders_ ? "unchanged" : "changed", resources_.models.size(), next.models.size());
  }
  resources_ = std::move(next);
  shaders_ = std::move(shaders);
  stackText_ = std::move(text);
  worldgenSources_ = std::move(worldgen);
  theme_ = std::move(theme);
  ++themeRevision_;
  loaded_ = true;
  std::lock_guard lock(mutex_);
  materials_ = std::move(materials);
}

void cResourceMgr::apply(IRenderer& renderer) const {
  if (!renderer.textureArray().upload(resources_.textures)) logError("resources: texture array upload reported a GL error");
  renderer.setMaterials(*materials());
  renderer.loadShaders(shaders_);
}

WorldgenSettings cResourceMgr::worldgen(uint64_t seed) const {
  WorldgenSettings s;
  s.seed = seed;
  s.generators = worldgenSources_;
  return s;
}

std::shared_ptr<const BlockMaterials> cResourceMgr::materials() const {
  std::lock_guard lock(mutex_);
  return materials_;
}

const Image* cResourceMgr::itemIcon(uint32_t itemId) const {
  const auto it = itemIcons_.find(itemId);
  return it != itemIcons_.end() && !it->second.empty() ? &it->second : nullptr;
}

std::string cResourceMgr::iconName(BlockId b) {
  const std::string tex(blockRegistry().def(b).tex);
  return tex == "grass" ? "grass_block" : tex;  // the art's icon file names follow the block names
}

}  // namespace lithokraft
