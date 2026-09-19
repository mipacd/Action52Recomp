# Cheetahmen reverse-engineering notes

These notes describe observations from the verified USA ROM. Captured frames,
ROM slices, and generated assets remain local build artifacts and are not source
repository inputs.

## Observed launch mapping

- PRG physical bank 28 is mapped at CPU `$8000` (ROM file offset `0x70010`).
- PRG physical bank 29 is mapped at CPU `$C000` (ROM file offset `0x74010`).
- Story pages use CHR banks 54, 55, and 56 in pairs. The playable program's
  mapper writes select CHR 0 for the mode selector, CHR 52 for the character
  instruction panel, and CHR 46 for level one.

The extractor emits these raw resources with provenance in the generated
manifest. They are the starting point for code/data classification; they are not
committed.

## Decompiled opening flow

PRG-28 contains a six-page state machine dispatched at CPU `$81BC`. The page
renderer consumes ROM tables rather than screenshots:

- metatile definitions: pointer table `$841B`
- 16x15 metatile maps: pointer table `$8427`
- 16-byte palettes: pointer table `$8433`
- per-metatile palette attributes: pointer table `$843F`
- story text writer: `$9189`
- story text pointers: `$91F2`

The text lengths decoded from their `$FF` terminators are 144, 89, 145, 216,
150, and 211 bytes. The text writer advances once per NMI and maps ASCII to the
ROM font, so text reveals left-to-right continuously. Each page then waits 256
frames before the next page. The complete story sequence lasts 2,521 frames.

The one-player path establishes this order:

1. Action Gamemaster at the console.
2. Story text and the television transition.
3. Three-Cheetahmen reveal.
4. Instruction panels.
5. `ACTION GAMEMASTER` one-player/two-player/main-menu selection. The screen is
   written by the state machine at `$DA49`; Select advances `$033D`, while Start
   confirms it in `$A815`.
6. A character-specific instruction panel beginning with Aries and his wooden
   clubs before stage play.

The native runtime reproduces all six steps from those PRG/CHR tables,
including the Original profile's one-frame transition corruption. It does not
use captured frames. The Aries panel expands `$E035` through `$DF8B`, uses CHR
52, and reveals the 199-byte `$E2C1` text one character per frame.

## Playable program and level one

The mapper-selected playable code is physical PRG banks 16-17, represented by
the `GAME_BANK_08` Ghidra overlay. The initial level uses the descriptor at
`$D663`; its first map descriptor resolves to `$C014`. That descriptor names a
seven-screen order table at `$C03E`, screen pointers at `$C030`, metatile
definitions at `$CEB6`, and metatile attributes at `$D00E`. The load-state
branch at `$A526` writes value 2 to mapper address `$840B`, selecting CHR bank
46 and the upper background pattern table. The palette-cycle pointer table is
at `$D72D`, with its initial 32-byte palette at `$D733`.

The native runtime now decodes the complete seven-screen level-one background,
scrolls it from deterministic player state, and draws the first Aries standing
metasprite from entity definition 2 at `$D156`. Its cel pointer targets `$D164`,
where `$11` is the sprite count and the records begin at `$D165`; sprite color
zero is transparent. The first level subdescriptor at `$D6DD` supplies starting
coordinates `$8F,$C1`; its input table routes horizontal and vertical directions
separately, so the opening area uses planar/isometric movement rather than gravity.
The stage streamer calls `$B1F2` for every entering metatile. `$D064` maps that
metatile to an entity ID (`$FF` means scenery), while `$D641` maps IDs to the
actual frame lists and metasprites. The native renderer uses those tables for
the foreground/water actors and their cartridge frame timing. Hostile movement,
collision, combat, and AI are still being classified.

The mode selector installs the 32-byte palette at `$DD03`. Palette 3
(`$0F,$05,$15,$25`) is used for its red text and cursor; the level palette at `$D733` must
not be reused on that screen.

## Opening music

The live sequencer reads the cartridge track descriptor at `$9615`, channel
streams at `$9624`, `$9637`, `$9647`, and `$9657`, shared pattern table `$9667`,
and note-period table `$A4E6`. The relevant sound-driver entry points are
`$9E61`, `$9E69`, `$9E76`, `$9F81`, `$9FC9`, and `$A37A`. The supplied NSF is
not an extraction input or runtime asset.

`tools/extract_cheetahmen.py` validates the supported ROM, emits the two PRG
banks and CHR bank, and records all of these addresses in `opening.json`.
`re/ghidra/ImportCheetahmenBanks.py` maps the mapper-selected PRG pair into a
Ghidra overlay so MCP decompilation sees CPU `$8000-$FFFF` correctly.

The stage score's four channel streams all end with `C0 96 E3 FF`: a branch to
pulse 1's `$E396` stream with repeat count `$FF`. Original preserves that shared
destination. `cheetahmen.music_loop` treats it as an indefinite top-level repeat
to each channel's own descriptor start in Bug Fixes and Remake.
Only pulse 1 begins with the shared pattern-table command at `$E396`; channels
2-4 depend on that shared state. Fixed looping therefore preserves the pattern
table while resetting each channel's call/loop state.

`TransitionCheetahmenStage` at `$A912` selects CHR 52 and calls the generator at
`$E145`. That generator expands map `$E035` through definitions `$DF8B` and
attributes `$E013`; `$E135` supplies the palette. `$E25F` then reveals the
199-byte Aries instruction text at `$E2C1` one character per frame.

## Next classification work

- Continue classifying collision, enemy movement, and attack routines
  from the first playable frame in `GAME_BANK_08`.
- Split banks 28-29 into code, pointer tables, text, music/SFX sequences, and
  level/entity data.
- Decode CHR 54 tilemaps, attribute tables, metasprites, and animation lists.
- Establish Original movement, collision, damage, death, and progression state
  hashes before enabling any Bug Fixes behavior.
