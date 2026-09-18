# Reverse-engineering record

This directory contains reproducibility metadata only. It must not contain ROM bytes,
extracted graphics or audio, generated disassembly listings, emulator recordings, or
other copyrighted output.

The supported reference image has SHA-256
`5d7fcec0fe96796cac020d45b79072d971dc0dd6f8d856ceeb30046ad002e3ce`.
It is an iNES mapper 228 image with 96 × 16 KiB PRG units, 64 × 8 KiB CHR
units, and no trainer. The menu launch table begins at file offset `0x805A1`
and contains 52 records of 18 bytes each.

Locally generated analysis belongs under `local-re/`, which is ignored. Symbol and
classification updates should be made to `symbols.csv` without including opcode or
data dumps.

## Evidence levels

- `observed`: confirmed in a debugger trace from the supported ROM.
- `static`: derived from control/data-flow analysis but not yet observed.
- `hypothesis`: useful working theory awaiting confirmation.

