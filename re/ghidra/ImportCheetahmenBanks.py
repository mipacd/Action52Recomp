# Ghidra Python script: map the Cheetahmen mapper-228 PRG pair as one overlay.
# @category Action52

import hashlib
import os

from java.io import ByteArrayInputStream


EXPECTED_SHA256 = "5d7fcec0fe96796cac020d45b79072d971dc0dd6f8d856ceeb30046ad002e3ce"
HEADER_SIZE = 16
BANK_SIZE = 0x4000
OVERLAY_NAME = "CHEETAHMEN"


rom_path = os.environ.get("A52_ROM")
if not rom_path:
    rom_path = currentProgram.getExecutablePath()
if not rom_path or not os.path.isfile(rom_path):
    raise RuntimeError("Set A52_ROM to the verified Action 52 ROM before running this script")

with open(rom_path, "rb") as source:
    rom = source.read()
digest = hashlib.sha256(rom).hexdigest()
if digest != EXPECTED_SHA256:
    raise RuntimeError("ROM SHA-256 mismatch: " + digest)

payload = rom[HEADER_SIZE + 28 * BANK_SIZE : HEADER_SIZE + 30 * BANK_SIZE]
if len(payload) != 0x8000:
    raise RuntimeError("Cheetahmen PRG bank pair is truncated")

memory = currentProgram.getMemory()
old_block = memory.getBlock(OVERLAY_NAME)
if old_block is not None:
    print("Overlay already exists; no ROM bytes were changed: " + OVERLAY_NAME)
else:
    base = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(0x8000)
    block = memory.createInitializedBlock(
        OVERLAY_NAME, base, ByteArrayInputStream(payload), len(payload), monitor, True
    )
    block.setRead(True)
    block.setWrite(False)
    block.setExecute(True)
    space = block.getStart().getAddressSpace()
    entries = [0x81BC, 0x9189, 0x9E61, 0x9E69, 0x9E76, 0x9F81, 0x9FC9, 0xA37A]
    names = [
        "CheetahOpeningDispatch", "CheetahOpeningWriteText",
        "SoundRequestTrack", "SoundEnableVblankUpdate", "SoundInitialize",
        "SoundLoadPendingTrack", "SoundLoadChannelDescriptors", "SoundUpdate",
    ]
    for offset, name in zip(entries, names):
        address = space.getAddress(offset)
        disassemble(address)
        createFunction(address, name)
    print("Mapped and seeded Cheetahmen PRG banks in overlay: " + OVERLAY_NAME)
