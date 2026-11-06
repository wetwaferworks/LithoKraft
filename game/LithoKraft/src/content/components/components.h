// Private to lithokraft_content: registration hooks for the built-in block behaviours.
#pragma once
#include <functional>
#include <memory>
#include <string_view>
#include "lithokraft/content/component.h"

namespace lithokraft::components {

using Registrar = std::function<void(std::string_view name, std::unique_ptr<cBlockBehavior>)>;

void registerPassive(const Registrar& add);  // wire, torch, lamp, color lamp (readout only)
void registerTorch(const Registrar& add);   // 2026-11-05 (JD): LED colour on cTorch
void registerLogic(const Registrar& add);    // repeator, thresholder, redirector, pulsor, memory cell, fast latch
void registerInputs(const Registrar& add);   // switch, button, keyboard
void registerSound(const Registrar& add);    // tone block
void registerCertificateTable(const Registrar& add);  // certificate table (describe-only seam; signing mechanic game-side, B7)
void registerPosts(const Registrar& add);             // 2026-10-25 Posts (standing / wall readouts)
void registerEtchor(const Registrar& add);            // Etchor (describe-only seam; etch flow game-side, B6)

}  // namespace lithokraft::components
