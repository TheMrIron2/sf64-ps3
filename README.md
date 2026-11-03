# sf64-ps3

`sf64-ps3` is an experimental PlayStation 3 port of Star Fox 64.

## Title-screen build

For the boot-logo diagnostic, use the setup below and run:

```sh
make ps3-boot-self CGCOMP_RUNTIME_DIR=/path/to/nvidia-cg/lib64
```

It follows the initial boot and logo-wait states, then runs the opening Star Fox,
“in,” and Nintendo 64 logos, the Great Fox camera pass, the team running
through the passage, the Great Fox close-up, the Arwing takeoff (deck sweep,
ignition flash, and boost), and the space fly-by, then enters the original title
screen. Fog, RDP scissor/clip-ratio state, the Corneria explosion effect, the
Arwing engine glow, and the passage's cycling ambient and sweeping key light
are applied; surface textures default to bilinear filtering while text overlays
stay point-filtered. Radio dialogue, save handling, and audio are not yet implemented
in this diagnostic. The regular title-screen build remains below.

`make frame-raster` builds a host software rasterizer that decodes a boot/title
scene frame with the same `n64ps3` F3DEX1 path the PS3 uses and writes it to a
PNG, which is handy for checking visual regressions without hardware:

```sh
make frame-raster
build/host/sf64_frame_raster space 128
build/host/sf64_frame_raster team 140
build/host/sf64_frame_raster takeoff 260
```

Initialize the runtime and asset tool submodules and build Torch:

```sh
git submodule update --init extern/n64ps3 game/tools/Torch
make title-tools
```

Torch's CMake build needs a C++20 compiler, CMake, and network access to fetch
its dependencies. Put a big-endian US Rev 1.1 ROM at `baserom.us.rev1.z64`.
The build verifies the ROM and generates asset declarations under ignored
`build/` directories. Do not commit the ROM or generated assets.

With PS3DEV and PSL1GHT configured, build the title SELF with:

```sh
make ps3-arwing-self CGCOMP_RUNTIME_DIR=/path/to/nvidia-cg/lib64
```

`cgcomp` needs NVIDIA Cg 3.1 to compile the vertex shader. The resulting file is
`build/arwing/ps3/sf64_ps3_arwing.self`. It is a title-screen harness, not the
complete game.

After the initial title animation, the left stick moves the original 3D `64`
logo. Start and Cross are held back until the option menu is implemented, so
they currently leave the title screen in place.

For the host decoder probe:

```sh
make arwing-probe
build/host/sf64_ps3_arwing_probe build/generated/sf64/baserom.us.rev1.uncompressed.z64
```

The source defaults to `game/`; set `SF64_SOURCE_DIR` to use another compatible
checkout. Set `SF64_TORCH` to use a separately built Torch executable.

`make option-probe` validates the option-menu asset bank and its card display
list. See [option menu bring-up](docs/OPTION_MENU_BRINGUP.md) for current gaps.
