#include "png_writer.hpp"

#include <array>
#include <fstream>
#include <vector>

namespace {
void append32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value >> 24));
  out.push_back(static_cast<std::uint8_t>(value >> 16));
  out.push_back(static_cast<std::uint8_t>(value >> 8));
  out.push_back(static_cast<std::uint8_t>(value));
}

std::uint32_t crc32(std::span<const std::uint8_t> data) {
  std::uint32_t crc = 0xffffffffu;
  for (const auto byte : data) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

std::uint32_t adler32(std::span<const std::uint8_t> data) {
  std::uint32_t a = 1, b = 0;
  for (const auto byte : data) { a = (a + byte) % 65521u; b = (b + a) % 65521u; }
  return (b << 16) | a;
}

void chunk(std::vector<std::uint8_t>& png, const std::array<char, 4>& type,
           std::span<const std::uint8_t> data) {
  append32(png, static_cast<std::uint32_t>(data.size()));
  const auto start = png.size();
  for (const auto c : type) png.push_back(static_cast<std::uint8_t>(c));
  png.insert(png.end(), data.begin(), data.end());
  append32(png, crc32(std::span(png).subspan(start, 4 + data.size())));
}
}  // namespace

bool writeRgbaPng(const std::filesystem::path& path, int width, int height,
                  std::span<const std::uint8_t> rgba, std::string& error) {
  if (width <= 0 || height <= 0 || rgba.size() != static_cast<std::size_t>(width * height * 4)) {
    error = "invalid PNG dimensions or pixel count";
    return false;
  }
  std::vector<std::uint8_t> filtered;
  filtered.reserve((static_cast<std::size_t>(width) * 4 + 1) * height);
  for (int y = 0; y < height; ++y) {
    filtered.push_back(0);
    const auto row = rgba.subspan(static_cast<std::size_t>(y) * width * 4, static_cast<std::size_t>(width) * 4);
    filtered.insert(filtered.end(), row.begin(), row.end());
  }
  std::vector<std::uint8_t> zlib{0x78, 0x01};
  std::size_t offset = 0;
  while (offset < filtered.size()) {
    const auto length = static_cast<std::uint16_t>(std::min<std::size_t>(65535, filtered.size() - offset));
    const bool final = offset + length == filtered.size();
    zlib.push_back(final ? 1 : 0);
    zlib.push_back(static_cast<std::uint8_t>(length));
    zlib.push_back(static_cast<std::uint8_t>(length >> 8));
    const auto inverse = static_cast<std::uint16_t>(~length);
    zlib.push_back(static_cast<std::uint8_t>(inverse));
    zlib.push_back(static_cast<std::uint8_t>(inverse >> 8));
    zlib.insert(zlib.end(), filtered.begin() + static_cast<std::ptrdiff_t>(offset),
                filtered.begin() + static_cast<std::ptrdiff_t>(offset + length));
    offset += length;
  }
  append32(zlib, adler32(filtered));

  std::vector<std::uint8_t> png{137,80,78,71,13,10,26,10};
  std::vector<std::uint8_t> ihdr;
  append32(ihdr, static_cast<std::uint32_t>(width));
  append32(ihdr, static_cast<std::uint32_t>(height));
  ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
  chunk(png, {'I','H','D','R'}, ihdr);
  chunk(png, {'I','D','A','T'}, zlib);
  chunk(png, {'I','E','N','D'}, {});

  std::ofstream file(path, std::ios::binary);
  if (!file) { error = "cannot create " + path.string(); return false; }
  file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
  if (!file) { error = "failed writing " + path.string(); return false; }
  return true;
}

