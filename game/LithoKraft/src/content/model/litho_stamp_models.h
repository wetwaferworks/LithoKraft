// ModelResolver over the baked Litho Formula gallery: resolves a preset name
// ("starfish") to its stampable unit cells for the "lithoform_stamp" worldgen
// feature. Thread-safe; each model loads from its .lthm exactly once.
#pragma once
#include <optional>
#include <string_view>
#include "litholattice/procgen/generator_loader.h"

namespace lithokraft::model {

/// Loads one gallery model by preset name; nullopt when absent or unreadable.
/// Exposed for tests; prefer makeLithoStampModelResolver() in production.
std::optional<litholattice::procgen::LithoStampModel> loadLithoStampModel(std::string_view name);

/// Builds the resolver handed to procgen's loadGenerator for "lithoform_stamp".
litholattice::procgen::ModelResolver makeLithoStampModelResolver();

/// --stub-models (2026-10-28, Archie bonus ruling; bench only): when
/// enabled, a preset name that does not resolve returns a small stub
/// model (flat 8x8 one-cell-high wafer plate, palette 1) instead of
/// failing closed, so a tree whose baked gallery is not wired up can
/// still generate its world for renderer benches. Each substitution is
/// logged once per name. Default OFF — production behaviour is
/// unchanged (unresolved presets still fail closed at generator load).
/// Set from main.cpp's --stub-models before the Game is constructed.
void setLithoStampModelStubEnabled(bool on);
bool lithoStampModelStubEnabled();

}  // namespace lithokraft::model
