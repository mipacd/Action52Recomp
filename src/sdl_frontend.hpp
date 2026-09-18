#pragma once

#include "action52/app.hpp"
#include "action52/audio.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <filesystem>
#include <mutex>
#include <string>

namespace a52 {

class SdlFrontend {
 public:
  ~SdlFrontend();
  bool initialize(int argc, char** argv, std::string& error);
  void handleEvent(const SDL_Event& event);
  bool iterate();

 private:
  SDL_Window* window_{};
  SDL_Renderer* renderer_{};
  SDL_Texture* texture_{};
  SDL_AudioStream* audioStream_{};
  std::array<SDL_Gamepad*, 2> gamepads_{};
  App app_;
  PixelBuffer pixels_;
  NesSynth synth_;
  std::mutex audioMutex_;
  std::filesystem::path configPath_{"action52.cfg"};
  std::uint64_t frame_{};
  std::uint64_t nextTick_{};
  std::array<std::uint8_t, 2> previousHeld_{};
  bool quit_{};
  int selfTestFrames_{};
  bool uiAudioLoaded_{};

  FrameInput pollInput();
  void render();
  void openGamepad(SDL_JoystickID id);
  void closeGamepad(SDL_JoystickID id);
  static void SDLCALL audioCallback(void* userdata, SDL_AudioStream* stream, int additionalAmount, int totalAmount);
};

}  // namespace a52
