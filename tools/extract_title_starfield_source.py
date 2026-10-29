#!/usr/bin/env python3
"""Generate a narrow TU from the original SF64 title starfield functions.

The SF64 source remains in its separate checkout. This extraction avoids
linking unrelated functions and assets from the original large translation
units. The output belongs under ignored build/.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def skip_literal_or_comment(source: str, index: int) -> int | None:
    if source.startswith("//", index):
        end = source.find("\n", index + 2)
        return len(source) if end < 0 else end
    if source.startswith("/*", index):
        end = source.find("*/", index + 2)
        return len(source) if end < 0 else end + 2
    if source[index] not in ('"', "'"):
        return None

    quote = source[index]
    index += 1
    while index < len(source):
        if source[index] == "\\":
            index += 2
        elif source[index] == quote:
            return index + 1
        else:
            index += 1
    return len(source)


def matching_delimiter(source: str, opening: int, left: str, right: str) -> int:
    depth = 0
    index = opening
    while index < len(source):
        skipped = skip_literal_or_comment(source, index)
        if skipped is not None:
            index = skipped
            continue
        if source[index] == left:
            depth += 1
        elif source[index] == right:
            depth -= 1
            if depth == 0:
                return index
        index += 1
    raise ValueError(f"unterminated {left}{right} block")


def extract_function(source: str, name: str) -> str:
    pattern = re.compile(rf"\b{re.escape(name)}\s*\(")
    for match in pattern.finditer(source):
        opening_paren = source.find("(", match.start())
        closing_paren = matching_delimiter(source, opening_paren, "(", ")")
        index = closing_paren + 1
        while index < len(source) and source[index].isspace():
            index += 1
        if index >= len(source) or source[index] != "{":
            continue
        closing_brace = matching_delimiter(source, index, "{", "}")
        start = source.rfind("\n", 0, match.start()) + 1
        return source[start : closing_brace + 1].strip()
    raise ValueError(f"could not find definition of {name}")


def extract_initializer(source: str, declaration: str) -> str:
    match = re.search(declaration, source, re.MULTILINE)
    if not match:
        raise ValueError(f"could not find declaration matching {declaration!r}")
    opening_brace = source.find("{", match.start(), match.end())
    closing_brace = matching_delimiter(source, opening_brace, "{", "}")
    semicolon = source.find(";", closing_brace)
    if semicolon < 0:
        raise ValueError("initializer declaration has no semicolon")
    start = source.rfind("\n", 0, match.start()) + 1
    return source[start : semicolon + 1].strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sf64-source-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    source_root = args.sf64_source_dir
    bg_source = (source_root / "src/engine/fox_bg.c").read_text(encoding="utf-8")
    play_source = (source_root / "src/engine/fox_play.c").read_text(encoding="utf-8")
    math_source = (source_root / "src/sys/sys_math.c").read_text(encoding="utf-8")
    libc_math_source = (source_root / "src/libc_math64.c").read_text(encoding="utf-8")
    memory_source = (source_root / "src/sys/sys_memory.c").read_text(encoding="utf-8")

    star_colors = extract_initializer(
        bg_source, r"^\s*u16\s+gStarColors\s*\[\s*16\s*\]\s*=\s*\{"
    )
    memory_buffer = re.search(
        r"(?ms)^\s*s32\s+sMemoryBuffer\s*\[\s*0x20000\s*\]\s*;\s*"
        r"s32\s*\*\s*sMemoryPtr\s*;",
        memory_source,
    )
    if not memory_buffer:
        raise ValueError("could not find the original SF64 allocator storage")

    source_parts = [
        '#include "global.h"',
        '#include "src/psp/renderer.h"',
        '#include "src/psp/renderer_starfield.h"',
        "#define STARFIELD_SOURCE_CAPACITY 1000",
        "#define MEM_ARRAY_ALLOCATE(arr, count) ((arr) = Memory_Allocate((count) * sizeof(*(arr))))",
        star_colors,
        memory_buffer.group(0).strip(),
        "s32 sSeededRandSeed1;",
        "s32 sSeededRandSeed2;",
        "s32 sSeededRandSeed3;",
        "GameState gGameState;",
        "s32 gStarCount;",
        "f32 gStarfieldX;",
        "f32 gStarfieldY;",
        "f32 gStarfieldRoll;",
        "f32 gStarfieldScrollX;",
        "f32 gStarfieldScrollY;",
        "LevelId gCurrentLevel;",
        "Player *gPlayer;",
        "f32 D_bg_8015F96C;",
        "f32* gStarOffsetsX;",
        "f32* gStarOffsetsY;",
        "u32* gStarFillColors;",
        extract_function(math_source, "Math_ModF"),
        extract_function(libc_math_source, "Math_FAtanF"),
        extract_function(math_source, "Math_Atan2F"),
        extract_function(math_source, "Rand_SetSeed"),
        extract_function(math_source, "Rand_ZeroOneSeeded"),
        extract_function(memory_source, "Memory_FreeAll"),
        extract_function(memory_source, "Memory_Allocate"),
        extract_function(play_source, "Play_GenerateStarfield"),
        extract_function(play_source, "Camera_SetStarfieldPos"),
        extract_function(bg_source, "Background_DrawStarfield"),
        "void sf64_original_title_starfield_init(void) {",
        "    Memory_FreeAll();",
        "    Play_GenerateStarfield();",
        "    gGameState = GSTATE_TITLE;",
        "    gStarCount = 800;",
        "    gStarfieldX = SCREEN_WIDTH;",
        "    gStarfieldY = SCREEN_HEIGHT;",
        "    gStarfieldRoll = 0.0f;",
        "    gStarfieldScrollX = 0.0f;",
        "    gStarfieldScrollY = 0.0f;",
        "    gCurrentLevel = LEVEL_CORNERIA;",
        "}",
        "void sf64_original_title_starfield_update(void) {",
        "    Camera_SetStarfieldPos(0.0f, 180.0f, 430.0f,",
        "                           0.0f, 180.0f, 0.0f);",
        "    gStarfieldScrollX -= 0.5f;",
        "    Background_DrawStarfield();",
        "}",
        "void sf64_original_title_starfield_draw(void) {",
        "    Background_DrawStarfield();",
        "}",
        "",
    ]

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n\n".join(source_parts), encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        raise SystemExit(f"extract_title_starfield_source.py: {error}")
