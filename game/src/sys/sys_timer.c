#include "sys.h"
#include "src/psp/time_conversion.h"
int sceKernelDelayThread(unsigned int delay);

TimerTask sTimerTasks[16];

TimerTask* Timer_AllocateTask(void) {
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sTimerTasks); i++) {
        if (!sTimerTasks[i].active) {
            return &sTimerTasks[i];
        }
    }
    return NULL;
}

s32 Timer_CreateTask(u64 time, TimerAction action, s32* address, s32 value) {
    TimerTask* task = Timer_AllocateTask();

    if (task == NULL) {
        return -1;
    }
    task->action = action;
    task->address = address;
    task->value = value;
    task->active = true;
    if (osSetTimer(&task->timer, time, 0, &gTimerTaskMesgQueue, task) != 0) {
        task->active = false;
        return -1;
    }
    return 0;
}

void Timer_Increment(s32* address, s32 value) {
    *address += value;
}

void Timer_SetValue(s32* address, s32 value) {
    *address = value;
}

void Timer_CompleteTask(TimerTask* task) {
    if (task->action != NULL) {
        task->action(task->address, task->value);
    }
    task->active = false;
}

void Timer_Wait(u64 time) {
    u64 usecs = PspTime_N64TicksToUsecs(time);
    if (time && usecs == 0) usecs = 1;
    while (usecs != 0) {
        unsigned int chunk = usecs > 0xFFFFFFFFULL ? 0xFFFFFFFFU : (unsigned int) usecs;
        if (sceKernelDelayThread(chunk) < 0) return;
        usecs -= chunk;
    }
}
