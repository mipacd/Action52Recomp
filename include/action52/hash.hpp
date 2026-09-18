#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace a52 {

std::array<std::uint8_t, 32> sha256(std::span<const std::uint8_t> data);
std::string hexLower(std::span<const std::uint8_t> data);
std::uint64_t fnv1a64(std::span<const std::byte> data);

}  // namespace a52

