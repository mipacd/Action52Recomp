#!/usr/bin/env python3
"""Extract the Cheetahmen intro resources from the supported Action 52 ROM.

This script deliberately emits data, not captured frames.  Runtime rendering and
sequencing remain implementations of the cartridge's 6502 data formats.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys


EXPECTED_SHA256 = "5d7fcec0fe96796cac020d45b79072d971dc0dd6f8d856ceeb30046ad002e3ce"
HEADER_SIZE = 16
PRG_BANK_SIZE = 16 * 1024
CHR_BANK_SIZE = 8 * 1024
PRG_SIZE = 96 * PRG_BANK_SIZE

PRG_BANKS = (16, 17, 28, 29)
OPENING_CHR_BANKS = (54, 55, 56)
GAME_CHR_BANK = 4
SELECTION_CHR_BANK = 0
INSTRUCTION_CHR_BANK = 52
LEVEL_ONE_CHR_BANK = 46
TEXT_POINTERS = (0x91FE, 0x928F, 0x92E9, 0x937B, 0x9454, 0x94EB)


def cpu_word(bank: bytes, address: int) -> int:
    if not 0x8000 <= address < 0xBFFF:
        raise ValueError(f"CPU address ${address:04X} is outside PRG bank 28")
    offset = address - 0x8000
    return bank[offset] | (bank[offset + 1] << 8)


def terminated_length(bank: bytes, address: int) -> int:
    offset = address - 0x8000
    end = bank.find(b"\xff", offset)
    if end < 0:
        raise ValueError(f"missing $FF terminator for text at ${address:04X}")
    return end - offset


def extract(rom_path: Path, output_root: Path) -> None:
    rom = rom_path.read_bytes()
    digest = hashlib.sha256(rom).hexdigest()
    if digest != EXPECTED_SHA256:
        raise ValueError(
            "unsupported Action 52 ROM: SHA-256 is "
            f"{digest}, expected {EXPECTED_SHA256}"
        )
    if len(rom) != HEADER_SIZE + PRG_SIZE + 64 * CHR_BANK_SIZE:
        raise ValueError(f"unsupported ROM size: {len(rom):,} bytes")
    if rom[:4] != b"NES\x1a" or rom[4] != 96 or rom[5] != 64:
        raise ValueError("unsupported iNES header; expected 96 PRG and 64 CHR banks")
    mapper = (rom[6] >> 4) | (rom[7] & 0xF0)
    if mapper != 228 or rom[6] & 4:
        raise ValueError("unsupported cartridge; expected mapper 228 without a trainer")

    destination = output_root / "cheetahmen"
    destination.mkdir(parents=True, exist_ok=True)
    banks: dict[int, bytes] = {}
    for number in PRG_BANKS:
        start = HEADER_SIZE + number * PRG_BANK_SIZE
        banks[number] = rom[start : start + PRG_BANK_SIZE]
        (destination / f"prg_{number}.bin").write_bytes(banks[number])
    for chr_bank in (*OPENING_CHR_BANKS, GAME_CHR_BANK, SELECTION_CHR_BANK,
                     INSTRUCTION_CHR_BANK, LEVEL_ONE_CHR_BANK):
        chr_start = HEADER_SIZE + PRG_SIZE + chr_bank * CHR_BANK_SIZE
        (destination / f"chr_{chr_bank}.bin").write_bytes(
            rom[chr_start : chr_start + CHR_BANK_SIZE]
        )

    code = banks[28]
    descriptor = cpu_word(code, 0x9615)
    channels: list[dict[str, int]] = []
    cursor = descriptor
    while True:
        offset = cursor - 0x8000
        channel = code[offset]
        if channel & 0x80:
            break
        channels.append(
            {
                "channel": channel,
                "stream_address": code[offset + 1] | (code[offset + 2] << 8),
            }
        )
        cursor += 3

    pages = []
    for index, text_address in enumerate(TEXT_POINTERS):
        pages.append(
            {
                "page": index,
                "metatile_definitions": cpu_word(code, 0x841B + index * 2),
                "metatile_map": cpu_word(code, 0x8427 + index * 2),
                "palette": cpu_word(code, 0x8433 + index * 2),
                "metatile_attributes": cpu_word(code, 0x843F + index * 2),
                "text_address": text_address,
                "text_length": terminated_length(code, text_address),
                "initial_vram_address": 0x2140 if index == 5 else 0x2240,
                "pattern_half": index & 1,
                "chr_bank": OPENING_CHR_BANKS[index // 2],
            }
        )

    metadata = {
        "schema_version": 1,
        "source_sha256": digest,
        "resources": {
            "opening_prg_banks": [28, 29],
            "opening_chr_banks": list(OPENING_CHR_BANKS),
            "game_prg_banks": [16, 17],
            "game_chr_bank": GAME_CHR_BANK,
            "selection_chr_bank": SELECTION_CHR_BANK,
            "instruction_chr_bank": INSTRUCTION_CHR_BANK,
            "level_one_chr_bank": LEVEL_ONE_CHR_BANK,
        },
        "opening": {
            "state_dispatch": 0x81BC,
            "text_writer": 0x9189,
            "pages": pages,
            "transition_note": (
                "Original changes the background pattern half before rebuilding the "
                "nametable, exposing one corrupted transition frame."
            ),
        },
        "music": {
            "track_pointer": 0x9615,
            "descriptor": descriptor,
            "channels": channels,
            "pattern_table": 0x9667,
            "note_period_table": 0xA4E6,
            "driver_entry_points": {
                "request_track": 0x9E61,
                "enable_vblank_update": 0x9E69,
                "initialize": 0x9E76,
                "load_pending_track": 0x9F81,
                "load_channel_descriptors": 0x9FC9,
                "update": 0xA37A,
            },
        },
        "level_one": {
            "prg_overlay": "GAME_BANK_08",
            "chr_bank": LEVEL_ONE_CHR_BANK,
            "descriptor": 0xD663,
            "map_descriptor": 0xC014,
            "screen_order": 0xC03E,
            "screen_count": 7,
            "screen_pointer_table": 0xC030,
            "metatile_definitions": 0xCEB6,
            "metatile_attributes": 0xD00E,
            "subdescriptor": 0xD6DD,
            "initial_player_position": {"x": 0x8F, "y": 0xC1},
            "sprite_palette": 0xDE44,
            "gameplay_palette_pointer_table": 0xD72D,
            "gameplay_palette": 0xD733,
            "mapper_write": {"address": 0x840B, "value": 2},
            "music_pointer_table": 0xD717,
            "music_track": 0,
            "music_descriptor": 0xE389,
            "note_pointer_table": 0xBE20,
        },
        "aries_instruction": {
            "chr_bank": INSTRUCTION_CHR_BANK,
            "map": 0xE035,
            "metatile_definitions": 0xDF8B,
            "metatile_attributes": 0xE013,
            "palette": 0xE135,
            "text": 0xE2C1,
            "text_length": 199,
            "renderer": 0xE145,
            "text_writer": 0xE25F,
            "mapper_write": {"address": 0x840D, "value": 0},
        },
    }
    (destination / "opening.json").write_text(
        json.dumps(metadata, indent=2) + "\n", encoding="utf-8", newline="\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    try:
        extract(args.rom, args.out)
    except (OSError, ValueError) as exc:
        print(f"Cheetahmen extraction failed: {exc}", file=sys.stderr)
        return 1
    print(f"Extracted ROM-derived Cheetahmen resources to {args.out / 'cheetahmen'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
