# sf64-ps3

`sf64-ps3` is an experimental PlayStation 3-facing Star Fox 64 port skeleton.
Its immediate job is to test and drive development of
[`TheMrIron2/n64ps3`](https://github.com/TheMrIron2/n64ps3), not to become a
playable port in one step.

No copyrighted assets, ROM data, generated assets, or decompilation payloads
are tracked. The standard host/PS3 smoke target checks platform services with a
synthetic SF64-shaped F3DEX1 triangle. A separate Arwing probe uses a local US
Rev 1 ROM to build an original skeleton/display-list task and send its textured
geometry through `n64ps3`; it does not boot the game's title loop. The user
confirmed the textured Arwing renders in RPCS3 at about 60 FPS. A zero mip-level
count in the first textured build caused an RSX crash and is now fixed. The
title camera, remaining RDP state, other title elements, and game-loop
integration remain.

## Setup

Add `n64ps3` under `extern/`:

```sh
git submodule add https://github.com/TheMrIron2/n64ps3 extern/n64ps3
```

For a fresh checkout after the submodule has been added:

```sh
git submodule update --init extern/n64ps3
```

Host and PS3 builds fail clearly when `extern/n64ps3` is absent.

## Host Smoke

```sh
make host
./build/host/sf64_ps3_smoke
```

The host smoke executable is built with `cc` by default, uses C99, includes
`include/`, and adds `extern/n64ps3/include` when available.

## PS3 Smoke

PS3 builds are Makefile-based. CMake is not required or assumed for PSL1GHT.

```sh
export PS3DEV=/path/to/ps3dev
export PSL1GHT=/path/to/PSL1GHT
make ps3
make ps3-self
```

PS3 shader builds require the PSL1GHT `cgcomp` tool and NVIDIA Cg 3.1's
`libCg.so`. If the runtime is not installed in a system library directory, set
`CGCOMP_RUNTIME_DIR` to the directory containing it. The Makefile checks this
before building.

The PS3 target is a smoke ELF with native framebuffer setup and presentation.
`make ps3-self` uses PSL1GHT's `ppu-strip`, `sprxlinker`, and `make_self` steps
to create the regular SELF booted by RPCS3 in the current milestone. Use
`make ps3-fake-self` for the separate `fself` debug format. Neither target
creates an installable PKG. The build checks the PSL1GHT environment and fails
with a diagnostic when the toolchain is missing. See
[docs/FIRST_GRAPHICS_MILESTONE.md](docs/FIRST_GRAPHICS_MILESTONE.md) for current
visual capability, build results, and hardware testing instructions.

## Original Arwing Geometry Probe

The repository-root `baserom.us.rev1.z64` is a local build input and is ignored
by Git. Build the original Arwing geometry SELF with:

```sh
export CGCOMP_RUNTIME_DIR=/path/to/nvidia-cg-3.1/lib
make ps3-arwing-self
```

The target validates the US Rev 1 ROM and extracts only `ast_arwing` into
ignored `build/arwing/` output. It does not copy ROM or asset bytes into source
control. The host decoder probe requires the ROM after MIO0 decompression; see
[docs/FIRST_GRAPHICS_MILESTONE.md](docs/FIRST_GRAPHICS_MILESTONE.md) for the
measured decode results and current renderer limits.

## Current Scope

- Test `n64ps3` runtime surface with SF64-shaped calls.
- Document missing APIs and semantic gaps.
- Keep audio as a null/logging smoke probe; the PS3 RSX path clears and presents
  the synthetic smoke task or separate original Arwing geometry/material task.
  The decoder handles a small matrix subset and the Arwing RGBA16 load path,
  while the title camera and general RDP state remain incomplete.
- Mine `TheMrIron2/sf64-psp` for requirements, especially platform, libultra,
  renderer, texture, and source-manifest clues.
