#include "gfx_psp_scratchpad_code.h"
#include <pspkernel.h>
#include <psppower.h>
#include <stdint.h>
#include <string.h>

void PspPlatform_LogLine(const char* line);

#ifndef PSP_SCRATCHPAD_ADDRESS
#define PSP_SCRATCHPAD_ADDRESS ((unsigned char*) 0x00010000)
#define PSP_SCRATCHPAD_UNCACHED ((volatile unsigned*) 0x40010000)
#endif

const unsigned char* volatile gPspScratchpadTargets[3] = {
    PspScratchpad_Original0, PspScratchpad_Original1, PspScratchpad_Original2
};
static unsigned sImage[PSP_SCRATCHPAD_STORAGE / 4] __attribute__((aligned(64)));
static int sReady, sLocked, sOwner, sInitialized;
static void scratchpad_end(void);

static void scratchpad_originals(void) {
    unsigned i;
    for (i = 0; i < 3; i++) gPspScratchpadTargets[i] = sScratchpadCode[i].original;
    __sync_synchronize();
}

static int scratchpad_valid(const ScratchpadCode* code) {
    const unsigned* words = (const unsigned*) code->original;
    unsigned i, source = (unsigned) (uintptr_t) code->original;
    unsigned target = (unsigned) (uintptr_t) (PSP_SCRATCHPAD_ADDRESS + code->offset);
    for (i = 0; i < code->bytes / 4; i++) {
        unsigned word = words[i], op = word >> 26;
        if ((word & code->masks[i]) != code->expected[i]) return 0;
        if (op == 2 || op == 3) {
            unsigned pc = source + i * 4 + 4;
            unsigned destination = (pc & 0xf0000000u) | ((word & 0x03ffffffu) << 2);
            if (destination >= source && destination < source + code->bytes) return 0;
            if ((pc & 0xf0000000u) != ((target + i * 4 + 4) & 0xf0000000u)) return 0;
        }
    }
    return 1;
}

static void scratchpad_init(void) {
    unsigned i;
    scratchpad_originals();
    sReady = 0;
    memset(sImage, 0xa5, sizeof(sImage));
    for (i = 0; i < 3; i++) {
        const ScratchpadCode* code = &sScratchpadCode[i];
        if (!scratchpad_valid(code)) {
            PspPlatform_LogLine("[gu] scratchpad code rejected, using RAM");
            return;
        }
        memcpy((unsigned char*) sImage + code->offset, code->original, code->bytes);
    }
    sReady = 1;
}

static void scratchpad_begin(void) {
    unsigned i;
    if (!sReady) return;
    if (sLocked) {
        if (sceKernelGetThreadId() != sOwner) {
            PspPlatform_LogLine("[gu] concurrent scratchpad render");
            sceKernelExitGame();
        }
        return;
    }
    if (scePowerIsLowBattery() != 0 || scePowerIsSuspendRequired() != 0 || scePowerLock(0) < 0) return;
    sLocked = 1;
    sOwner = sceKernelGetThreadId();
    if (scePowerTick(PSP_POWER_TICK_SUSPEND) < 0) {
        scratchpad_end();
        return;
    }

    // The GU renderer owns this range and can repair it only before entering copied code
    for (i = 0; i < sizeof(sImage) / 4; i++) if (PSP_SCRATCHPAD_UNCACHED[i] != sImage[i]) break;
    if (i != sizeof(sImage) / 4) {
        sceKernelDcacheWritebackInvalidateRange(PSP_SCRATCHPAD_ADDRESS, sizeof(sImage));
        for (i = 0; i < sizeof(sImage) / 4; i++) PSP_SCRATCHPAD_UNCACHED[i] = sImage[i];
        __sync_synchronize();
        sceKernelDcacheWritebackInvalidateRange(PSP_SCRATCHPAD_ADDRESS, sizeof(sImage));
        for (i = 0; i < sizeof(sImage) / 4; i++) {
            if (PSP_SCRATCHPAD_UNCACHED[i] != sImage[i]) {
                sReady = 0;
                PspPlatform_LogLine("[gu] scratchpad write failed, using RAM");
                scratchpad_end();
                return;
            }
        }
    }
    sceKernelIcacheInvalidateRange(PSP_SCRATCHPAD_ADDRESS, sizeof(sImage));
    for (i = 0; i < 3; i++) gPspScratchpadTargets[i] = PSP_SCRATCHPAD_ADDRESS + sScratchpadCode[i].offset;
    __sync_synchronize();
}

static void scratchpad_end(void) {
    if (!sLocked) return;
    if (sceKernelGetThreadId() != sOwner) {
        PspPlatform_LogLine("[gu] scratchpad release from another thread");
        sceKernelExitGame();
        return;
    }
    // Pending suspend is released only after every dispatch target is back in RAM
    scratchpad_originals();
    sLocked = 0;
    if (scePowerUnlock(0) < 0) {
        sReady = 0;
        PspPlatform_LogLine("[gu] scratchpad power unlock failed");
        sceKernelExitGame();
    }
}

static void scratchpad_shutdown(void) {
    scratchpad_end();
    scratchpad_originals();
    sReady = 0;
}

int __real_PspGfxGuDevice_BeginFrame(void);
int __real_PspGfxGuDevice_Submit(void);
void __real_PspGfxGuDevice_Shutdown(void);

int __wrap_PspGfxGuDevice_BeginFrame(void) {
    int result = __real_PspGfxGuDevice_BeginFrame();
    if (result) {
        if (!sInitialized) { scratchpad_init(); sInitialized = 1; }
        scratchpad_begin();
    }
    return result;
}

int __wrap_PspGfxGuDevice_Submit(void) {
    scratchpad_end();
    return __real_PspGfxGuDevice_Submit();
}

void __wrap_PspGfxGuDevice_Shutdown(void) {
    scratchpad_shutdown();
    sInitialized = 0;
    __real_PspGfxGuDevice_Shutdown();
}
