#ifndef PSP_SETTINGS_H
#define PSP_SETTINGS_H

#include <n64psp/display.h>
#include "src/psp/gfx/gfx_psp_backend.h"

void PspSettings_Init(const char* executablePath, PspGfxBackendKind buildDefault);
PspGfxBackendKind PspSettings_GetGraphicsBackend(void);
int PspSettings_SetGraphicsBackend(PspGfxBackendKind backend);
int PspSettings_GraphicsBackendSaveFailed(void);
void PspSettings_SetActiveGraphicsBackend(PspGfxBackendKind backend);
PspGfxBackendKind PspSettings_GetActiveGraphicsBackend(void);
void PspSettings_SetDisplayMode(n64psp_display_mode mode);
void PspSettings_SetPresentationHz(int hz);
int PspSettings_NeedsBoundary(void);
void PspSettings_Apply(void);
int PspSettings_PerformanceVisible(void);
void PspSettings_TogglePerformance(void);

#endif
