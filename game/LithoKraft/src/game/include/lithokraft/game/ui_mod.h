// Mod UI confinement + mod-content badging, ported to the facade from the
// old toolkit's mod_ui.h / widget_factory.h (engine ui at 67c1590). The
// rules are unchanged (see the threat model in the migration report):
// mods build UI only through ModUiFactory::createPanel from JSON, the
// factory's type table is closed (no dialog/modal/popup types), reserved
// style classes are rejected, JSON is pre-scanned for depth/node budget,
// panels are widget-budget capped, and every panel root is a cModPanel
// whose amber MOD badge (fixed colours) paints after its children.
// Mod panels attach to the cUiRoot main layer only, never as modals.
#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include "litholattice/core/json.h"
#include "lithokraft/game/ui_widgets.h"

namespace lithokraft {
namespace ui {

/// Builds a widget tree from widget-factory JSON (the old closed type
/// table). nullptr + `err` (with the JSON path) on error.
std::unique_ptr<cWidget> buildWidget(const litholattice::Json& json, std::string& err);
std::unique_ptr<cWidget> buildWidget(const std::string& text, std::string& err);

/// Style class the engine applies to every mod panel root. Reserved: the
/// confined factory rejects it in mod JSON.
inline constexpr const char* kModPanelStyleClass = "mod_panel";

/// Style classes a mod may never select in its JSON.
inline constexpr const char* kModReservedStyles[] = {
    "dialog", "tooltip", "mod_panel", "mod_badge",
};

/// Hard limits on mod-supplied layout JSON (DoS guard; the factory itself
/// recurses, so depth is pre-scanned iteratively before it runs).
inline constexpr size_t kModUiMaxJsonDepth = 32;
inline constexpr size_t kModUiMaxJsonNodes = 1024;
/// Max widgets in one mod panel (factory output, counted post-build).
inline constexpr size_t kModUiMaxWidgets = 256;
/// Max title length (registerUiPanel title).
inline constexpr size_t kModUiMaxTitleLen = 128;

/// A cPanel that is always visibly badged as mod content. `final`: neither
/// a subclass nor a mod can override or skip the badge painting. The badge
/// strip is engine-drawn with fixed colours, the panel arranges children
/// strictly below the strip, and the strip paints after the children.
class cModPanel final : public cPanel {
 public:
  cModPanel(std::string modId, std::string title);

  const std::string& modId() const { return modId_; }
  const std::string& title() const { return title_; }

  /// The mod's content goes here (filled by the confined factory from JSON).
  cStackPanel& body() { return *body_; }
  const cStackPanel& body() const { return *body_; }

  /// Height of the engine badge strip, in pixels at the current theme scale.
  float badgeHeight() const;
  /// The badge strip rect in panel-local coordinates (valid after place()).
  Rect badgeRect() const;

 protected:
  void arrange(const Theme& theme) override;
  void paint(Painter& p) const override;
  /// Children painted ONCE, clipped to the body rect, then the badge on
  /// top of everything (old Archie review C2).
  void paintChildren(Painter& p) const override;

 private:
  void paintBadge(Painter& p) const;

  std::string modId_, title_;
  cStackPanel* body_ = nullptr;
};

/// The confined widget factory for mods: the ONLY sanctioned path for mod
/// UI (script layouts go through here too).
class ModUiFactory {
 public:
  ModUiFactory() = delete;

  /// Builds a badged cModPanel for `modId` (must be [a-z0-9_]{1,64}).
  /// `panelId` becomes the widget id; `title` is shown in the badge strip
  /// (capped at kModUiMaxTitleLen). `bodyJson` may be empty or a
  /// widget-factory JSON object. Returns nullptr + `err` on any violation.
  static std::unique_ptr<cModPanel> createPanel(const std::string& modId,
                                                const std::string& panelId,
                                                const std::string& title,
                                                const std::string& bodyJson, std::string& err);

  /// True when `styleClass` is reserved (rejected in mod JSON).
  static bool isReservedStyle(std::string_view styleClass);

  /// Validates a mod/panel id: [a-z0-9_]{1,64}.
  static bool isValidModId(std::string_view id);

  /// Attaches a built panel to the cUiRoot MAIN layer (never a modal or
  /// popup). Returns the attached panel.
  static cModPanel& attachToRoot(cUiRoot& ui, std::unique_ptr<cModPanel> panel);
};

}  // namespace ui
}  // namespace lithokraft
