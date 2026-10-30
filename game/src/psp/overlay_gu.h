#ifndef PSP_OVERLAY_GU_H
#define PSP_OVERLAY_GU_H

#include "PR/ultratypes.h"

int PspOverlayGu_Begin(const u8 font[64][5]);
void PspOverlayGu_Rect(int x, int y, int width, int height, u32 color);
void PspOverlayGu_MenuFont(const u8* pixels);
void PspOverlayGu_Glyph(int x, int y, int u, int v, int width, int height, u32 color, int menu);
void PspOverlayGu_End(void);

#endif
