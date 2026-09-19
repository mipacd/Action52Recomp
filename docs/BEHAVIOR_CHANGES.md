# Behavior changes ledger

Every intentional deviation from the verified cartridge behavior is recorded
here. Entries must name the affected profile, evidence, implementation status,
and regression coverage. A suspected cartridge defect is not enabled in the
Bug Fixes preset until a trace or repeatable test confirms it.

## Cross-profile fidelity corrections

| ID | Change | Evidence | Status |
|---|---|---|---|
| `frontend.intro_pcm_seam` | Play the ROM-decoded 29,999-sample title loop without an audible host-side buffer gap. | PRG playback routine and `$4011` trace; the decoded loop endpoints meet at DAC level 64. | Implemented for all profiles. This corrects host playback and is not a gameplay change. |
| `cheetahmen.selector_palette` | Use the selector's palette at `$DD03` rather than the level palette at `$D733`. | `RenderCheetahmenModeSelect` installs `$DD03`; the previous native renderer incorrectly shared the stage palette. | Implemented for all profiles; this is a reconstruction correction. |
| `cheetahmen.sprite_transparency` | Treat sprite color zero as transparent and begin the Aries cel records at `$D165`. | Entity definition 2 points to `$D164`; `$D164=$11` is the count and the first OAM record is `$D165`. | Implemented for all profiles; this removes the black tile rectangles. |
| `cheetahmen.map_entities` | Decode map-triggered actors through `$D064` and animate their ROM metasprites through `$D641`. | The stage streamer calls `$B1F2` for each entering metatile; `$FF` is scenery and IDs 5-11 select the level actors. | Implemented for all profiles; replaces the former duplicate-Aries placeholders and restores animated foreground/water actors. |
| `cheetahmen.stage_sprite_palette` | Use the sprite half of the level palette at `$CEA6`. | `$CE96` is the 32-byte palette named by the level map descriptor; its second half is the PPU sprite palette. `$DE44` was a misclassified table. | Implemented for all profiles; Aries palette 0 is black, white, and yellow. |
| `cheetahmen.player_animation` | Advance Aries' two standing/running cels only when movement invokes the cartridge animation update, and select attack definitions 3/4 on A. | `$8A2C` advances the entity cel from movement handlers; `$B479` replaces entity 2 with the attack entity selected through `$D6C8`. | Implemented for all profiles. |
| `cheetahmen.level_card_layout` | Place `LEVEL 1`, lives/player, and the six-digit score at the ROM VRAM coordinates. | `$D9F3` writes at `$46`; `$DBF7` writes lives + ` PLAYER 1` at `$63`; `$DC59` writes the score at `$69`. | Implemented for all profiles. |
| `cheetahmen.basic_collision` | Block movement on map collision class 1 and against active map entities using the ROM's per-entity extents. | `$8D2E/$8D60` query `$D00E`; the directional handlers reject class 1. `$D785/$D796` provide horizontal/vertical extents. | Implemented for all profiles; special tiles, damage, and attack hit resolution still need full engine reconstruction. |

## Cheetahmen: Bug Fixes profile

| ID | Change | Evidence | Original | Bug Fixes / Remake |
|---|---|---|---|---|
| `cheetahmen.clean_intro_transitions` | Make each story-page pattern-table/palette/nametable change atomic. | The state dispatcher at PRG-28 `$81BC` changes the background pattern half before the following state rebuilds the nametable. The cartridge consequently exposes the old map with the new pattern half for one frame. | Preserves the mismatched transition frame. | Suppresses that frame. |
| `cheetahmen.music_loop` | Loop each music channel to its own descriptor start. | All four channel streams end in `C0 96 E3 FF`, incorrectly targeting pulse 1 at `$E396`; the first pass is correct and later passes converge on the pulse-1 stream. | Preserves the shared `$E396` branch and post-loop distortion. | Interprets the `$FF` top-level repeat as an indefinite branch to that channel's original descriptor pointer. |

The gameplay IDs `cheetahmen.stage_progression` and
`cheetahmen.collision_bounds` remain reserved and default off until cartridge
evidence and independent regression tests exist.

## Cheetahmen: Remake profile

| ID | Enhancement | Original/Bug Fixes | Remake | Status |
|---|---|---|---|---|
| `cheetahmen.responsive_controls` | Acceleration and deceleration on both axes of the isometric first-stage movement plane. | Disabled | Enabled by default | Implemented; tuning will be revisited as later-stage movement modes are classified. |
| `cheetahmen.skip_opening` | Press Start during the story sequence to jump to the one/two-player selection screen. | Disabled | Enabled by default | Implemented; does not alter story assets or gameplay state. |
| `cheetahmen.smooth_scrolling` | Redraw the pixel-precise camera atomically. | Uses immediate pixel scrolling. | Uses the same responsive tracking while avoiding partially updated nametable seams. | Implemented without the former eased-camera lag. |
| `cheetahmen.pause_menu` | Start opens an in-game menu using the cartridge selector font and palette, with resume, in-memory save/load, cheats, achievements, game menu, and main menu. Select cycles and Start activates, matching the Cheetahmen selector. | Unavailable | Enabled | Implemented. Cheats and save-slot contents are included in deterministic serialization. |

## Change procedure

For each new game or behavior change:

1. Record the cartridge observation and trace/frame fixture.
2. Implement and test Original behavior first.
3. Add a stable fix/enhancement ID and this ledger entry.
4. Test the flag alone, disabled, and through its profile preset.
5. Keep content, graphics, and audio unchanged in Remake unless a separately
   documented replacement pack is active.
