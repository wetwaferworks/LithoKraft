// Emscripten stand-in for the Vulkan backend factory — WASM checkpoint,
// 2026-10-18 overnight. NEW FILE: the real render_vk library is untouched and
// still linked on native builds; CMake adds this stub to the lithokraft exe
// only under EMSCRIPTEN, where src/render_vk is not built (no Vulkan in the
// browser). Both entry points fail exactly the way they do on a machine with
// no Vulkan loader, so the app's existing fallback (Vulkan -> GL33) and the
// selftest's environmental SKIP path behave identically.
#include "litholattice/render_vk/vulkan_renderer.h"

namespace litholattice {

std::unique_ptr<IRenderer> createVulkanRenderer(const VulkanRendererDesc&, std::string* error) {
  if (error) *error = "Vulkan is not available in the browser (WebAssembly) build";
  return nullptr;
}

VulkanProbe probeVulkan() {
  VulkanProbe p;
  p.loaderFound = false;
  p.ok = false;
  p.noDevices = true;  // environmental absence: selftest reports SKIP, not FAIL
  p.summary = "no Vulkan in the browser (WebAssembly) build";
  return p;
}

}  // namespace litholattice
