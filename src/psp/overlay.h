#ifndef PSP_OVERLAY_H
#define PSP_OVERLAY_H

#include "PR/ultratypes.h"

int PspOverlay_Begin(void);
int PspOverlay_BeginMenuFrame(void);
void PspOverlay_Rect(int x, int y, int width, int height, u32 color);
void PspOverlay_Text(int x, int y, u32 color, const char* text);
void PspOverlay_End(void);
void PspOverlay_MenuBegin(void);
void PspOverlay_MenuEnd(void);
int PspOverlay_MenuTextWidth(const char* text);
void PspOverlay_MenuText(int x, int y, u32 color, const char* text);

#endif
