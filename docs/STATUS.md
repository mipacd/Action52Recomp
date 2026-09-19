# Reconstruction status

## Implemented and testable

- Cross-platform CMake/C++20 layout and SDL3 frontend.
- Verified-ROM extraction boundary and deterministic manifest.
- Complete 52-entry mapper launch inventory.
- CHR 2bpp decoder and standalone PNG encoder.
- Fixed-frame input, profile/fix configuration, save-state serialization, bounded rollback history, state hashes,
  controller hotplug, display scaling, and basic 2A03-style synthesis.
- ROM-derived intro keyframes, pixel-exact captures of all three cartridge menu
  pages, full-label cursor palettes, PRG-ROM-decoded title/voice audio, traced menu
  pulse effects, options/unavailable state flow, and a playable three-stage
  Cheetahmen scaffold.
- Cheetahmen opening PRG banks 28-29 and CHR banks 54-56 decoded into six metatile
  scenes, per-character story text, cartridge-timed pauses, profile-specific
  transition behavior, and a live ROM-sequence music stream. No Cheetahmen
  screenshot is loaded by the runtime.
- Cheetahmen's exact ROM-written one/two-player/main-menu selector and its
  Select/Start behavior are implemented from `GAME_BANK_08`. Level one's seven
  screen background, initial palette, animated Aries metasprite, and track-zero
  music stream are decoded from PRG banks 16-17 and CHR bank 46.
- The selector now uses its `$DD03` red palette. Sprite color zero is transparent,
  and the Aries cel records correctly begin after the count byte at `$D165`.
- The ROM-generated Aries instruction panel now appears between player selection
  and the level/score card. It uses map `$E035`, definitions `$DF8B`, palette
  `$E135`, CHR 52, and the 199-byte text stream at `$E2C1`.
- Level one starts Aries at descriptor coordinates `$8F,$C1`, uses isometric
  four-direction movement without platformer gravity, rotates the three ROM
  water palettes, and uses the level's sprite palette at `$CEA6`. Class-1 map
  cells now block Aries using the ROM's entity-2 collision extents.
- Remake has a ROM-font pause menu controlled with Select/Start, deterministic
  in-memory save/load, cheats, achievements, game-menu/main-menu exits, and
  responsive full-frame scrolling without the former eased-camera lag.
- Level-one map markers are decoded through the ROM's entity-ID table at `$D064`.
  Their actual metasprites and frame timing come from the `$D641` definition table;
  this restores the animated water/foreground actors and removes the former
  duplicate-Aries placeholders. Full hostile movement, collision, and AI remain
  under classification.
## Required before calling milestone one faithful

1. Continue Ghidra decompilation from the mapper-aware Cheetahmen bank overlay;
   use emulator traces only as differential verification evidence.
2. Extend the Cheetahmen RAM map and code/data classifications in `re/symbols.csv`.
3. Decode exact metasprite animations, collision shapes, entity spawn tables, music,
   SFX, and stage transitions into the generated manifest.
4. Replace scaffold physics/entities/stages with behavior derived from those traces.
5. Add frame-, state-, and audio-event differential fixtures for every stage and death,
   restart, ending, and menu-return path.
6. Confirm each named bug against Original before enabling its Bug Fixes behavior.

Netplay, CPU opponents, Ooze improvements, replacement-pack loading, and games 1–51
remain later milestones. The deterministic `GameModule` snapshot interface is the
foundation for rollback but no network transport is claimed as implemented yet.
