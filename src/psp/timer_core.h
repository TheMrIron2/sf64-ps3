#ifndef PSP_TIMER_CORE_H
#define PSP_TIMER_CORE_H

#define PSP_TIMER_CORE_CAPACITY 32

enum { PSP_TIMER_CORE_FREE, PSP_TIMER_CORE_ACTIVE, PSP_TIMER_CORE_FIRING };

typedef struct {
    void* owner;
    unsigned long long deadline;
    unsigned long long interval;
    void* queue;
    void* message;
    unsigned generation;
    int state;
} PspTimerCoreSlot;

typedef struct { PspTimerCoreSlot slots[PSP_TIMER_CORE_CAPACITY]; } PspTimerCore;

typedef struct {
    unsigned slot;
    unsigned generation;
    int oneShot;
    void* queue;
    void* message;
} PspTimerCoreDispatch;

int PspTimerCore_Register(PspTimerCore* core, void* owner, unsigned long long deadline,
                          unsigned long long interval, void* queue, void* message);
int PspTimerCore_Cancel(PspTimerCore* core, void* owner);
int PspTimerCore_NextDeadline(const PspTimerCore* core, unsigned long long* deadline);
int PspTimerCore_TakeDue(PspTimerCore* core, unsigned long long now, PspTimerCoreDispatch* dispatch);
int PspTimerCore_DispatchValid(const PspTimerCore* core, const PspTimerCoreDispatch* dispatch);
void PspTimerCore_Complete(PspTimerCore* core, const PspTimerCoreDispatch* dispatch);

#endif
