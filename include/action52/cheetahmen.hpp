#pragma once

#include "action52/fix_registry.hpp"
#include "action52/game_module.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace a52 {

class Cheetahmen final : public GameModule {
 public:
  bool loadAssets(const std::filesystem::path& assetDirectory, std::string& error);
  void reset(BehaviorProfile profile, const FixRegistry& fixes, std::uint32_t seed) override;
  void tick(const FrameInput& input) override;
  void render(PixelBuffer& target) const override;
  [[nodiscard]] std::span<const AudioEvent> audioEvents() const override;
  [[nodiscard]] std::vector<std::byte> serialize() const override;
  bool deserialize(std::span<const std::byte> state) override;
  [[nodiscard]] std::uint64_t stateHash() const override;
  [[nodiscard]] bool requestsMenu() const override { return menuRequested_; }

 private:
  struct State {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t vx{};
    std::int32_t vy{};
    std::uint32_t rng{};
    std::uint32_t score{};
    std::uint32_t frame{};
    std::uint32_t openingFrame{};
    std::uint32_t phaseFrame{};
    std::int32_t cameraX{};
    std::uint16_t stage{};
    std::uint8_t lives{};
    std::uint8_t health{};
    std::uint8_t flags{};
    std::uint8_t phase{};
    std::uint8_t selection{};
    std::uint8_t achievements{};
  } state_{};

  BehaviorProfile profile_{BehaviorProfile::Original};
  const FixRegistry* fixes_{};
  bool menuRequested_{};
  int jumpBuffer_{};
  int coyoteFrames_{};
  bool paused_{};
  std::uint8_t pausePage_{};
  std::uint8_t pauseSelection_{};
  std::uint16_t pauseMessageFrames_{};
  bool cheatInvincible_{};
  bool cheatInfiniteLives_{};
  bool hasSave_{};
  State savedState_{};
  std::array<AudioEvent, 4> events_{};
  std::size_t eventCount_{};
  struct MusicChannel {
    std::uint16_t pc{};
    std::uint16_t loopStart{};
    std::uint16_t returnPc{};
    std::uint16_t patternTable{};
    std::uint16_t loopCommand{};
    std::uint16_t duration{};
    std::uint8_t note{};
    std::uint8_t volume{};
    std::uint8_t duty{};
    std::uint8_t loopRemaining{};
    bool active{};
    bool audible{};
  };
  std::array<MusicChannel,4> openingMusic_{};
  std::array<MusicChannel,4> stageMusic_{};
  std::array<MusicChannel,4> savedStageMusic_{};
  std::array<std::uint8_t,16384> openingPrg_{};
  std::array<std::array<std::uint8_t,8192>,3> openingChr_{};
  std::array<std::uint8_t,32768> gamePrg_{};
  std::array<std::uint8_t,8192> selectionChr_{};
  std::array<std::uint8_t,8192> instructionChr_{};
  std::array<std::uint8_t,8192> gameChr_{};
  std::array<std::vector<std::uint32_t>,6> openingBackgrounds_;
  std::array<std::vector<std::uint32_t>,6> openingTransitionCorruption_;
  std::vector<std::uint32_t> stageWorld_;
  std::vector<std::uint32_t> characterIntro_;
  std::vector<std::uint8_t> stagePalettePixels_;
  std::vector<std::uint8_t> stageCollision_;
  struct StageMarker {
    std::uint16_t x{};
    std::uint8_t y{};
    std::uint8_t entity{};
  };
  std::vector<StageMarker> stageMarkers_;
  int stageWorldWidth_{};
  std::array<std::array<std::uint8_t,32*30>,6> openingTilePalettes_{};
  std::array<std::array<std::uint8_t,16>,6> openingPalettes_{};
  std::array<std::uint16_t,6> openingTextAddresses_{};
  std::array<std::uint16_t,6> openingTextLengths_{};
  bool openingAssetsLoaded_{};
  bool gameAssetsLoaded_{};

  void tickOriginal(const ControllerInput& input);
  void tickRemake(const ControllerInput& input);
  void decodeOpeningScene(std::size_t mapPage, std::size_t palettePage,
                          std::size_t patternPage, std::vector<std::uint32_t>& output,
                          std::array<std::uint8_t,32*30>* tilePalettes);
  void drawOpeningTile(PixelBuffer& target, int tileX, int tileY, std::uint8_t tile,
                       std::size_t page, std::uint8_t palette) const;
  void drawOpeningText(PixelBuffer& target, std::size_t page, std::size_t characters) const;
  void drawPlayerSelect(PixelBuffer& target) const;
  void drawCharacterIntroText(PixelBuffer& target, std::size_t characters) const;
  void drawLevelIntro(PixelBuffer& target) const;
  void drawPauseMenu(PixelBuffer& target) const;
  void drawMenuTile(PixelBuffer& target, int x, int y, std::uint8_t tile,
                    std::uint8_t palette = 3) const;
  void decodeFirstStage();
  void decodeCharacterIntro();
  void drawGameTile(PixelBuffer& target, int x, int y, std::uint8_t tile,
                    bool background, std::uint8_t palette) const;
  void resetOpeningMusic();
  void tickOpeningMusic();
  void resetStageMusic();
  void tickStageMusic();
  void tickPauseMenu(const ControllerInput& input);
};

}  // namespace a52
