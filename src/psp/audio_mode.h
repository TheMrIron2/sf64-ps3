#ifndef PSP_AUDIO_MODE_H
#define PSP_AUDIO_MODE_H

typedef enum {
    PSP_AUDIO_OFF,
    PSP_AUDIO_SYNC,
    PSP_AUDIO_ASYNC,
    PSP_AUDIO_MODE_COUNT
} PspAudioMode;

void PspAudioMode_Request(PspAudioMode mode);
PspAudioMode PspAudioMode_GetRequested(void);
PspAudioMode PspAudioMode_GetActive(void);
void PspAudioMode_Applied(PspAudioMode mode);

#endif
