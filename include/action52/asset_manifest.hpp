#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace a52 {

struct GameAssetEntry {
  int number{};
  std::string title;
  std::uint16_t mapperAddress{};
  std::uint8_t mapperValue{};
  std::uint8_t prgChip{};
  std::uint8_t prgPage{};
  std::uint8_t chrBank{};
};

struct AssetManifest {
  int schemaVersion{};
  std::string romSha256;
  std::vector<GameAssetEntry> games;
};

bool loadManifest(const std::filesystem::path& path, AssetManifest& manifest, std::string& error);

}  // namespace a52

