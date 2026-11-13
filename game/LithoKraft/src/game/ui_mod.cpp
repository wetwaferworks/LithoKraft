// ui_mod.cpp: the widget factory (closed JSON type table) plus the mod UI
// confinement layer (cModPanel + ModUiFactory), ported from the old
// toolkit's widget_factory.cpp / mod_ui.cpp onto facade widgets. Behavior
// and error strings follow the old code.
#include "lithokraft/game/ui_mod.h"
#include <functional>
#include <map>
#include <set>
#include <vector>
#include "lithokraft/game/ui_textedit.h"

namespace lithokraft {
namespace ui {
namespace {

using Json = litholattice::Json;

const std::set<std::string, std::less<>> kCommonKeys = {"type", "id", "style", "tooltip", "visible", "enabled",
                                                       "layout", "on", "children"};

Vec2 vec2(const Json* j, Vec2 def) {
  if (!j || j->type != Json::Type::Array || j->arr.size() != 2) return def;
  return {float(j->arr[0].num), float(j->arr[1].num)};
}

Edges edges(const Json* j) {
  if (!j) return {};
  if (j->type == Json::Type::Number) return Edges::all(float(j->num));
  if (j->type == Json::Type::Array && j->arr.size() == 4)
    return {float(j->arr[0].num), float(j->arr[1].num), float(j->arr[2].num), float(j->arr[3].num)};
  return {};
}

std::vector<std::string> strings(const Json* j) {
  std::vector<std::string> out;
  if (j && j->type == Json::Type::Array)
    for (const Json& v : j->arr) out.push_back(v.str);
  return out;
}

std::string text(const Json& j, const char* key) {
  const Json* v = j.get(key);
  return v && v->type == Json::Type::String ? v->str : std::string();
}

Orientation orientation(const Json& j, Orientation def) {
  const std::string o = text(j, "orientation");
  if (o == "horizontal") return Orientation::Horizontal;
  if (o == "vertical") return Orientation::Vertical;
  return def;
}

struct TypeInfo {
  std::set<std::string, std::less<>> keys;  // type-specific keys
  std::function<std::unique_ptr<cWidget>(const Json&)> make;
  bool takesChildren = true;
};

const std::map<std::string, TypeInfo, std::less<>>& types() {
  static const std::map<std::string, TypeInfo, std::less<>> kTypes = {
      {"widget", {{}, [](const Json&) { return std::make_unique<cWidget>(); }}},
      {"panel", {{}, [](const Json&) { return std::make_unique<cPanel>(); }}},
      {"label", {{"text"}, [](const Json& j) { return std::make_unique<cLabel>(text(j, "text")); }, false}},
      {"button", {{"text"}, [](const Json& j) { return std::make_unique<cButton>(text(j, "text")); }, false}},
      {"image",
       {{"image", "size"}, [](const Json& j) { return std::make_unique<Image>(text(j, "image"), vec2(j.get("size"), {32, 32})); }, false}},
      {"checkbox",
       {{"text", "checked", "toggle"},
        [](const Json& j) { return std::make_unique<cCheckbox>(text(j, "text"), j.getBool("checked", false), j.getBool("toggle", false)); },
        false}},
      {"progress", {{"value"}, [](const Json& j) {
                      auto p = std::make_unique<cProgressBar>();
                      p->setValue(float(j.getNum("value", 0)));
                      return p;
                    }, false}},
      {"slider",
       {{"min", "max", "value", "step"},
        [](const Json& j) {
          return std::make_unique<cSlider>(float(j.getNum("min", 0)), float(j.getNum("max", 1)), float(j.getNum("value", 0)),
                                           float(j.getNum("step", 0)));
        },
        false}},
      {"number",
       {{"min", "max", "value"},
        [](const Json& j) {
          return std::make_unique<cNumberField>(int(j.getNum("min", 0)), int(j.getNum("max", 64)), int(j.getNum("value", 0)));
        },
        false}},
      {"slot", {{"size", "image"}, [](const Json& j) {
                  auto s = std::make_unique<cSlot>(vec2(j.get("size"), {40, 40}));
                  s->setIcon(text(j, "image"));
                  return s;
                }, false}},
      {"divider", {{"orientation", "thickness"}, [](const Json& j) {
                    return std::make_unique<cDivider>(orientation(j, Orientation::Horizontal), float(j.getNum("thickness", 1)));
                  }, false}},
      {"stack",
       {{"orientation", "spacing"},
        [](const Json& j) { return std::make_unique<cStackPanel>(orientation(j, Orientation::Vertical), float(j.getNum("spacing", 4))); }}},
      {"grid",
       {{"columns", "cell", "spacing"},
        [](const Json& j) {
          return std::make_unique<cGridPanel>(int(j.getNum("columns", 4)), vec2(j.get("cell"), {40, 40}), float(j.getNum("spacing", 4)));
        }}},
      {"textbox", {{"text", "placeholder"}, [](const Json& j) {
                     auto t = std::make_unique<cTextBox>(text(j, "text"));
                     t->setPlaceholder(text(j, "placeholder"));
                     return t;
                   }, false}},
      {"editor", {{"text", "line_numbers"}, [](const Json& j) {
                    auto t = std::make_unique<cTextEditor>();
                    t->setText(text(j, "text"));
                    t->setLineNumbers(j.getBool("line_numbers", true));
                    return t;
                  }, false}},
      {"list", {{"items", "selected"}, [](const Json& j) {
                  auto l = std::make_unique<cListView>();
                  l->setItems(strings(j.get("items")));
                  l->setSelected(int(j.getNum("selected", -1)));
                  return l;
                }, false}},
      {"dropdown",
       {{"items", "selected"},
        [](const Json& j) { return std::make_unique<cDropdown>(strings(j.get("items")), int(j.getNum("selected", 0))); },
        false}},
  };
  return kTypes;
}

std::unique_ptr<cWidget> build(const Json& j, const std::string& path, std::string& err) {
  if (j.type != Json::Type::Object) {
    err = path + ": widget must be an object";
    return nullptr;
  }
  const std::string type = text(j, "type");
  const auto& table = types();
  // Containers with special child handling.
  const bool scroll = type == "scroll", split = type == "splitter", tabs = type == "tabs";
  const auto it = table.find(type);
  if (it == table.end() && !scroll && !split && !tabs) {
    err = path + ": unknown widget type '" + type + "'";
    return nullptr;
  }
  static const std::set<std::string, std::less<>> kSplitKeys = {"orientation", "ratio"}, kTabKeys = {"titles"}, kNone;
  const auto& extra = scroll ? kNone : split ? kSplitKeys : tabs ? kTabKeys : it->second.keys;
  for (const auto& [key, value] : j.obj)
    if (!kCommonKeys.count(key) && !extra.count(key)) {
      err = path + ": unknown key '" + key + "' for type '" + type + "'";
      return nullptr;
    }

  // Children first (so scroll / splitter / tabs can adopt them).
  std::vector<std::unique_ptr<cWidget>> kids;
  if (const Json* c = j.get("children")) {
    if (it != table.end() && !it->second.takesChildren) {
      err = path + ": '" + type + "' takes no children";
      return nullptr;
    }
    for (size_t i = 0; i < c->arr.size(); ++i) {
      auto k = build(c->arr[i], path + "/" + type + "[" + std::to_string(i) + "]", err);
      if (!k) return nullptr;
      kids.push_back(std::move(k));
    }
  }

  std::unique_ptr<cWidget> w;
  if (scroll) {
    if (kids.size() != 1) {
      err = path + ": 'scroll' needs exactly one child";
      return nullptr;
    }
    auto sv = std::make_unique<cScrollView>();
    sv->setContent(std::move(kids[0]));
    w = std::move(sv);
  } else if (split) {
    if (kids.size() != 2) {
      err = path + ": 'splitter' needs exactly two children";
      return nullptr;
    }
    auto sp = std::make_unique<cSplitter>(orientation(j, Orientation::Horizontal), float(j.getNum("ratio", 0.3)));
    sp->setPane(0, std::move(kids[0]));
    sp->setPane(1, std::move(kids[1]));
    w = std::move(sp);
  } else if (tabs) {
    const std::vector<std::string> titles = strings(j.get("titles"));
    if (titles.size() != kids.size()) {
      err = path + ": 'tabs' needs one title per child";
      return nullptr;
    }
    auto tv = std::make_unique<cTabView>();
    for (size_t i = 0; i < kids.size(); ++i) tv->addTab(titles[i], std::move(kids[i]));
    w = std::move(tv);
  } else {
    w = it->second.make(j);
    for (auto& k : kids) w->addChild(std::move(k));
  }

  w->setId(text(j, "id"));
  if (const std::string s = text(j, "style"); !s.empty()) w->setStyleClass(s);
  w->setTooltip(text(j, "tooltip"));
  w->setVisible(j.getBool("visible", true));
  w->setEnabled(j.getBool("enabled", true));
  if (const Json* l = j.get("layout")) {
    Layout& lay = w->layout();
    lay.anchorMin = vec2(l->get("anchor_min"), lay.anchorMin);
    lay.anchorMax = vec2(l->get("anchor_max"), lay.anchorMax);
    lay.insets = edges(l->get("insets"));
    lay.offset = vec2(l->get("offset"), {});
    lay.pivot = vec2(l->get("pivot"), {});
    lay.size = vec2(l->get("size"), {});
    lay.minSize = vec2(l->get("min_size"), {});
    lay.margin = edges(l->get("margin"));
    lay.flex = float(l->getNum("flex", 0));
  }
  if (const Json* on = j.get("on"))
    for (const auto& [event, decl] : on->obj) w->onScript(event, decl.str);
  return w;
}

}  // namespace

std::unique_ptr<cWidget> buildWidget(const Json& json, std::string& err) { return build(json, "ui", err); }

std::unique_ptr<cWidget> buildWidget(const std::string& text, std::string& err) {
  Json j;
  if (!Json::parse(text, j, &err)) return nullptr;
  return buildWidget(j, err);
}

// ------------------------------------------------------------- mod layer --
namespace {

// The MOD tag colours are FIXED (not theme lookups): no theme pack — and no
// mod, which ships no theme packs — can restyle or hide the badge.
constexpr Color kBadgeStripBg{0.10f, 0.07f, 0.02f, 1.f};
constexpr Color kBadgeTagBg{1.f, 0.62f, 0.f, 1.f};
constexpr Color kBadgeTagText{0.f, 0.f, 0.f, 1.f};
constexpr Color kBadgeText{1.f, 0.85f, 0.55f, 1.f};

bool isIdChar(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; }

// Iterative pre-scan of mod JSON: nesting depth + node count, so the
// recursive widget factory below can never be stack-overflowed by a hostile
// layout. Counts every value node; depth counts object/array nesting.
bool scanJson(const Json& root, std::string& err) {
  struct Frame {
    const Json* j;
    size_t child = 0;
  };
  std::vector<Frame> stack;
  stack.push_back({&root, 0});
  size_t nodes = 0;
  size_t maxDepth = 1;
  while (!stack.empty()) {
    Frame& f = stack.back();
    if (f.child == 0) {
      ++nodes;
      if (nodes > kModUiMaxJsonNodes) {
        err = "mod UI: layout exceeds the node budget";
        return false;
      }
      maxDepth = std::max(maxDepth, stack.size());
      if (maxDepth > kModUiMaxJsonDepth) {
        err = "mod UI: layout nests too deep";
        return false;
      }
    }
    const Json* next = nullptr;
    if (f.j->type == Json::Type::Object && f.child < f.j->obj.size()) {
      next = &f.j->obj[f.child].second;
      ++f.child;
    } else if (f.j->type == Json::Type::Array && f.child < f.j->arr.size()) {
      next = &f.j->arr[f.child];
      ++f.child;
    }
    if (next) {
      stack.push_back({next, 0});
    } else {
      stack.pop_back();
    }
  }
  return true;
}

// Walks a built widget tree (iterative): counts widgets and rejects reserved
// style classes anywhere in the subtree, including on the content root.
bool validateTree(const cWidget& root, std::string& err) {
  std::vector<const cWidget*> stack{&root};
  size_t count = 0;
  while (!stack.empty()) {
    const cWidget* w = stack.back();
    stack.pop_back();
    if (++count > kModUiMaxWidgets) {
      err = "mod UI: panel exceeds the widget budget";
      return false;
    }
    if (ModUiFactory::isReservedStyle(w->styleClass())) {
      err = std::string("mod UI: reserved style class '") + w->styleClass() +
            "' is not available to mods";
      return false;
    }
    for (const auto& child : w->children())
      if (const auto* cw = dynamic_cast<const cWidget*>(child.get())) stack.push_back(cw);
  }
  return true;
}

}  // namespace

cModPanel::cModPanel(std::string modId, std::string title)
    : cPanel(kModPanelStyleClass), modId_(std::move(modId)), title_(std::move(title)) {
  body_ = &add<cStackPanel>(Orientation::Vertical, 6.f);
}

float cModPanel::badgeHeight() const { return theme().glyphSize(style().textScale).y + 8.f; }

Rect cModPanel::badgeRect() const {
  const float h = badgeHeight() + style().padding.top;
  const Rect r = fromGui(rect());
  return {r.x, r.y, r.w, h};
}

void cModPanel::arrange(const Theme& theme) {
  const Rect c = fromGui(rect()).inset(style().padding);
  const float strip = badgeHeight() + style().padding.top;
  body_->place({c.x, c.y + strip, c.w, std::max(0.f, c.h - strip)}, theme);
}

void cModPanel::paint(Painter& p) const { cWidget::paint(p); }  // background + border of the "mod_panel" frame treatment

// Children are painted here, once, clipped to the body (a negative margin
// can't reach the strip), and the badge is painted LAST: nothing the mod
// owns can cover or remove it.
void cModPanel::paintChildren(Painter& p) const {
  {
    Painter::ClipScope clip(p, fromGui(body_->rect()));
    cWidget::paintChildren(p);
  }
  paintBadge(p);
}

void cModPanel::paintBadge(Painter& p) const {
  const Rect bar = badgeRect();
  if (bar.empty()) return;
  p.fill(bar, kBadgeStripBg);
  const float tagW = theme().textWidth("MOD", style().textScale) + 12.f;
  const Rect tag{bar.x + 6.f, bar.y + 4.f, tagW, std::max(0.f, bar.h - 8.f)};
  p.fill(tag, kBadgeTagBg);
  p.text({tag.x + 6.f, tag.y + 2.f}, "MOD", kBadgeTagText, style().textScale);
  const std::string who = modId_ + " - " + title_;
  p.text({tag.right() + 8.f, bar.y + 4.f}, who, kBadgeText, style().textScale);
  // A fixed amber rule under the strip: the frame treatment survives even a
  // pack that recolours "mod_panel" into invisibility.
  p.fill({bar.x, bar.bottom() - 1.f, bar.w, 1.f}, kBadgeTagBg);
}

bool ModUiFactory::isReservedStyle(std::string_view styleClass) {
  for (const char* r : kModReservedStyles)
    if (styleClass == r) return true;
  return false;
}

bool ModUiFactory::isValidModId(std::string_view id) {
  if (id.empty() || id.size() > 64) return false;
  for (char c : id)
    if (!isIdChar(c)) return false;
  return true;
}

std::unique_ptr<cModPanel> ModUiFactory::createPanel(const std::string& modId,
                                                     const std::string& panelId,
                                                     const std::string& title,
                                                     const std::string& bodyJson,
                                                     std::string& err) {
  if (!isValidModId(modId)) {
    err = "mod UI: invalid mod id (want [a-z0-9_]{1,64})";
    return nullptr;
  }
  if (!isValidModId(panelId)) {
    err = "mod UI: invalid panel id (want [a-z0-9_]{1,64})";
    return nullptr;
  }
  if (title.empty() || title.size() > kModUiMaxTitleLen) {
    err = "mod UI: title must be 1..128 characters";
    return nullptr;
  }

  auto panel = std::make_unique<cModPanel>(modId, title);
  panel->setId(panelId);

  if (!bodyJson.empty()) {
    Json json;
    if (!Json::parse(bodyJson, json, &err)) {
      err = "mod UI: bad layout JSON: " + err;
      return nullptr;
    }
    if (!scanJson(json, err)) return nullptr;  // depth / node DoS guard
    std::string buildErr;
    std::unique_ptr<cWidget> content = buildWidget(json, buildErr);
    if (!content) {
      err = "mod UI: " + buildErr;
      return nullptr;
    }
    // The factory's type table is closed (no dialog/modal/popup types exist),
    // and mod handlers are script functions only; the remaining check is the
    // reserved style classes (no system-look spoofing).
    if (!validateTree(*content, err)) return nullptr;
    panel->body().addChild(std::move(content));
  }
  return panel;
}

cModPanel& ModUiFactory::attachToRoot(cUiRoot& ui, std::unique_ptr<cModPanel> panel) {
  // Main layer only. Modals and popups stay engine-exclusive: a mod can never
  // seize the screen with a fake system prompt.
  cWidget& w = ui.root().addChild(std::move(panel));
  return static_cast<cModPanel&>(w);
}

}  // namespace ui
}  // namespace lithokraft
