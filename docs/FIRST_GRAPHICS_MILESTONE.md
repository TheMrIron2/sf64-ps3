# First Graphics Milestone

## Objective

Establish a native RSX framebuffer and a portable F3DEX1-to-RSX path, then feed
original title-scene data through `n64ps3`. The standard smoke build still uses
a synthetic triangle. A separate title-scene build extracts the local US Rev 1
ROM's Arwing bank at build time, calls the original SF64 seeded starfield
generator and background traversal from the local SF64 source checkout, adapts
their callbacks into FILLRECT commands, and calls the original
skeleton-generated Arwing display lists with their RGBA16 textures.

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
4. `Game_Update()` performs game-state/scene setup and, while
   `gGameState == GSTATE_TITLE`, calls the menu overlay's
   `OVLCALL_TITLE_UPDATE` entry. The harness now preserves that dispatch
   boundary: `sf64_original_title_logic_update()` calls the extracted original
   `OvlMenu_CallFunction()`, which calls `Title_Main()`. The harness still
   bypasses the outer `Game_Update()` setup and scheduler. When
   `gDrawMode == DRAW_TITLE`, the original `Game_Draw()` draws the starfield
   and calls `OVLCALL_TITLE_DRAW`; the title-state gate now calls the original
   `Title_StarfoxLogo_Draw()`. Its 30 original RGBA16 strip requests are
   captured and appended as textured F3DEX1 quads after the Arwing. The full
   original `Title_Draw()` body calls `Title_Screen_Draw()` for the team and
   Arwing, followed by logos and title text. The draw-side sequence is `Game_Draw()` →
   `OvlMenu_CallFunction(OVLCALL_TITLE_DRAW)` → `Title_Draw()` →
   `Title_Screen_Draw()` → `Title_Team_Draw()` for FOX/FALCO/SLIPPY/PEPPY and
   `Title_Arwing_Draw(0)`, then the Star Fox logo, copyright, PRESS START, and
   64 logo routines. The harness currently executes only the original Star Fox
   logo subcall; it still does not execute the rest of that sequence.
   `Title_Arwing_Draw(0)` sets the one-light state and `SETUPDL_23`, applies
   the `sTitleArwing[FOX]` transform, and calls `Display_Arwing_Skel()`. The
   game-state branch selects `aAwWingsClosedAnim` for `GSTATE_TITLE` (the
   half-open animation is specific to `GSTATE_MENU`) and traverses the Arwing
   skeleton. The current task builder now uses that title animation offset.
   Its body and wing display lists use RGBA16 textures, so geometry
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
and rotates it `(35, -90, -15)` degrees. `Title_Screen_Setup` sets the camera
target to `(0,180,0)` and distance to `430`; the first `Title_Screen_Update`
places the eye at `(0,180,430)`. The camera uses a 45-degree perspective, 4:3
aspect, and 10/12800 near/far planes. `Title_Screen_Draw()` calls
`Title_Arwing_Draw(0)`, which installs the title light and `SETUPDL_23`, applies
that transform, then calls `Display_Arwing_Skel()`. The title branch obtains
frame zero of `aAwWingsClosedAnim` and calls `Animation_DrawSkeleton()` with
`aAwArwingSkel`; the skeleton traversal emits the body and wing display lists.
The game-specific harness now follows this asset path: it parses the 18-limb
skeleton and animation data, builds the original camera, light, and Arwing
transforms, then emits a task that calls the original body and wing lists. The
initial Arwing light values match `Title_Screen_Setup`: directional RGB
30/29/40, ambient RGB 6/1/11, and light rotations −70°/+22°. The first-frame
starfield is generated from the game's seeded random sequence and 16-entry
RGBA5551 palette. The original title uses 1000 source stars and sets
`gStarCount` to 800. Its first title update computes the `(0,180,430)` camera
and calls `Camera_SetStarfieldPos`, which sets initial field offsets to
`(120,120)` with zero scroll; 456 stars land in the 320x240 viewport. A narrow
build-time source extractor keeps the original starfield, camera-offset,
seeded-random, and allocator functions needed for this path; generated C is
ignored under `build/`. Each frame now calls the original camera-offset and
background traversal functions while preserving their star state and applying
the title's `gStarfieldScrollX -= 0.5` update. The Arwing task also applies the
original `xRot += 0.6` title update. The title callback adapter writes original
palette colors and pixel positions as big-endian `SETFILLCOLOR` and `FILLRECT`
commands. The portable decoder emits these as screen-space triangles. The
PS3 title build initializes title state with the title skip flag set, then
enters original `Title_Main()` through
`OvlMenu_CallFunction(OVLCALL_TITLE_UPDATE)` so it selects the main title
instead of the opening cutscene. A quiet virtual controller port keeps the
original controller checks and input update safe; audio commands are inert.
Per-frame overlay dispatch reaches original `Title_Main()`,
`Title_UpdateEntry()`, `Title_Screen_Update()`,
`Title_Screen_Setup()`, and title input logic. The runtime logs the original
title and cutscene states at boot. The title segment (`ast_title`, 313,904
bytes) is extracted from the same local ROM at build time to satisfy original
segment-6 animation references. The Arwing builder now uses the closed-wing
animation that `Display_Arwing_Skel()` selects in `GSTATE_TITLE`. Rendering
uses the custom Arwing/starfield task builder, with the original
`Title_StarfoxLogo_Draw()` subcall supplying its 30 RGBA16 strips. The harness
does not execute the full `Title_Draw()` or `Title_Screen_Draw()` bodies; the
team, copyright, and PRESS START are still missing. The movable `64` is a
separate 3D title object, not part of these logo strips.

The first logo adapter drew a white rectangle because the RSX backend rejected
236-pixel-wide title textures at its 64-pixel dimension limit and substituted
the white fallback. The backend now accepts dimensions through 256 pixels.
The adapter groups three adjacent original 2-row strips into each six-row
draw region. To preserve bilinear continuity, each interior texture tile also
loads the neighboring row above and below, making an eight-row (3,776-byte)
upload; the first and last tiles need one guard row. The texture table now has
64 entries. The user confirmed the logo texture appears and the 16:9
correction works. The ROM's 236x60 RGBA16 logo pixels do not contain the
remaining seams.
The movable Nintendo 64 title logo now follows the original update position
and seven-piece `aTitle64LogoDL` layout. Its seven CI8 images and per-piece
RGBA16 palettes are converted into task-local RGBA16 textures, so it can use
the existing backend texture path without adding CI8/TLUT support to `n64ps3`.
The original model vertices, texture coordinates, 1.5x/3.0x scale, 90-degree
rotation, and title-camera placement are used. The title task polls the PS3
controller each frame and feeds the analog axes into the original title input
routine, which moves the logo's target. The first RPCS3 image showed a white,
disfigured object. Diagnosis found the generated MOVEWORD command used index
`0x07` instead of `G_MW_SEGMENT` (`0x06`), so segment 7 was never installed and
the texture addresses could not resolve. It also used identical triangle
indices for every panel, unlike the original display list's two winding/order
patterns. Both issues were corrected. A subsequent visual review showed the
64 at the expected height but horizontally stretched and too far from the
Star Fox logo on widescreen output. The PSP renderer keeps the 320x240 game
image centered in its 480x272 framebuffer; the PS3 64 projection now applies
the equivalent centered 4:3 fit, matching the title-strip layout while
preserving the original camera and object transform. The latest SELF still
needs RPCS3 confirmation of its final placement, silhouette, and input
response.
The PSP reference shows `Title_StarfoxLogo_Draw()` installs `SETUPDL_83`, whose
`G_RM_XLU_SURF` mode blends using texture alpha. The adapter had replaced that
setup, while the PS3 backend disabled blending; passing `FORCE_BL` through the
F3DEX1 decoder and enabling source-alpha blending did not remove the visible
seams. Clamp-to-edge removed most of the visible seams, according to the user,
but small seams remain. The next build adds adjacent source rows as guard texels
around each interior tile so bilinear samples at a tile boundary see the same
neighboring source pixels as a continuous image. The UI projection fits the
game's 4:3 coordinate space into the queried output resolution, and logo UVs
span the full texel extent at quad edges.

The host probe and PS3 harness preserve segment 3 references. The host probe
decodes the original lists and confirms their geometry; the PS3 SELF embeds
only the generated bank needed by this harness. Keep the ROM and generated bank
out of source control.

The title update path now has the game state, save data, controller state,
starfield, and asset segment references it needs. The harness enters
`OVLCALL_TITLE_DRAW`, checks the original `gDrawMode == DRAW_TITLE` and
`sCutsceneState == TITLE_SCREEN` gates, and executes the original
`Title_StarfoxLogo_Draw()` call through a narrow capture adapter. The next
game-call milestone is to run the rest of the original `Title_Draw()` body and
`Title_Screen_Draw()` team path. That needs a `gMasterDisp` command-buffer
bridge, original matrix/light setup and animation traversal, plus IA8 and
CI8/TLUT texture rectangles for the remaining title UI. Normal boot also
initializes audio and game threads; those services are not part of this harness.
The title uses copyrighted game asset data, which is not copied into this
repository.

## Adopted Framework and RSX Architecture

`n64ps3` owns the renderer backend seam. Host builds continue to select
`rdpnull`; the PS3 Makefile fragment selects `rdprsx`. The PS3 backend currently:

- queries and preserves the active video resolution and aspect;
- initializes an RSX context with a 1 MiB aligned host buffer;
- allocates and registers two XRGB display buffers plus a 24-bit depth / 8-bit
  stencil buffer;
- clears each render target to black, matching the initial title background;
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
texture coordinates, dimensions, tile origin, masks, shifts, and wrap modes per
triangle. It also handles
RGBA5551 `SETFILLCOLOR` and `FILLRECT` by emitting screen-space triangles; this
fixed 320x240 viewport is sufficient for the title starfield. `LOADSYNC`,
`PIPESYNC`, and `TILESYNC` are accepted as no-ops; other fixed-width commands
remain unsupported.

The RSX backend converts the Arwing's N64 RGBA16 texture blocks to linear
ARGB8888, samples them with the original vertex texture coordinates, and
multiplies them by decoded vertex lighting, matching the title setup's
texture-times-shade combine mode, then applies the PSP reference's
sqrt-based RGB transfer to approximate N64 VI gamma. Gamma dithering remains
unimplemented. It applies tile wrap/clamp modes and groups triangles by texture
and geometry state. `G_ZBUFFER` controls RSX depth and cull bits select RSX
face culling. The depth surface uses RSX `Z24S8` and clears to its 24-bit far
value to reduce depth quantization where nearby Arwing panels overlap. The user
confirmed that the preceding-flip wait makes the static Arwing textures stable
and the scene updates smoothly at 60 Hz. General combiner, RDP render-mode,
alpha, fog, and framebuffer state remain unsupported except for the
`SETUPDL_83` translucent mode now used by the Star Fox logo. The first textured
SELF crashed in RPCS3 because the RSX descriptor advertised zero mip levels. The initializer
now advertises its single base level; the rebuilt SELF runs and displays the
textured Arwing in RPCS3. The title starfield colors come from the original
5551 palette, and the clear is black.

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
- Added portable RDP `SETFILLCOLOR`/`FILLRECT` decoding for the title starfield;
  fill rectangles become callback triangles over a 320x240 logical viewport.
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
- Added a game-specific title task builder that reads the original skeleton and
  frame-zero wing animation, writes the original camera/projection and light,
  initializes and advances the original SF64 starfield path per frame, adapts
  its callbacks into an N64 display list, applies the original Arwing rotation
  update, and calls the original body and wing display lists.
- Added a build-time source extractor for the narrow original starfield path. It
  reads a local SF64 source checkout and writes generated code only under
  ignored `build/`; `SF64_SOURCE_DIR` can select another checkout.
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
- `make -B arwing-probe`, build-time `ast_arwing` extraction, and
  `build/host/sf64_ps3_arwing_probe --segment build/arwing/ast_arwing.bin`:
  passed. The extracted original title starfield path generated 456 visible
  stars as a 1,371-command list; a second update generated 457 stars and 1,374
  commands. Both complete frame tasks decoded with zero skipped commands; frame
  two had 1,730 commands, 1,088 triangles, 457 fills, and 174 textured Arwing
  triangles. The first textured Arwing vertex also moved, confirming the 0.6°
  rotation step. The harness found 18 skeleton limbs and 9 original Arwing lists.
  Frame one had 1,727 commands, 32 vertex loads,
  1,086 total triangles (912 starfield + 174 Arwing), 27 RGBA16 texture loads,
  456 fill colors/rectangles, and zero skipped commands. It found 14 unique
  texture sources (32x32 and 64x32).
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
- High-level Cg shader compilation requires PSL1GHT `cgcomp` and NVIDIA Cg
  3.1's `libCg.so`. The production fragment program now uses assembly directly.
  The runtime is now extracted under ignored
  `build/toolchain/nvidia-cg-3.1/` from the official
  [NVIDIA Cg Toolkit package](https://developer.nvidia.com/cg-toolkit-download)
  without a system-wide install. Earlier builds used a temporary `/tmp` copy.
- The first sqrt-transfer Cg build compiled and linked on 2026-10-30, but
  the user's image showed magenta output. Inspection of actual microcode found
  that `cgcomp` mapped Cg temporary `R0` onto RSX's COLOR output register.
  Interleaved temporary/output writes corrupted red and blue. Reassembling
  that same program also preserved the bug; binary equality alone was not a
  color correctness check.
- The replacement `rdprsx_triangle.fpa` reserves `R0` for output and uses `R1`
  for intermediate values. It assembles without Cg and passes the new
  `tools/check_rsx_gamma.py` probe on 582 color/alpha cases. That probe rejects
  the previous shader with the same magenta channel clipping as the screenshot.
  The build checks the temporary binary before replacing the output and saves
  current assembly beside it as `rdprsx_triangle.fpo.dump`. The updated SELF
  builds with `make ps3-arwing-self PSL1GHT=/usr/local/ps3dev/PSL1GHT` using the
  cached vertex program; the corrected image still needs visual confirmation.
  See [RSX shader toolchain findings](RSX_SHADER_TOOLCHAIN.md).
- The strict host target uses C99. The PSL1GHT target uses GNU C99 because its
  system headers use the `asm` extension; project source remains C99-compatible.
- `PS3DEV=/usr/local/ps3dev PSL1GHT=/usr/local/ps3dev/PSL1GHT CGCOMP_RUNTIME_DIR=/tmp/nvidia-cg-3.1/usr/lib/x86_64-linux-gnu make -B ps3-arwing-self`:
  passed after the opaque XRGB clear change. The build compiled the Arwing PPU
  branch and generated original title-starfield functions, extracted the asset
  bank under ignored `build/arwing/ps3/resources/`, and produced
  `build/arwing/ps3/sf64_ps3_arwing.self` with the textured RSX shader.
- `PS3DEV=/usr/local/ps3dev PSL1GHT=/usr/local/ps3dev/PSL1GHT make ps3-arwing-self`:
  passed after wiring the original title update. It reused cached shader
  binaries without Cg and rebuilt `build/arwing/ps3/sf64_ps3_arwing.self`.
- `PS3DEV=/usr/local/ps3dev PSL1GHT=/usr/local/ps3dev/PSL1GHT make ps3-arwing-static-self`:
  passed without Cg and produced
  `build/arwing-static/ps3/sf64_ps3_arwing_static.self`. This diagnostic holds
  only the Arwing's X rotation at the original 35-degree setup angle while the
  title update and starfield continue. The PS3 Makefile shares cached shader
  binaries across the normal and diagnostic builds.
- The first static-pose SELF crashed in original `Animation_GetFrameData` during
  `Title_Screen_Setup`. `Sf64N64Animation32` used the PSP header's `u32` alias,
  which is 64-bit `unsigned long` in this PPU build, so the segment pointers
  were read at offsets 8/16 instead of 4/8. The extractor now declares those
  fields as `uint32_t`. Both the static and regular Arwing SELF were rebuilt;
  PPU disassembly confirms the corrected 32-bit field loads. An RPCS3 retest is
  successful, but the user reports that the surface glitch persists with the
  Arwing's X rotation held at 35 degrees. Rotation alone is not the cause.
- The user ran the flip-wait-enabled static-pose SELF and confirmed stable
  Arwing textures and smooth 60 Hz updates. Its earlier RPCS3 log records
  identical textured-triangle position, shade, UV, binding, source-texture, and
  converted-texture hashes over 34 samples from frame 0 through frame 3480.
- The RSX backend now waits for the preceding flip before clearing or rewriting
  shared vertex/index buffers. This fixed the reported surface flicker.
- Both `make ps3-arwing-self` and `make ps3-arwing-static-self` pass after the
  original `Title_Main()` setup/update path was added. The current SELF files
  are `build/arwing/ps3/sf64_ps3_arwing.self` and
  `build/arwing-static/ps3/sf64_ps3_arwing_static.self`. The earlier static
  build has visual confirmation; the current builds with original title-state
  integration still need an RPCS3 run.

## Visual and Hardware Status

The user confirms that the flip-wait-enabled static Arwing has stable textures
and smooth 60 Hz updates. The earlier capture shows a dark model. The PS3
shader now applies the PSP reference's sqrt-based RGB transfer; gamma dithering
remains missing and the corrected image awaits emulator confirmation. The
latest title build initializes the original title state and
dispatches updates through `OvlMenu_CallFunction(OVLCALL_TITLE_UPDATE)` into
`Title_Main()`/`Title_Setup()`, and routes drawing through
`OVLCALL_TITLE_DRAW` with the original title-state gates. The task contains
the original-source starfield, rotating Arwing, and the Star Fox logo asset
drawn through the original `Title_StarfoxLogo_Draw()` subcall. It still skips
original `Game_Draw()` setup and the rest of `Title_Draw()` /
`Title_Screen_Draw()`, so the team and PRESS START are not present. The
player-movable 3D `64` object is also outside this logo-strip adapter. The
updated rotating SELF adds the original logo's translucent render mode,
clamp-to-edge tile addressing, and adjacent-row filter guards. Alpha blending
alone did not remove the seams; clamp-to-edge improved them but left small
seams. The guard-row build needs an RPCS3 visual check. No PS3 hardware result
has been reported.

## Hardware Evaluation

Build from the repository root:

```sh
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=/usr/local/ps3dev/PSL1GHT
# Set CGCOMP_RUNTIME_DIR only when shader binaries need regeneration.
make ps3-self
make ps3-fake-self # optional fself debug-format output
```

The PPU compiler and link libraries are taken from `PS3DEV`; PSL1GHT headers
are taken from `PSL1GHT`.

For the original title-scene and RGBA16 material harness, place a legally
obtained US Rev 1 `.z64` ROM at the repository root as
`baserom.us.rev1.z64`, and a local SF64 source checkout (by default
`../sf64-psp`), then run `make ps3-arwing-self`. Set `SF64_SOURCE_DIR` if the
checkout is elsewhere. This target extracts the Arwing bank and generates the
narrow original starfield source into ignored `build/arwing/` output and creates
`build/arwing/ps3/sf64_ps3_arwing.self`. It does not execute the original game
loop or render the rest of the title screen.

The static-pose diagnostic is built with `make ps3-arwing-static-self` at
`build/arwing-static/ps3/sf64_ps3_arwing_static.self`. It keeps the original
title update and starfield, but holds the Arwing's X rotation at 35 degrees.
The user's capture showed flickering in the prior instrumented build. The new
SELF additionally waits for the preceding flip before reusing geometry buffers
and logs the wait count with position, shade, UV, texture-binding, source, and
converted-texture hashes. Reload this updated SELF in RPCS3 and inspect whether
the Arwing now remains visually stable; its hashes should remain identical.

Load `build/ps3/sf64_ps3_smoke.self` with the local `ps3load` workflow or another
PS3 homebrew loader. The expected smoke image is a black clear and one amber
triangle, with the `rdprsx ready: WIDTHxHEIGHT` diagnostic. To test the
title-scene build, load `build/arwing/ps3/sf64_ps3_arwing.self`; the supplied
screenshot shows the textured Arwing and starfield over the black clear.
The earlier image showed dark materials. A prior gamma approximation was
reverted after it made the Arwing magenta; the current shader uses the PSP
reference's sqrt-based RGB transfer, pending visual confirmation. The
source-generated starfield SELF runs in RPCS3 according to the user; it has not
been tested on PS3 hardware.
Both are regular SELF files, not installable PKGs.

For RPCS3 from Windows, the standard smoke SELF is at
`C:\Users\dwelc\Documents\GitHub\sf64-ps3\build\ps3\sf64_ps3_smoke.self`;
the original Arwing geometry SELF is at
`C:\Users\dwelc\Documents\GitHub\sf64-ps3\build\arwing\ps3\sf64_ps3_arwing.self`.
The `.fake.self` output is the optional PSL1GHT `fself` format.

## Remaining Obstacles

1. Compare title camera/light output and material colors against a reference;
   validate the sqrt-based gamma transfer and implement N64 VI dithering, then
   address UV orientation, wrapping, and alpha.
2. Expand RDP render-mode, alpha, fog, and general combiner translation beyond
   the fixed texture-times-shade mode used by this harness.
3. Implement the libultra services and scheduler needed to boot the real game
   loop. The PSP `ultra_reimpl.c`, `sys_main.c`, and scheduler identify
   requirements, but their PSP-specific implementations are not direct PS3
   substitutes.
4. Replace the remaining task-builder title setup with original title setup and
   draw state, then add title effects and transitions through game-generated
   display lists. The four animated team skeletons and pulsing PRESS START
   texture are submitted from original title state.

The synthetic triangle remains a backend diagnostic. Original Arwing geometry,
RGBA16 texture sampling, the original-source starfield, the 64 mark and the
animated team with original title lighting have been visually confirmed by the
user in RPCS3. The title currently remains alive past the point where the
previous build crashed around PRESS START. The source asset is IA8 (4-bit
intensity in the high nibble and 4-bit alpha in the low nibble), as confirmed
by the PSP implementation. RPCS3 logs showed 166 texture loads in the title
task and RSX texture-table exhaustion at 64 entries; the renderer then used its
opaque white fallback. The cache now has 256 entries and PRESS START draws.
The latest build preserves IA8 coverage as RGBA32 and applies the PSP
soft-coverage transfer. A new comparison build uses point filtering for only
the PRESS START texture while retaining bilinear filtering for the logo strips
and 64 mark; its hardware appearance is awaiting confirmation. Correct
material output and the gamma transfer still need visual confirmation; gamma
dithering, copyright, input and normal title loop remain outstanding.
