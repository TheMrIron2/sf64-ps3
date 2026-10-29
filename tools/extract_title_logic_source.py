#!/usr/bin/env python3
"""Extract the original title boot/setup/update path into a build-only TU.

The generated file keeps the title's real setup and per-frame state machine,
while avoiding the rest of the game's menu, audio, and cutscene link graph.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

from extract_title_starfield_source import extract_function


TITLE_ANIMATION_SYMBOLS = {
    "aTitleFoxRunningAnim": "Animation",
    "aTitleFoxSpeakAnim2": "Animation",
    "aTitleFoxSkel": "Limb **",
    "aTitleFalcoRunningAnim": "Animation",
    "aTitleFalcoSpeakAnim": "Animation",
    "aTitleFalcoSkel": "Limb **",
    "aTitlePeppyRunningAnim": "Animation",
    "aTitlePeppySpeakAnim": "Animation",
    "aTitlePeppySkel": "Limb **",
    "aTitleSlippyRunningAnim": "Animation",
    "aTitleSlippySpeakAnim": "Animation",
    "aTitleSlippySkel": "Limb **",
}


def title_symbol_offset(yaml_source: str, name: str) -> int:
    match = re.search(
        rf"(?m)^\s*{re.escape(name)}:\s*\{{[^\n]*?offset:\s*(0x[0-9a-fA-F]+)",
        yaml_source,
    )
    if not match:
        raise ValueError(f"could not find {name} in ast_title metadata")
    address = int(match.group(1), 16)
    if address >> 24 != 0x06:
        raise ValueError(f"{name} is not in N64 segment 6")
    return address


def extract_state_globals(source: str) -> str:
    start = source.find("f32 D_menu_801B7BB0;")
    end = source.find("void Title_Setup(void)", start)
    if start < 0 or end < 0:
        raise ValueError("could not find title state globals")

    globals_source = source[start:end]
    messages = re.search(
        r"(?ms)^u16\*\s*sGralPepperMsg\[5\]\s*=\s*\{.*?^\};\s*",
        globals_source,
    )
    if messages:
        globals_source = globals_source[: messages.start()] + globals_source[messages.end() :]
    return globals_source.strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sf64-source-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    source_root = args.sf64_source_dir
    title_source = (source_root / "src/overlays/ovl_menu/fox_title.c").read_text(
        encoding="utf-8"
    )
    menu_dispatch_source = (
        source_root / "src/overlays/ovl_menu/fox_i_menu.c"
    ).read_text(encoding="utf-8")
    stdlib_source = (source_root / "src/engine/fox_std_lib.c").read_text(
        encoding="utf-8"
    )
    sys_math_source = (source_root / "src/sys/sys_math.c").read_text(
        encoding="utf-8"
    )
    title_yaml = (source_root / "assets/yaml/us/rev1/ast_title.yaml").read_text(
        encoding="utf-8"
    )

    aliases = []
    for name, c_type in TITLE_ANIMATION_SYMBOLS.items():
        address = title_symbol_offset(title_yaml, name)
        if c_type == "Animation":
            aliases.append(
                f"#define {name} (*(Animation *)(uintptr_t)0x{address:08x}U)"
            )
        else:
            aliases.append(
                f"#define {name} ((Limb **)(uintptr_t)0x{address:08x}U)"
            )

    # The original PSP camera helper is retained because it is the title
    # source's platform implementation and avoids importing the full N64
    # matrix library just to produce the title camera basis.
    animation_frame_data = extract_function(stdlib_source, "Animation_GetFrameData")
    animation_frame_data = animation_frame_data.replace(
        "Animation* animation = SEGMENTED_TO_VIRTUAL(animationSegment);",
        "Sf64N64Animation32* animation = "
        "(Sf64N64Animation32*)SEGMENTED_TO_VIRTUAL(animationSegment);",
    )
    animation_frame_data = animation_frame_data.replace(
        "JointKey* key = SEGMENTED_TO_VIRTUAL(animation->jointKey);",
        "JointKey* key = (JointKey*)SEGMENTED_TO_VIRTUAL(animation->jointKey);",
    )
    animation_frame_data = animation_frame_data.replace(
        "u16* frameData = SEGMENTED_TO_VIRTUAL(animation->frameData);",
        "u16* frameData = (u16*)SEGMENTED_TO_VIRTUAL(animation->frameData);",
    )
    animation_frame_count = extract_function(stdlib_source, "Animation_GetFrameCount")
    animation_frame_count = animation_frame_count.replace(
        "Animation* animation = SEGMENTED_TO_VIRTUAL(animationSegment);",
        "Sf64N64Animation32* animation = "
        "(Sf64N64Animation32*)SEGMENTED_TO_VIRTUAL(animationSegment);",
    )

    source_parts = [
        "#define TARGET_PSP 1",
        '#include "global.h"',
        '#include "fox_title.h"',
        "#include <stdint.h>",
        "",
        "extern void *sf64_original_segmented_to_virtual(uintptr_t address);",
        "#undef SEGMENTED_TO_VIRTUAL",
        "#define SEGMENTED_TO_VIRTUAL(address) sf64_original_segmented_to_virtual((uintptr_t)(address))",
        "#undef AUDIO_PLAY_BGM",
        "#define AUDIO_PLAY_BGM(sequence) ((void)(sequence))",
        "#undef AUDIO_SET_SPEC",
        "#define AUDIO_SET_SPEC(layout, spec) ((void)0)",
        "#undef AUDIO_PLAY_SFX",
        "#define AUDIO_PLAY_SFX(id, source, token) ((void)0)",
        "#undef SEQCMD_STOP_SEQUENCE",
        "#define SEQCMD_STOP_SEQUENCE(player, duration) ((void)0)",
        "#define PSP_TRACE(message) ((void)0)",
        "extern s32 sRandSeed1;",
        "extern s32 sRandSeed2;",
        "extern s32 sRandSeed3;",
        "",
        "/* N64 asset pointers stay 32-bit on the 64-bit PPU. */",
        "typedef struct Sf64N64Animation32 {",
        "    s16 frameCount;",
        "    s16 limbCount;",
        "    uint32_t frameData;",
        "    uint32_t jointKey;",
        "} Sf64N64Animation32;",
        "",
        *aliases,
        "",
        extract_state_globals(title_source),
        extract_function(title_source, "PspTitle_WrapRadians"),
        extract_function(title_source, "PspTitle_SinApprox"),
        extract_function(title_source, "PspTitle_CosApprox"),
        animation_frame_data,
        animation_frame_count,
        extract_function(stdlib_source, "Math_SmoothStepToF"),
        extract_function(stdlib_source, "Math_SmoothStepToAngle"),
        extract_function(stdlib_source, "Math_SmoothStepToVec3fArray"),
        extract_function(sys_math_source, "Rand_ZeroOne"),
        extract_function(title_source, "Title_CheckControllers"),
        extract_function(title_source, "Title_GetRankTotalHits"),
        extract_function(title_source, "Title_Setup"),
        extract_function(title_source, "Title_Main"),
        extract_function(title_source, "Title_UpdateEntry"),
        extract_function(title_source, "Title_NextState_Check"),
        extract_function(title_source, "Title_NextState_TitleScreen"),
        extract_function(title_source, "Title_NextState_OptionMenu"),
        extract_function(title_source, "Title_Screen_Input"),
        extract_function(title_source, "Title_Screen_Setup"),
        extract_function(title_source, "Title_Screen_Update"),
        extract_function(title_source, "Title_SetCamUp3"),
        "/* Keep the original overlay call boundary for title updates. */",
        "void Title_Main(void);",
        "void Title_Draw(void);",
        "void Option_Main(void);",
        "void Option_Entry_Draw(void);",
        "void Option_GameOver(void);",
        "void Option_8019E8C8(void);",
        "void Map_Main(void);",
        "void Map_Draw(void);",
        "static int s_title_screen_draw_requested;",
        extract_function(menu_dispatch_source, "OvlMenu_CallFunction"),
        "/* Non-title overlay targets are excluded from this title-only build. */",
        "void Title_Draw(void) {",
        "    /* Mirrors the original Title_Draw TITLE_SCREEN dispatch gate. */",
        "    s_title_screen_draw_requested = (sCutsceneState == TITLE_SCREEN);",
        "}",
        "int sf64_original_title_logic_draw(void) {",
        "    s_title_screen_draw_requested = 0;",
        "    if (gDrawMode != DRAW_TITLE) {",
        "        return 0;",
        "    }",
        "    OvlMenu_CallFunction(OVLCALL_TITLE_DRAW, NULL);",
        "    return s_title_screen_draw_requested;",
        "}",
        "void Option_Main(void) {}",
        "void Option_Entry_Draw(void) {}",
        "void Option_GameOver(void) {}",
        "void Option_8019E8C8(void) {}",
        "void Map_Main(void) {}",
        "void Map_Draw(void) {}",
        "/* These branches are unreachable in the title-only boot harness. */",
        "void Title_Ranking_Update(void) {}",
        "void Title_CsGreatFoxTraveling_Update(void) {}",
        "void Title_CsTeamRunning_Update(void) {}",
        "void Title_CsGreatFoxCloseUp_Update(void) {}",
        "void Title_CsTakeOff_Update(void) {}",
        "void Title_CsTakeOffSpace_Update(void) {}",
        "",
    ]

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n\n".join(source_parts), encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        raise SystemExit(f"extract_title_logic_source.py: {error}")
