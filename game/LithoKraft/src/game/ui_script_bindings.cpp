#include "lithokraft/game/ui_script_bindings.h"
#include <string>
#include "litholattice/core/log.h"
#include "litholattice/script/script_generic.h"
#include "litholattice/ui/controls.h"
#include "litholattice/ui/lists.h"
#include "litholattice/ui/text_edit.h"
#include "litholattice/ui/widget_factory.h"
#include "litholattice/ui/mod_ui.h"

namespace lithokraft {
namespace {

ui::Widget* find(UiScriptBindings* b, const std::string& id) {
  ui::Widget* w = b->ui().root().find(id);
  if (!w) logError("[script] ui: no widget '%s'", id.c_str());
  return w;
}

std::string uiText(const std::string& id, UiScriptBindings* b) {
  ui::Widget* w = find(b, id);
  if (auto* l = dynamic_cast<ui::Label*>(w)) return l->text();
  if (auto* t = dynamic_cast<ui::Button*>(w)) return t->text();
  if (auto* e = dynamic_cast<ui::cTextEditBase*>(w)) return e->text();
  if (auto* d = dynamic_cast<ui::cDropdown*>(w); d && !d->items().empty()) return d->items()[size_t(d->selected())];
  if (auto* l = dynamic_cast<ui::cListView*>(w); l && l->selectedItem()) return *l->selectedItem();
  return {};
}

void uiSetText(const std::string& id, const std::string& text, UiScriptBindings* b) {
  ui::Widget* w = find(b, id);
  if (auto* l = dynamic_cast<ui::Label*>(w)) l->setText(text);
  else if (auto* t = dynamic_cast<ui::Button*>(w)) t->setText(text);
  else if (auto* e = dynamic_cast<ui::cTextEditBase*>(w)) e->setText(text);
}

bool uiChecked(const std::string& id, UiScriptBindings* b) {
  auto* c = dynamic_cast<ui::cCheckbox*>(find(b, id));
  return c && c->checked();
}

void uiSetChecked(const std::string& id, bool on, UiScriptBindings* b) {
  if (auto* c = dynamic_cast<ui::cCheckbox*>(find(b, id))) c->setChecked(on);
}

float uiValue(const std::string& id, UiScriptBindings* b) {
  ui::Widget* w = find(b, id);
  if (auto* s = dynamic_cast<ui::cSlider*>(w)) return s->value();
  if (auto* n = dynamic_cast<ui::cNumberField*>(w)) return float(n->value());
  if (auto* p = dynamic_cast<ui::cProgressBar*>(w)) return p->value();
  if (auto* d = dynamic_cast<ui::cDropdown*>(w)) return float(d->selected());
  if (auto* l = dynamic_cast<ui::cListView*>(w)) return float(l->selected());
  return 0.f;
}

void uiSetValue(const std::string& id, float v, UiScriptBindings* b) {
  ui::Widget* w = find(b, id);
  if (auto* s = dynamic_cast<ui::cSlider*>(w)) s->setValue(v);
  else if (auto* n = dynamic_cast<ui::cNumberField*>(w)) n->setValue(int(v));
  else if (auto* p = dynamic_cast<ui::cProgressBar*>(w)) p->setValue(v);
  else if (auto* d = dynamic_cast<ui::cDropdown*>(w)) d->setSelected(int(v));
  else if (auto* l = dynamic_cast<ui::cListView*>(w)) l->setSelected(int(v));
}

void uiSetVisible(const std::string& id, bool on, UiScriptBindings* b) {
  if (ui::Widget* w = find(b, id)) w->setVisible(on);
}

void uiSetEnabled(const std::string& id, bool on, UiScriptBindings* b) {
  if (ui::Widget* w = find(b, id)) w->setEnabled(on);
}

bool uiShow(const std::string& json, UiScriptBindings* b) {
  // Hardened path: ModUiFactory::createPanel pre-scans the JSON (depth/node
  // DoS guard), builds via the closed type table, rejects reserved style
  // classes, enforces the widget budget, and wraps everything in a badged
  // ModPanel. Never call buildWidget directly here (bypasses all of that).
  std::string err;
  litholattice::Json parsed;
  if (!litholattice::Json::parse(json, parsed, &err)) {
    logError("[script] ui_show: bad JSON: %s", err.c_str());
    return false;
  }
  const std::string panelId = parsed.getStr("id", "script_panel");
  const std::string title = parsed.getStr("title", "Script UI");
  auto panel = ui::ModUiFactory::createPanel("script", panelId, title, json, err);
  if (!panel) {
    logError("[script] ui_show: %s", err.c_str());
    return false;
  }
  ui::ModUiFactory::attachToRoot(b->ui(), std::move(panel));
  return true;
}

void uiClose(const std::string& id, UiScriptBindings* b) {
  ui::Widget* w = find(b, id);
  if (w && w->parent()) b->ui().destroyLater(w->parent()->removeChild(*w));  // may be closing itself
}

}  // namespace

bool UiScriptBindings::registerApi() {
  // Every binding goes through the script binding allowlist: the engine
  // refuses anything not allowlisted, or not covered by the mod's permissions
  // (the game sets those on the ScriptEngine before calling registerApi()).
  struct Fn {
    const char* decl;
    ScriptEngine::ScriptFn fn;  // fix 3c: native + generic forms
  };
  const Fn fns[] = {
      {"string ui_text(const string &in)", LL_SCRIPT_FN_OBJLAST(uiText)},
      {"void ui_setText(const string &in, const string &in)", LL_SCRIPT_FN_OBJLAST(uiSetText)},
      {"bool ui_checked(const string &in)", LL_SCRIPT_FN_OBJLAST(uiChecked)},
      {"void ui_setChecked(const string &in, bool)", LL_SCRIPT_FN_OBJLAST(uiSetChecked)},
      {"float ui_value(const string &in)", LL_SCRIPT_FN_OBJLAST(uiValue)},
      {"void ui_setValue(const string &in, float)", LL_SCRIPT_FN_OBJLAST(uiSetValue)},
      {"void ui_setVisible(const string &in, bool)", LL_SCRIPT_FN_OBJLAST(uiSetVisible)},
      {"void ui_setEnabled(const string &in, bool)", LL_SCRIPT_FN_OBJLAST(uiSetEnabled)},
      {"bool ui_show(const string &in)", LL_SCRIPT_FN_OBJLAST(uiShow)},
      {"void ui_close(const string &in)", LL_SCRIPT_FN_OBJLAST(uiClose)},
  };
  for (const Fn& f : fns)
    if (scripts_.bindFunction(f.decl, f.fn, this) < 0) {
      logError("AngelScript: registering %s failed (allowlist or permissions)", f.decl);
      return false;
    }
  return true;
}

bool UiScriptBindings::callHandler(std::string_view functionDecl, std::string_view widgetId) {
  return scripts_.callWithString(std::string(functionDecl), std::string(widgetId));
}

}  // namespace lithokraft
