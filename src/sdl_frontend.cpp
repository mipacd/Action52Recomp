#include "sdl_frontend.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <mutex>
#include <vector>

namespace a52 {
namespace {
std::filesystem::path findAssetDirectory(int argc, char** argv) {
  for (int i=1; i<argc; ++i) if (std::string_view(argv[i]) == "--assets" && i+1<argc) return argv[i+1];
#ifdef A52_DEFAULT_ASSET_DIR
  return A52_DEFAULT_ASSET_DIR;
#else
  if (const auto* base = SDL_GetBasePath()) return std::filesystem::path(base) / "generated-assets";
  return "generated-assets";
#endif
}
}

SdlFrontend::~SdlFrontend() {
  std::string ignored; saveConfig(configPath_, app_.config(), ignored);
  for (auto* gamepad : gamepads_) if (gamepad) SDL_CloseGamepad(gamepad);
  if (audioStream_) SDL_DestroyAudioStream(audioStream_);
  if (texture_) SDL_DestroyTexture(texture_);
  if (renderer_) SDL_DestroyRenderer(renderer_);
  if (window_) SDL_DestroyWindow(window_);
}

bool SdlFrontend::initialize(int argc, char** argv, std::string& error) {
  for (int i=1; i<argc; ++i) if (std::string_view(argv[i]) == "--self-test") selfTestFrames_ = 8;
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) { error = SDL_GetError(); return false; }
  if (!loadConfig(configPath_, app_.config(), error)) return false;
  const auto assetDirectory=findAssetDirectory(argc,argv);
  if (!app_.initialize(assetDirectory, error)) return false;
  if (!SDL_CreateWindowAndRenderer("Action 52 Native", 1024, 720, SDL_WINDOW_RESIZABLE, &window_, &renderer_)) { error = SDL_GetError(); return false; }
  texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kLogicalWidth, kLogicalHeight);
  if (!texture_) { error = SDL_GetError(); return false; }
  SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
  SDL_SetRenderVSync(renderer_, 1);
  if (app_.config().fullscreen) SDL_SetWindowFullscreen(window_, true);

  SDL_AudioSpec specification{SDL_AUDIO_F32, 2, 48000};
  audioStream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specification, audioCallback, this);
  synth_.setSampleRate(specification.freq);
  std::string audioError;
  uiAudioLoaded_=synth_.loadUiAudio(assetDirectory,audioError);
  if (!uiAudioLoaded_) std::cerr << audioError << " (continuing without intro/menu samples)\n";
  if (audioStream_) SDL_ResumeAudioStreamDevice(audioStream_);
  nextTick_ = SDL_GetTicksNS();
  return true;
}

void SdlFrontend::openGamepad(SDL_JoystickID id) {
  if (!SDL_IsGamepad(id)) return;
  for (auto*& slot : gamepads_) if (!slot) { slot = SDL_OpenGamepad(id); return; }
}

void SdlFrontend::closeGamepad(SDL_JoystickID id) {
  for (auto*& slot : gamepads_) if (slot && SDL_GetGamepadID(slot) == id) { SDL_CloseGamepad(slot); slot = nullptr; }
}

void SdlFrontend::handleEvent(const SDL_Event& event) {
  if (event.type == SDL_EVENT_QUIT) quit_ = true;
  else if (event.type == SDL_EVENT_GAMEPAD_ADDED) openGamepad(event.gdevice.which);
  else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) closeGamepad(event.gdevice.which);
  else if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F11 && !event.key.repeat) {
    const bool fullscreen = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0;
    SDL_SetWindowFullscreen(window_, !fullscreen); app_.config().fullscreen = !fullscreen;
  }
}

FrameInput SdlFrontend::pollInput() {
  FrameInput result; result.frame = frame_;
  const auto* keys = SDL_GetKeyboardState(nullptr);
  auto keyboard = [&](SDL_Scancode code) { return keys[code]; };
  std::uint8_t held{};
  if (keyboard(SDL_SCANCODE_Z)) held |= mask(Button::A);
  if (keyboard(SDL_SCANCODE_X)) held |= mask(Button::B);
  if (keyboard(SDL_SCANCODE_RSHIFT)) held |= mask(Button::Select);
  if (keyboard(SDL_SCANCODE_RETURN)) held |= mask(Button::Start);
  if (keyboard(SDL_SCANCODE_UP)) held |= mask(Button::Up);
  if (keyboard(SDL_SCANCODE_DOWN)) held |= mask(Button::Down);
  if (keyboard(SDL_SCANCODE_LEFT)) held |= mask(Button::Left);
  if (keyboard(SDL_SCANCODE_RIGHT)) held |= mask(Button::Right);
  result.controllers[0].held = held;
  for (std::size_t player=0; player<gamepads_.size(); ++player) if (const auto* pad = gamepads_[player]) {
    auto button = [pad](SDL_GamepadButton b) { return SDL_GetGamepadButton(const_cast<SDL_Gamepad*>(pad), b); };
    auto& state = result.controllers[player].held;
    if (button(SDL_GAMEPAD_BUTTON_SOUTH)) state |= mask(Button::A);
    if (button(SDL_GAMEPAD_BUTTON_EAST)) state |= mask(Button::B);
    if (button(SDL_GAMEPAD_BUTTON_BACK)) state |= mask(Button::Select);
    if (button(SDL_GAMEPAD_BUTTON_START)) state |= mask(Button::Start);
    if (button(SDL_GAMEPAD_BUTTON_DPAD_UP)) state |= mask(Button::Up);
    if (button(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) state |= mask(Button::Down);
    if (button(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) state |= mask(Button::Left);
    if (button(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) state |= mask(Button::Right);
  }
  for (std::size_t player=0; player<2; ++player) {
    result.controllers[player].pressed = result.controllers[player].held & ~previousHeld_[player];
    previousHeld_[player] = result.controllers[player].held;
  }
  return result;
}

void SdlFrontend::render() {
  app_.render(pixels_);
  SDL_UpdateTexture(texture_, nullptr, pixels_.rgba.data(), kLogicalWidth * static_cast<int>(sizeof(std::uint32_t)));
  int width{}, height{}; SDL_GetRenderOutputSize(renderer_, &width, &height);
  const float targetAspect = app_.config().squarePixels ? 256.0f/240.0f : 4.0f/3.0f;
  float drawWidth = static_cast<float>(width), drawHeight = drawWidth / targetAspect;
  if (drawHeight > height) { drawHeight = static_cast<float>(height); drawWidth = drawHeight * targetAspect; }
  if (app_.config().integerScaling) {
    const float unitWidth = app_.config().squarePixels ? 256.0f : 320.0f;
    const float scale = std::max(1.0f, std::floor(std::min(width/unitWidth, height/240.0f)));
    drawWidth = unitWidth*scale; drawHeight=240.0f*scale;
  }
  const SDL_FRect destination{(width-drawWidth)/2.0f,(height-drawHeight)/2.0f,drawWidth,drawHeight};
  SDL_SetRenderDrawColor(renderer_,0,0,0,255); SDL_RenderClear(renderer_);
  SDL_RenderTexture(renderer_,texture_,nullptr,&destination); SDL_RenderPresent(renderer_);
}

bool SdlFrontend::iterate() {
  if (quit_) return false;
  constexpr std::uint64_t frameNs = static_cast<std::uint64_t>(1'000'000'000.0 / kNtscFramesPerSecond);
  auto now = SDL_GetTicksNS();
  int catchup = 0;
  while (now >= nextTick_ && catchup < 4) {
    app_.tick(pollInput());
    { const std::lock_guard lock(audioMutex_); synth_.apply(app_.audioEvents()); synth_.handleUiCues(app_.uiAudioCues()); }
    ++frame_; nextTick_ += frameNs; ++catchup;
  }
  if (now > nextTick_ + frameNs*4) nextTick_ = now;
  render();
  if (selfTestFrames_ > 0 && --selfTestFrames_ == 0) return false;
  return true;
}

void SDLCALL SdlFrontend::audioCallback(void* userdata, SDL_AudioStream* stream, int additionalAmount, int) {
  auto& self = *static_cast<SdlFrontend*>(userdata);
  const auto floats = std::max(0, additionalAmount / static_cast<int>(sizeof(float)));
  std::vector<float> samples(static_cast<std::size_t>(floats));
  { const std::lock_guard lock(self.audioMutex_); self.synth_.render(samples); }
  SDL_PutAudioStreamData(stream, samples.data(), additionalAmount);
}
}  // namespace a52
