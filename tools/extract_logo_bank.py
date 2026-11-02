#!/usr/bin/env python3
"""Extract the US Rev 1 Nintendo logo bank into an ignored C resource."""

import argparse
from pathlib import Path
from extract_title_bank import DMA_TABLE_OFFSET, read_u32

SIZE = 128 * 74


def extract_bank(rom):
    if len(rom) < 0x40 or rom[:4] != b"\x80\x37\x12\x40" or rom[0x3e:0x40] != b"E\x01":
        raise ValueError("expected a big-endian Star Fox 64 US Rev 1 ROM")
    entry = DMA_TABLE_OFFSET + 48 * 16
    vrom, start, end, flags = (read_u32(rom, entry + i * 4) for i in range(4))
    if vrom != 0xd25100 or flags != 0 or end - start != SIZE or end > len(rom):
        raise ValueError("invalid US Rev 1 ast_logo DMA entry")
    return rom[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        data = extract_bank(args.rom.read_bytes())
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("w", encoding="ascii", newline="\n") as output:
            output.write("/* Generated from a local US Rev 1 ROM; do not commit. */\n")
            output.write("const unsigned char sf64_boot_logo_data[] = {\n")
            for index in range(0, len(data), 12):
                output.write("    " + ", ".join(f"0x{b:02x}" for b in data[index:index + 12]) + ",\n")
            output.write("};\nconst unsigned int sf64_boot_logo_size = sizeof(sf64_boot_logo_data);\n")
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"extracted ast_logo: {len(data)} bytes into {args.output}")


if __name__ == "__main__":
    main()
