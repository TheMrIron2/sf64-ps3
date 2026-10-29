# AGENTS.md

This repository is an experimental Star Fox 64 to PlayStation 3 port.

- Abstract generic N64 lessons to `TheMrIron2/n64ps3`, a N64 library for PS3. Low priority and can be done once behaviour is settled.
- Use `TheMrIron2/sf64-psp` and `harbourmasters/starship` as references whenever you need to adapt N64 behaviour.
- Become a 60FPS HD port of Star Fox 64 to PS3. (Matrix interpolation for 60FPS)

Write in native RSX GCM for graphics where possible; porting GL code initially via RSXGL is acceptable if it expedites the overall progress.

If there is a limitation of the tooling and toolchain that makes porting difficult, mention it explicitly - it will be flagged as an opportunity to improve the SDK and toolchain upstream.
