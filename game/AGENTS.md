# Star Fox 64 PSP

## Project objective & conditions

A full-speed, educational quality native PSP port of Star Fox 64.

Graphics interpretation should remain backend-neutral. Fix N64 graphics semantics in the renderer where possible.
Reusable PSP-optimised N64 functionality belongs in n64psp.

Ignore other regions' assets and source trees (EU, JP) and `tools/`, unless requested.
When writing comments in code, please keep them concise (1 line, 2 lines max) and without punctuation.

Be economical with documentation and minimise jargon.

When a suitable opportunity to commit appears, mention it, and I will do so. Don't commit anything yourself; allow me to review. Convert any CRLF to LF.
When a Makefile option has been proven good, promote it to default and bin its Makefile flag to avoid bloat.
include/mods.h is a local debug configuration file. Preserve its current working-tree contents.

## Docs

Refer to `docs/optimisation.md` and https://themriron2.github.io/psp-survival-guide/optimisation.html for optimising the code. It contains useful lessons and should be updated if new learnings are discovered.

## Validation

- Build the PSP target after changes. (You may use -j$(nproc) to compile faster)
- Report exact commands run.
- Provide a short PSP hardware test checklist where relevant.

## Useful references

- jnmartin84/sf64-dc - Dreamcast OpenGL-driven port.
- z2442/OOT-psp - Ocarina of Time ported to PSP via PSPGL.
- DaedalusX64/daedalus - Mature VFPU code and native interpretations of N64 code.

## Goals

- Full-speed with PSPGL and PSPGU
- 60FPS GU rendering backend
- Using the obstacles and lessons from this codebase to improve upstream projects, community and research (PSPGL, n64psp, VME)