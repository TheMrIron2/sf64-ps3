#!/usr/bin/env python3
"""Check the assembled title gamma shader, including RSX output/R0 aliasing.

This executes the small TEX/MUL/RSQ/RCP/MOV subset used by this shader. Texture
samples are supplied directly, so this checks compiled color math and register
lifetimes; it does not simulate rasterization or replace GPU visual checks.
Instruction fields follow PSL1GHT tools/cgcomp/include/nvfx_shader.h.
"""

import argparse
import math
from pathlib import Path
import struct


def reciprocal(value):
    return math.inf if value == 0.0 else 1.0 / value


def load_program(path):
    data = path.read_bytes()
    if len(data) < 36:
        raise ValueError("truncated fragment program")
    header = struct.unpack_from(">6H4I4H", data)
    if header[0] != 0x4650 or not header[9] & 0x40:
        raise ValueError("expected PSL1GHT FP program with float32 COLOR output")
    count, offset = header[5], header[8]
    if count == 0 or offset < 36 or offset + count * 16 != len(data):
        raise ValueError("invalid instruction range")
    program = []
    for index in range(count):
        words = struct.unpack_from(">4I", data, offset + index * 16)
        # Fragment microcode stores each instruction word with halves swapped.
        words = tuple(((word >> 16) | (word << 16)) & 0xFFFFFFFF
                      for word in words)
        if bool(words[0] & 1) != (index == count - 1):
            raise ValueError("invalid program end marker")
        program.append(words)
    return program


def evaluate(program, texel, shade):
    # On RSX, writing COLOR writes physical R0; it is not a separate register.
    registers = [[0.0] * 4 for _ in range(64)]

    def source(words, position):
        operand = words[position + 1]
        if operand & (1 << 8):
            raise ValueError("half register sources are outside this probe's subset")
        kind = operand & 3
        if kind == 0:
            values = registers[(operand >> 2) & 63]
        elif kind == 1 and ((words[0] >> 13) & 15) == 1:
            values = shade
        else:
            raise ValueError("unsupported source register")
        values = [values[(operand >> (9 + channel * 2)) & 3]
                  for channel in range(4)]
        if operand & (1 << (29 if position == 0 else 18)):
            values = [abs(value) for value in values]
        if operand & (1 << 17):
            values = [-value for value in values]
        return values

    for words in program:
        operation = (words[0] >> 24) & 63
        if words[0] & ((1 << 7) | (1 << 30)) or (words[1] >> 18) & 7 != 7:
            raise ValueError("unsupported destination or conditional instruction")
        if operation == 0x17:  # TEX: supplied sample from texture unit zero.
            if (words[0] >> 17) & 15:
                raise ValueError("unexpected texture unit")
            result = texel
        elif operation == 0x01:  # MOV
            result = source(words, 0)
        elif operation == 0x02:  # MUL
            result = [a * b for a, b in zip(source(words, 0), source(words, 1))]
        elif operation == 0x1A:  # RCP
            result = [reciprocal(source(words, 0)[0])] * 4
        elif operation == 0x1B:  # RSQ
            result = [reciprocal(math.sqrt(abs(source(words, 0)[0])))] * 4
        else:
            raise ValueError(f"unsupported opcode 0x{operation:02x}")
        precision = (words[0] >> 22) & 3
        if precision > 1:
            raise ValueError("unsupported instruction precision")
        number_format = "e" if precision == 1 else "f"
        destination = registers[(words[0] >> 1) & 63]
        for channel, value in enumerate(result):
            if words[0] & (1 << (9 + channel)):
                if words[0] & (1 << 31):
                    value = min(1.0, max(0.0, value))
                destination[channel] = struct.unpack(
                    number_format, struct.pack(number_format, value))[0]
    return tuple(min(1.0, max(0.0, value)) for value in registers[0])


def color_cases():
    yield (0.04, 0.16, 0.36, 0.8), (1.0, 1.0, 1.0, 0.5)
    for rgb in ((0, 0, 0), (1, 1, 1), (1, 0, 0), (0, 1, 0), (0, 0, 1)):
        yield (*rgb, 1.0), (1.0, 1.0, 1.0, 1.0)
    for value in range(256):
        yield (value / 255.0, (255 - value) / 255.0,
               (value * 73 % 256) / 255.0, value / 255.0), (
                   0.75, 0.25, 0.0625, (255 - value) / 255.0)
    for texel in range(32):
        for light in (0, 1, 6, 11, 29, 30, 40, 64, 128, 255):
            yield (texel / 31.0,) * 4, (light / 255.0,) * 4


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path)
    args = parser.parse_args()
    try:
        program = load_program(args.program)
        count = 0
        for texel, shade in color_cases():
            actual = evaluate(program, texel, shade)
            expected = tuple(math.sqrt(texel[i] * shade[i]) for i in range(3))
            expected += (texel[3] * shade[3],)
            if any(not math.isfinite(a) or abs(a - b) > 2e-6
                   for a, b in zip(actual, expected)):
                raise ValueError(f"color mismatch: texel={texel}, shade={shade}, "
                                 f"expected={expected}, actual={actual}")
            count += 1
    except (OSError, ValueError, struct.error) as error:
        parser.exit(1, f"RSX gamma probe failed: {error}\n")
    print(f"RSX gamma probe passed: {count} color/alpha cases, "
          f"{len(program)} instructions, shared R0/COLOR register modeled")


if __name__ == "__main__":
    main()
