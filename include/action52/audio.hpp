#pragma once

#include "action52/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace a52 {

class NesSynth {
 public:
  void setSampleRate(int sampleRate);
  bool loadUiAudio(const std::filesystem::path& assetDirectory, std::string& error);
  void handleUiCues(std::span<const UiAudioCue> cues);
  void apply(std::span<const AudioEvent> events);
  void render(std::span<float> interleavedStereo);

 private:
  struct Voice {
    double phase{};
    double frequency{};
    float volume{};
    std::uint8_t duty{};
    bool enabled{};
  };
  int sampleRate_{48000};
  std::array<Voice, 5> voices_{};
  std::uint32_t noise_{1};

  struct Clip {
    std::vector<float> mono;
    int sampleRate{48000};
    double cursor{-1.0};
    double maxSeconds{};
    bool loop{};
  };
  struct UiPulse {
    double phase{};
    double elapsed{};
    double duration{};
    int timerPeriod{};
    int duty{};
    int volume{};
    int sweepShift{};
    double sweepInterval{};
    double nextSweep{};
    bool pulse1{};
    bool active{};
  };
  std::array<Clip,2> clips_{};
  std::array<UiPulse,2> uiPulses_{};
};

}  // namespace a52
