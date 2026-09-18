#include "extractor.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  std::filesystem::path rom, out;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--rom" && i + 1 < argc) rom = argv[++i];
    else if (arg == "--out" && i + 1 < argc) out = argv[++i];
    else if (arg == "--help") {
      std::cout << "Usage: a52_extract --rom <Action 52 ROM> --out <directory>\n";
      return 0;
    } else { std::cerr << "Unknown or incomplete argument: " << arg << '\n'; return 2; }
  }
  if (rom.empty() || out.empty()) {
    std::cerr << "Both --rom and --out are required. The ROM is read but never copied.\n"; return 2;
  }
  std::string error;
  if (!extractAction52(rom, out, error)) { std::cerr << "Extraction failed: " << error << '\n'; return 1; }
  std::cout << "Assets generated in " << out << "\n";
  return 0;
}

