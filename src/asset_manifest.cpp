#include "action52/asset_manifest.hpp"

#include <fstream>
#include <regex>
#include <sstream>

namespace a52 {

bool loadManifest(const std::filesystem::path& path, AssetManifest& manifest, std::string& error) {
  std::ifstream file(path);
  if (!file) { error = "Assets are missing. Run a52_extract with your verified ROM, or configure CMake with -DA52_ROM=<path>."; return false; }
  std::ostringstream contents; contents << file.rdbuf();
  const auto text = contents.str();
  std::smatch match;
  if (!std::regex_search(text, match, std::regex(R"("schema_version"\s*:\s*(\d+))"))) { error = "manifest has no schema_version"; return false; }
  manifest.schemaVersion = std::stoi(match[1].str());
  if (manifest.schemaVersion != 1) { error = "unsupported asset manifest schema"; return false; }
  if (!std::regex_search(text, match, std::regex(R"re("rom_sha256"\s*:\s*"([0-9a-f]{64})")re"))) { error = "manifest has no valid ROM hash"; return false; }
  manifest.romSha256 = match[1].str();
  const std::regex gamePattern(R"re(\{"number":(\d+),"title":"([^"]+)","mapper_address":(\d+),"mapper_value":(\d+),"menu_selector":\d+,"prg_chip":(\d+),"prg_page":(\d+),"chr_bank":(\d+)\})re");
  manifest.games.clear();
  for (auto it = std::sregex_iterator(text.begin(), text.end(), gamePattern); it != std::sregex_iterator(); ++it) {
    const auto& m = *it;
    manifest.games.push_back({std::stoi(m[1].str()), m[2].str(), static_cast<std::uint16_t>(std::stoi(m[3].str())),
                              static_cast<std::uint8_t>(std::stoi(m[4].str())), static_cast<std::uint8_t>(std::stoi(m[5].str())),
                              static_cast<std::uint8_t>(std::stoi(m[6].str())), static_cast<std::uint8_t>(std::stoi(m[7].str()))});
  }
  if (manifest.games.size() != 52) { error = "manifest must contain exactly 52 games"; return false; }
  return true;
}
}  // namespace a52
