#ifndef PSP_SETTINGS_H
#define PSP_SETTINGS_H

#include <n64psp/display.h>

void PspSettings_SetDisplayMode(n64psp_display_mode mode);
void PspSettings_SetPresentationHz(int hz);
int PspSettings_NeedsBoundary(void);
void PspSettings_Apply(void);
int PspSettings_PerformanceVisible(void);
void PspSettings_TogglePerformance(void);

#endif
