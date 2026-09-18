#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "sdl_frontend.hpp"

#include <iostream>
#include <memory>

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
  auto frontend = std::make_unique<a52::SdlFrontend>();
  std::string error;
  if (!frontend->initialize(argc, argv, error)) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Action 52 Native", error.c_str(), nullptr);
    std::cerr << error << '\n'; return SDL_APP_FAILURE;
  }
  *appstate = frontend.release(); return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  static_cast<a52::SdlFrontend*>(appstate)->handleEvent(*event); return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  return static_cast<a52::SdlFrontend*>(appstate)->iterate() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

void SDL_AppQuit(void* appstate, SDL_AppResult) { delete static_cast<a52::SdlFrontend*>(appstate); }

