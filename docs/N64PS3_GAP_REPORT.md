# n64ps3 Gap Report

This report tracks what the `sf64-ps3` smoke skeleton can prove against
`extern/n64ps3`.

## Build Results

- `extern/n64ps3`: added as a git submodule from
  `https://github.com/TheMrIron2/n64ps3`.
- `make host`: passed. It built `build/host/sf64_ps3_smoke` through
  `extern/n64ps3/make/n64ps3_host.mk`.
- `./build/host/sf64_ps3_smoke`: passed and returned `0`. Required systems
  worked: runtime init/shutdown, message queue creation, VI event registration,
  host retrace raise, and VI message receive. Optional probes still report TODO
  warnings for controller semantics, ROM-free PI reads, and dummy graphics task
  semantics.
- `make ps3`: passed with `PS3DEV=/usr/local/ps3dev`,
  `PSL1GHT=/usr/local/ps3dev`, and `/usr/local/ps3dev/ppu/bin/ppu-gcc`. It
  built `build/ps3/sf64_ps3_smoke.elf` through
  `extern/n64ps3/make/n64ps3_ps3.mk`. SELF/PKG packaging is intentionally not
  faked.
- VI duplicate-delivery regression check: fixed in `extern/n64ps3` by making
  `n64ps3_vi_raise_retrace_for_host` deliver through `OS_EVENT_VI` only. The
  updated `n64ps3` `vi_retrace` sample proves one host retrace yields exactly
  one queue message.

## Latest Verification

- `n64ps3 make host`: not run because `extern/n64ps3` has no root Makefile or
  `host` target.
- `n64ps3 vi_retrace` sample: passed via existing CMake sample build in
  `/tmp/n64ps3-vi-test`; output included `vi_retrace=1`.
- `sf64-ps3 make host`: passed.
- `sf64-ps3 ./build/host/sf64_ps3_smoke`: passed with exit `0`; it now fails if
  one retrace leaves a duplicate VI message in the queue.
- `sf64-ps3 make ps3`: passed and built `build/ps3/sf64_ps3_smoke.elf`.

## Make Fragments Used

- `extern/n64ps3/make/n64ps3_sources.mk`: exports `N64PS3_ROOT`,
  `N64PS3_INCLUDE_DIRS`, `N64PS3_CPPFLAGS`, `N64PS3_COMMON_C_FILES`,
  `N64PS3_GFX_BACKEND_C_FILES`, and `N64PS3_C_FILES`.
- `extern/n64ps3/make/n64ps3_host.mk`: exports host include flags and C99 host
  defaults via `N64PS3_HOST_CPPFLAGS` and `N64PS3_HOST_CFLAGS`.
- `extern/n64ps3/make/n64ps3_ps3.mk`: exports PS3 include flags and `-O0` C99
  defaults via `N64PS3_PS3_CPPFLAGS` and `N64PS3_PS3_CFLAGS`.

Toolchain note: this local `ppu-gcc` previously hit an internal compiler error
with `-Os`, so the initial PS3 smoke default remains `-O0`.

## APIs Successfully Used

- `n64ps3_config_default`
- `n64ps3_runtime_init`
- `n64ps3_runtime_shutdown`
- `osCreateMesgQueue`
- `osViSetEvent`
- `n64ps3_vi_raise_retrace_for_host`
- `osRecvMesg`
- `n64ps3_pad_init`
- `n64ps3_pad_read`
- `n64ps3_gfx_begin_frame`
- `n64ps3_gfx_submit_task`
- `n64ps3_gfx_end_frame`
- `n64ps3_pi_read_rom`, present but optional smoke fails without a mounted ROM
- `n64ps3_rdpnull_renderer`, indirectly through runtime init

## Fixed Semantic Issues

- Duplicate VI retrace delivery: `osViSetEvent` registers the VI event binding
  with `osSetEventMesg`, and `n64ps3_vi_raise_retrace_for_host` now raises only
  `OS_EVENT_VI` after retrace-count gating. It no longer sends directly to the
  VI queue and then raises the OS event.

## Missing APIs

- TODO(n64ps3): `osJamMesg`.
- TODO(n64ps3): libultra-shaped controller APIs used by SF64-style code, such as
  `osContInit`, `osContStartReadData`, and `osContGetReadData`, or documented
  guidance to use the native `n64ps3_pad_*` APIs instead.
- TODO(n64ps3): PI DMA or host asset-read abstraction for ROM-free smoke data.
- TODO(n64ps3): audio task completion stubs.

## Present But Semantically Incomplete

- `osSendMesg` and `osRecvMesg` exist, but currently ignore `OS_MESG_BLOCK`.
  Empty blocking receives return immediately with failure instead of blocking.
- VI event delivery works in host smoke through `n64ps3_vi_raise_retrace_for_host`
  with exactly one message per retrace, but this is still a host helper rather
  than a real 50/60 Hz backend tick.
- `n64ps3_pad_read` exists and returns a zeroed controller state, but it does not
  yet prove PS3 controller plumbing or libultra-compatible controller status.
- `n64ps3_pi_read_rom` exists, but the current smoke has no ROM and needs a
  ROM-free asset-read path for first integration tests.
- `n64ps3_gfx_submit_task` reaches `rdpnull` and logs the dummy display list, but
  this is not real SP/DP execution or RSX rendering.
- Runtime init registers `rdpnull`, which is useful for smoke tests but not a
  PS3 renderer backend.

## Next Required APIs

1. Fix message queue semantics: implement `OS_MESG_BLOCK`, add `osJamMesg`, and
   add queue-full/queue-empty tests.
2. Promote VI retrace behaviour beyond host manual raise: backend-driven 50/60 Hz
   event delivery, retrace-count handling tests, and diagnostics for dropped
   events.
3. Decide and document controller API shape: either add libultra-shaped
   `osCont*` compatibility or make `n64ps3_pad_*` the supported frontend, then
   wire PS3 pad state.
4. Add PI DMA / asset-read abstraction and first SP/DP task boundary tests so
   SF64 can exercise ROM-free asset reads and display-list inventory.
5. Add dummy audio task completion stubs and completion-event tests.

Remaining priorities after the top five:

- First GBI/display-list inventory.
- Texture format requirements from SF64 PSP.
