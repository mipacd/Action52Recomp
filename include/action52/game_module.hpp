#pragma once

#include "action52/types.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace a52 {

class FixRegistry;

class GameModule {
 public:
  virtual ~GameModule() = default;
  virtual void reset(BehaviorProfile profile, const FixRegistry& fixes, std::uint32_t seed) = 0;
  virtual void tick(const FrameInput& input) = 0;
  virtual void render(PixelBuffer& target) const = 0;
  [[nodiscard]] virtual std::span<const AudioEvent> audioEvents() const = 0;
  [[nodiscard]] virtual std::vector<std::byte> serialize() const = 0;
  virtual bool deserialize(std::span<const std::byte> state) = 0;
  [[nodiscard]] virtual std::uint64_t stateHash() const = 0;
  [[nodiscard]] virtual bool requestsMenu() const = 0;
};

}  // namespace a52

