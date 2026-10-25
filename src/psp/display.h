#ifndef PSP_DISPLAY_H
#define PSP_DISPLAY_H

typedef struct n64psp_display_config n64psp_display_config;

int PspDisplay_Init(void);
const n64psp_display_config* PspDisplay_GetConfig(void);
float PspDisplay_GetProjectionAspect(void);
int PspDisplay_IsWidescreen(void);
float PspDisplay_GetUiScaleY(void);
float PspDisplay_UiFromLeft(float x);
int PspDisplay_IsUiScalingEnabled(void);
int PspDisplay_SetMode(int mode);

#endif
