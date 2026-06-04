# AGENTS.md

This repository is an experimental Star Fox 64 to PlayStation 3 port skeleton.

Primary purpose:
- Test and drive development of `TheMrIron2/n64ps3`.
- Mine lessons from `TheMrIron2/sf64-psp`.
- Identify the smallest runtime surface required for a future PS3 port.

Do not attempt to make Star Fox 64 fully playable in one task.

Priorities:
1. Build a small host-side skeleton first.
2. Link against or vendor `n64ps3`.
3. Exercise n64ps3 APIs with SF64-shaped code:
   - message queues
   - VI retrace events
   - controller polling
   - PI/DMA or asset reads
   - graphics task submission
4. Produce clear documentation of missing n64ps3 APIs and PS3 priorities.
5. Keep renderer work as a null/logging path unless explicitly requested.

Relevant reference repositories:
- `TheMrIron2/n64ps3`
- `TheMrIron2/sf64-psp`
- `HarbourMasters/Starship`
- `Kenix3/libultraship`
- `N64Recomp/N64ModernRuntime`
- `rt64/rt64`

Rules:
- Prefer small, reviewable patches.
- Do not copy copyrighted assets or ROM data.
- Do not require a ROM for the first smoke tests.
- Do not create a full generic N64 runtime inside this repo; reusable code should be proposed for `n64ps3`.
- Preserve C99 compatibility unless a file is clearly marked otherwise.
- Test `n64ps3` first; this repository is currently a diagnostic consumer, not a full port.
- Document missing or incomplete `n64ps3` APIs in `docs/N64PS3_GAP_REPORT.md`.
- Use Makefiles for PS3/PSL1GHT builds. Do not make CMake a PS3 build requirement.
- Treat `sf64-psp` as a requirements source and design clue, not as code to blindly copy.
