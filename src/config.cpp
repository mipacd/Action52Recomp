#include "action52/config.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace a52 {
namespace {
std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}
bool boolean(std::string_view value, bool& result) {
  if (value == "true" || value == "1" || value == "on") { result = true; return true; }
  if (value == "false" || value == "0" || value == "off") { result = false; return true; }
  return false;
}
}

std::string_view profileName(BehaviorProfile profile) {
  switch (profile) {
    case BehaviorProfile::Original: return "original";
    case BehaviorProfile::BugFixes: return "bug-fixes";
    case BehaviorProfile::Remake: return "remake";
  }
  return "original";
}

bool loadConfig(const std::filesystem::path& path, AppConfig& config, std::string& error) {
  std::ifstream file(path);
  if (!file) return true;
  std::string line;
  int lineNumber = 0;
  while (std::getline(file, line)) {
    ++lineNumber;
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;
    const auto equals = line.find('=');
    if (equals == std::string::npos) { error = "invalid config line " + std::to_string(lineNumber); return false; }
    const auto key = trim(line.substr(0, equals));
    const auto value = trim(line.substr(equals + 1));
    if (key == "profile") {
      if (value == "original") config.profile = BehaviorProfile::Original;
      else if (value == "bug-fixes") config.profile = BehaviorProfile::BugFixes;
      else if (value == "remake") config.profile = BehaviorProfile::Remake;
      else { error = "unknown profile on line " + std::to_string(lineNumber); return false; }
    } else if (key == "fullscreen") {
      if (!boolean(value, config.fullscreen)) { error = "invalid fullscreen value"; return false; }
    } else if (key == "square_pixels") {
      if (!boolean(value, config.squarePixels)) { error = "invalid square_pixels value"; return false; }
    } else if (key == "integer_scaling") {
      if (!boolean(value, config.integerScaling)) { error = "invalid integer_scaling value"; return false; }
    } else if (key == "master_volume") {
      try { config.masterVolume = std::clamp(std::stoi(value), 0, 100); }
      catch (...) { error = "invalid master_volume value"; return false; }
    } else if (key == "replacement_pack") config.replacementPack = value;
    else if (key.starts_with("fix.")) {
      if (value == "inherit") { if (!config.fixes.setOverride(key.substr(4), std::nullopt)) { error = "unknown fix: " + key; return false; } }
      else { bool enabled{}; if (!boolean(value, enabled) || !config.fixes.setOverride(key.substr(4), enabled)) { error = "invalid fix override: " + key; return false; } }
    }
  }
  return true;
}

bool saveConfig(const std::filesystem::path& path, const AppConfig& config, std::string& error) {
  std::ofstream file(path);
  if (!file) { error = "cannot write configuration: " + path.string(); return false; }
  file << "profile=" << profileName(config.profile) << '\n'
       << "fullscreen=" << (config.fullscreen ? "true" : "false") << '\n'
       << "square_pixels=" << (config.squarePixels ? "true" : "false") << '\n'
       << "integer_scaling=" << (config.integerScaling ? "true" : "false") << '\n'
       << "master_volume=" << config.masterVolume << '\n'
       << "replacement_pack=" << config.replacementPack.string() << '\n';
  for (const auto& [id, override] : config.fixes.overrides())
    file << "fix." << id << '=' << (override ? (*override ? "true" : "false") : "inherit") << '\n';
  if (!file) { error = "failed writing configuration"; return false; }
  return true;
}
}  // namespace a52

