#include "extractor.hpp"

#include "action52/hash.hpp"
#include "png_writer.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <span>
#include <vector>

namespace {
constexpr std::string_view kExpectedSha = "5d7fcec0fe96796cac020d45b79072d971dc0dd6f8d856ceeb30046ad002e3ce";
constexpr std::size_t kHeaderSize = 16;
constexpr std::size_t kPrgSize = 96 * 16384;
constexpr std::size_t kChrSize = 64 * 8192;
constexpr std::size_t kMenuTable = 0x805a1;
constexpr std::size_t kIntroCodeBank = kHeaderSize + 14 * 16384;
constexpr std::size_t kIntroDataBank = kHeaderSize + 15 * 16384;

constexpr std::array<std::string_view, 52> kTitles = {
  "Fire Breathers","Star Evil","Illuminator","G-Force Fighters","Ooze","Silver Sword",
  "Critical Bypass","Jupiter Scope","Alfredo","Operation Full Moon","Dam Busters","Thrusters",
  "Haunted Hills","Chill Out","Sharks","Megalonia","French Baker","Atmos Quake","Meong",
  "Space Dreams","Streemerz","Spread-Fire","Bubblegum Rosie","Micro Mike","Underground",
  "Rocket Jockey","Non Human","Cry Baby","Slashers","Crazy Shuffle","Fuzz Power",
  "Shooting Gallery","Lollipops","Evil Empire","Sombreros","Storm Over Desert","Mash Man",
  "They Came","Lazer League","Billy Bob","City of Doom","Bits and Pieces","Beeps and Blips",
  "Manchester","Boss","Dedant","Hambo","Time Warp Tickers","Jigsaw","Ninja Assault",
  "Robbie Robot","Cheetahmen"};

struct Entry { std::uint16_t address; std::uint8_t selector; std::uint8_t value; };

std::string escape(std::string_view text) {
  std::string out;
  for (const auto c : text) { if (c == '"' || c == '\\') out.push_back('\\'); out.push_back(c); }
  return out;
}

bool readFile(const std::filesystem::path& path, std::vector<std::uint8_t>& bytes, std::string& error) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) { error = "cannot open ROM: " + path.string(); return false; }
  const auto size = file.tellg();
  if (size < 0) { error = "cannot determine ROM size"; return false; }
  bytes.resize(static_cast<std::size_t>(size));
  file.seekg(0);
  file.read(reinterpret_cast<char*>(bytes.data()), size);
  if (!file) { error = "failed reading ROM"; return false; }
  return true;
}

bool decodeEntry(std::span<const std::uint8_t> bytes, std::size_t offset, Entry& out) {
  if (offset + 18 > bytes.size()) return false;
  const auto x = bytes.subspan(offset, 18);
  const std::array<std::uint8_t, 14> fixedPositions = {0,2,3,4,6,7,8,10,11,12,13,15,16,17};
  const std::array<std::uint8_t, 14> fixedValues = {0xa9,0x85,0xfe,0xa9,0x85,0xff,0xa9,0x8d,0xff,0x07,0xa9,0x4c,0x12,0x80};
  for (std::size_t i = 0; i < fixedPositions.size(); ++i)
    if (x[fixedPositions[i]] != fixedValues[i]) return false;
  out.address = static_cast<std::uint16_t>(x[1] | (x[5] << 8));
  out.selector = x[9];
  out.value = x[14];
  return true;
}

bool writeTilesheet(const std::filesystem::path& path, std::span<const std::uint8_t> chr, std::string& error) {
  constexpr int tilesAcross = 32, tilesDown = 16, scale = 2;
  constexpr int width = tilesAcross * 8 * scale, height = tilesDown * 8 * scale;
  constexpr std::array<std::array<std::uint8_t,4>,4> colors = {{{0,0,0,0},{85,85,85,255},{170,170,170,255},{255,255,255,255}}};
  std::vector<std::uint8_t> rgba(width * height * 4);
  for (int tile = 0; tile < 512; ++tile) {
    const int tx = (tile % tilesAcross) * 8 * scale, ty = (tile / tilesAcross) * 8 * scale;
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
      const auto bit = 7 - x;
      const auto pixel = ((chr[tile*16+y] >> bit) & 1) | (((chr[tile*16+y+8] >> bit) & 1) << 1);
      for (int sy = 0; sy < scale; ++sy) for (int sx = 0; sx < scale; ++sx) {
        const auto at = static_cast<std::size_t>(((ty+y*scale+sy)*width + tx+x*scale+sx)*4);
        for (int c = 0; c < 4; ++c) rgba[at+c] = colors[pixel][c];
      }
    }
  }
  return writeRgbaPng(path, width, height, rgba, error);
}

void writeLe16(std::ostream& out, std::uint16_t value) {
  const std::array<char,2> bytes{char(value),char(value>>8)}; out.write(bytes.data(),bytes.size());
}
void writeLe32(std::ostream& out, std::uint32_t value) {
  const std::array<char,4> bytes{char(value),char(value>>8),char(value>>16),char(value>>24)}; out.write(bytes.data(),bytes.size());
}

bool writeDacWav(const std::filesystem::path& path, std::span<const std::uint8_t> levels,
                 std::uint32_t sampleRate, std::string& error) {
  std::ofstream out(path,std::ios::binary);
  if (!out) { error="cannot create ROM-derived audio asset: "+path.string(); return false; }
  const auto dataSize=static_cast<std::uint32_t>(levels.size()*2);
  out.write("RIFF",4); writeLe32(out,36+dataSize); out.write("WAVEfmt ",8); writeLe32(out,16);
  writeLe16(out,1); writeLe16(out,1); writeLe32(out,sampleRate); writeLe32(out,sampleRate*2);
  writeLe16(out,2); writeLe16(out,16); out.write("data",4); writeLe32(out,dataSize);
  for (const auto level:levels) writeLe16(out,static_cast<std::uint16_t>(static_cast<std::int16_t>((int(level)-64)*512)));
  if (!out) { error="failed writing ROM-derived audio asset: "+path.string(); return false; }
  return true;
}

bool extractIntroPcm(std::span<const std::uint8_t> rom, const std::filesystem::path& audioDirectory,
                     std::string& error) {
  const auto code=rom.subspan(kIntroCodeBank,16384);
  const auto data=rom.subspan(kIntroDataBank,16384);
  const std::uint16_t encodedLength=std::uint16_t(data[0])|std::uint16_t(data[1])<<8;
  const std::uint16_t voiceLength=std::uint16_t(code[0x1200])|std::uint16_t(code[0x1201])<<8;
  if (encodedLength!=15000 || data[2]!=7 || data[3]!=15 || data[4]!=64 ||
      voiceLength!=10999 || code[0x1202]!=9 || code[0x1203]!=40 || code[0x1204]!=64) {
    error="intro PCM metadata does not match the supported ROM"; return false;
  }
  std::array<std::int8_t,16> deltas{};
  for (std::size_t i=0;i<deltas.size();++i) deltas[i]=static_cast<std::int8_t>(code[0xf6a+i]);
  std::vector<std::uint8_t> title; title.reserve(29999);
  std::uint8_t level=data[4]; title.push_back(level);
  for (std::size_t i=0;i<encodedLength-1;++i) {
    const auto packed=data[10+i];
    level=static_cast<std::uint8_t>(level+deltas[packed&15]); title.push_back(level);
    level=static_cast<std::uint8_t>(level+deltas[packed>>4]); title.push_back(level);
  }
  std::vector<std::uint8_t> voice; voice.reserve(voiceLength); voice.push_back(code[0x1204]);
  voice.insert(voice.end(),code.begin()+0x120a,code.begin()+0x120a+voiceLength-1);
  return writeDacWav(audioDirectory/"intro_loop.wav",title,14144,error) &&
         writeDacWav(audioDirectory/"selection_voice.wav",voice,7395,error);
}

}  // namespace

bool extractAction52(const std::filesystem::path& romPath,
                     const std::filesystem::path& outputDirectory,
                     std::string& error) {
  std::vector<std::uint8_t> rom;
  if (!readFile(romPath, rom, error)) return false;
  if (rom.size() != kHeaderSize + kPrgSize + kChrSize) {
    error = "unsupported ROM size; expected 2,097,168 bytes"; return false;
  }
  if (!(rom[0]=='N' && rom[1]=='E' && rom[2]=='S' && rom[3]==0x1a && rom[4]==96 && rom[5]==64)) {
    error = "unsupported iNES header (expected 96 PRG units and 64 CHR units)"; return false;
  }
  const auto mapper = static_cast<int>((rom[6] >> 4) | (rom[7] & 0xf0));
  if (mapper != 228 || (rom[6] & 4) != 0) { error = "unsupported cartridge; expected mapper 228 without trainer"; return false; }
  const auto digest = a52::hexLower(a52::sha256(rom));
  if (digest != kExpectedSha) { error = "ROM SHA-256 mismatch: got " + digest; return false; }

  std::array<Entry, 52> entries{};
  for (std::size_t i = 0; i < entries.size(); ++i) {
    if (!decodeEntry(rom, kMenuTable + i * 18, entries[i])) {
      error = "menu launch table signature mismatch at entry " + std::to_string(i + 1); return false;
    }
  }

  std::error_code ec;
  std::filesystem::create_directories(outputDirectory / "graphics" / "chr", ec);
  std::filesystem::create_directories(outputDirectory / "audio", ec);
  std::filesystem::create_directories(outputDirectory / "metadata", ec);
  std::filesystem::create_directories(outputDirectory / "ui", ec);
  if (ec) { error = "cannot create output directories: " + ec.message(); return false; }

  const auto chrStart = kHeaderSize + kPrgSize;
  for (std::size_t bank = 0; bank < 64; ++bank) {
    std::ostringstream name; name << "bank_" << std::setw(2) << std::setfill('0') << bank << ".png";
    if (!writeTilesheet(outputDirectory / "graphics" / "chr" / name.str(),
                        std::span(rom).subspan(chrStart + bank * 8192, 8192), error)) return false;
  }

  {
    std::ofstream menuChr(outputDirectory / "ui" / "menu.chr", std::ios::binary);
    menuChr.write(reinterpret_cast<const char*>(rom.data() + chrStart), 8192);
    if (!menuChr) { error = "failed writing ROM-derived menu CHR"; return false; }
  }
  if (!extractIntroPcm(rom,outputDirectory/"audio",error)) return false;

  std::filesystem::create_directories(outputDirectory/"cheetahmen",ec);
  if (ec) { error="cannot create Cheetahmen asset directory: "+ec.message(); return false; }
  const auto writeRange=[&](const std::filesystem::path& path,std::size_t offset,std::size_t size) {
    std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(rom.data()+offset),static_cast<std::streamsize>(size));
    if (!out) { error="failed writing Cheetahmen ROM resource: "+path.string(); return false; }
    return true;
  };
  if (!writeRange(outputDirectory/"cheetahmen"/"prg_28.bin",kHeaderSize+28*16384,16384) ||
      !writeRange(outputDirectory/"cheetahmen"/"prg_29.bin",kHeaderSize+29*16384,16384) ||
      !writeRange(outputDirectory/"cheetahmen"/"chr_54.bin",chrStart+54*8192,8192)) return false;

  std::ofstream manifest(outputDirectory / "manifest.json", std::ios::binary);
  if (!manifest) { error = "cannot create manifest.json"; return false; }
  manifest << "{\n  \"schema_version\": 1,\n  \"rom_sha256\": \"" << digest << "\",\n"
           << "  \"mapper\": 228,\n  \"games\": [\n";
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const auto& e = entries[i];
    const int chip = (e.address >> 11) & 3;
    const int page = (e.address >> 6) & 31;
    const int chr = ((e.address & 15) << 2) | (e.value & 3);
    manifest << "    {\"number\":" << i+1 << ",\"title\":\"" << escape(kTitles[i])
             << "\",\"mapper_address\":" << e.address << ",\"mapper_value\":" << int(e.value)
             << ",\"menu_selector\":" << int(e.selector) << ",\"prg_chip\":" << chip
             << ",\"prg_page\":" << page << ",\"chr_bank\":" << chr << "}"
             << (i + 1 == entries.size() ? "\n" : ",\n");
  }
  manifest << "  ],\n  \"generated_files\": {\"chr_sheets\":64,\"audio_sequences\":2,\"cheetahmen_raw_resources\":3},\n"
              "  \"intro_audio\": {\"title_loop\":{\"file\":\"audio/intro_loop.wav\",\"prg_bank\":15,\"cpu_address\":49162,\"decoded_samples\":29999,\"plays\":6},\"selection_voice\":{\"file\":\"audio/selection_voice.wav\",\"prg_bank\":14,\"cpu_address\":37386,\"decoded_samples\":10999,\"start_frame\":825}},\n"
              "  \"cheetahmen\": {\"entry_prg_banks\":[28,29],\"opening_chr_bank\":54,\"metadata\":\"cheetahmen/opening.json\",\"evidence\":\"ROM code at $9E61/$9E69/$9E76 and track table at $9615\"},\n"
              "  \"notes\": \"Intro PCM and Cheetahmen raw resources are extracted directly from verified ROM; no NSF input is used.\"\n}\n";
  if (!manifest) { error = "failed writing manifest.json"; return false; }

  std::ofstream provenance(outputDirectory / "metadata" / "provenance.json", std::ios::binary);
  provenance << "{\"schema_version\":1,\"source_sha256\":\"" << digest
             << "\",\"extractor\":\"a52_extract\",\"copyrighted_outputs\":true}\n";
  return static_cast<bool>(provenance);
}
