#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

bool writeRgbaPng(const std::filesystem::path& path, int width, int height,
                  std::span<const std::uint8_t> rgba, std::string& error);

