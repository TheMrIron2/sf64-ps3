# First Graphics Milestone

## Objective

Establish a native RSX framebuffer and a portable F3DEX1-to-RSX path, then feed
the original Arwing from the title pose through `n64ps3`. The standard smoke
build still uses a synthetic triangle. A separate Arwing build extracts the
local US Rev 1 ROM's asset bank at build time and submits its skeleton-generated
display lists and RGBA16 textures.

## Original Title Execution and Rendering Path

The public [`TheMrIron2/sf64-psp` reference](https://github.com/TheMrIron2/sf64-psp)
contains the original game source used for this investigation. No separate
decompilation checkout is required for the current asset harness.

1. `src/psp/main.c` initializes the PSP `n64psp` runtime and platform services,
   then calls `bootproc()`.
2. `src/sys/sys_main.c:bootproc` checks the IPL3 state, calls `osInitialize()`
   and `Main_Initialize()`, then starts the idle thread. The idle thread sets
   the VI mode, initializes PI services, and starts the main thread.
3. The main thread starts audio, graphics, timer, and serial threads and
   dispatches VI/SP/DP messages. The PSP build uses a frame scheduler in
   `src/sys/sys_main.c` to run `Game_Update()`, build a graphics task, and submit
   it for rendering.
4. While `gGameState == GSTATE_TITLE`, `Game_Update()` calls the menu overlay's
   title update entry. `Title_Main()` advances setup and title animation state.
   When `gDrawMode == DRAW_TITLE`, `Game_Draw()` draws the starfield and calls
   the menu overlay title draw entry. `Title_Draw()` calls
   `Title_Screen_Draw()` for the team and Arwing, followed by logos and title
   text.
   `Title_Arwing_Draw(0)` sets the one-light state and `SETUPDL_23`, applies
   the `sTitleArwing[FOX]` transform, and calls `Display_Arwing_Skel()`. The
   menu path selects the half-open wing animation and traverses the Arwing
   skeleton. Its body and wing display lists use RGBA16 textures, so geometry
   transforms and lighting are only part of the required renderer surface.
5. The completed N64 display list is interpreted by the PSP Fast3D frontend in
   `src/psp/gfx/gfx_psp_dl.c` and passed through the PSP backend/device seams.
   The GU backend is in `src/psp/gfx/gfx_psp_backend_gu.c`.

### Arwing Asset and Display-List Entry Point

The attached local US Rev 1 ROM is ignored by Git. Its DMA metadata maps
`ast_arwing` into N64 segment 3 at ROM offset `0x8BFC00`. SF64-PSP identifies
`aAwBodyDL` at segmented address
`0x03009B60`, `aAwRightWingDL` at `0x03015D80`, `aAwArwingSkel` at
`0x03016610`, and `aAwLeftWingDL` at `0x03016660`. The extractor validates the
ROM revision, DMA entry, and asset signatures, then emits the 105,968-byte
segment as a generated C resource under ignored `build/` output. No ROM or
extracted asset data is tracked by Git.

The title setup places Fox's Arwing at `(-80, 220, -360)`, scales it by `4`,
and rotates it `(35, -90, -15)` degrees. `Title_Screen_Draw()` calls
`Title_Arwing_Draw(0)`, which installs the title light and `SETUPDL_23`, applies
that transform, then calls `Display_Arwing_Skel()`. The menu branch obtains
frame zero of `aAwWingsHalfOpenAnim` and calls `Animation_DrawSkeleton()` with
`aAwArwingSkel`; the skeleton traversal emits the body and wing display lists.
The game-specific harness now follows this asset path: it parses the 18-limb
skeleton and animation data, builds the title transform, and emits a task that
calls the original body and wing lists. It cancels the title world translation
and applies a 0.75 fit scale because its current projection is an orthographic
diagnostic rather than the title camera. The original lists use RGBA16
textures. The current Arwing build decodes and samples these textures with the
title's texture-times-shade combine mode; the full title camera and remaining
RDP state are not yet reproduced.

The host probe and PS3 harness preserve segment 3 references. The host probe
decodes the original lists and confirms their geometry; the PS3 SELF embeds
only the generated bank needed by this harness. Keep the ROM and generated bank
out of source control.

Starting the original code requires much more than calling `Title_Draw()`:
the game state, overlay dispatch, update state, matrix/light globals, starfield,
asset segment addresses, animation data, and display-list memory all need to be
valid. Normal boot also initializes save/audio/game services and game threads.
The title uses copyrighted game asset data, which is not copied into this
repository.

## Adopted Framework and RSX Architecture

`n64ps3` owns the renderer backend seam. Host builds continue to select
`rdpnull`; the PS3 Makefile fragment selects `rdprsx`. The PS3 backend currently:

- queries and preserves the active video resolution and aspect;
- initializes an RSX context with a 1 MiB aligned host buffer;
- allocates and registers two XRGB display buffers plus a depth buffer;
- clears each render target to a dark blue space color;
- decodes submitted F3DEX1 tasks through the portable `n64ps3` frontend,
  collecting triangles into RSX-resident vertex/index buffers;
- forwards per-vertex RGBA through an RSX vertex attribute and interpolating
  shader; and
- submits synchronized flips at the active display refresh.

Renderer initialization failures now propagate back through
`n64ps3_runtime_init()`. The frontend remains renderer-neutral. The PS3 smoke
program runs continuously and checks PSL1GHT exit callbacks so a frame can stay
visible for inspection.

The portable decoder handles big-endian F3DEX1 `VTX`, `TRI1`, `TRI2`,
`DL`/`ENDDL`, segment bases, directional/ambient lights, geometry mode,
`MTX`/`POPMTX`, and the RGBA16 `SETTIMG`/`SETTILE`/`LOADBLOCK`/`SETTILESIZE`
subset exercised by the Arwing. It bounds command count and list depth,
resolves 32-bit segmented addresses through bounded task RDRAM, decodes N64
split integer/fraction matrices, and emits clip-space vertices. It also carries
texture coordinates, dimensions, and wrap modes per triangle. `LOADSYNC`,
`PIPESYNC`, and `TILESYNC` are accepted as no-ops; other fixed-width commands
remain unsupported.

The RSX backend converts the Arwing's N64 RGBA16 texture blocks to linear
ARGB8888, samples them with the original vertex texture coordinates, and
multiplies them by decoded vertex lighting, matching the title setup's
texture-times-shade combine mode. It applies tile wrap/clamp modes and groups
triangles by texture and geometry state. `G_ZBUFFER` controls RSX depth and cull
bits select RSX face culling. General combiner, RDP render-mode, alpha, fog, and
framebuffer state remain unsupported. The first textured SELF crashed in
RPCS3 because the RSX descriptor advertised zero mip levels. The initializer
now advertises its single base level; the rebuilt SELF runs and displays the
textured Arwing in RPCS3.

## Functionality by Repository

### `n64ps3`

- Added a portable F3DEX1 decoder with nested display-list calls, segment-base
  handling, `MTX` load/multiply, modelview push/pop, vertex transformation,
  geometry mode, directional/ambient light state, lit vertex colors, triangle
  callbacks, and bounded memory resolution.
- Extended graphics tasks with optional RDRAM bounds and an N64-address
  resolver.
- Added the `rdprsx` backend prototype under `src/backends/ps3/rdprsx.c`.
- Added a vertex/fragment shader pair and RSX indexed drawing that binds
  decoded per-vertex RGBA at its own vertex-buffer offset, and translates depth
  state and face culling from F3DEX1 geometry mode. The interleaved color field
  binds at byte offset 16 with attribute frequency 0. The backend invalidates
  the vertex cache before indexed draws and logs the first decoded vertex and
  RGBA as a one-time probe.
- Extended the frontend with RGBA16 texture-block commands, texture-coordinate
  and tile state, and per-triangle texture references. The RSX backend uploads
  5-5-5-1 pixels as ARGB8888 and samples them in its shader.
- Made renderer initialization return a status and propagate failures.
- Fixed duplicate VI event delivery in the host retrace helper.
- Updated `rdpnull` to report F3DEX1 decode statistics for host validation.

### `sf64-ps3`

- Linked the PSL1GHT RSX, GCM, video, pad, and system utility libraries.
- Added shader compilation and binary embedding to the PS3 Makefile flow.
- Added the PS3 process metadata and exit callback loop.
- Added a synthetic segmented F3DEX1 display list and backing RDRAM to the
  smoke task, including host-side assertions for big-endian matrix decoding and
  transformed clip coordinates. The `n64ps3` sample additionally checks an
  F3DEX1 directional plus ambient lighting sequence without game assets.
- Added a game-specific Arwing task builder that reads the original skeleton
  and frame-zero wing animation, writes title-pose matrices/lights, and calls
  the original body and wing display lists.
- Added `tools/extract_arwing_bank.py`, a PS3 Arwing SELF target, and a host
  probe. Generated asset data and SELF files remain under ignored `build/`.
- Added this milestone report and refreshed the runtime gap report.

## Build and Test Results

- `make -B host` and `./build/host/sf64_ps3_smoke`: passed. The SF64 smoke
  display list contains 9 commands, 1 vertex load, 1 triangle, 1 nested list,
  and two matrix commands. Its callback checks transformed coordinates, amber
  RGBA, and zero geometry mode so the visual smoke triangle runs without depth
  or culling. Runtime, queue, and VI event probes work. Controller polling remains
  incomplete and PI reads remain unavailable without a ROM; both are optional
  smoke probes.
- `cmake -S extern/n64ps3 -B /tmp/n64ps3-title-goal-build` and
  `cmake --build /tmp/n64ps3-title-goal-build -j2`: passed for the host library
  and samples.
- `gfx_task` ran and passed its decoder assertions. It validates a segmented
  nested `TRI1` list, big-endian matrix decoding, clip-space coordinates, a
  direct-RDRAM `TRI2` list with matrix push/pop, matrix multiplication, and
  ambient plus directional F3DEX1 lighting (expected RGB 50,70,90) and captures
  the Arwing setup's lighting, z-buffer, and backface-cull bits.
- `make -B arwing-probe` and
  `build/host/sf64_ps3_arwing_probe /tmp/baserom.us.rev1.uncompressed.z64`:
  passed against the local US Rev 1 ROM after MIO0 decompression. The harness
  found 18 skeleton limbs, emitted 9 original lists, and `n64ps3` decoded 352
  commands, 32 vertex loads, 27 RGBA16 texture loads, and 174 textured original
  triangles with no skipped commands. It found 14 unique texture sources
  (32x32 and 64x32). Fitted clip bounds were x `[-0.643, 0.952]`,
  y `[-0.229, 0.986]`; 522 vertices were inside the viewport.
- `vi_retrace`, `hello_runtime`, and `os_queue` sample runs passed;
  `vi_retrace` reported one message per retrace and `os_queue` received `42`.
- `PS3DEV=/usr/local/ps3dev PSL1GHT=/usr/local/ps3dev/PSL1GHT CGCOMP_RUNTIME_DIR=/tmp/nvidia-cg-3.1/usr/lib/x86_64-linux-gnu make -B ps3-self`:
  passed after the vertex attribute offset and depth/cull changes. It compiled
  both Cg shaders,
  embedded their binaries, linked the big-endian PPU ELF, and produced
  `build/ps3/sf64_ps3_smoke.self` using
  `ppu-strip` → `sprxlinker` → `make_self`. The RPCS3-booted comparison used
  that same PSL1GHT packaging pipeline. `make -B ps3-fake-self` also passed and
  produced `build/ps3/sf64_ps3_smoke.fake.self` with `fself`. Neither target
  creates an installable PKG.
- Shader compilation requires PSL1GHT `cgcomp` and NVIDIA Cg 3.1's `libCg.so`.
  The runtime was extracted to `/tmp` from the official
  [NVIDIA Cg Toolkit package](https://developer.nvidia.com/cg-toolkit-download)
  for this build; it was not installed system-wide.
- The strict host target uses C99. The PSL1GHT target uses GNU C99 because its
  system headers use the `asm` extension; project source remains C99-compatible.
- `CGCOMP_RUNTIME_DIR=/tmp/nvidia-cg-3.1/usr/lib/x86_64-linux-gnu make -B
  ps3-arwing-self`: passed. The build compiled the Arwing PPU branch, extracted
  the asset bank under ignored `build/arwing/ps3/resources/`, and produced
  `build/arwing/ps3/sf64_ps3_arwing.self` with the textured RSX shader.

## Visual and Hardware Status

RPCS3 0.0.41-19517 is running on Windows from `C:\Program Files\RPCS3`. The
latest standard SELF was booted with the original Vulkan renderer and LLVM PPU
recompiler settings. The RPCS3 log records `rdprsx ready: 1280x720`, a full
one-time backend probe of
`position=0 color=3 vertex=(-0.701,-0.580,0.000,1.000)` and
`rgba=(1.000,0.820,0.122,1.000)`, plus repeated PPU output reporting one
decoded triangle per task. This ties the latest code to the expected amber
vertex data and the F3DEX1 task path. The user reports that the triangle draws
correctly. The earlier triangle capture was covered by RPCS3's Vulkan pipeline
compilation overlay; the later Arwing screenshot is unobscured and provides the
current visual confirmation.

The user launched the earlier geometry-only `sf64_ps3_arwing.self` in RPCS3
0.0.41-19517 using Vulkan. Their screenshot shows a recognizable Arwing
silhouette against the dark-blue clear at about 60 FPS, confirming original
skeleton and body/wing geometry passed through the PPU build and RSX renderer.
The SELF has since been rebuilt with RGBA16 texture sampling; that newer image
has not yet been inspected in RPCS3. Neither build executes the original
title-screen game loop, and no PS3 hardware test has been performed.

The bootable artifacts are regular SELF files produced by `ppu-strip` →
`sprxlinker` → `make_self`; these are not installable PKGs. Keep the current
emulator settings and inspect the texture-enabled SELF before extending the
renderer or adding other title elements.

## Hardware Evaluation

Build from the repository root:

```sh
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=/usr/local/ps3dev/PSL1GHT
# This WSL session extracted NVIDIA Cg under /tmp; use your own path if needed.
export CGCOMP_RUNTIME_DIR=/tmp/nvidia-cg-3.1/usr/lib/x86_64-linux-gnu
make ps3-self
make ps3-fake-self # optional fself debug-format output
```

The PPU compiler and link libraries are taken from `PS3DEV`; PSL1GHT headers
are taken from `PSL1GHT`.

For the authentic geometry harness, place a legally obtained US Rev 1 `.z64`
ROM at the repository root as `baserom.us.rev1.z64`, then run
`make ps3-arwing-self`. This target validates and extracts only the Arwing bank
into ignored `build/arwing/` output and creates
`build/arwing/ps3/sf64_ps3_arwing.self`. It does not execute the original game
loop or draw the Arwing's RGBA16 textures yet.

Load `build/ps3/sf64_ps3_smoke.self` with the local `ps3load` workflow or another
PS3 homebrew loader. The expected smoke image is a dark-blue clear and one
amber triangle, with the `rdprsx ready: WIDTHxHEIGHT` diagnostic. To test the
Arwing geometry build, load `build/arwing/ps3/sf64_ps3_arwing.self`; RPCS3 has
shown a dark, untextured Arwing silhouette on the same blue clear. PS3 hardware
validation has not been performed. Both are regular SELF files, not installable
PKGs.

For RPCS3 from Windows, the standard smoke SELF is at
`C:\Users\dwelc\Documents\GitHub\sf64-ps3\build\ps3\sf64_ps3_smoke.self`;
the original Arwing geometry SELF is at
`C:\Users\dwelc\Documents\GitHub\sf64-ps3\build\arwing\ps3\sf64_ps3_arwing.self`.
The `.fake.self` output is the optional PSL1GHT `fself` format.

## Remaining Obstacles

1. Run the texture-enabled SELF in RPCS3 and inspect material colors, UV
   orientation, wrapping, and alpha. The screenshot so far covers the earlier
   geometry-only build.
2. Reproduce the title camera and verify viewport conversion and lighting
   against original title display lists. The current task uses a fit transform
   in place of the title camera and only a subset of N64 state.
3. Expand RDP render-mode, alpha, fog, and general combiner translation beyond
   the fixed texture-times-shade mode used by this harness.
4. Implement the libultra services and scheduler needed to boot the real game
   loop. The PSP `ultra_reimpl.c`, `sys_main.c`, and scheduler identify
   requirements, but their PSP-specific implementations are not direct PS3
   substitutes.
5. After the textured, correctly lit Arwing, add the starfield, team, title graphics,
   effects, and title transitions.

The synthetic triangle remains a backend diagnostic. Original Arwing geometry
is emulator-verified. The current build adds the original RGBA16 materials,
with visual verification pending; the next checkpoint is a textured Arwing
with the title camera and lighting.
