#pragma once

#include "action52/asset_manifest.hpp"
#include "action52/cheetahmen.hpp"
#include "action52/config.hpp"
#include "action52/ui_assets.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace a52 {

enum class AppScreen : std::uint8_t { Intro, Menu, Unavailable, Cheetahmen, Options };

class App {
 public:
  bool initialize(const std::filesystem::path& assetDirectory, std::string& error);
  void tick(const FrameInput& input);
  void render(PixelBuffer& target) const;

  [[nodiscard]] AppScreen screen() const { return screen_; }
  [[nodiscard]] int selectedGame() const { return selectedGame_; }
  [[nodiscard]] const AssetManifest& manifest() const { return manifest_; }
  [[nodiscard]] AppConfig& config() { return config_; }
  [[nodiscard]] const AppConfig& config() const { return config_; }
  [[nodiscard]] std::span<const AudioEvent> audioEvents() const;
  [[nodiscard]] std::span<const UiAudioCue> uiAudioCues() const { return uiAudioCues_; }

 private:
  AppScreen screen_{AppScreen::Intro};
  AssetManifest manifest_;
  AppConfig config_;
  Cheetahmen cheetahmen_;
  UiAssets uiAssets_;
  std::uint64_t introFrame_{};
  std::uint64_t uiFrame_{};
  int selectedGame_{1};
  int menuPage_{};
  int menuRow_{};
  int menuColumn_{};
  int unavailableFrames_{};
  std::vector<UiAudioCue> uiAudioCues_;

  void updateSelectedGame();
};

void clear(PixelBuffer& target, std::uint32_t color);
void fillRect(PixelBuffer& target, int x, int y, int width, int height, std::uint32_t color);
void drawText(PixelBuffer& target, int x, int y, std::string_view text, std::uint32_t color, int scale = 1);

}  // namespace a52
