#include <pspctrl.h>
#include <pspkernel.h>
#include <stdio.h>

#include "src/psp/menu.h"
#include "src/psp/input.h"
#include "src/psp/overlay.h"
#include "src/psp/settings.h"
#include "src/psp/display.h"
#include "src/psp/frame_scheduler.h"
#include "src/psp/profiler.h"
#include "src/psp/hw_counter_profile.h"
#include "src/psp/gfx/gfx_psp_dl.h"

#define MENU_ROWS 12
#define MENU_TEXT 60
#define MENU_VISIBLE_ROWS 5

enum { MENU_MAIN, MENU_GRAPHICS, MENU_CONTROLS, MENU_DIAGNOSTICS, MENU_ABOUT };
enum { ACTION_NONE, ACTION_START, ACTION_STOP, ACTION_TYPE, ACTION_SCENE_PREV, ACTION_SCENE_NEXT,
       ACTION_TRACE, ACTION_COVERAGE };

typedef struct {
    int open;
    int selected;
    int count;
    int cursor;
    int main;
    char title[MENU_TEXT];
    char rows[MENU_ROWS][MENU_TEXT];
    char status[MENU_TEXT];
} PspMenuView;

static int sOpen;
static int sScreen;
static int sSelected;
#if PSP_MENU_DIAGNOSTICS
static int sAction;
#endif
#if PSP_MENU_CAPTURES
// Phase and gprof captures take precedence when several capture types are built
static int sHardware = PROFILE_HW_COUNTERS && !(PROFILE_GPROF || PROFILE_PHASES);
#endif
static PspMenuView sView;

static int menu_rows(void) {
    if (sScreen == MENU_MAIN) return 4 + !!PSP_MENU_DIAGNOSTICS;
    if (sScreen == MENU_GRAPHICS) return 2 + !!PSP_FPS_OVERLAY;
    if (sScreen == MENU_CONTROLS) {
        int count;
        PspInput_GetBindings(&count);
        return count + 1 > MENU_VISIBLE_ROWS ? count + 2 - MENU_VISIBLE_ROWS : 1;
    }
    if (sScreen == MENU_DIAGNOSTICS) {
        return 1 + (PSP_MENU_CAPTURES ? 3 : 0) + !!PROFILE_HW_COUNTERS +
               !!PSP_RENDERER_DIAGNOSTICS + !!(PSP_NATIVE_COVERAGE_AB && !PSP_GFX_BACKEND_GU);
    }
    return 1;
}

int PspMenu_IsOpen(void) {
    return sOpen;
}

#if PSP_MENU_CAPTURES
void PspMenu_RequestCapture(int start) {
    sAction = start ? ACTION_START : ACTION_STOP;
}
#endif

void PspMenu_Update(u32 pressed) {
    int activate = (pressed & PSP_CTRL_CROSS) != 0;
    int adjust = (pressed & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) != 0;
    int direction = (pressed & PSP_CTRL_LEFT) ? -1 : 1;
    int row;

    if (pressed & PSP_CTRL_SELECT) {
        sOpen = !sOpen;
        sScreen = MENU_MAIN;
        sSelected = 0;
        return;
    }
    if (!sOpen) return;
    if (pressed & PSP_CTRL_CIRCLE) {
        if (sScreen == MENU_MAIN) sOpen = 0;
        sScreen = MENU_MAIN;
        sSelected = 0;
        return;
    }
    if (pressed & PSP_CTRL_UP) sSelected = (sSelected + menu_rows() - 1) % menu_rows();
    else if (pressed & PSP_CTRL_DOWN) sSelected = (sSelected + 1) % menu_rows();
    if (!activate && !adjust) return;

    if (sScreen == MENU_MAIN && activate) {
        if (sSelected == 0) sOpen = 0;
        else {
            sScreen = sSelected == 1 ? MENU_GRAPHICS : sSelected == 2 ? MENU_CONTROLS :
#if PSP_MENU_DIAGNOSTICS
                      sSelected == 3 ? MENU_DIAGNOSTICS :
#endif
                      MENU_ABOUT;
            sSelected = 0;
        }
    } else if (sScreen == MENU_GRAPHICS) {
        if (sSelected == 0) {
            int mode = PspDisplay_GetConfig()->mode;
            PspSettings_SetDisplayMode((mode + direction + N64PSP_DISPLAY_PSP_MODE_COUNT) % N64PSP_DISPLAY_PSP_MODE_COUNT);
        } else if (sSelected == 1) {
            PspSettings_SetPresentationHz(PspFrameScheduler_GetPresentationVIs() == 1 ? 30 : 60);
#if PSP_FPS_OVERLAY
        } else if (sSelected == 2) {
            PspSettings_TogglePerformance();
#endif
        }
    } else if (sScreen == MENU_DIAGNOSTICS) {
        row = 0;
#if PSP_MENU_CAPTURES
        if (sSelected == row++) sAction = ACTION_TYPE;
        else if (sSelected == row++ && activate) sAction = ACTION_START;
        else if (sSelected == row++ && activate) sAction = ACTION_STOP;
        else
#endif
#if PROFILE_HW_COUNTERS
        if (sSelected == row++) sAction = direction < 0 ? ACTION_SCENE_PREV : ACTION_SCENE_NEXT;
        else
#endif
#if PSP_RENDERER_DIAGNOSTICS
        if (sSelected == row++ && activate) sAction = ACTION_TRACE;
        else
#endif
#if PSP_NATIVE_COVERAGE_AB && !PSP_GFX_BACKEND_GU
        if (sSelected == row++) sAction = ACTION_COVERAGE;
        else
#endif
        if (sSelected == row && activate) { sScreen = MENU_MAIN; sSelected = 0; }
    } else if (activate) {
        sScreen = MENU_MAIN;
        sSelected = 0;
    }
}

int PspMenu_NeedsBoundary(void) {
#if PSP_MENU_DIAGNOSTICS
    if (sAction != ACTION_NONE) return 1;
#endif
#if PROFILE_GPROF || PROFILE_PHASES
    if (PspProfiler_NeedsService()) return 1;
#endif
#if PROFILE_HW_COUNTERS
    if (PspHwCounterProfile_NeedsService()) return 1;
#endif
    return PspSettings_NeedsBoundary();
}

// Diagnostic writes run on the scheduler thread with no graphics task in flight
void PspMenu_Apply(void) {
#if PROFILE_GPROF || PROFILE_PHASES
    PspProfiler_Service();
#endif
    PspHwCounterProfile_Service();
#if PSP_MENU_CAPTURES
    if (sAction == ACTION_START) {
        if (!PspProfiler_IsCapturing()
#if PROFILE_HW_COUNTERS
            && !PspHwCounterProfile_IsBusy()
#endif
        ) {
#if PROFILE_HW_COUNTERS
            if (sHardware) PspHwCounterProfile_StartCapture();
            else
#endif
            PspProfiler_StartCapture();
        }
    } else if (sAction == ACTION_STOP) {
#if PROFILE_HW_COUNTERS
        if (sHardware) PspHwCounterProfile_StopCapture();
        else
#endif
        {
            PspProfiler_StopCapture();
            PspProfiler_DumpCapture();
        }
    } else if (sAction == ACTION_TYPE) {
#if PROFILE_HW_COUNTERS && (PROFILE_GPROF || PROFILE_PHASES)
        if (!PspProfiler_IsCapturing() && !PspHwCounterProfile_IsBusy()) sHardware = !sHardware;
#endif
    }
#endif
#if PROFILE_HW_COUNTERS
    if (sAction == ACTION_SCENE_PREV) PspHwCounterProfile_CycleScene(-1);
    if (sAction == ACTION_SCENE_NEXT) PspHwCounterProfile_CycleScene(1);
#endif
#if PSP_RENDERER_DIAGNOSTICS
    if (sAction == ACTION_TRACE) PspGfxDl_TraceRequest();
#endif
#if PSP_NATIVE_COVERAGE_AB && !PSP_GFX_BACKEND_GU
    if (sAction == ACTION_COVERAGE) PspGfxDl_ToggleNativeCoverage();
#endif
#if PSP_MENU_DIAGNOSTICS
    sAction = ACTION_NONE;
#endif
    PspSettings_Apply();
}

void PspMenu_Publish(void) {
    PspMenuView view;
    int n = 0;
    int lock;
    if (!sOpen) {
        lock = sceKernelCpuSuspendIntr();
        sView.open = 0;
        sceKernelCpuResumeIntr(lock);
        return;
    }
    view = (PspMenuView){ 0 };
    view.open = 1;
    view.selected = sSelected;
    view.cursor = sSelected;
    view.main = sScreen == MENU_MAIN;
#define ROW(...) snprintf(view.rows[n++], MENU_TEXT, __VA_ARGS__)
    if (sScreen == MENU_MAIN) {
        snprintf(view.title, MENU_TEXT, "%s", "Star Fox 64 PSP");
        ROW("Resume Game"); ROW("Graphics & Display"); ROW("Controls");
#if PSP_MENU_DIAGNOSTICS
        ROW("Performance / Diagnostics");
#endif
        ROW("About");
    } else if (sScreen == MENU_GRAPHICS) {
        int mode = PspDisplay_GetConfig()->mode;
        int vis = PspFrameScheduler_GetPresentationVIs();
        snprintf(view.title, MENU_TEXT, "%s", "Graphics & Display");
        static const char* const modes[] = { "Original", "4:3", "16:9" };
        ROW("Aspect: <%s>", modes[mode]);
        ROW("Presentation: <%d Hz>", 60 / vis);
#if PSP_FPS_OVERLAY
        ROW("Performance overlay: %s", PspSettings_PerformanceVisible() ? "On" : "Off");
#endif
    } else if (sScreen == MENU_CONTROLS) {
        int count, i;
        const PspInputBinding* bindings = PspInput_GetBindings(&count);
        snprintf(view.title, MENU_TEXT, "%s", "Controls");
        for (i = 0; i < count && n < MENU_ROWS - 1; i++) ROW("%s: %s", bindings[i].pspName, bindings[i].n64Name);
        ROW("Analog: N64 stick");
        view.selected = -1;
    } else if (sScreen == MENU_DIAGNOSTICS) {
        snprintf(view.title, MENU_TEXT, "%s", "Performance / Diagnostics");
#if PSP_MENU_CAPTURES
        ROW("Capture: %s", sHardware ? "Hardware counters" : "Phase / gprof");
        ROW("Start capture"); ROW("Stop and save capture");
        snprintf(view.status, MENU_TEXT, "%s: %s", sHardware ? "HW" : "PROF",
#if PROFILE_HW_COUNTERS
                 sHardware ? PspHwCounterProfile_GetStatus() :
#endif
                 PspProfiler_GetStatus());
#endif
#if PROFILE_HW_COUNTERS
        ROW("Capture scene: %s", PspHwCounterProfile_GetScene());
#endif
#if PSP_RENDERER_DIAGNOSTICS
        ROW("Trace: %s", PspGfxDl_TracePending() ? "Armed" : "Capture next task");
#endif
#if PSP_NATIVE_COVERAGE_AB && !PSP_GFX_BACKEND_GU
        {
            PspGfxDlNativeCoverageStats stats;
            PspGfxDl_GetNativeCoverageStats(&stats);
            ROW("Native coverage mode: %lu", (unsigned long) stats.mode);
        }
#endif
        ROW("Back");
    } else {
        snprintf(view.title, MENU_TEXT, "%s", "About");
        ROW("Star Fox 64 PSP");
        ROW("TheMrIron2/sf64-psp");
        ROW("Educational native PSP port");
        ROW("Backend: %s", PSP_GFX_BACKEND_NAME);
        view.selected = -1;
    }
#undef ROW
    view.count = n;
    lock = sceKernelCpuSuspendIntr();
    sView = view;
    sceKernelCpuResumeIntr(lock);
}

int PspMenu_Visible(void) {
    int lock = sceKernelCpuSuspendIntr();
    int open = sView.open;
    sceKernelCpuResumeIntr(lock);
    return open;
}

static void menu_label(int x, int y, int width, int centered, u32 color, const char* text) {
    char label[MENU_TEXT];
    int length = 0;
    int clipped = 0;
    while (text[length] && length < MENU_TEXT - 1) {
        label[length] = text[length];
        length++;
    }
    label[length] = 0;
    if (PspOverlay_MenuTextWidth(label) > width) {
        clipped = 1;
        while (length > 3 && PspOverlay_MenuTextWidth(label) + PspOverlay_MenuTextWidth("...") > width) {
            label[--length] = 0;
        }
    }
    if (clipped && length < MENU_TEXT - 3) {
        label[length++] = '.';
        label[length++] = '.';
        label[length++] = '.';
        label[length] = 0;
    }
    if (centered) x += (width - PspOverlay_MenuTextWidth(label)) / 2;
    PspOverlay_MenuText(x, y, color, label);
}

void PspMenu_Draw(void) {
    PspMenuView view;
    int visible, first, height, top, rowTop, rowHeight, i;
    int lock = sceKernelCpuSuspendIntr();
    view = sView;
    sceKernelCpuResumeIntr(lock);
    if (!view.open) return;
    visible = view.count < MENU_VISIBLE_ROWS ? view.count : MENU_VISIBLE_ROWS;
    first = view.cursor - (view.selected >= 0 ? visible / 2 : 0);
    if (first > view.count - visible) first = view.count - visible;
    if (first < 0) first = 0;
    rowHeight = view.main ? 36 : 24;
    height = visible * rowHeight + (view.main ? 20 : 50) + (view.status[0] ? 24 : 0);
    top = (272 - height) / 2;
    rowTop = top + (view.main ? 10 : 38);
    PspOverlay_MenuBegin();
    PspOverlay_Rect(90, top, 300, height, 0xE0000000);
    if (!view.main) menu_label(110, top + 9, 260, 1, 0xFFF5FFFF, view.title);
    for (i = 0; i < visible; i++) {
        int row = first + i;
        int y = rowTop + i * rowHeight;
        u32 color = 0xFFDAE0DA;
        if (row == view.selected) {
            PspOverlay_Rect(104, y + (view.main ? 2 : 0), 272, view.main ? 30 : 24, 0xCD4E5C26);
            color = 0xFFF5FFFF;
        }
        menu_label(114, y + (view.main ? 7 : 2), 252, view.main, color, view.rows[row]);
    }
    if (first > 0) PspOverlay_MenuText(375, rowTop - 4, 0xFFB4BEAA, "^");
    if (first + visible < view.count)
        PspOverlay_MenuText(375, rowTop + (visible - 1) * rowHeight + 3, 0xFFB4BEAA, "v");
    if (view.status[0]) menu_label(110, rowTop + visible * rowHeight + 4, 260, 1, 0xFFB4BEAA, view.status);
}
