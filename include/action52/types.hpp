#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace a52 {

constexpr int kLogicalWidth = 256;
constexpr int kLogicalHeight = 240;
constexpr double kNtscFramesPerSecond = 60.0988138974405;

enum class BehaviorProfile : std::uint8_t { Original, BugFixes, Remake };

enum class Button : std::uint8_t {
  A = 1 << 0,
  B = 1 << 1,
  Select = 1 << 2,
  Start = 1 << 3,
  Up = 1 << 4,
  Down = 1 << 5,
  Left = 1 << 6,
  Right = 1 << 7,
};

constexpr std::uint8_t mask(Button button) {
  return static_cast<std::uint8_t>(button);
}

struct ControllerInput {
  std::uint8_t held{};
  std::uint8_t pressed{};

  [[nodiscard]] bool isHeld(Button button) const { return (held & mask(button)) != 0; }
  [[nodiscard]] bool wasPressed(Button button) const { return (pressed & mask(button)) != 0; }
};

struct FrameInput {
  std::uint64_t frame{};
  std::array<ControllerInput, 2> controllers{};
};

struct PixelBuffer {
  int width{kLogicalWidth};
  int height{kLogicalHeight};
  std::vector<std::uint32_t> rgba = std::vector<std::uint32_t>(kLogicalWidth * kLogicalHeight, 0xFF000000u);
};

enum class AudioChannel : std::uint8_t { Pulse1, Pulse2, Triangle, Noise, Dmc };

struct AudioEvent {
  AudioChannel channel{};
  std::uint16_t period{};
  std::uint8_t volume{};
  std::uint8_t duty{};
  bool enabled{};
};

enum class UiAudioCue : std::uint8_t {
  IntroTitle,
  IntroStop,
  IntroVoice,
  MenuMove,
  MenuPage,
};

}  // namespace a52
