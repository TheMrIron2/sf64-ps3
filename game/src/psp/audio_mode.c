#include <pspkernel.h>
#include "src/psp/audio_mode.h"

static PspAudioMode sRequestedAudio;
static PspAudioMode sActiveAudio;

void PspAudioMode_Request(PspAudioMode mode) {
    int lock;
    if (mode < PSP_AUDIO_OFF || mode >= PSP_AUDIO_MODE_COUNT) return;
    lock = sceKernelCpuSuspendIntr();
    sRequestedAudio = mode;
    sceKernelCpuResumeIntr(lock);
}

PspAudioMode PspAudioMode_GetRequested(void) {
    int lock = sceKernelCpuSuspendIntr();
    PspAudioMode mode = sRequestedAudio;
    sceKernelCpuResumeIntr(lock);
    return mode;
}

PspAudioMode PspAudioMode_GetActive(void) {
    int lock = sceKernelCpuSuspendIntr();
    PspAudioMode mode = sActiveAudio;
    sceKernelCpuResumeIntr(lock);
    return mode;
}

// Only the audio scheduling thread acknowledges completed handovers
void PspAudioMode_Applied(PspAudioMode mode) {
    int lock = sceKernelCpuSuspendIntr();
    sActiveAudio = mode;
    sceKernelCpuResumeIntr(lock);
}

