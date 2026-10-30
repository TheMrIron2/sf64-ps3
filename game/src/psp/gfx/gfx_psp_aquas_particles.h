#ifndef PSP_GFX_AQUAS_PARTICLES_H
#define PSP_GFX_AQUAS_PARTICLES_H

#include "src/psp/aquas_particles.h"
#include "src/psp/gfx/gfx_psp_backend.h"

int PspAquasParticles_ValidMatrix(u32 address);
int PspAquasParticles_Draw(const Gfx* records, u32 count, const float* projection,
                           u32 projectionSerial, PspGfxViewportPolicy viewport);
void PspAquasParticles_Restore(Gfx* output, const Gfx* record, int draw);

#endif
