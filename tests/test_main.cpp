#include "action52/cheetahmen.hpp"
#include "action52/audio.hpp"
#include "action52/config.hpp"
#include "action52/hash.hpp"
#include "action52/rollback.hpp"
#include "png_writer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures{};
void check(bool condition, std::string_view message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main() {
  const std::string abc = "abc";
  const auto digest = a52::sha256(std::span(reinterpret_cast<const std::uint8_t*>(abc.data()), abc.size()));
  check(a52::hexLower(digest) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 known vector");

  a52::FixRegistry fixes;
  check(!fixes.enabled("cheetahmen.stage_progression", a52::BehaviorProfile::Original), "original disables fixes");
  check(!fixes.enabled("cheetahmen.stage_progression", a52::BehaviorProfile::BugFixes), "unverified fix stays disabled");
  check(!fixes.enabled("cheetahmen.responsive_controls", a52::BehaviorProfile::BugFixes), "bug-fix excludes redesign");
  check(fixes.enabled("cheetahmen.responsive_controls", a52::BehaviorProfile::Remake), "remake controls");
  check(!fixes.enabled("cheetahmen.skip_opening", a52::BehaviorProfile::BugFixes), "bug-fix keeps original opening");
  check(fixes.enabled("cheetahmen.skip_opening", a52::BehaviorProfile::Remake), "remake can skip opening");
  check(!fixes.enabled("cheetahmen.clean_intro_transitions", a52::BehaviorProfile::Original), "original preserves intro transition corruption");
  check(fixes.enabled("cheetahmen.clean_intro_transitions", a52::BehaviorProfile::BugFixes), "bug-fix cleans intro transitions");
  check(!fixes.enabled("cheetahmen.music_loop", a52::BehaviorProfile::Original), "original preserves music loop defect");
  check(fixes.enabled("cheetahmen.music_loop", a52::BehaviorProfile::BugFixes), "bug-fix repairs music loop");
  check(!fixes.enabled("cheetahmen.smooth_scrolling", a52::BehaviorProfile::BugFixes), "bug-fix keeps cartridge camera");
  check(fixes.enabled("cheetahmen.smooth_scrolling", a52::BehaviorProfile::Remake), "remake smooth camera");
  check(fixes.setOverride("cheetahmen.responsive_controls", false), "known override accepted");
  check(!fixes.enabled("cheetahmen.responsive_controls", a52::BehaviorProfile::Remake), "override wins");

  a52::Cheetahmen first, second;
  first.reset(a52::BehaviorProfile::Original, fixes, 1234);
  second.reset(a52::BehaviorProfile::Original, fixes, 1234);
  for (std::uint64_t frame=0; frame<120; ++frame) {
    a52::FrameInput input; input.frame=frame; input.controllers[0].held=a52::mask(a52::Button::Right);
    if (frame==10) input.controllers[0].pressed=a52::mask(a52::Button::A);
    first.tick(input); second.tick(input);
  }
  check(first.stateHash() == second.stateHash(), "deterministic simulation");
  const auto snapshot = first.serialize();
  a52::Cheetahmen restored; restored.reset(a52::BehaviorProfile::Original, fixes, 1);
  check(restored.deserialize(snapshot), "snapshot deserializes");
  check(restored.stateHash() == first.stateHash(), "snapshot round trip");
  auto damaged = snapshot; damaged.push_back(std::byte{0});
  check(!restored.deserialize(damaged), "snapshot rejects trailing data");

  a52::RollbackHistory history(3);
  history.capture(120, first);
  const auto savedHash = first.stateHash();
  a52::FrameInput advance; advance.frame=121; advance.controllers[0].held=a52::mask(a52::Button::Left);
  first.tick(advance);
  check(first.stateHash() != savedHash, "simulation advances after snapshot");
  check(history.restore(120, first), "rollback restores captured frame");
  check(first.stateHash() == savedHash, "rollback state hash matches");
  check(history.hashAt(120) == savedHash, "rollback exposes desync hash");

  a52::NesSynth synth;
  synth.setSampleRate(48000);
  const std::array cues{a52::UiAudioCue::MenuMove};
  synth.handleUiCues(cues);
  std::array<float,960> audio{};
  synth.render(audio);
  check(std::any_of(audio.begin(),audio.end(),[](float value){ return value != 0.0f; }), "menu APU cue produces audio");

  const auto pngPath = std::filesystem::temp_directory_path() / "a52_png_writer_test.png";
  std::array<std::uint8_t,16> pixels{255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
  std::string error;
  check(writeRgbaPng(pngPath,2,2,pixels,error), "PNG writer succeeds");
  std::ifstream png(pngPath,std::ios::binary); std::array<std::uint8_t,8> signature{}; png.read(reinterpret_cast<char*>(signature.data()),8);
  check(signature == std::array<std::uint8_t,8>{137,80,78,71,13,10,26,10}, "PNG signature");
  std::error_code ec; std::filesystem::remove(pngPath,ec);

  if (failures) { std::cerr << failures << " test(s) failed\n"; return 1; }
  std::cout << "All tests passed\n"; return 0;
}
