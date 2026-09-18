#pragma once

#include "action52/game_module.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace a52 {

struct RollbackSnapshot {
  std::uint64_t frame{};
  std::uint64_t hash{};
  std::vector<std::byte> state;
};

class RollbackHistory {
 public:
  explicit RollbackHistory(std::size_t capacity = 12);
  void capture(std::uint64_t frame, const GameModule& game);
  [[nodiscard]] bool restore(std::uint64_t frame, GameModule& game) const;
  void discardAfter(std::uint64_t frame);
  [[nodiscard]] std::optional<std::uint64_t> hashAt(std::uint64_t frame) const;
  [[nodiscard]] std::size_t size() const { return snapshots_.size(); }

 private:
  std::size_t capacity_;
  std::deque<RollbackSnapshot> snapshots_;
};

}  // namespace a52

