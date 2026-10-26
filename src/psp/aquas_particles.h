#ifndef PSP_AQUAS_PARTICLES_H
#define PSP_AQUAS_PARTICLES_H

#include "PR/ultratypes.h"
#include "PR/gbi.h"

#ifndef PSP_FAST_AQUAS
#define PSP_FAST_AQUAS 0
#endif

#define PSP_AQUAS_PARTICLE_MARKER 0x50535214u
#define PSP_AQUAS_PARTICLE_BATCH 32u

int PspSettings_UseFastGraphics(void);
int PspAquasParticles_TryEmit(float scale, int index, int alpha);
#endif
