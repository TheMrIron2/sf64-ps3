# RSX shader toolchain findings

## Fragment output register collision

The first SF64 sqrt-transfer shader compiled and linked, but the user's RPCS3
capture showed magenta geometry, UI, and stars. The transfer formula is the
same `255 * sqrt(value / 255)` used by the local SF64-PSP renderer's
`src/psp/gfx/gfx_psp_color.c`. The error was in the generated RSX program.

NVIDIA Cg 3.1's `fp40` frontend treats temporary `R0` and `result.color` as
distinct registers. The installed PSL1GHT `cgcomp` assembler emits both at
physical register zero. Its `CCompilerFP::emit_dst()` and `emit_src()` forward
register indices directly; `Prepare()` reserves used registers but does not
relocate these overlapping temporaries. The optimized shader interleaves
output writes with further calculations in temporary `R0`:

```text
RSQR R0.x, R0.x;
RCPR oCol.x, R0.x;  # writes physical R0.x
RSQR R0.x, R0.y;    # destroys the red output
...
RCPR oCol.y, R0.x;  # also destroys a subsequent blue calculation's source
RCPR oCol.z, R0.y;
```

The resulting RGB for texture `(0.04, 0.16, 0.36)` and white shade is
`(1.0, 0.4, 1.0)` after framebuffer clamping, instead of `(0.2, 0.4, 0.6)`.
This reproduces the screenshot's red/blue clipping. Reassembling the unchanged
Cg output produces identical bytes and preserves the error.

The production shader is now
`extern/n64ps3/src/backends/ps3/shaders/rdprsx_triangle.fpa`. Its nine
instructions use `R1` for sampled color and intermediate results, and reserve
`R0` for COLOR output. RGB receives the sqrt transfer; alpha receives the
original texture-times-shade product. The build uses `cgcomp -a -f -d`, which
does not load `libCg.so`.

An SDK fix should assign fp40 temporaries to physical registers that do not
overlap live output registers, including half/full register aliases and MRT
outputs. The explicit assembly is a bounded workaround for this shader.

## Assembled microcode check

`tools/check_rsx_gamma.py` reads the actual `.fpo`, including its endian and
instruction word layout. It executes the shader's TEX/MUL/RSQ/RCP/MOV subset,
source swizzles, destination masks, precision, and shared R0/COLOR storage.
It supplies sampled texels directly and compares 582 color/alpha cases with
the intended transfer. Cases include independent RGB channels, black, white,
primary colors, every 8-bit input level, and 5-bit texture/title light pairs.
The old shader fails; the explicit assembly passes.

```sh
python3 tools/check_rsx_gamma.py \
    build/arwing/ps3/n64ps3/src/backends/ps3/rdprsx_triangle.fpo
make ps3-arwing-self PSL1GHT=/usr/local/ps3dev/PSL1GHT
```

The fragment build checks a temporary binary before installing it and saves
current assembly as `rdprsx_triangle.fpo.dump`. This host probe checks compiled
color math and register lifetimes. GPU rendering and the final title image
still need visual confirmation; VI dithering remains unimplemented.

## Assembly parser crash

The installed `CFPParser::Parse()` also crashes on empty lines: `strtok()`
returns null and `FindOpcode()` calls `strncmp()` with that null pointer.
The shader assembly contains no empty lines to avoid this defect. The SDK
parser should skip empty lines before opcode lookup.
