# PSP Mining Notes

`TheMrIron2/sf64-psp` is a requirements source for this repository. It should
not be copied blindly into the PS3 skeleton.

## Mine For Requirements

- `src/psp/ultra_reimpl.c`: libultra calls SF64 actually needs, especially
  queues, controller reads, DMA-like asset access, timers, and task completion.
- `src/psp/platform.c` and `src/psp/platform.h`: lifecycle boundaries, input
  hooks, filesystem expectations, and platform-owned state.
- `src/psp/renderer.c` and `src/psp/renderer_texture.c`: first inventory of
  display-list, texture, palette, and format requirements.
- `src/psp/sources.mk`: explicit source manifest discipline. The PS3 skeleton
  should keep early source lists small and deliberate.
- `Makefile`: native platform build style and target organization.

## Useful As PS3 Requirements

- Which libultra calls must behave correctly before real SF64 code can boot.
- Which renderer operations can initially be logged or inventoried.
- Which texture formats, dimensions, and upload patterns appear early.
- Which platform seams are game-facing and should be implemented in `n64ps3`
  rather than locally in `sf64-ps3`.

## PSP-Specific Pieces Not To Copy

- PSP GU, PSPGL, Media Engine, memory layout, and controller glue.
- PSP-specific threading, callbacks, and packaging assumptions.
- Any generated assets, ROM-derived data, or decompilation payloads.
- Large source manifests that pull in real game code before the runtime surface
  is ready.

## How It Informs n64ps3

The PSP port shows the minimum runtime behaviours SF64 expects from a non-N64
platform. `sf64-ps3` should turn those observations into narrow smoke tests and
then upstream reusable runtime work to `n64ps3`: queues, VI, controller input,
PI/DMA-style reads, SP/DP task boundaries, display-list inventory, texture format
support, and audio completion signalling.
