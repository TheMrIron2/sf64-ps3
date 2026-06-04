# sf64-ps3

`sf64-ps3` is an experimental PlayStation 3-facing Star Fox 64 port skeleton.
Its immediate job is to test and drive development of
[`TheMrIron2/n64ps3`](https://github.com/TheMrIron2/n64ps3), not to become a
playable port in one step.

No copyrighted assets, ROM data, generated assets, or decompilation payloads are
included. The first smoke test is intentionally SF64-shaped but tiny: it probes
message queues, VI event delivery, controller polling, and dummy graphics task
submission where `n64ps3` exposes those APIs.

## Setup

Add `n64ps3` under `extern/`:

```sh
git submodule add https://github.com/TheMrIron2/n64ps3 extern/n64ps3
```

This repository keeps `extern/n64ps3` out of tree until you add it. Host and PS3
builds fail clearly when it is absent.

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
```

The initial PS3 target is the smallest practical smoke ELF. The build checks the
PSL1GHT environment and fails with a diagnostic instead of faking success.

## Current Scope

- Test `n64ps3` runtime surface with SF64-shaped calls.
- Document missing APIs and semantic gaps.
- Keep renderer and audio paths as null/logging smoke probes.
- Mine `TheMrIron2/sf64-psp` for requirements, especially platform, libultra,
  renderer, texture, and source-manifest clues.
