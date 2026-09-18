#include "action52/ui_assets.hpp"

#include <algorithm>
#include <array>
#include <fstream>

namespace a52 {
namespace {
constexpr std::size_t kFramePixels = kLogicalWidth * kLogicalHeight;
constexpr std::array<std::string_view, 52> kMenuLabels = {
  "01.FIREBREATHER","02.STAREVIL","03.ILLUMINATOR","04.G-FORCE FGT.","05.OOZE","06.SILVER SWORD",
  "07.CRITICAL BP.","08.JUPITR SCOPE","09.ALFREDO","10.OPERAT. MOON","11.DAM BUSTERS","12.THRUSTERS",
  "13.HAUNTED HILL","14.CHILL OUT","15.SHARKS","16.MEGALONIA","17.FRENCH BAKER","18.ATMOS QUAKE",
  "19.MEONG","20.SPACE DREAMS","21.STREEMERZ","22.SPREAD FIRE","23.BUBLGUM ROSY","24.MICRO MIKE",
  "25.UNDERGROUND","26.ROCKET JOCK","27.NON HUMAN","28.CRY BABY","29.SLASHERS","30.CRAZY SHUFLE",
  "31.FUZZ POWER","32.SHOOTING GAL","33.LOLLIPOPS","34.EVIL EMPIRE","35.SOMBREROS","36.STORM OVER D",
  "37.MASH MAN","38.THEY CAME","39.LAZER LEAGUE","40.BILLY BOB","41.CITY OF DOOM","42.BITS N PIECE",
  "43.BEEPS N BLIP","44.MANCHESTER","45.BOSS","46.DEDANT","47.HAMBO","48.TIMEWARP","49.JIGSAW",
  "50.NINJA ASAULT","51.ROBBIE ROBOT","52.CHEETAH MEN"};

bool readExact(const std::filesystem::path& path, void* output, std::size_t size, std::string& error) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file || static_cast<std::size_t>(file.tellg()) != size) {
    error = "Missing or invalid ROM-derived UI asset: " + path.string(); return false;
  }
  file.seekg(0); file.read(static_cast<char*>(output), static_cast<std::streamsize>(size));
  if (!file) { error = "Failed reading UI asset: " + path.string(); return false; }
  return true;
}
}

bool UiAssets::load(const std::filesystem::path& assetDirectory, std::string& error) {
  const auto ui = assetDirectory / "ui";
  for (std::size_t i=0; i<introFrames_.size(); ++i) {
    introFrames_[i].resize(kFramePixels);
    const auto name = "intro_0" + std::to_string(i) + ".argb";
    if (!readExact(ui/name, introFrames_[i].data(), kFramePixels*sizeof(std::uint32_t), error)) return false;
  }
  for (std::size_t i=0; i<menuPages_.size(); ++i) {
    menuPages_[i].resize(kFramePixels);
    const auto name = "menu_page_" + std::to_string(i) + ".argb";
    if (!readExact(ui/name, menuPages_[i].data(), kFramePixels*sizeof(std::uint32_t), error)) return false;
  }
  return readExact(ui/"menu.chr", menuChr_.data(), menuChr_.size(), error);
}

void UiAssets::renderIntro(std::uint64_t frame, PixelBuffer& target) const {
  const std::size_t scene = frame < 33 ? 0 : frame < 165 ? 1 : frame < 298 ? 2 :
                            frame < 430 ? 3 : frame < 562 ? 4 : frame < 919 ? 5 :
                            frame < 944 ? 6 : 7;
  target.rgba = introFrames_[scene];
}

void UiAssets::renderMenu(int selectedGame, std::uint64_t frame, PixelBuffer& target) const {
  (void)frame;
  const int page = selectedGame <= 18 ? 0 : selectedGame <= 36 ? 1 : 2;
  target.rgba = menuPages_[static_cast<std::size_t>(page)];
  const int within = selectedGame - (page == 0 ? 1 : page == 1 ? 19 : 37);
  const int column = page < 2 ? within/9 : within/8;
  const int row = page < 2 ? within%9 : within%8;
  constexpr std::array<std::uint32_t,2> normal{0xfffe6eccu,0xffb71e7bu};
  constexpr std::array<std::array<std::uint32_t,2>,3> highlight{{
    {0xff64b0ffu,0xff155fd9u}, {0xffe4e594u,0xffbcbe00u}, {0xff45e082u,0xff008f32u}}};
  const int x0=column*128, y0=64+row*16;
  const int width=std::min(128,static_cast<int>(kMenuLabels[selectedGame-1].size())*8);
  for (int y=y0; y<y0+8; ++y) for (int x=x0; x<x0+width; ++x) {
    auto& pixel=target.rgba[static_cast<std::size_t>(y*target.width+x)];
    if (pixel==normal[0]) pixel=highlight[page][0];
    else if (pixel==normal[1]) pixel=highlight[page][1];
  }
}
}  // namespace a52
