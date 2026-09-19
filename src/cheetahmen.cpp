#include "action52/cheetahmen.hpp"

#include "action52/app.hpp"
#include "action52/hash.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <type_traits>

namespace a52 {
namespace {
constexpr int kUnit = 256;
constexpr int kLeft = 12 * kUnit;
constexpr int kRight = 236 * kUnit;
constexpr std::array<std::uint16_t,6> kTextPointers{0x91fe,0x928f,0x92e9,0x937b,0x9454,0x94eb};
constexpr std::array<std::uint32_t,64> kNesPalette{
  0xff666666,0xff002a88,0xff1412a7,0xff3b00a4,0xff5c007e,0xff6e0040,0xff6c0700,0xff561d00,
  0xff333500,0xff0b4800,0xff005200,0xff004f08,0xff00404d,0xff000000,0xff000000,0xff000000,
  0xffadadad,0xff155fd9,0xff4240ff,0xff7527fe,0xffa01acc,0xffb71e7b,0xffb53120,0xff994e00,
  0xff6b6d00,0xff388700,0xff0c9300,0xff008f32,0xff007c8d,0xff000000,0xff000000,0xff000000,
  0xfffffeff,0xff64b0ff,0xff9290ff,0xffc676ff,0xfff36aff,0xfffe6ecc,0xfffe8170,0xffea9e22,
  0xffbcbe00,0xff88d800,0xff5ce430,0xff45e082,0xff48cdde,0xff4f4f4f,0xff000000,0xff000000,
  0xfffffeff,0xffc0dfff,0xffd3d2ff,0xffe8c8ff,0xfffac2ff,0xffffc4ea,0xffffccc5,0xfff7d8a5,
  0xffe4e594,0xffcfef96,0xffbdf4ab,0xffb3f3cc,0xffb5ebf2,0xffb8b8b8,0xff000000,0xff000000};

std::uint16_t wordAt(const std::array<std::uint8_t,16384>& bytes, std::uint16_t cpuAddress) {
  if (cpuAddress<0x8000 || cpuAddress>=0xbfff) return 0;
  const auto offset=static_cast<std::size_t>(cpuAddress-0x8000);
  return static_cast<std::uint16_t>(bytes[offset]|(bytes[offset+1]<<8));
}

std::uint16_t wordAt(const std::array<std::uint8_t,32768>& bytes, std::uint16_t cpuAddress) {
  if (cpuAddress<0x8000 || cpuAddress==0xffff) return 0;
  const auto offset=static_cast<std::size_t>(cpuAddress-0x8000);
  return static_cast<std::uint16_t>(bytes[offset]|(bytes[offset+1]<<8));
}

template <typename T> void appendLe(std::vector<std::byte>& out, T value) {
  using U = std::make_unsigned_t<T>;
  const auto bits = static_cast<U>(value);
  for (std::size_t i = 0; i < sizeof(T); ++i) out.push_back(static_cast<std::byte>((bits >> (i * 8)) & 0xff));
}

template <typename T> bool readLe(std::span<const std::byte>& in, T& value) {
  if (in.size() < sizeof(T)) return false;
  using U = std::make_unsigned_t<T>;
  U bits = 0;
  for (std::size_t i = 0; i < sizeof(T); ++i) bits |= static_cast<U>(std::to_integer<std::uint8_t>(in[i])) << (i * 8);
  value = static_cast<T>(bits);
  in = in.subspan(sizeof(T)); return true;
}
}

bool Cheetahmen::loadAssets(const std::filesystem::path& assetDirectory,std::string& error) {
  const auto readExact=[&](const std::filesystem::path& path,auto& bytes) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if (!file || static_cast<std::size_t>(file.tellg())!=bytes.size()) {
      error="Missing or invalid ROM-derived Cheetahmen resource: "+path.string(); return false;
    }
    file.seekg(0); file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if (!file) { error="Failed reading Cheetahmen resource: "+path.string(); return false; }
    return true;
  };
  const auto root=assetDirectory/"cheetahmen";
  std::array<std::uint8_t,16384> gameLower{},gameUpper{};
  if (!readExact(root/"prg_28.bin",openingPrg_) ||
      !readExact(root/"chr_54.bin",openingChr_[0]) ||
      !readExact(root/"chr_55.bin",openingChr_[1]) ||
      !readExact(root/"chr_56.bin",openingChr_[2])) {
    openingAssetsLoaded_=false; return false;
  }
  if (!readExact(root/"prg_16.bin",gameLower) || !readExact(root/"prg_17.bin",gameUpper) ||
      !readExact(root/"chr_0.bin",selectionChr_) || !readExact(root/"chr_52.bin",instructionChr_) ||
      !readExact(root/"chr_46.bin",gameChr_)) {
    gameAssetsLoaded_=false; return false;
  }
  std::copy(gameLower.begin(),gameLower.end(),gamePrg_.begin());
  std::copy(gameUpper.begin(),gameUpper.end(),gamePrg_.begin()+16384);
  for (std::size_t page=0;page<6;++page) {
    openingTextAddresses_[page]=kTextPointers[page];
    const auto start=static_cast<std::size_t>(kTextPointers[page]-0x8000);
    std::size_t length=0;
    while (start+length<openingPrg_.size() && openingPrg_[start+length]!=0xff) ++length;
    if (start+length>=openingPrg_.size()) { error="Cheetahmen text terminator is missing"; openingAssetsLoaded_=false; return false; }
    openingTextLengths_[page]=static_cast<std::uint16_t>(length);
    decodeOpeningScene(page,page,page,openingBackgrounds_[page],&openingTilePalettes_[page]);
    // The mapper switches the pattern half first.  During the exposed Original-mode
    // transition the old nametable, palette, and CHR bank are still visible.
    decodeOpeningScene(page? page-1:0,page? page-1:0,page,openingTransitionCorruption_[page],nullptr);
  }
  openingAssetsLoaded_=true;
  gameAssetsLoaded_=true;
  decodeCharacterIntro();
  decodeFirstStage();
  return true;
}

void Cheetahmen::resetOpeningMusic() {
  openingMusic_={};
  if (!openingAssetsLoaded_) return;
  auto descriptor=wordAt(openingPrg_,0x9615);
  while (descriptor>=0x8000 && descriptor<0xc000) {
    const auto at=static_cast<std::size_t>(descriptor-0x8000);
    const auto channel=openingPrg_[at];
    if (channel&0x80) break;
    if (channel<openingMusic_.size()) {
      auto& state=openingMusic_[channel];
      state.pc=static_cast<std::uint16_t>(openingPrg_[at+1]|(openingPrg_[at+2]<<8));
      state.loopStart=state.pc;
      state.active=true; state.volume=15;
    }
    descriptor=static_cast<std::uint16_t>(descriptor+3);
  }
}

void Cheetahmen::tickOpeningMusic() {
  eventCount_=0;
  for (std::size_t channel=0;channel<openingMusic_.size();++channel) {
    auto& state=openingMusic_[channel];
    if (!state.active) continue;
    if (state.duration) --state.duration;
    for (int commands=0;state.duration==0 && state.active && commands<64;++commands) {
      if (state.pc<0x8000 || state.pc>=0xc000) { state.active=false; break; }
      const auto commandAddress=state.pc;
      const auto command=openingPrg_[state.pc++-0x8000];
      if (command<0x80) {
        if (state.pc>=0xc000) { state.active=false; break; }
        state.duration=command;
        state.note=openingPrg_[state.pc++-0x8000];
        state.audible=state.duration!=0;
      } else switch(command&0xf0) {
        case 0x80:
          if (state.pc>0xbff8) { state.active=false; break; }
          state.duty=openingPrg_[state.pc-0x8000]&3;
          state.volume=openingPrg_[state.pc+1-0x8000]&15;
          state.pc=static_cast<std::uint16_t>(state.pc+8);
          break;
        case 0x90:
          if (command==0x91) { if (state.pc>=0xc000) state.active=false; else ++state.pc; }
          break;
        case 0xa0:
          state.active=false; state.audible=false;
          break;
        case 0xb0:
          if (state.pc>=0xc000) state.active=false;
          else { state.duration=openingPrg_[state.pc++-0x8000]; state.audible=false; }
          break;
        case 0xc0: {
          if (state.pc>0xbffd) { state.active=false; break; }
          const auto destination=wordAt(openingPrg_,state.pc);
          const auto count=openingPrg_[state.pc+2-0x8000];
          if (state.loopCommand!=commandAddress) { state.loopCommand=commandAddress; state.loopRemaining=count; }
          if (state.loopRemaining) { --state.loopRemaining; state.pc=destination; }
          else { state.loopCommand=0; state.pc=static_cast<std::uint16_t>(state.pc+3); }
          break;
        }
        case 0xd0:
          if (state.pc>=0xbfff) { state.active=false; break; }
          state.patternTable=wordAt(openingPrg_,state.pc);
          for (auto& shared:openingMusic_) shared.patternTable=state.patternTable;
          state.pc=static_cast<std::uint16_t>(state.pc+2);
          break;
        case 0xe0: {
          const auto index=command&0x1f;
          state.returnPc=state.pc;
          state.pc=wordAt(openingPrg_,static_cast<std::uint16_t>(state.patternTable+index*2));
          break;
        }
        case 0xf0:
          if (command==0xff) { state.pc=state.returnPc; state.returnPc=0; }
          else {
            const auto index=command&0x1f;
            state.returnPc=state.pc;
            state.pc=wordAt(openingPrg_,static_cast<std::uint16_t>(state.patternTable+index*2));
          }
          break;
      }
    }
    if (!state.active || !state.audible || !state.duration) continue;
    std::uint16_t period{};
    if (channel==3) period=state.note&15;
    else {
      const auto noteAddress=static_cast<std::uint16_t>(0xa4e6+static_cast<std::uint16_t>(state.note)*2);
      if (noteAddress>=0xbffe) continue;
      period=wordAt(openingPrg_,noteAddress);
    }
    if (eventCount_<events_.size()) events_[eventCount_++]={static_cast<AudioChannel>(channel),period,
      static_cast<std::uint8_t>(channel==2?15:state.volume),state.duty,true};
  }
}

void Cheetahmen::resetStageMusic() {
  stageMusic_={};
  if (!gameAssetsLoaded_) return;
  // The level descriptor at $D663 points to the track table at $D717.
  // Track zero is the four-channel first-level score at $E389.
  auto descriptor=wordAt(gamePrg_,0xD717);
  while (descriptor>=0x8000 && descriptor<0xffff) {
    const auto at=static_cast<std::size_t>(descriptor-0x8000);
    const auto channel=gamePrg_[at];
    if (channel&0x80) break;
    if (channel<stageMusic_.size()) {
      auto& state=stageMusic_[channel];
      state.pc=static_cast<std::uint16_t>(gamePrg_[at+1]|(gamePrg_[at+2]<<8));
      state.loopStart=state.pc;
      state.active=true; state.volume=15;
    }
    descriptor=static_cast<std::uint16_t>(descriptor+3);
  }
}

void Cheetahmen::tickStageMusic() {
  eventCount_=0;
  const auto byteAt=[&](std::uint16_t address)->std::uint8_t {
    return address>=0x8000?gamePrg_[address-0x8000]:0;
  };
  for (std::size_t channel=0;channel<stageMusic_.size();++channel) {
    auto& state=stageMusic_[channel];
    if (!state.active) continue;
    if (state.duration) --state.duration;
    for (int commands=0;state.duration==0 && state.active && commands<64;++commands) {
      if (state.pc<0x8000 || state.pc==0xffff) { state.active=false; break; }
      const auto commandAddress=state.pc;
      const auto command=byteAt(state.pc++);
      if (command<0x80) {
        state.duration=command;
        state.note=byteAt(state.pc++);
        state.audible=state.duration!=0;
      } else switch(command&0xf0) {
        case 0x80:
          state.duty=byteAt(state.pc)&3; state.volume=byteAt(state.pc+1)&15;
          state.pc=static_cast<std::uint16_t>(state.pc+8); break;
        case 0x90:
          if (command==0x91) ++state.pc;
          break;
        case 0xa0: state.active=false; state.audible=false; break;
        case 0xb0: state.duration=byteAt(state.pc++); state.audible=false; break;
        case 0xc0: {
          const auto destination=wordAt(gamePrg_,state.pc);
          const auto count=byteAt(static_cast<std::uint16_t>(state.pc+2));
          if (count==0xff && fixes_ && fixes_->enabled("cheetahmen.music_loop",profile_)) {
            // Every cartridge channel branches to pulse 1's $E396 stream here.
            // Fixed profiles retain the indefinite repeat but restore the
            // descriptor start belonging to this channel.
            state.pc=state.loopStart;
            state.returnPc=0;
            state.loopCommand=0;
            state.loopRemaining=0;
            state.duration=0;
            state.audible=false;
            break;
          }
          if (state.loopCommand!=commandAddress) { state.loopCommand=commandAddress; state.loopRemaining=count; }
          if (state.loopRemaining) { --state.loopRemaining; state.pc=destination; }
          else { state.loopCommand=0; state.pc=static_cast<std::uint16_t>(state.pc+3); }
          break;
        }
        case 0xd0:
          state.patternTable=wordAt(gamePrg_,state.pc);
          for (auto& shared:stageMusic_) shared.patternTable=state.patternTable;
          state.pc=static_cast<std::uint16_t>(state.pc+2); break;
        case 0xe0:
        case 0xf0: {
          if (command==0xff) { state.pc=state.returnPc; state.returnPc=0; break; }
          const auto index=command&0x1f;
          state.returnPc=state.pc;
          state.pc=wordAt(gamePrg_,static_cast<std::uint16_t>(state.patternTable+index*2));
          break;
        }
      }
    }
    if (!state.active||!state.audible||!state.duration) continue;
    std::uint16_t period{};
    if (channel==3) period=state.note&15;
    else {
      const auto table=wordAt(gamePrg_,static_cast<std::uint16_t>(0xBE20+channel*2));
      period=wordAt(gamePrg_,static_cast<std::uint16_t>(table+state.note*2));
    }
    if (eventCount_<events_.size()) events_[eventCount_++]={static_cast<AudioChannel>(channel),period,
      static_cast<std::uint8_t>(channel==2?15:state.volume),state.duty,true};
  }
}

void Cheetahmen::decodeOpeningScene(std::size_t mapPage,std::size_t palettePage,
                                    std::size_t patternPage,std::vector<std::uint32_t>& output,
                                    std::array<std::uint8_t,32*30>* tilePalettes) {
  const auto map=wordAt(openingPrg_,static_cast<std::uint16_t>(0x8427+mapPage*2));
  const auto definitions=wordAt(openingPrg_,static_cast<std::uint16_t>(0x841b+mapPage*2));
  const auto attributes=wordAt(openingPrg_,static_cast<std::uint16_t>(0x843f+mapPage*2));
  const auto palette=wordAt(openingPrg_,static_cast<std::uint16_t>(0x8433+palettePage*2));
  for (std::size_t i=0;i<16;++i) openingPalettes_[palettePage][i]=openingPrg_[palette-0x8000+i]&0x3f;
  output.assign(kLogicalWidth*kLogicalHeight,0xff000000u);
  for (int my=0;my<15;++my) for (int mx=0;mx<16;++mx) {
    const auto index=openingPrg_[map-0x8000+static_cast<std::size_t>(my*16+mx)];
    const auto selectedPalette=static_cast<std::uint8_t>((openingPrg_[attributes-0x8000+index]&0x0c)>>2);
    for (int qy=0;qy<2;++qy) for (int qx=0;qx<2;++qx) {
      const auto tile=openingPrg_[definitions-0x8000+static_cast<std::size_t>(index)*4+qy*2+qx];
      const int tx=mx*2+qx,ty=my*2+qy;
      if (tilePalettes) (*tilePalettes)[static_cast<std::size_t>(ty*32+tx)]=selectedPalette;
      const auto chrBase=(patternPage&1u)*4096+static_cast<std::size_t>(tile)*16;
      for (int y=0;y<8;++y) for (int x=0;x<8;++x) {
        const auto bit=7-x;
        const auto& chr=openingChr_[patternPage/2];
        const auto pixel=static_cast<std::uint8_t>(((chr[chrBase+y]>>bit)&1)|(((chr[chrBase+y+8]>>bit)&1)<<1));
        const auto colorIndex=pixel? openingPalettes_[palettePage][selectedPalette*4+pixel]:openingPalettes_[palettePage][0];
        output[static_cast<std::size_t>((ty*8+y)*kLogicalWidth+tx*8+x)]=kNesPalette[colorIndex];
      }
    }
  }
}

void Cheetahmen::drawOpeningTile(PixelBuffer& target,int tileX,int tileY,std::uint8_t tile,
                                 std::size_t page,std::uint8_t palette) const {
  const auto chrBase=(page&1u)*4096+static_cast<std::size_t>(tile)*16;
  for (int y=0;y<8;++y) for (int x=0;x<8;++x) {
    const auto bit=7-x;
    const auto& chr=openingChr_[page/2];
    const auto pixel=static_cast<std::uint8_t>(((chr[chrBase+y]>>bit)&1)|(((chr[chrBase+y+8]>>bit)&1)<<1));
    const auto colorIndex=pixel?openingPalettes_[page][palette*4+pixel]:openingPalettes_[page][0];
    target.rgba[static_cast<std::size_t>((tileY*8+y)*kLogicalWidth+tileX*8+x)]=kNesPalette[colorIndex];
  }
}

void Cheetahmen::drawOpeningText(PixelBuffer& target,std::size_t page,std::size_t characters) const {
  const auto startTile=page==5? 8*32:18*32;
  const auto text=static_cast<std::size_t>(openingTextAddresses_[page]-0x8000);
  characters=std::min<std::size_t>(characters,openingTextLengths_[page]);
  for (std::size_t i=0;i<characters;++i) {
    const auto value=openingPrg_[text+i];
    const auto tile=static_cast<std::uint8_t>(value=='.'?0xe4:value==' '?0xff:value+0xa4);
    const auto position=startTile+1+i;
    const int tx=static_cast<int>(position%32),ty=static_cast<int>(position/32);
    if (ty>=30) break;
    drawOpeningTile(target,tx,ty,tile,page,openingTilePalettes_[page][static_cast<std::size_t>(ty*32+tx)]);
  }
}

void Cheetahmen::drawPlayerSelect(PixelBuffer& target) const {
  clear(target,0xff000000u);
  // GAME_BANK_08 $DA49-$DB2F writes these four strings at encoded VRAM
  // positions $34/$66/$76/$86.  Background rendering uses CHR 4's upper
  // pattern table and palette 3 from $DD03.
  constexpr std::array<std::string_view,4> lines{" ACTION GAMEMASTER ","ONE PLAYER","TWO PLAYER","MAIN MENU"};
  constexpr std::array<int,4> xs{8,12,12,12};
  constexpr std::array<int,4> ys{6,12,14,16};
  for (std::size_t line=0;line<lines.size();++line) {
    for (std::size_t i=0;i<lines[line].size();++i)
      drawMenuTile(target,(xs[line]+static_cast<int>(i))*8,ys[line]*8,
                   static_cast<std::uint8_t>(lines[line][i]));
  }
  for (int choice=0;choice<3;++choice)
    drawMenuTile(target,10*8,(12+choice*2)*8,
                 static_cast<std::uint8_t>(choice==state_.selection?7:0x20));
}

void Cheetahmen::drawLevelIntro(PixelBuffer& target) const {
  clear(target,0xff000000u);
  const auto text=[&](int x,int y,std::string_view value) {
    for (std::size_t i=0;i<value.size();++i)
      drawMenuTile(target,(x+static_cast<int>(i))*8,y*8,static_cast<std::uint8_t>(value[i]));
  };
  // RenderCheetahmenLevelLabel writes "LEVEL" at encoded VRAM position $46.
  text(12,8,"LEVEL 1");
  // $DBF7 writes the lives digit plus " PLAYER 1" at encoded VRAM
  // position $63 ($2186, tile 6/12). $DC59 writes six score digits at
  // $69 ($2192, tile 18/12).
  const char lives=static_cast<char>('0'+std::min<std::uint8_t>(state_.lives,9));
  std::array<char,11> playerLine{lives,' ','P','L','A','Y','E','R',' ','1','\0'};
  text(6,12,playerLine.data());
  std::array<char,7> score{};
  auto value=state_.score;
  for (int i=5;i>=0;--i) { score[static_cast<std::size_t>(i)]=static_cast<char>('0'+value%10); value/=10; }
  text(18,12,score.data());
}

void Cheetahmen::drawCharacterIntroText(PixelBuffer& target,std::size_t characters) const {
  constexpr std::uint16_t textAddress=0xE2C1;
  constexpr std::uint16_t paletteAddress=0xE135;
  const auto available=static_cast<std::size_t>(199);
  characters=std::min(characters,available);
  for (std::size_t i=0;i<characters;++i) {
    const auto value=gamePrg_[textAddress-0x8000+i];
    const auto tile=static_cast<std::uint8_t>(value=='.'?0xe4:value==' '?0xff:value+0xa4);
    const auto position=18u*32u+i;
    const int tileX=static_cast<int>(position%32),tileY=static_cast<int>(position/32);
    if (tileY>=30) break;
    const auto chrBase=static_cast<std::size_t>(tile)*16;
    for (int y=0;y<8;++y) for (int x=0;x<8;++x) {
      const auto bit=7-x;
      const auto pixel=static_cast<std::uint8_t>(((instructionChr_[chrBase+y]>>bit)&1)|
                                                 (((instructionChr_[chrBase+y+8]>>bit)&1)<<1));
      const auto color=gamePrg_[paletteAddress-0x8000+(pixel?pixel:0)]&0x3f;
      target.rgba[static_cast<std::size_t>((tileY*8+y)*kLogicalWidth+tileX*8+x)]=kNesPalette[color];
    }
  }
}

void Cheetahmen::drawMenuTile(PixelBuffer& target,int x0,int y0,std::uint8_t tile,
                              std::uint8_t palette) const {
  const auto chrBase=4096u+static_cast<std::size_t>(tile)*16;
  constexpr auto paletteBase=static_cast<std::size_t>(0xDD03-0x8000);
  for (int y=0;y<8;++y) for (int x=0;x<8;++x) {
    const int px=x0+x,py=y0+y;
    if (px<0||px>=target.width||py<0||py>=target.height) continue;
    const auto bit=7-x;
    const auto value=static_cast<std::uint8_t>(((selectionChr_[chrBase+y]>>bit)&1)|
                                               (((selectionChr_[chrBase+y+8]>>bit)&1)<<1));
    const auto color=value?gamePrg_[paletteBase+palette*4+value]&0x3f:gamePrg_[paletteBase]&0x3f;
    target.rgba[static_cast<std::size_t>(py*target.width+px)]=kNesPalette[color];
  }
}

void Cheetahmen::drawGameTile(PixelBuffer& target,int x0,int y0,std::uint8_t tile,
                              bool background,std::uint8_t attributes) const {
  const auto chrBase=static_cast<std::size_t>(background?4096:0)+static_cast<std::size_t>(tile)*16;
  // The map descriptor's palette at $CE96 contains 16 background bytes
  // followed by the four sprite palettes at $CEA6. Palette 0 is the
  // black/white/yellow palette used by Aries.
  const auto paletteBase=static_cast<std::size_t>((background?0xD733:0xCEA6)-0x8000);
  for (int y=0;y<8;++y) for (int x=0;x<8;++x) {
    const int px=x0+x,py=y0+y;
    if (px<0||px>=target.width||py<0||py>=target.height) continue;
    const auto sourceX=(!background && (attributes&0x40))?7-x:x;
    const auto sourceY=(!background && (attributes&0x80))?7-y:y;
    const auto bit=7-sourceX;
    const auto& chr=state_.phase==1?selectionChr_:gameChr_;
    const auto value=static_cast<std::uint8_t>(((chr[chrBase+sourceY]>>bit)&1)|
                                                (((chr[chrBase+sourceY+8]>>bit)&1)<<1));
    if (!background && value==0) continue;
    const auto palette=attributes&3;
    const auto color=value?gamePrg_[paletteBase+palette*4+value]&0x3f:gamePrg_[paletteBase]&0x3f;
    target.rgba[static_cast<std::size_t>(py*target.width+px)]=kNesPalette[color];
  }
}

void Cheetahmen::drawPauseMenu(PixelBuffer& target) const {
  clear(target,0xff000000u);
  const auto text=[&](int x,int y,std::string_view value) {
    for (std::size_t i=0;i<value.size();++i)
      drawMenuTile(target,(x+static_cast<int>(i))*8,y*8,static_cast<std::uint8_t>(value[i]));
  };
  text(11,3,pausePage_==0?"GAME PAUSED":pausePage_==1?"CHEATS":"ACHIEVEMENTS");
  if (pausePage_==0) {
    constexpr std::array<std::string_view,7> choices{
      "RETURN TO GAME","SAVE GAME","LOAD GAME","CHEATS","ACHIEVEMENTS","GAME MENU","MAIN MENU"};
    for (std::size_t i=0;i<choices.size();++i) {
      const int row=7+static_cast<int>(i)*2;
      drawMenuTile(target,5*8,row*8,static_cast<std::uint8_t>(i==pauseSelection_?7:0x20));
      text(7,row,choices[i]);
    }
    if (pauseMessageFrames_) text(9,23,hasSave_?"GAME SAVED":"NO SAVE DATA");
    return;
  }
  if (pausePage_==1) {
    constexpr std::array<std::string_view,2> choices{"INVINCIBLE","INFINITE LIVES"};
    const std::array<bool,2> enabled{cheatInvincible_,cheatInfiniteLives_};
    for (std::size_t i=0;i<choices.size();++i) {
      const int row=9+static_cast<int>(i)*3;
      drawMenuTile(target,4*8,row*8,static_cast<std::uint8_t>(i==pauseSelection_?7:0x20));
      text(6,row,choices[i]); text(23,row,enabled[i]?"ON":"OFF");
    }
    text(8,20,"B TO GO BACK"); return;
  }
  constexpr std::array<std::string_view,3> names{"STORY COMPLETE","STAGE ONE STARTED","LADDER CLIMBER"};
  for (std::size_t i=0;i<names.size();++i) {
    const int row=8+static_cast<int>(i)*3;
    text(5,row,names[i]); text(24,row,(state_.achievements&(1u<<i))?"YES":"NO");
  }
  text(8,20,"B TO GO BACK");
}

void Cheetahmen::decodeCharacterIntro() {
  constexpr std::uint16_t map=0xE035;
  constexpr std::uint16_t definitions=0xDF8B;
  constexpr std::uint16_t attributes=0xE013;
  constexpr std::uint16_t palette=0xE135;
  const auto at=[&](std::uint16_t address)->std::uint8_t { return gamePrg_[address-0x8000]; };
  characterIntro_.assign(kLogicalWidth*kLogicalHeight,0xff000000u);
  for (int my=0;my<15;++my) for (int mx=0;mx<16;++mx) {
    const auto metatile=at(static_cast<std::uint16_t>(map+my*16+mx));
    const auto selectedPalette=static_cast<std::uint8_t>((at(static_cast<std::uint16_t>(attributes+metatile))&0x0c)>>2);
    for (int qy=0;qy<2;++qy) for (int qx=0;qx<2;++qx) {
      const auto tile=at(static_cast<std::uint16_t>(definitions+metatile*4+qy*2+qx));
      const auto chrBase=static_cast<std::size_t>(tile)*16; // A912 clears PPUCTRL bit 4.
      for (int y=0;y<8;++y) for (int x=0;x<8;++x) {
        const auto bit=7-x;
        const auto pixel=static_cast<std::uint8_t>(((instructionChr_[chrBase+y]>>bit)&1)|
                                                   (((instructionChr_[chrBase+y+8]>>bit)&1)<<1));
        const auto color=pixel?at(static_cast<std::uint16_t>(palette+selectedPalette*4+pixel))&0x3f:
                               at(palette)&0x3f;
        const int px=mx*16+qx*8+x,py=my*16+qy*8+y;
        characterIntro_[static_cast<std::size_t>(py*kLogicalWidth+px)]=kNesPalette[color];
      }
    }
  }
}

void Cheetahmen::decodeFirstStage() {
  constexpr std::uint16_t mapDescriptor=0xC014;
  const auto at=[&](std::uint16_t address)->std::uint8_t { return gamePrg_[address-0x8000]; };
  const auto screenCount=at(mapDescriptor);
  const auto screenOrder=wordAt(gamePrg_,static_cast<std::uint16_t>(mapDescriptor+6));
  const auto definitions=wordAt(gamePrg_,static_cast<std::uint16_t>(mapDescriptor+8));
  const auto attributes=wordAt(gamePrg_,static_cast<std::uint16_t>(mapDescriptor+10));
  stageWorldWidth_=static_cast<int>(screenCount)*256;
  stageWorld_.assign(static_cast<std::size_t>(stageWorldWidth_)*256,0xff000000u);
  stagePalettePixels_.assign(static_cast<std::size_t>(stageWorldWidth_)*256,0);
  stageCollision_.assign(static_cast<std::size_t>(screenCount)*16*16,0);
  stageMarkers_.clear();
  const auto entityIds=wordAt(gamePrg_,static_cast<std::uint16_t>(mapDescriptor+18));
  for (int screen=0;screen<screenCount;++screen) {
    const auto screenId=at(static_cast<std::uint16_t>(screenOrder+screen));
    const auto map=wordAt(gamePrg_,static_cast<std::uint16_t>(mapDescriptor+0x1c+screenId*2));
    for (int my=0;my<16;++my) for (int mx=0;mx<16;++mx) {
      const auto metatile=at(static_cast<std::uint16_t>(map+my*16+mx));
      const auto collision=at(static_cast<std::uint16_t>(attributes+metatile));
      stageCollision_[static_cast<std::size_t>(my*screenCount*16+screen*16+mx)]=collision;
      const auto entity=at(static_cast<std::uint16_t>(entityIds+metatile));
      // StreamCheetahmenStageTiles passes every entering metatile to $B1F2.
      // $FF means ordinary scenery; other values index the entity-definition
      // pointer table at $D641.  Keep the trigger's world position so the
      // native renderer can use the same ROM metasprite and animation data.
      if (entity!=0xff)
        stageMarkers_.push_back({static_cast<std::uint16_t>(screen*256+mx*16),
                                 static_cast<std::uint8_t>(my*16),entity});
      const auto palette=static_cast<std::uint8_t>((collision&0x0c)>>2);
      for (int qy=0;qy<2;++qy) for (int qx=0;qx<2;++qx) {
        const auto tile=at(static_cast<std::uint16_t>(definitions+metatile*4+qy*2+qx));
        // The load-state branch at $A526 writes 2 to mapper $840B (CHR 46)
        // and then sets PPUCTRL bit 4 for the stage background table.
        const auto chrBase=4096u+static_cast<std::size_t>(tile)*16;
        for (int y=0;y<8;++y) for (int x=0;x<8;++x) {
          const auto bit=7-x;
          const auto value=static_cast<std::uint8_t>(((gameChr_[chrBase+y]>>bit)&1)|
                                                     (((gameChr_[chrBase+y+8]>>bit)&1)<<1));
          const auto paletteOffset=static_cast<std::size_t>(0xD733-0x8000)+palette*4;
          const auto color=value?gamePrg_[paletteOffset+value]&0x3f:gamePrg_[0xD733-0x8000]&0x3f;
          const int px=screen*256+mx*16+qx*8+x,py=my*16+qy*8+y;
          stageWorld_[static_cast<std::size_t>(py*stageWorldWidth_+px)]=kNesPalette[color];
          stagePalettePixels_[static_cast<std::size_t>(py*stageWorldWidth_+px)]=
            static_cast<std::uint8_t>(palette*4+value);
        }
      }
    }
  }
}

void Cheetahmen::reset(BehaviorProfile profile, const FixRegistry& fixes, std::uint32_t seed) {
  profile_ = profile; fixes_ = &fixes; menuRequested_ = false; jumpBuffer_ = coyoteFrames_ = 0; eventCount_ = 0;
  paused_=false; pausePage_=pauseSelection_=0; pauseMessageFrames_=0;
  cheatInvincible_=cheatInfiniteLives_=hasSave_=false; savedState_={}; savedStageMusic_={};
  state_ = {}; state_.x = 0x8f * kUnit; state_.y = 0xc1 * kUnit; state_.rng = seed ? seed : 0x52c4ee7u;
  state_.stage = 1; state_.lives = 3; state_.health = 4;
  state_.phase = openingAssetsLoaded_ ? 0 : 2;
  resetOpeningMusic();
  resetStageMusic();
}

void Cheetahmen::tickOriginal(const ControllerInput& input) {
  // The level descriptor loads $035B with four; the four directional
  // handlers add/subtract that amount once per fixed frame.
  constexpr int speed = 4 * kUnit;
  state_.vx = input.isHeld(Button::Left) ? -speed : input.isHeld(Button::Right) ? speed : 0;
  state_.vy = input.isHeld(Button::Up) ? -speed : input.isHeld(Button::Down) ? speed : 0;
}

void Cheetahmen::tickRemake(const ControllerInput& input) {
  constexpr int acceleration = 128, deceleration = 192, maxSpeed = 4 * kUnit;
  const int directionX = input.isHeld(Button::Left) ? -1 : input.isHeld(Button::Right) ? 1 : 0;
  const int directionY = input.isHeld(Button::Up) ? -1 : input.isHeld(Button::Down) ? 1 : 0;
  if (directionX) state_.vx = std::clamp(state_.vx + directionX * acceleration, -maxSpeed, maxSpeed);
  else if (state_.vx > 0) state_.vx = std::max(0, state_.vx - deceleration);
  else if (state_.vx < 0) state_.vx = std::min(0, state_.vx + deceleration);
  if (directionY) state_.vy = std::clamp(state_.vy + directionY * acceleration, -maxSpeed, maxSpeed);
  else if (state_.vy > 0) state_.vy = std::max(0, state_.vy - deceleration);
  else if (state_.vy < 0) state_.vy = std::min(0, state_.vy + deceleration);
}

void Cheetahmen::tickPauseMenu(const ControllerInput& input) {
  if (pauseMessageFrames_) --pauseMessageFrames_;
  const std::uint8_t count=pausePage_==0?7:pausePage_==1?2:1;
  if (input.wasPressed(Button::Select)) pauseSelection_=static_cast<std::uint8_t>((pauseSelection_+1)%count);
  if (input.wasPressed(Button::B)) {
    if (pausePage_) { pausePage_=0; pauseSelection_=0; }
    else paused_=false;
    return;
  }
  if (!input.wasPressed(Button::Start)) return;
  if (pausePage_==1) {
    if (pauseSelection_==0) cheatInvincible_=!cheatInvincible_;
    else cheatInfiniteLives_=!cheatInfiniteLives_;
    return;
  }
  if (pausePage_==2) { pausePage_=0; pauseSelection_=0; return; }
  switch (pauseSelection_) {
    case 0: paused_=false; break;
    case 1:
      savedState_=state_; savedStageMusic_=stageMusic_; hasSave_=true;
      pauseMessageFrames_=90; break;
    case 2:
      if (hasSave_) { state_=savedState_; stageMusic_=savedStageMusic_; paused_=false; }
      else pauseMessageFrames_=90;
      break;
    case 3: pausePage_=1; pauseSelection_=0; break;
    case 4: pausePage_=2; pauseSelection_=0; break;
    case 5:
      paused_=false; state_.phase=1; state_.selection=0; eventCount_=0; break;
    case 6: menuRequested_=true; break;
  }
}

void Cheetahmen::tick(const FrameInput& input) {
  eventCount_ = 0;
  const auto& pad = input.controllers[0];
  if (pad.isHeld(Button::Start) && pad.isHeld(Button::Select)) { menuRequested_ = true; return; }
  if (state_.phase==0) {
    tickOpeningMusic();
    ++state_.openingFrame;
    std::uint32_t total=0; for (const auto length:openingTextLengths_) total+=static_cast<std::uint32_t>(length)+261;
    if ((fixes_ && fixes_->enabled("cheetahmen.skip_opening",profile_) && pad.wasPressed(Button::Start)) || state_.openingFrame>=total) state_.phase=1;
    ++state_.frame; return;
  }
  if (state_.phase==1) {
    if (pad.wasPressed(Button::Select)) state_.selection=static_cast<std::uint8_t>((state_.selection+1)%3);
    if (pad.wasPressed(Button::Start)) {
      if (state_.selection==2) menuRequested_=true;
      else { state_.phase=2; state_.phaseFrame=0; state_.x=0x8f*kUnit; state_.y=0xc1*kUnit;
             state_.cameraX=0; state_.achievements|=1; }
    }
    ++state_.frame; return;
  }
  if (state_.phase==2) {
    ++state_.phaseFrame;
    if (state_.phaseFrame>=199 && (pad.wasPressed(Button::Start)||state_.phaseFrame>=399)) {
      state_.phase=3; state_.phaseFrame=0;
    }
    ++state_.frame; return;
  }
  if (state_.phase==3) {
    ++state_.phaseFrame;
    if (state_.phaseFrame>=120 || pad.wasPressed(Button::Start)) {
      state_.phase=4; state_.phaseFrame=0; state_.achievements|=2; resetStageMusic();
    }
    ++state_.frame; return;
  }
  if (paused_) { tickPauseMenu(pad); eventCount_=0; return; }
  if (profile_==BehaviorProfile::Remake && pad.wasPressed(Button::Start)) {
    paused_=true; pausePage_=pauseSelection_=0; pauseMessageFrames_=0; eventCount_=0; return;
  }
  tickStageMusic();
  if (pad.wasPressed(Button::A) && state_.attackFrames==0) {
    state_.attackFrames=11; // two five-frame cels after this tick's decrement
  }
  if (state_.attackFrames) {
    state_.vx=state_.vy=0;
    --state_.attackFrames;
  } else if (fixes_ && fixes_->enabled("cheetahmen.responsive_controls", profile_)) tickRemake(pad);
  else tickOriginal(pad);
  if (pad.isHeld(Button::Right)) state_.facing=0;
  else if (pad.isHeld(Button::Up)) state_.facing=1;
  else if (pad.isHeld(Button::Left)) state_.facing=2;
  else if (pad.isHeld(Button::Down)) state_.facing=3;
  const auto blocked=[&](int x,int y) {
    constexpr int halfWidth=9,halfHeight=6;
    const int cellsWide=stageWorldWidth_/16;
    const auto solid=[&](int px,int py) {
      if (px<0||py<0||px>=stageWorldWidth_||py>=256) return true;
      const auto cell=static_cast<std::size_t>((py/16)*cellsWide+px/16);
      return (stageCollision_[cell]&0xf3)==1;
    };
    if (solid(x-halfWidth,y-halfHeight)||solid(x+halfWidth,y-halfHeight)||
        solid(x-halfWidth,y+halfHeight)||solid(x+halfWidth,y+halfHeight)) return true;
    for (std::size_t i=0;i<stageMarkers_.size()&&i<32;++i) {
      const auto entity=stageMarkers_[i].entity;
      if (entity>=17) continue;
      const int entityHalfWidth=gamePrg_[0xD785-0x8000+entity];
      const int entityHalfHeight=gamePrg_[0xD796-0x8000+entity];
      if (std::abs(x-static_cast<int>(stageMarkers_[i].x))<=halfWidth+entityHalfWidth&&
          std::abs(y-static_cast<int>(stageMarkers_[i].y))<=halfHeight+entityHalfHeight) return true;
    }
    return false;
  };
  const int proposedX=state_.x+state_.vx;
  if (!blocked(proposedX/kUnit,state_.y/kUnit)) state_.x=proposedX;
  else state_.vx=0;
  const int proposedY=state_.y+state_.vy;
  if (!blocked(state_.x/kUnit,proposedY/kUnit)) state_.y=proposedY;
  else state_.vy=0;
  if (state_.vx||state_.vy) ++state_.playerAnimFrame;
  state_.y=std::clamp(state_.y,0x12*kUnit,0xf4*kUnit);
  state_.x = std::clamp(state_.x,kLeft,std::max(kLeft,(stageWorldWidth_-12)*kUnit));
  const int desiredScroll=std::clamp(state_.x/kUnit-0x95,0,std::max(0,stageWorldWidth_-kLogicalWidth));
  // Both paths track the cartridge's pixel scroll target directly. Remake's
  // improvement is the atomic full-frame redraw, which avoids nametable seams
  // without introducing the lag/judder of an eased camera.
  state_.cameraX=desiredScroll*kUnit;
  if (state_.y<0xb1*kUnit) state_.achievements|=4;
  if (cheatInvincible_) state_.health=4;
  if (cheatInfiniteLives_) state_.lives=9;
  if (pad.wasPressed(Button::B)) state_.score += 10;
  state_.rng = state_.rng * 1664525u + 1013904223u;
  ++state_.frame;
}

void Cheetahmen::render(PixelBuffer& target) const {
  if (state_.phase<2 && openingAssetsLoaded_) {
    if (state_.phase==1) { drawPlayerSelect(target); return; }
    std::uint32_t remaining=state_.openingFrame;
    std::size_t page=0;
    for (;page<openingTextLengths_.size();++page) {
      const auto duration=static_cast<std::uint32_t>(openingTextLengths_[page])+261;
      if (remaining<duration) break;
      remaining-=duration;
    }
    if (page>=openingTextLengths_.size()) { drawPlayerSelect(target); return; }
    const bool clean=fixes_ && fixes_->enabled("cheetahmen.clean_intro_transitions",profile_);
    target.rgba=(!clean && page>0 && remaining==0)?openingTransitionCorruption_[page]:openingBackgrounds_[page];
    if (remaining>=4) drawOpeningText(target,page,std::min<std::uint32_t>(remaining-3,openingTextLengths_[page]));
    return;
  }
  if (!gameAssetsLoaded_||stageWorld_.empty()) { clear(target,0xff000000u); return; }
  if (state_.phase==2) {
    target.rgba=characterIntro_;
    drawCharacterIntroText(target,std::min<std::uint32_t>(state_.phaseFrame,199));
    return;
  }
  if (state_.phase==3) { drawLevelIntro(target); return; }
  if (paused_) { drawPauseMenu(target); return; }
  const int worldX=state_.x/kUnit;
  const int scroll=std::clamp(state_.cameraX/kUnit,0,std::max(0,stageWorldWidth_-kLogicalWidth));
  const auto animatedPalette=static_cast<std::size_t>(0xD733-0x8000)+
                             static_cast<std::size_t>((state_.frame/8)%3)*16;
  for (int y=0;y<kLogicalHeight;++y) for (int x=0;x<kLogicalWidth;++x) {
    const auto source=static_cast<std::size_t>(y*stageWorldWidth_+scroll+x);
    const auto encoded=stagePalettePixels_[source];
    const auto pixel=encoded&3, palette=encoded>>2;
    const auto color=gamePrg_[animatedPalette+(pixel?palette*4+pixel:0)]&0x3f;
    target.rgba[static_cast<std::size_t>(y*kLogicalWidth+x)]=kNesPalette[color];
  }
  const int px=worldX-scroll,py=state_.y/kUnit;
  const auto drawMetasprite=[&](int x,int y,std::uint16_t records,int count) {
    for (int sprite=0;sprite<count;++sprite) {
      const auto offset=static_cast<std::size_t>(records-0x8000+sprite*4);
      const auto dy=static_cast<std::int8_t>(gamePrg_[offset]);
      const auto tile=gamePrg_[offset+1];
      const auto attributes=gamePrg_[offset+2];
      const auto dx=static_cast<std::int8_t>(gamePrg_[offset+3]);
      drawGameTile(target,x+dx,y+dy,tile,false,attributes);
    }
  };
  // Entity definition 2 at $D156 points to cel $D164.  Its first byte is
  // the $11 sprite count; the OAM-like records begin at $D165.
  const auto entityCel=[&](std::uint8_t entity,std::uint32_t clock)->std::pair<std::uint16_t,int> {
    if (entity>=17) return {0,0};
    const auto definition=wordAt(gamePrg_,static_cast<std::uint16_t>(0xD641+entity*2));
    const auto frameCount=gamePrg_[definition-0x8000];
    const auto frameDelay=std::max<std::uint8_t>(1,gamePrg_[definition-0x8000+1]);
    const auto frame=static_cast<std::uint8_t>((clock/frameDelay)%frameCount);
    const auto cel=wordAt(gamePrg_,static_cast<std::uint16_t>(definition+4+frame*2));
    return {static_cast<std::uint16_t>(cel+1),gamePrg_[cel-0x8000]};
  };
  for (std::size_t markerIndex=0;markerIndex<stageMarkers_.size();++markerIndex) {
    const auto& marker=stageMarkers_[markerIndex];
    const int markerX=static_cast<int>(marker.x)-scroll;
    if (markerX < -40 || markerX > kLogicalWidth+40) continue;
    const auto [records,count]=entityCel(marker.entity,state_.frame);
    if (records && count) drawMetasprite(markerX,marker.y,records,count);
  }
  const bool attacking=state_.attackFrames!=0;
  const auto playerEntity=static_cast<std::uint8_t>(attacking?(state_.facing==0?3:4):2);
  const auto playerClock=attacking?static_cast<std::uint32_t>(10-state_.attackFrames):
                                   static_cast<std::uint32_t>(state_.vx||state_.vy?state_.playerAnimFrame:0);
  const auto [playerRecords,playerCount]=entityCel(playerEntity,playerClock);
  drawMetasprite(px,py,playerRecords,playerCount);
  if (state_.flags & 1) {
    fillRect(target, 28, 82, 200, 60, 0xff000000u);
    drawText(target, 72, 100, "ALL STAGES CLEAR", 0xffffd060u, 1);
    drawText(target, 60, 120, "PRESS START TO RETURN", 0xffffffffu, 1);
  }
}

std::span<const AudioEvent> Cheetahmen::audioEvents() const { return {events_.data(), eventCount_}; }

std::vector<std::byte> Cheetahmen::serialize() const {
  std::vector<std::byte> out; out.reserve(256);
  appendLe(out, state_.x); appendLe(out, state_.y); appendLe(out, state_.vx); appendLe(out, state_.vy);
  appendLe(out, state_.rng); appendLe(out, state_.score); appendLe(out, state_.frame); appendLe(out, state_.stage);
  appendLe(out,state_.openingFrame); appendLe(out,state_.phaseFrame); appendLe(out,state_.cameraX); appendLe(out, state_.lives); appendLe(out, state_.health); appendLe(out, state_.flags); appendLe(out,state_.phase); appendLe(out,state_.selection); appendLe(out,state_.achievements); appendLe(out,state_.playerAnimFrame); appendLe(out,state_.attackFrames); appendLe(out,state_.facing);
  appendLe(out, static_cast<std::uint8_t>(menuRequested_)); appendLe(out, static_cast<std::uint8_t>(jumpBuffer_)); appendLe(out, static_cast<std::uint8_t>(coyoteFrames_));
  appendLe(out,static_cast<std::uint8_t>(paused_)); appendLe(out,pausePage_); appendLe(out,pauseSelection_); appendLe(out,pauseMessageFrames_);
  appendLe(out,static_cast<std::uint8_t>(cheatInvincible_)); appendLe(out,static_cast<std::uint8_t>(cheatInfiniteLives_)); appendLe(out,static_cast<std::uint8_t>(hasSave_));
  for (const auto& music:openingMusic_) {
    appendLe(out,music.pc); appendLe(out,music.loopStart); appendLe(out,music.returnPc); appendLe(out,music.patternTable);
    appendLe(out,music.loopCommand); appendLe(out,music.duration); appendLe(out,music.note);
    appendLe(out,music.volume); appendLe(out,music.duty); appendLe(out,music.loopRemaining);
    appendLe(out,static_cast<std::uint8_t>(music.active)); appendLe(out,static_cast<std::uint8_t>(music.audible));
  }
  for (const auto& music:stageMusic_) {
    appendLe(out,music.pc); appendLe(out,music.loopStart); appendLe(out,music.returnPc); appendLe(out,music.patternTable);
    appendLe(out,music.loopCommand); appendLe(out,music.duration); appendLe(out,music.note);
    appendLe(out,music.volume); appendLe(out,music.duty); appendLe(out,music.loopRemaining);
    appendLe(out,static_cast<std::uint8_t>(music.active)); appendLe(out,static_cast<std::uint8_t>(music.audible));
  }
  appendLe(out,savedState_.x); appendLe(out,savedState_.y); appendLe(out,savedState_.vx); appendLe(out,savedState_.vy);
  appendLe(out,savedState_.rng); appendLe(out,savedState_.score); appendLe(out,savedState_.frame); appendLe(out,savedState_.openingFrame); appendLe(out,savedState_.phaseFrame);
  appendLe(out,savedState_.cameraX); appendLe(out,savedState_.stage); appendLe(out,savedState_.lives); appendLe(out,savedState_.health);
  appendLe(out,savedState_.flags); appendLe(out,savedState_.phase); appendLe(out,savedState_.selection); appendLe(out,savedState_.achievements); appendLe(out,savedState_.playerAnimFrame); appendLe(out,savedState_.attackFrames); appendLe(out,savedState_.facing);
  for (const auto& music:savedStageMusic_) {
    appendLe(out,music.pc); appendLe(out,music.loopStart); appendLe(out,music.returnPc); appendLe(out,music.patternTable);
    appendLe(out,music.loopCommand); appendLe(out,music.duration); appendLe(out,music.note);
    appendLe(out,music.volume); appendLe(out,music.duty); appendLe(out,music.loopRemaining);
    appendLe(out,static_cast<std::uint8_t>(music.active)); appendLe(out,static_cast<std::uint8_t>(music.audible));
  }
  return out;
}

bool Cheetahmen::deserialize(std::span<const std::byte> input) {
  std::uint8_t menu{}, jump{}, coyote{},paused{},invincible{},infiniteLives{},hasSave{};
  if (!readLe(input,state_.x)||!readLe(input,state_.y)||!readLe(input,state_.vx)||!readLe(input,state_.vy)||
      !readLe(input,state_.rng)||!readLe(input,state_.score)||!readLe(input,state_.frame)||!readLe(input,state_.stage)||!readLe(input,state_.openingFrame)||!readLe(input,state_.phaseFrame)||!readLe(input,state_.cameraX)||
      !readLe(input,state_.lives)||!readLe(input,state_.health)||!readLe(input,state_.flags)||!readLe(input,state_.phase)||!readLe(input,state_.selection)||!readLe(input,state_.achievements)||!readLe(input,state_.playerAnimFrame)||!readLe(input,state_.attackFrames)||!readLe(input,state_.facing)||!readLe(input,menu)||
      !readLe(input,jump)||!readLe(input,coyote)||!readLe(input,paused)||!readLe(input,pausePage_)||!readLe(input,pauseSelection_)||!readLe(input,pauseMessageFrames_)||
      !readLe(input,invincible)||!readLe(input,infiniteLives)||!readLe(input,hasSave)) return false;
  for (auto& music:openingMusic_) {
    std::uint8_t active{},audible{};
    if (!readLe(input,music.pc)||!readLe(input,music.loopStart)||!readLe(input,music.returnPc)||!readLe(input,music.patternTable)||
        !readLe(input,music.loopCommand)||!readLe(input,music.duration)||!readLe(input,music.note)||
        !readLe(input,music.volume)||!readLe(input,music.duty)||!readLe(input,music.loopRemaining)||
        !readLe(input,active)||!readLe(input,audible)) return false;
    music.active=active!=0; music.audible=audible!=0;
  }
  for (auto& music:stageMusic_) {
    std::uint8_t active{},audible{};
    if (!readLe(input,music.pc)||!readLe(input,music.loopStart)||!readLe(input,music.returnPc)||!readLe(input,music.patternTable)||
        !readLe(input,music.loopCommand)||!readLe(input,music.duration)||!readLe(input,music.note)||
        !readLe(input,music.volume)||!readLe(input,music.duty)||!readLe(input,music.loopRemaining)||
        !readLe(input,active)||!readLe(input,audible)) return false;
    music.active=active!=0; music.audible=audible!=0;
  }
  if (!readLe(input,savedState_.x)||!readLe(input,savedState_.y)||!readLe(input,savedState_.vx)||!readLe(input,savedState_.vy)||
      !readLe(input,savedState_.rng)||!readLe(input,savedState_.score)||!readLe(input,savedState_.frame)||!readLe(input,savedState_.openingFrame)||!readLe(input,savedState_.phaseFrame)||
      !readLe(input,savedState_.cameraX)||!readLe(input,savedState_.stage)||!readLe(input,savedState_.lives)||!readLe(input,savedState_.health)||
      !readLe(input,savedState_.flags)||!readLe(input,savedState_.phase)||!readLe(input,savedState_.selection)||!readLe(input,savedState_.achievements)||!readLe(input,savedState_.playerAnimFrame)||!readLe(input,savedState_.attackFrames)||!readLe(input,savedState_.facing)) return false;
  for (auto& music:savedStageMusic_) {
    std::uint8_t active{},audible{};
    if (!readLe(input,music.pc)||!readLe(input,music.loopStart)||!readLe(input,music.returnPc)||!readLe(input,music.patternTable)||
        !readLe(input,music.loopCommand)||!readLe(input,music.duration)||!readLe(input,music.note)||
        !readLe(input,music.volume)||!readLe(input,music.duty)||!readLe(input,music.loopRemaining)||
        !readLe(input,active)||!readLe(input,audible)) return false;
    music.active=active!=0; music.audible=audible!=0;
  }
  if (!input.empty()) return false;
  menuRequested_=menu!=0; jumpBuffer_=jump; coyoteFrames_=coyote; paused_=paused!=0;
  cheatInvincible_=invincible!=0; cheatInfiniteLives_=infiniteLives!=0; hasSave_=hasSave!=0; return true;
}

std::uint64_t Cheetahmen::stateHash() const { const auto state = serialize(); return fnv1a64(state); }
}  // namespace a52
