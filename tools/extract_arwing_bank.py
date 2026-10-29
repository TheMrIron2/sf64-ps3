#!/usr/bin/env python3
"""Extract the US Rev 1 ast_arwing bank into a build-only C resource."""

import argparse
from pathlib import Path
import struct


DMA_TABLE_OFFSET = 0x000DE480
AST_ARWING_INDEX = 9
AST_ARWING_ROM_START = 0x008BFC00
AST_ARWING_SIZE = 0x00019DF0
BODY_DL_OFFSET = 0x00009B60
SKELETON_OFFSET = 0x00016610


def read_u32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise ValueError("ROM does not contain the required US Rev 1 DMA entry")
    return struct.unpack_from(">I", data, offset)[0]


def decode_mio0(data: bytes) -> bytes:
    if len(data) < 16 or data[:4] != b"MIO0":
        raise ValueError("ast_arwing payload is not a MIO0 bank")

    output_size, compressed_offset, raw_offset = struct.unpack_from(">III", data, 4)
    if output_size != AST_ARWING_SIZE:
        raise ValueError(
            f"ast_arwing MIO0 size is 0x{output_size:x}; expected 0x{AST_ARWING_SIZE:x}"
        )
    if not (16 <= compressed_offset <= len(data)) or not (
        16 <= raw_offset <= len(data)
    ):
        raise ValueError("ast_arwing MIO0 offsets are outside the DMA payload")

    control_offset = 16
    compressed_cursor = compressed_offset
    raw_cursor = raw_offset
    output = bytearray()

    while len(output) < output_size:
        if control_offset >= len(data):
            raise ValueError("truncated MIO0 control stream")
        control = data[control_offset]
        control_offset += 1

        for bit in range(7, -1, -1):
            if len(output) >= output_size:
                break
            if control & (1 << bit):
                if raw_cursor >= len(data):
                    raise ValueError("truncated MIO0 literal stream")
                output.append(data[raw_cursor])
                raw_cursor += 1
            else:
                if compressed_cursor + 2 > len(data):
                    raise ValueError("truncated MIO0 back-reference stream")
                first = data[compressed_cursor]
                second = data[compressed_cursor + 1]
                compressed_cursor += 2
                distance = (((first & 0x0F) << 8) | second) + 1
                length = (first >> 4) + 3
                if distance > len(output):
                    raise ValueError("MIO0 back-reference precedes output start")
                for _ in range(min(length, output_size - len(output))):
                    output.append(output[-distance])

    return bytes(output)


def extract_bank(rom: bytes) -> bytes:
    if len(rom) < 0x40 or rom[:4] != b"\x80\x37\x12\x40":
        raise ValueError("expected a big-endian N64 .z64 ROM")
    if rom[0x3E:0x40] != b"E\x01":
        raise ValueError("expected Star Fox 64 US Rev 1 (region E, revision 1)")

    entry = DMA_TABLE_OFFSET + AST_ARWING_INDEX * 16
    vrom_start = read_u32(rom, entry)
    data_start = read_u32(rom, entry + 4)
    data_end = read_u32(rom, entry + 8)
    flags = read_u32(rom, entry + 12)
    if vrom_start != AST_ARWING_ROM_START or data_end < data_start:
        raise ValueError("US Rev 1 DMA entry does not identify ast_arwing")
    if data_end > len(rom):
        raise ValueError("ast_arwing DMA payload extends past the ROM")

    payload = rom[data_start:data_end]
    if flags == 1:
        segment = decode_mio0(payload)
    elif flags == 0 and data_start == AST_ARWING_ROM_START and len(payload) == AST_ARWING_SIZE:
        segment = payload
    else:
        raise ValueError(f"unsupported ast_arwing DMA flags/layout: {flags}")

    if segment[BODY_DL_OFFSET : BODY_DL_OFFSET + 8] != bytes.fromhex(
        "fd1000000300ac48"
    ):
        raise ValueError("decompressed body display list does not match US Rev 1 metadata")
    if segment[SKELETON_OFFSET : SKELETON_OFFSET + 4] != bytes.fromhex(
        "030165f0"
    ):
        raise ValueError("decompressed skeleton pointer does not match US Rev 1 metadata")
    return segment


def write_c_resource(segment: bytes, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="ascii", newline="\n") as source:
        source.write("/* Generated from a local US Rev 1 ROM; do not commit. */\n")
        source.write("const unsigned char sf64_arwing_segment_data[] = {\n")
        for start in range(0, len(segment), 12):
            values = ", ".join(f"0x{byte:02x}" for byte in segment[start : start + 12])
            source.write(f"    {values},\n")
        source.write("};\n")
        source.write(
            "const unsigned int sf64_arwing_segment_size = "
            "(unsigned int)sizeof(sf64_arwing_segment_data);\n"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path, help="local US Rev 1 .z64 ROM")
    parser.add_argument("output_c", type=Path, help="generated C file under build/")
    parser.add_argument(
        "--binary-output",
        type=Path,
        help="also write the extracted segment as a build-only binary",
    )
    args = parser.parse_args()

    try:
        segment = extract_bank(args.rom.read_bytes())
        write_c_resource(segment, args.output_c)
        if args.binary_output:
            args.binary_output.parent.mkdir(parents=True, exist_ok=True)
            args.binary_output.write_bytes(segment)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    print(
        f"extracted ast_arwing: {len(segment)} bytes from US Rev 1 ROM "
        f"into {args.output_c}"
    )
    if args.binary_output:
        print(f"also wrote extracted segment to {args.binary_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
