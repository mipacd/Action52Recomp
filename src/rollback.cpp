#include "action52/rollback.hpp"

#include <algorithm>

namespace a52 {

RollbackHistory::RollbackHistory(std::size_t capacity) : capacity_(std::max<std::size_t>(1, capacity)) {}

void RollbackHistory::capture(std::uint64_t frame, const GameModule& game) {
  const auto existing = std::find_if(snapshots_.begin(), snapshots_.end(), [frame](const auto& snapshot) { return snapshot.frame == frame; });
  RollbackSnapshot snapshot{frame, game.stateHash(), game.serialize()};
  if (existing != snapshots_.end()) *existing = std::move(snapshot);
  else snapshots_.push_back(std::move(snapshot));
  while (snapshots_.size() > capacity_) snapshots_.pop_front();
}

bool RollbackHistory::restore(std::uint64_t frame, GameModule& game) const {
  const auto found = std::find_if(snapshots_.begin(), snapshots_.end(), [frame](const auto& snapshot) { return snapshot.frame == frame; });
  return found != snapshots_.end() && game.deserialize(found->state);
}

void RollbackHistory::discardAfter(std::uint64_t frame) {
  std::erase_if(snapshots_, [frame](const auto& snapshot) { return snapshot.frame > frame; });
}

std::optional<std::uint64_t> RollbackHistory::hashAt(std::uint64_t frame) const {
  const auto found = std::find_if(snapshots_.begin(), snapshots_.end(), [frame](const auto& snapshot) { return snapshot.frame == frame; });
  return found == snapshots_.end() ? std::nullopt : std::optional(found->hash);
}
}  // namespace a52

