#pragma once

#include "action52/types.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace a52 {

class UiAssets {
 public:
  bool load(const std::filesystem::path& assetDirectory, std::string& error);
  void renderIntro(std::uint64_t frame, PixelBuffer& target) const;
  void renderMenu(int selectedGame, std::uint64_t frame, PixelBuffer& target) const;

 private:
  std::array<std::vector<std::uint32_t>, 8> introFrames_;
  std::array<std::vector<std::uint32_t>, 3> menuPages_;
  std::array<std::uint8_t, 8192> menuChr_{};

};

}  // namespace a52
