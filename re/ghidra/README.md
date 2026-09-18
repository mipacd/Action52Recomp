# Ghidra setup for mapper-selected banks

Importing the complete `.nes` file as a flat 16-bit image truncates/wraps the
2 MiB cartridge and does not expose the mapper-228 bank selected for a game.
The MCP server will then decompile unrelated bytes at the same CPU addresses.

For Cheetahmen:

1. Set `A52_ROM` to the supported ROM path before starting Ghidra.
2. In Ghidra's Script Manager, add this directory as a script directory.
3. Run `ImportCheetahmenBanks.py` once.
4. Save the program and select an address in the `CHEETAHMEN` overlay.

The script validates the full ROM SHA-256, maps physical PRG banks 28 and 29 as
one executable overlay at CPU `$8000-$FFFF`, and seeds the known opening and
sound-driver entry points. It never copies ROM bytes into the repository.

With the overlay active, MCP address arguments may be written as
`CHEETAHMEN:9189`, `CHEETAHMEN:9E76`, and so on. This keeps decompilation tied
to the actual mapper-selected code instead of the flat-ROM import.
