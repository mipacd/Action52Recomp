# Action 52 (NES) Recomp

A port of Action 52 (NES) to C++20/SDL3. Only the intro, main menu, and a small part
of Cheetahmen is currently implemented. 

## Planned Features

- Original mode: preserves all bugs
- Bugfix mode: Fix bugs, but otherwise plays the same
- Remake mode: Includes enhancements such as per game save/load, cheats,
achievements, smoother controls, 2-player LAN, Add single player AI to
2 player only games
- Graphical and audio mod support
- Ports to PC, Mac, Linux, Web, PS Vita, Switch 1 

## Current milestone

- Exact ROM/header/SHA-256 validation.
- Mapper 228 menu-table decoding for all 52 entries.
- Deterministic generation of all 64 CHR tile-sheet PNGs and a versioned manifest.
- ROM-derived spotlight/camera/title intro and 52-entry, three-page menu, plus
  options, unavailable-game screen, keyboard/gamepad input, fixed NTSC-rate
  simulation, aspect correction, and live audio synthesis.
- A deterministic, serializable Cheetahmen gameplay module with three-stage flow
  and separate Original, Bug Fixes, and Remake control paths.
- An initial ROM-derived Cheetahmen story/title flow plus extracted opening PRG
  banks 28-29 and CHR bank 54. Behavior deviations are tracked in
  `docs/BEHAVIOR_CHANGES.md`.

The Cheetahmen module is an executable reconstruction scaffold. Its current stage
geometry, entities, audiovisual sequencing, and original-mode physics have **not yet
been validated against cartridge traces**, so it is not presented as a completed
faithful port. See `docs/STATUS.md` for the remaining reverse-engineering gates.

## Build

Requirements are CMake 3.24+, Python 3, a C++20 compiler, Git, and optionally an
installed SDL3 development package. CMake fetches SDL3 3.4.12 when it is not
installed. Python is used only for deterministic ROM resource extraction.

```sh
cmake -S . -B build -DA52_ROM="/path/to/Action 52 (USA) (Unl).nes"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Without `A52_ROM`, the extractor and application still compile, but the application
will explain how to generate its local assets. Assets can also be generated manually:

```sh
build/a52_extract --rom "/path/to/Action 52 (USA) (Unl).nes" --out generated-assets
python tools/extract_cheetahmen.py --rom "/path/to/Action 52 (USA) (Unl).nes" --out generated-assets
build/action52 --assets generated-assets
```

Keyboard controls: arrows, `Z` (A), `X` (B), right Shift (Select), Enter
(Start), and F11 (fullscreen). Select cycles the three cartridge menu pages;
B opens the behavior-profile options.

Generated files are copyrighted output from the user's ROM and are ignored by Git.
This repository structure is not legal advice.
