# Intro audio provenance and timing

The verified USA ROM is the sole source for the runtime intro audio. The NSF in
`Action52old` was useful as a listening reference but is not read by the
extractor, build, or runtime.

The boot code maps PRG banks 14 and 15 and writes software-decoded PCM directly
to the 2A03 `$4011` DAC. A Mesen write trace through the complete boot sequence
was used as the reference:

- Frames 33, 165, 298, 430, 562, and 694 each start the title loop.
- Frame 825 starts the 10,999-sample “Make your selection now” recording while
  the final Action 52/Cheetahmen image remains visible.
- PCM playback finishes during frame 916. The final image remains through frame
  918, followed by the cartridge's short blank/menu-drawing transition; the
  completed menu is visible at frame 950.

`a52_extract` reproduces those streams from the ROM itself:

- `audio/intro_loop.wav`: PRG bank 15, header at CPU `$C000`, packed data at
  `$C00A`. The ROM's 16-entry delta table is in PRG bank 14 at CPU `$8F6A`.
  The 15,000-byte packed source decodes low nibble then high nibble to 29,999
  `$4011` levels. The cartridge invokes it six times.
- `audio/selection_voice.wav`: PRG bank 14, header at CPU `$9200`, raw data at
  `$920A`. The cartridge outputs 10,999 `$4011` levels once.

The decoded sequences were compared byte-for-byte with the emulator's `$4011`
write trace: all 29,999 title-loop values and all 10,999 voice values match.
Generated WAVs and traces remain ignored because they are ROM-derived outputs.
