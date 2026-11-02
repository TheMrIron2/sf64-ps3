#!/usr/bin/env python3
"""Extract the US Rev 1 option-menu bank to an ignored build file."""

import argparse
from pathlib import Path

from extract_title_bank import DMA_TABLE_OFFSET, decode_mio0, read_u32


OPTION_INDEX = 43
OPTION_VROM_START = 0x00CA9DE0
OPTION_SIZE = 0x00016090


def extract_bank(rom: bytes) -> bytes:
    if len(rom) < 0x40 or rom[:4] != b"\x80\x37\x12\x40" or rom[0x3E:0x40] != b"E\x01":
        raise ValueError("expected a big-endian Star Fox 64 US Rev 1 ROM")
    entry = DMA_TABLE_OFFSET + OPTION_INDEX * 16
    vrom = read_u32(rom, entry)
    start = read_u32(rom, entry + 4)
    end = read_u32(rom, entry + 8)
    flags = read_u32(rom, entry + 12)
    if vrom != OPTION_VROM_START or start >= end or end > len(rom) or flags != 1:
        raise ValueError("invalid US Rev 1 ast_option DMA entry")
    return decode_mio0(rom[start:end], OPTION_SIZE)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--c-resource", action="store_true")
    args = parser.parse_args()
    try:
        bank = extract_bank(args.rom.read_bytes())
        args.output.parent.mkdir(parents=True, exist_ok=True)
        if args.c_resource:
            with args.output.open("w", encoding="ascii", newline="\n") as output:
                output.write("/* Generated from a local US Rev 1 ROM; do not commit. */\n")
                output.write("const unsigned char sf64_option_segment_data[] = {\n")
                for index in range(0, len(bank), 12):
                    values = ", ".join(f"0x{value:02x}" for value in bank[index:index + 12])
                    output.write(f"    {values},\n")
                output.write("};\nconst unsigned int sf64_option_segment_size = ")
                output.write("(unsigned int)sizeof(sf64_option_segment_data);\n")
        else:
            args.output.write_bytes(bank)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"extracted ast_option: {len(bank)} bytes into {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
