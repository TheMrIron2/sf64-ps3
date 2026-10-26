#ifndef PSP_AQUAS_PARTICLES_H
#define PSP_AQUAS_PARTICLES_H

#include "PR/ultratypes.h"
#include "PR/gbi.h"

#define PSP_AQUAS_PARTICLE_MARKER 0x50535214u
#define PSP_AQUAS_PARTICLE_BATCH 32u

int PspAquasParticles_Available(void);
int PspAquasParticles_TryEmit(float scale, int index, int alpha);
#endif
