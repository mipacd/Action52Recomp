#pragma once

#include "action52/types.hpp"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace a52 {

struct FixDefinition {
  std::string id;
  std::string description;
  bool bugFixDefault{};
  bool remakeDefault{};
};

class FixRegistry {
 public:
  FixRegistry();
  [[nodiscard]] bool enabled(std::string_view id, BehaviorProfile profile) const;
  bool setOverride(std::string_view id, std::optional<bool> value);
  [[nodiscard]] const std::vector<FixDefinition>& definitions() const { return definitions_; }
  [[nodiscard]] const std::map<std::string, std::optional<bool>, std::less<>>& overrides() const { return overrides_; }

 private:
  std::vector<FixDefinition> definitions_;
  std::map<std::string, std::optional<bool>, std::less<>> overrides_;
};

}  // namespace a52
