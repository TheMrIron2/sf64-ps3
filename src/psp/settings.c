#include "src/psp/settings.h"
#include "src/psp/display.h"
#include "src/psp/frame_scheduler.h"
#include "src/psp/frame_interpolation.h"

#include <pspiofilemgr.h>
#include <pspiofilemgr_fcntl.h>

#define PSP_SETTINGS_PATH_SIZE 256

static char sSettingsPath[PSP_SETTINGS_PATH_SIZE] = "sf64-settings.cfg";
static PspGfxBackendKind sGraphicsBackend = PSP_GFX_KIND_GU;
static PspGfxBackendKind sActiveGraphicsBackend = PSP_GFX_KIND_GU;
static int sBackendSaveFailed;

static int sDisplayMode = -1;
static int sPresentationHz;
#if PSP_FPS_OVERLAY
static int sPerformanceVisible = 1;
static int sTogglePerformance;
#endif

static int psp_settings_equal(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void psp_settings_copy_path(char* output, unsigned capacity, const char* input, unsigned length) {
    unsigned i;
    if (length + 1 > capacity) return;
    for (i = 0; i < length; i++) output[i] = input[i];
    output[length] = 0;
}

static void psp_settings_set_path(const char* executablePath) {
    unsigned length = 0, separator = 0, i;
    static const char filename[] = "sf64-settings.cfg";

    if (executablePath == NULL) return;
    while (executablePath[length] && length < PSP_SETTINGS_PATH_SIZE) {
        if (executablePath[length] == '/' || executablePath[length] == '\\') separator = length + 1;
        length++;
    }
    if (length >= PSP_SETTINGS_PATH_SIZE || separator + sizeof(filename) > PSP_SETTINGS_PATH_SIZE) return;
    for (i = 0; i < separator; i++) sSettingsPath[i] = executablePath[i];
    psp_settings_copy_path(sSettingsPath + separator, PSP_SETTINGS_PATH_SIZE - separator,
                           filename, sizeof(filename) - 1);
}

static void psp_settings_suffix(char* output, const char* suffix) {
    unsigned i = 0;
    while (sSettingsPath[i] && i < PSP_SETTINGS_PATH_SIZE - 8) {
        output[i] = sSettingsPath[i];
        i++;
    }
    while (*suffix && i < PSP_SETTINGS_PATH_SIZE - 1) output[i++] = *suffix++;
    output[i] = 0;
}

static int psp_settings_read_backend(const char* path, PspGfxBackendKind* backend) {
    static const char gu[] = "version=1\ngraphics_backend=gu\n";
    static const char pspgl[] = "version=1\ngraphics_backend=pspgl\n";
    char data[40];
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    int bytes;

    if (fd < 0) return 0;
    bytes = sceIoRead(fd, data, sizeof(data) - 1);
    sceIoClose(fd);
    if (bytes < 0 || bytes >= (int) sizeof(data)) return 0;
    data[bytes] = 0;
    if (bytes == (int) sizeof(gu) - 1 && psp_settings_equal(data, gu)) {
        *backend = PSP_GFX_KIND_GU;
        return 1;
    }
    if (bytes == (int) sizeof(pspgl) - 1 && psp_settings_equal(data, pspgl)) {
        *backend = PSP_GFX_KIND_PSPGL;
        return 1;
    }
    return 0;
}

static int psp_settings_save_backend(PspGfxBackendKind backend) {
    char temporary[PSP_SETTINGS_PATH_SIZE];
    char backup[PSP_SETTINGS_PATH_SIZE];
    const char* data = backend == PSP_GFX_KIND_PSPGL ?
        "version=1\ngraphics_backend=pspgl\n" : "version=1\ngraphics_backend=gu\n";
    unsigned length = 0;
    int hadPrevious = 0;
    int writeResult;
    SceUID fd;

    psp_settings_suffix(temporary, ".tmp");
    psp_settings_suffix(backup, ".bak");
    sceIoRemove(temporary);
    fd = sceIoOpen(temporary, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0666);
    if (fd < 0) return 0;
    while (data[length]) length++;
    writeResult = sceIoWrite(fd, data, length);
    if (sceIoClose(fd) < 0 || writeResult != (int) length) {
        sceIoRemove(temporary);
        return 0;
    }

    fd = sceIoOpen(sSettingsPath, PSP_O_RDONLY, 0);
    if (fd >= 0) {
        sceIoClose(fd);
        sceIoRemove(backup);
        if (sceIoRename(sSettingsPath, backup) < 0) {
            sceIoRemove(temporary);
            return 0;
        }
        hadPrevious = 1;
    }
    if (sceIoRename(temporary, sSettingsPath) < 0) {
        if (hadPrevious) sceIoRename(backup, sSettingsPath);
        sceIoRemove(temporary);
        return 0;
    }
    if (hadPrevious) sceIoRemove(backup);
    return 1;
}

void PspSettings_Init(const char* executablePath, PspGfxBackendKind buildDefault) {
    char backup[PSP_SETTINGS_PATH_SIZE];
    char temporary[PSP_SETTINGS_PATH_SIZE];

    psp_settings_set_path(executablePath);
    sGraphicsBackend = buildDefault;
    sActiveGraphicsBackend = buildDefault;
    sBackendSaveFailed = 0;
    psp_settings_suffix(backup, ".bak");
    psp_settings_suffix(temporary, ".tmp");
    if (!psp_settings_read_backend(sSettingsPath, &sGraphicsBackend) &&
        !psp_settings_read_backend(backup, &sGraphicsBackend)) {
        psp_settings_read_backend(temporary, &sGraphicsBackend);
    }
}

PspGfxBackendKind PspSettings_GetGraphicsBackend(void) {
    return sGraphicsBackend;
}

int PspSettings_SetGraphicsBackend(PspGfxBackendKind backend) {
    if (backend != PSP_GFX_KIND_GU && backend != PSP_GFX_KIND_PSPGL) return 0;
    if (backend == sGraphicsBackend && !sBackendSaveFailed) return 1;
    sGraphicsBackend = backend;
    sBackendSaveFailed = !psp_settings_save_backend(backend);
    return !sBackendSaveFailed;
}

int PspSettings_GraphicsBackendSaveFailed(void) {
    return sBackendSaveFailed;
}

void PspSettings_SetActiveGraphicsBackend(PspGfxBackendKind backend) {
    if (backend == PSP_GFX_KIND_GU || backend == PSP_GFX_KIND_PSPGL) sActiveGraphicsBackend = backend;
}

PspGfxBackendKind PspSettings_GetActiveGraphicsBackend(void) {
    return sActiveGraphicsBackend;
}

void PspSettings_SetDisplayMode(n64psp_display_mode mode) {
    if (mode >= 0 && mode < N64PSP_DISPLAY_PSP_MODE_COUNT) {
        sDisplayMode = mode;
    }
}

void PspSettings_SetPresentationHz(int hz) {
    if (hz == 30 || hz == 60) {
        sPresentationHz = hz;
    }
}

int PspSettings_NeedsBoundary(void) {
    return sDisplayMode >= 0 || sPresentationHz != 0
#if PSP_FPS_OVERLAY
           || sTogglePerformance
#endif
           ;
}

// Called before simulation generation with the previous graphics task complete
void PspSettings_Apply(void) {
#if PSP_FPS_OVERLAY
    if (sTogglePerformance) {
        sPerformanceVisible = !sPerformanceVisible;
        sTogglePerformance = 0;
    }
#endif
    if (sDisplayMode >= 0) {
        if (PspDisplay_SetMode(sDisplayMode)) {
            PspFrameInterpolation_Reset();
        }
        sDisplayMode = -1;
    }
    if (sPresentationHz != 0) {
        PspFrameScheduler_SetPresentationHz(sPresentationHz);
        sPresentationHz = 0;
    }
}

int PspSettings_PerformanceVisible(void) {
#if PSP_FPS_OVERLAY
    return sPerformanceVisible;
#else
    return 0;
#endif
}

void PspSettings_TogglePerformance(void) {
#if PSP_FPS_OVERLAY
    sTogglePerformance = !sTogglePerformance;
#endif
}
