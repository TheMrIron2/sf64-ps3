# n64ps3 Gap Report

This report tracks what the `sf64-ps3` smoke skeleton can prove against
`extern/n64ps3`.

## Build Results

- `make host`: failed before compilation because `extern/n64ps3` is missing.
  Diagnostic:
  `setup: git submodule add https://github.com/TheMrIron2/n64ps3 extern/n64ps3`
- `make ps3`: attempted because `PS3DEV`, `PSL1GHT`, and `ppu-gcc` were visible
  locally. It failed before compilation because `extern/n64ps3` is missing.
  Diagnostic:
  `setup: git submodule add https://github.com/TheMrIron2/n64ps3 extern/n64ps3`

Update this section after each smoke attempt. Do not record invented success.

Developer note: the local C scaffold was compile-checked with a temporary fake
`N64PS3_DIR` under `/tmp` to catch syntax/build errors. That is not a real
`n64ps3` smoke result. The PS3 compile check reached `ppu-gcc`; this local
toolchain hit an internal compiler error with `-Os`, so the initial PS3 smoke
default is `-O0`.

## APIs Successfully Used

None confirmed yet. `extern/n64ps3` must be added before the smoke target can
compile and run against real code.

Expected first confirmations:

- `osCreateMesgQueue`
- `osViSetEvent`
- `osSendMesg` or an explicit VI retrace pump
- `osRecvMesg`
- controller read path, if exposed
- dummy SP/DP task dispatch, if exposed

## Missing APIs

Expected or likely gaps until proven otherwise:

- TODO(n64ps3): documented runtime init entry point, such as `n64ps3_init`.
- TODO(n64ps3): documented runtime shutdown entry point.
- TODO(n64ps3): message queue API surface compatible with SF64/libultra usage.
- TODO(n64ps3): VI event/retrace delivery API that can be driven in host smoke.
- TODO(n64ps3): controller structs and polling entry points.
- TODO(n64ps3): PI DMA or host asset-read abstraction for ROM-free smoke data.
- TODO(n64ps3): dummy SP/DP graphics task submission.
- TODO(n64ps3): audio task completion stubs.

## Present But Semantically Incomplete

Not confirmed yet. The smoke program marks simulated VI delivery and dummy graphics
submission as incomplete even when a call path exists, because those paths do not
prove real VI timing or RSX/display-list execution.

## Recommended n64ps3 Commits

1. Message queue correctness, including `OS_MESG_BLOCK` and `osJamMesg`.
2. VI event/retrace behaviour.
3. Controller structs and polling.
4. PI DMA / asset-read abstraction.
5. SP/DP dummy task dispatch.
6. First GBI/display-list inventory.
7. Texture format requirements from SF64 PSP.
8. Audio task completion stubs.
