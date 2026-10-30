#ifndef PSP_GFX_DEVICE_IMPL_H
#define PSP_GFX_DEVICE_IMPL_H

#if PSP_GFX_BACKEND_GU
int PspGfxDeviceGu_Init(void);
int PspGfxDeviceGu_IsReady(void);
int PspGfxDeviceGu_BeginFrame(void);
int PspGfxDeviceGu_Submit(void);
int PspGfxDeviceGu_Present(void);
void PspGfxDeviceGu_Shutdown(void);
void* PspGfxDeviceGu_GetOverlayFrameBuffer(int* stride, int* pixelFormat);
#endif

#if PSP_GFX_BACKEND_PSPGL
int PspGfxDevicePspgl_Init(void);
int PspGfxDevicePspgl_IsReady(void);
int PspGfxDevicePspgl_BeginFrame(void);
int PspGfxDevicePspgl_Submit(void);
int PspGfxDevicePspgl_Present(void);
void PspGfxDevicePspgl_Shutdown(void);
void* PspGfxDevicePspgl_GetOverlayFrameBuffer(int* stride, int* pixelFormat);
#endif

#endif
