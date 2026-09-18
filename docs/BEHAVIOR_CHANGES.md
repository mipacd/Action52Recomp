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
| `cheetahmen.smooth_scrolling` | Ease the pixel-precise camera toward the cartridge scroll target. | Uses immediate pixel scrolling. | Enabled by default | Implemented with a full-frame background redraw, avoiding partially updated nametable seams. |
| `cheetahmen.pause_menu` | Start opens an in-game menu using the cartridge selector font and palette, with resume, in-memory save/load, cheats, achievements, game menu, and main menu. Select cycles and Start activates, matching the Cheetahmen selector. | Unavailable | Enabled | Implemented. Cheats and save-slot contents are included in deterministic serialization. |

## Change procedure

For each new game or behavior change:

1. Record the cartridge observation and trace/frame fixture.
2. Implement and test Original behavior first.
3. Add a stable fix/enhancement ID and this ledger entry.
4. Test the flag alone, disabled, and through its profile preset.
5. Keep content, graphics, and audio unchanged in Remake unless a separately
   documented replacement pack is active.
