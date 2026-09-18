#include "action52/app.hpp"

#include <algorithm>
#include <array>
#include <map>

namespace a52 {
namespace {
using Glyph = std::array<std::uint8_t, 7>;
const std::map<char, Glyph> kFont = {
 {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}}, {'C',{14,17,16,16,16,17,14}},
 {'D',{30,17,17,17,17,17,30}}, {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}}, {'I',{14,4,4,4,4,4,14}},
 {'J',{7,2,2,2,2,18,12}}, {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}}, {'O',{14,17,17,17,17,17,14}},
 {'P',{30,17,17,30,16,16,16}}, {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}}, {'U',{17,17,17,17,17,17,14}},
 {'V',{17,17,17,17,17,10,4}}, {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}}, {'2',{14,17,1,2,4,8,31}},
 {'3',{30,1,1,14,1,1,30}}, {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}}, {'8',{14,17,17,14,17,17,14}},
 {'9',{14,17,17,15,1,1,14}}, {'-',{0,0,0,31,0,0,0}}, {':',{0,4,4,0,4,4,0}},
 {'/',{1,2,2,4,8,8,16}}, {' ',{0,0,0,0,0,0,0}}
};
}

void clear(PixelBuffer& target, std::uint32_t color) { std::fill(target.rgba.begin(), target.rgba.end(), color); }

void fillRect(PixelBuffer& target, int x, int y, int width, int height, std::uint32_t color) {
  const int left = std::max(0,x), top = std::max(0,y), right = std::min(target.width,x+width), bottom = std::min(target.height,y+height);
  for (int py = top; py < bottom; ++py) for (int px = left; px < right; ++px) target.rgba[py*target.width+px] = color;
}

void drawText(PixelBuffer& target, int x, int y, std::string_view text, std::uint32_t color, int scale) {
  int cursor = x;
  for (char c : text) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    const auto glyph = kFont.find(c);
    if (glyph != kFont.end()) for (int row=0; row<7; ++row) for (int col=0; col<5; ++col)
      if (glyph->second[row] & (1 << (4-col))) fillRect(target,cursor+col*scale,y+row*scale,scale,scale,color);
    cursor += 6 * scale;
  }
}

bool App::initialize(const std::filesystem::path& assetDirectory, std::string& error) {
  if (!loadManifest(assetDirectory / "manifest.json", manifest_, error)) return false;
  if (manifest_.romSha256 != "5d7fcec0fe96796cac020d45b79072d971dc0dd6f8d856ceeb30046ad002e3ce") {
    error = "asset manifest was generated from an unsupported ROM"; return false;
  }
  if (!uiAssets_.load(assetDirectory, error)) return false;
  std::string cheetahAssetError;
  cheetahmen_.loadAssets(assetDirectory,cheetahAssetError);
  screen_ = AppScreen::Intro; introFrame_ = 0; uiFrame_ = 0;
  menuPage_ = menuRow_ = menuColumn_ = 0; updateSelectedGame(); return true;
}

void App::updateSelectedGame() {
  const int start = menuPage_ == 0 ? 1 : menuPage_ == 1 ? 19 : 37;
  const int rows = menuPage_ < 2 ? 9 : 8;
  selectedGame_ = start + menuColumn_ * rows + menuRow_;
}

std::span<const AudioEvent> App::audioEvents() const {
  return screen_ == AppScreen::Cheetahmen ? cheetahmen_.audioEvents() : std::span<const AudioEvent>{};
}

void App::tick(const FrameInput& input) {
  uiAudioCues_.clear();
  ++uiFrame_;
  const auto& pad = input.controllers[0];
  switch (screen_) {
    case AppScreen::Intro:
      ++introFrame_;
      if (introFrame_==33) uiAudioCues_.push_back(UiAudioCue::IntroTitle);
      if (introFrame_==825) uiAudioCues_.push_back(UiAudioCue::IntroVoice);
      if (introFrame_ >= 950 || pad.wasPressed(Button::Start) || pad.wasPressed(Button::A)) {
        uiAudioCues_.push_back(UiAudioCue::IntroStop);
        screen_ = AppScreen::Menu;
      }
      break;
    case AppScreen::Menu:
      {
        const int rows = menuPage_ < 2 ? 9 : 8;
        bool moved=false;
        if (pad.wasPressed(Button::Up)) { menuRow_=(menuRow_+rows-1)%rows; moved=true; }
        if (pad.wasPressed(Button::Down)) { menuRow_=(menuRow_+1)%rows; moved=true; }
        if (pad.wasPressed(Button::Left) || pad.wasPressed(Button::Right)) { menuColumn_=1-menuColumn_; moved=true; }
        if (moved) { updateSelectedGame(); uiAudioCues_.push_back(UiAudioCue::MenuMove); }
      }
      if (pad.wasPressed(Button::Select)) {
        menuPage_=(menuPage_+1)%3;
        const int rows=menuPage_<2?9:8;
        menuRow_=std::min(menuRow_,rows-1);
        updateSelectedGame();
        uiAudioCues_.push_back(UiAudioCue::MenuPage);
      }
      if (pad.wasPressed(Button::B)) screen_ = AppScreen::Options;
      if (pad.wasPressed(Button::A) || pad.wasPressed(Button::Start)) {
        if (selectedGame_ == 52) { cheetahmen_.reset(config_.profile, config_.fixes, 0x52c4ee7u); screen_ = AppScreen::Cheetahmen; }
        else { unavailableFrames_ = 0; screen_ = AppScreen::Unavailable; }
      }
      break;
    case AppScreen::Unavailable:
      ++unavailableFrames_;
      if (unavailableFrames_ > 180 || pad.wasPressed(Button::A) || pad.wasPressed(Button::B) || pad.wasPressed(Button::Start)) screen_ = AppScreen::Menu;
      break;
    case AppScreen::Cheetahmen:
      cheetahmen_.tick(input);
      if (cheetahmen_.requestsMenu()) screen_ = AppScreen::Menu;
      break;
    case AppScreen::Options:
      if (pad.wasPressed(Button::Left)) config_.profile = static_cast<BehaviorProfile>((static_cast<int>(config_.profile)+2)%3);
      if (pad.wasPressed(Button::Right) || pad.wasPressed(Button::A) || pad.wasPressed(Button::Select)) config_.profile = static_cast<BehaviorProfile>((static_cast<int>(config_.profile)+1)%3);
      if (pad.wasPressed(Button::B) || pad.wasPressed(Button::Start)) screen_ = AppScreen::Menu;
      break;
  }
}

void App::render(PixelBuffer& target) const {
  if (screen_ == AppScreen::Cheetahmen) { cheetahmen_.render(target); return; }
  if (screen_ == AppScreen::Intro) {
    uiAssets_.renderIntro(introFrame_, target); return;
  }
  if (screen_ == AppScreen::Menu) {
    uiAssets_.renderMenu(selectedGame_,uiFrame_,target); return;
  }
  clear(target, 0xff101028u);
  drawText(target, 72, 4, "ACTION 52", 0xffffd030u, 2);
  if (screen_ == AppScreen::Unavailable) {
    drawText(target, 36, 86, manifest_.games[selectedGame_-1].title, 0xffffd030u, 1);
    drawText(target, 35, 112, "NOT YET IMPLEMENTED", 0xffffffffu, 2);
    drawText(target, 64, 148, "PRESS A B OR START", 0xff60d0ffu, 1); return;
  }
  drawText(target, 72, 72, "BEHAVIOR PROFILE", 0xffffffffu, 1);
  drawText(target, 64, 100, std::string(profileName(config_.profile)), 0xffffd030u, 2);
  drawText(target, 22, 140, "LEFT RIGHT A OR SELECT TO CHANGE", 0xff60d0ffu, 1);
  drawText(target, 47, 158, "B OR START TO RETURN", 0xffffffffu, 1);
}
}  // namespace a52
