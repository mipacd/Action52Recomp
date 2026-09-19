#include "action52/fix_registry.hpp"

#include <algorithm>

namespace a52 {

FixRegistry::FixRegistry()
    : definitions_{{"cheetahmen.stage_progression", "Reserved pending cartridge evidence; currently inactive", false, false},
                   {"cheetahmen.collision_bounds", "Reserved pending cartridge evidence; currently inactive", false, false},
                   {"cheetahmen.clean_intro_transitions", "Rebuild Cheetahmen intro screens atomically instead of showing the cartridge's one-frame pattern/palette mismatch", true, true},
                   {"cheetahmen.music_loop", "Loop each stage-music channel to its own descriptor start instead of the cartridge's shared pulse-one destination", true, true},
                   {"cheetahmen.responsive_controls", "Use accelerated and decelerated isometric movement", false, true},
                   {"cheetahmen.smooth_scrolling", "Redraw the complete scrolled background atomically to avoid nametable seams", false, true},
                   {"cheetahmen.skip_opening", "Allow Start to skip the Cheetahmen story sequence", false, true}} {
  for (const auto& fix : definitions_) overrides_.emplace(fix.id, std::nullopt);
}

bool FixRegistry::enabled(std::string_view id, BehaviorProfile profile) const {
  const auto definition = std::find_if(definitions_.begin(), definitions_.end(), [id](const auto& fix) { return fix.id == id; });
  if (definition == definitions_.end()) return false;
  const auto override = overrides_.find(id);
  if (override != overrides_.end() && override->second.has_value()) return *override->second;
  if (profile == BehaviorProfile::Original) return false;
  return profile == BehaviorProfile::BugFixes ? definition->bugFixDefault : definition->remakeDefault;
}

bool FixRegistry::setOverride(std::string_view id, std::optional<bool> value) {
  const auto found = overrides_.find(id);
  if (found == overrides_.end()) return false;
  found->second = value;
  return true;
}
}  // namespace a52
