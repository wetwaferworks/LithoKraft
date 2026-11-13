// ui_theme.cpp: the old per-class Theme model (parse / defaults / style
// lookup) plus its projection onto lithogui::Style. Parse semantics are the
// old engine theme.cpp's, verbatim where the JSON shapes agree (colours as
// [r,g,b(,a)] or "#rrggbb[aa]", edges as number or [l,t,r,b], unknown keys
// are errors, classes inherit from a possibly-overridden "default").
#include "lithokraft/game/ui_facade.h"
#include <cstdlib>

namespace lithokraft {
namespace ui {
namespace {

bool parseColor(const litholattice::Json& v, Color& out) {
  using J = litholattice::Json;
  if (v.type == J::Type::Array && (v.arr.size() == 3 || v.arr.size() == 4)) {
    out = {float(v.arr[0].num), float(v.arr[1].num), float(v.arr[2].num),
           v.arr.size() == 4 ? float(v.arr[3].num) : 1.f};
    return true;
  }
  if (v.type == J::Type::String && v.str.size() >= 7 && v.str[0] == '#') {
    const unsigned long bits = std::strtoul(v.str.c_str() + 1, nullptr, 16);
    const bool hasAlpha = v.str.size() == 9;
    const auto byte = [&](int i) { return float((bits >> (8 * i)) & 0xFF) / 255.f; };
    out = hasAlpha ? Color{byte(3), byte(2), byte(1), byte(0)} : Color{byte(2), byte(1), byte(0), 1.f};
    return v.str.size() == 7 || hasAlpha;
  }
  return false;
}

bool parseEdges(const litholattice::Json& v, Edges& out) {
  using J = litholattice::Json;
  if (v.type == J::Type::Number) {
    out = Edges::all(float(v.num));
    return true;
  }
  if (v.type == J::Type::Array && v.arr.size() == 4) {
    out = {float(v.arr[0].num), float(v.arr[1].num), float(v.arr[2].num), float(v.arr[3].num)};
    return true;
  }
  return false;
}

bool parseStyle(const litholattice::Json& j, Style& s, const std::string& name, std::string& err) {
  using J = litholattice::Json;
  if (j.type != J::Type::Object) {
    err = "style '" + name + "' must be an object";
    return false;
  }
  const std::pair<const char*, Color*> colors[] = {
      {"background", &s.background}, {"hover", &s.hover},   {"pressed", &s.pressed},
      {"disabled", &s.disabled},     {"text", &s.text},     {"text_disabled", &s.textDisabled},
      {"accent", &s.accent},         {"border", &s.border}};
  for (const auto& [key, value] : j.obj) {
    bool ok = true, known = false;
    for (const auto& [k, c] : colors)
      if (key == k) {
        known = true;
        ok = parseColor(value, *c);
      }
    if (key == "border_width") {
      known = true;
      s.borderWidth = float(value.num);
    } else if (key == "selected_width") {  // D9: selected-slot outline thickness (px)
      known = true;
      s.selectedWidth = float(value.num);
    } else if (key == "text_scale") {
      known = true;
      s.textScale = float(value.num);
    } else if (key == "padding") {
      known = true;
      ok = parseEdges(value, s.padding);
    }
    if (!known) {
      err = "style '" + name + "': unknown key '" + key + "'";
      return false;
    }
    if (!ok) {
      err = "style '" + name + "': bad value for '" + key + "'";
      return false;
    }
  }
  return true;
}

}  // namespace

Theme Theme::defaults() {
  Theme t;
  Style base;
  t.styles_["default"] = base;
  Style panel = base;
  panel.background = {0.f, 0.f, 0.f, 0.72f};
  panel.padding = Edges::all(8.f);
  t.styles_["panel"] = panel;
  Style bare = base;  // layout-only containers: draw nothing, no padding
  bare.background = {0, 0, 0, 0};
  bare.borderWidth = 0;
  bare.padding = Edges::all(0.f);
  for (const char* name : {"none", "root", "stack", "scrollview", "splitter", "image", "label"}) t.styles_[name] = bare;
  Style title = bare;  // launch-menu title: big flat text, no background
  title.textScale = 4.5f;
  t.styles_["title"] = title;
  Style hudSmall = bare;
  hudSmall.textScale = 0.5f;
  t.styles_["hud_small"] = hudSmall;
  Style tagline = bare;  // launch-menu tagline: contrast plate
  tagline.background = {0.f, 0.f, 0.f, 0.55f};
  tagline.padding = Edges::all(4.f);
  t.styles_["menu_tagline"] = tagline;
  Style edit = base;
  edit.background = {0.05f, 0.05f, 0.07f, 0.95f};
  t.styles_["textbox"] = edit;
  t.styles_["editor"] = edit;
  Style menu = base;
  menu.background = {0.16f, 0.16f, 0.19f, 0.98f};
  t.styles_["menu"] = menu;
  Style modPanel = base;  // mod-content frame treatment (fixed by the house)
  modPanel.background = {0.09f, 0.07f, 0.04f, 0.96f};
  modPanel.border = {1.f, 0.62f, 0.f, 1.f};
  modPanel.borderWidth = 2.f;
  modPanel.padding = Edges::all(8.f);
  t.styles_["mod_panel"] = modPanel;
  Style tooltip = base;
  tooltip.background = {0.02f, 0.02f, 0.03f, 0.95f};
  tooltip.text = {1.f, 1.f, 0.85f, 1.f};
  t.styles_["tooltip"] = tooltip;
  Style slot = base;
  slot.background = {0.12f, 0.12f, 0.14f, 0.9f};
  slot.hover = {0.45f, 0.45f, 0.5f, 0.95f};
  slot.accent = {1.f, 1.f, 1.f, 0.95f};
  t.styles_["slot"] = slot;
  // C9 2026-11-12 (JD): the HUD hotbar's selected-slot outline has its own class; every other slot and
  // the rest of the white UI chrome keep "slot". D9 2026-11-12 (JD): deeper, more saturated violet
  // #8A2EFF (138,46,255; HSV 266 deg / 82% / 100%, vs C9's #A970FF at 56% saturation; the component
  // readout #A970FF itself is unchanged), opaque, steady, 3 px thick (unselected slots keep the thin
  // dark border).
  Style hotbarSlot = slot;
  hotbarSlot.accent = {138.f / 255.f, 46.f / 255.f, 1.f, 1.f};
  hotbarSlot.selectedWidth = 3.f;
  t.styles_["hotbar_slot"] = hotbarSlot;
  return t;
}

bool Theme::parse(const litholattice::Json& json, Theme& out, std::string& err) {
  using J = litholattice::Json;
  Theme t = defaults();
  if (json.type != J::Type::Object) {
    err = "theme must be an object";
    return false;
  }
  if (const litholattice::Json* g = json.get("glyph")) {
    if (g->type != J::Type::Array || g->arr.size() != 2) {
      err = "'glyph' must be [width, height]";
      return false;
    }
    t.glyph_ = {float(g->arr[0].num), float(g->arr[1].num)};
  }
  t.scale_ = float(json.getNum("scale", 1));
  if (const litholattice::Json* styles = json.get("styles")) {
    Style base = t.styles_["default"];
    if (const litholattice::Json* d = styles->get("default"); d && !parseStyle(*d, base, "default", err)) return false;
    t.styles_["default"] = base;
    for (const auto& [name, value] : styles->obj) {
      if (name == "default") continue;
      auto it = t.styles_.find(name);
      Style s = it != t.styles_.end() ? it->second : base;
      if (!parseStyle(value, s, name, err)) return false;
      t.styles_[name] = s;
    }
  }
  out = std::move(t);
  return true;
}

bool Theme::parse(const std::string& text, Theme& out, std::string& err) {
  litholattice::Json j;
  std::string parseErr;
  if (!litholattice::Json::parse(text, j, &parseErr)) {
    err = parseErr;
    return false;
  }
  return parse(j, out, err);
}

const Style& Theme::style(std::string_view widgetClass) const {
  const auto it = styles_.find(widgetClass);
  return it != styles_.end() ? it->second : styles_.find("default")->second;
}

const Theme& builtinTheme() {
  static const Theme t = Theme::defaults();
  return t;
}

lithogui::Style projectTheme(const Theme& theme, const lithogui::Style& base) {
  lithogui::Style s = base;
  const Style& d = theme.style("default");
  s.panelBg = toGui(d.background);
  s.controlBg = toGui(d.background);
  s.windowBg = toGui(d.background);
  s.controlBgHot = toGui(d.hover);
  s.selection = toGui(d.accent);
  s.text = toGui(d.text);
  s.textMuted = toGui(d.textDisabled);
  s.border = toGui(d.border);
  s.borderWidth = d.borderWidth;
  s.defaultPadding = (d.padding.left + d.padding.right + d.padding.top + d.padding.bottom) * 0.25f;
  // The old glyph cell (8x16 at textScale 1) maps onto the new text sizes:
  // body text renders one glyph cell tall at the base text size.
  s.bodyText.size = theme.glyph().y;
  s.bodyText.lineHeightMul = 1.0f;
  s.titleText.size = theme.glyph().y;
  s.titleText.lineHeightMul = 1.0f;
  s.rowHeight = theme.glyph().y + 6.f;
  s.titleBarHeight = theme.glyph().y + 12.f;
  // NOTE (migration): the old theme scale is a TEXT scale (integer-snapped
  // glyph scaling, applied per widget through pixelTextScale), not a
  // coordinate scale. It is deliberately NOT projected onto uiScale: the
  // facade lays out and routes input in pixel coordinates, so a non-1
  // uiScale would desync hit testing from the facade rects.
  s.uiScale = base.uiScale;
  return s;
}

void Painter::frame(const Rect& r, Color c, float w) {
  if (c.a <= 0 || w <= 0 || r.empty()) return;
  fill({r.x, r.y, r.w, w}, c);
  fill({r.x, r.bottom() - w, r.w, w}, c);
  fill({r.x, r.y + w, w, r.h - 2 * w}, c);
  fill({r.right() - w, r.y + w, w, r.h - 2 * w}, c);
}

void Painter::textIn(const Rect& r, std::string_view s, Color c, bool center, float textScale) {
  const Vec2 g = theme_.glyphSize(textScale);
  const float w = float(s.size()) * g.x;
  const float x = center ? r.x + (r.w - w) * 0.5f : r.x;
  text({x, r.y + (r.h - g.y) * 0.5f}, s, c, textScale);
}

}  // namespace ui
}  // namespace lithokraft
