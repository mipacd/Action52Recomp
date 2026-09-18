#pragma once

#include "action52/fix_registry.hpp"

#include <filesystem>
#include <string>

namespace a52 {

struct AppConfig {
  BehaviorProfile profile{BehaviorProfile::Original};
  bool fullscreen{};
  bool squarePixels{};
  bool integerScaling{true};
  int masterVolume{80};
  std::filesystem::path replacementPack;
  FixRegistry fixes;
};

bool loadConfig(const std::filesystem::path& path, AppConfig& config, std::string& error);
bool saveConfig(const std::filesystem::path& path, const AppConfig& config, std::string& error);
std::string_view profileName(BehaviorProfile profile);

}  // namespace a52

