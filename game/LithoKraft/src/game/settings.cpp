#include "lithokraft/game/settings.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "litholattice/core/json.h"

namespace lithokraft {

int nearestTps(int v) {
  int best = kSettingsTpsModes[0];
  for (int i = 1; i < kSettingsTpsModeCount; ++i)
    if (std::abs(v - kSettingsTpsModes[i]) < std::abs(v - best)) best = kSettingsTpsModes[i];
  return best;
}

namespace {

int clampInt(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

}  // namespace

GameSettings GameSettings::load(const std::string& path, const GameSettings& defaults) {
  GameSettings s = defaults;
  if (path.empty()) return s;
  std::ifstream in(path, std::ios::binary);
  if (!in) return s;  // first run: no file yet
  std::stringstream ss;
  ss << in.rdbuf();
  litholattice::Json json;
  std::string err;
  if (!litholattice::Json::parse(ss.str(), json, &err)) return s;  // corrupt: defaults
  s.masterVolume = std::clamp(float(json.getNum("masterVolume", s.masterVolume)), 0.f, 1.f);
  s.muted = json.getBool("muted", s.muted);
  s.hudScale = clampInt(int(json.getNum("hudScale", s.hudScale)), 0, 4);  // Cortana 2026-10-16: 1-4
  s.displayMode = clampInt(int(json.getNum("displayMode", s.displayMode)), 0, kSettingsDisplayModeCount - 1);
  s.vsync = json.getBool("vsync", s.vsync);
  s.swapImages = clampInt(int(json.getNum("swapImages", s.swapImages)), 2, 3);
  s.floorPulse = clampInt(int(json.getNum("floorPulse", s.floorPulse)), 0, 2);
  s.tps = nearestTps(int(json.getNum("tps", s.tps)));
  s.tickRadius = clampInt(int(json.getNum("tickRadius", s.tickRadius)), 0, 512);
  // JD 2026-10-16: the old default 8 moves to the new simulation distance 32 once (files saved
  // before this build carry no "simDistanceV2"); any other saved value is kept.
  if (!json.getBool("simDistanceV2", false) && s.tickRadius == 8) s.tickRadius = 32;
  // Cortana 2026-10-16 2335 F55-5 (JD): default sim distance 32 -> 16 once (files without "simDistanceV3").
  if (!json.getBool("simDistanceV3", false) && s.tickRadius == 32) s.tickRadius = 16;
  // F56-10 (Proposed by JD, Built by Cortana, 2026-10-17): default sim distance 16 -> 8 once (files without "simDistanceV4").
  if (!json.getBool("simDistanceV4", false) && s.tickRadius == 16) s.tickRadius = 8;
  s.streaming = json.getBool("streaming", s.streaming);
  s.streamRadius = clampInt(int(json.getNum("streamRadius", s.streamRadius)), 0, 64);
  s.euv = json.getBool("euv", s.euv);
  s.mesher = clampInt(int(json.getNum("mesher", s.mesher)), 0, kSettingsMesherCount - 1);
  s.pbrTier = clampInt(int(json.getNum("pbrTier", s.pbrTier)), 0, 3);
  s.ambientMode = clampInt(int(json.getNum("ambientMode", s.ambientMode)), 0, 1);
  s.fpsCap = clampInt(int(json.getNum("fpsCap", s.fpsCap)), -1, 1000);  // Cortana 2026-10-15 1610: -1 = auto
  if (const std::string r = json.getStr("renderer", s.renderer); r == "software" || r == "gl33" || r == "vulkan") s.renderer = r;  // JD 2026-10-24
  s.fpsCounter = json.getBool("fpsCounter", s.fpsCounter);
  s.timingHandshake = json.getBool("timingHandshake", s.timingHandshake);
  s.handshakePhases = clampInt(int(json.getNum("handshakePhases", s.handshakePhases)), 1, 16);
  s.renderThreads = clampInt(int(json.getNum("renderThreads", s.renderThreads)), 0, 64);  // Cortana 2026-10-15 1156
  s.bulbShellSolid = json.getBool("bulbShellSolid", s.bulbShellSolid);  // 1551 (JD)
  s.vkFx = clampInt(int(json.getNum("vkFx", s.vkFx)), 0, 127);  // JD Vulkan push
  s.vkBloomStrength = std::clamp(float(json.getNum("vkBloomStrength", s.vkBloomStrength)), 0.f, 4.f);
  s.vkHeartbeatGlow = std::clamp(float(json.getNum("vkHeartbeatGlow", s.vkHeartbeatGlow)), 0.f, 4.f);  // [C53-3]
  s.vkFarLod = json.getBool("vkFarLod", s.vkFarLod);  // [C54-1]
  s.roundSimArea = json.getBool("roundSimArea", s.roundSimArea);  // F56-7
  s.diamondSimArea = json.getBool("diamondSimArea", s.diamondSimArea);  // diamond (JD 2026-10-23)
  s.diamondStreamArea = json.getBool("diamondStreamArea", s.diamondStreamArea);  // diamond (JD 2026-10-23)
  return s;
}

bool GameSettings::save(const std::string& path) const {
  if (path.empty()) return false;
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) return false;
  litholattice::Json json = litholattice::Json::object();
  json.add("masterVolume", litholattice::Json::number(masterVolume));
  json.add("muted", litholattice::Json::boolean(muted));
  json.add("hudScale", litholattice::Json::number(hudScale));
  json.add("displayMode", litholattice::Json::number(displayMode));
  json.add("vsync", litholattice::Json::boolean(vsync));
  json.add("swapImages", litholattice::Json::number(swapImages));
  json.add("floorPulse", litholattice::Json::number(floorPulse));
  json.add("tps", litholattice::Json::number(tps));
  json.add("tickRadius", litholattice::Json::number(tickRadius));
  json.add("simDistanceV2", litholattice::Json::boolean(true));
  json.add("simDistanceV3", litholattice::Json::boolean(true));  // Cortana 2026-10-16 2335 F55-5
  json.add("simDistanceV4", litholattice::Json::boolean(true));  // F56-10
  json.add("streaming", litholattice::Json::boolean(streaming));
  json.add("streamRadius", litholattice::Json::number(streamRadius));
  json.add("euv", litholattice::Json::boolean(euv));
  json.add("mesher", litholattice::Json::number(mesher));
  json.add("pbrTier", litholattice::Json::number(pbrTier));
  json.add("ambientMode", litholattice::Json::number(ambientMode));
  json.add("fpsCap", litholattice::Json::number(fpsCap));
  json.add("renderer", litholattice::Json::string(renderer));  // JD 2026-10-24
  json.add("fpsCounter", litholattice::Json::boolean(fpsCounter));  // Cortana 2026-10-15 1610
  json.add("timingHandshake", litholattice::Json::boolean(timingHandshake));
  json.add("handshakePhases", litholattice::Json::number(handshakePhases));
  json.add("renderThreads", litholattice::Json::number(renderThreads));  // Cortana 2026-10-15 1156
  json.add("bulbShellSolid", litholattice::Json::boolean(bulbShellSolid));  // 1551 (JD)
  json.add("vkFx", litholattice::Json::number(vkFx));  // JD Vulkan push
  json.add("vkBloomStrength", litholattice::Json::number(vkBloomStrength));
  json.add("vkHeartbeatGlow", litholattice::Json::number(vkHeartbeatGlow));  // [C53-3]
  json.add("vkFarLod", litholattice::Json::boolean(vkFarLod));  // [C54-1]
  json.add("roundSimArea", litholattice::Json::boolean(roundSimArea));  // F56-7
  json.add("diamondSimArea", litholattice::Json::boolean(diamondSimArea));  // diamond (JD 2026-10-23)
  json.add("diamondStreamArea", litholattice::Json::boolean(diamondStreamArea));  // diamond (JD 2026-10-23)
  out << json.dump(2) << "\n";
  return bool(out);
}

}  // namespace lithokraft
