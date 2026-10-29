#!/usr/bin/env python3
"""Embed a binary file as a 64-byte-aligned C array."""

import argparse
import pathlib
import re


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("symbol")
    args = parser.parse_args()

    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", args.symbol):
        parser.error("symbol must be a C identifier")

    data = args.input.read_bytes()
    if not data:
        parser.error("cannot embed an empty file")

    lines = [
        "/* Generated from {}. Do not edit. */".format(args.input.name),
        "static const unsigned char {}[] __attribute__((aligned(64))) = {{".format(
            args.symbol
        ),
    ]
    for start in range(0, len(data), 12):
        chunk = data[start : start + 12]
        lines.append("    " + ", ".join("0x{:02x}".format(value) for value in chunk) + ",")
    lines.extend(["};", ""])

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="ascii")


if __name__ == "__main__":
    main()
