#ifndef SF64_TASK_PENDING_H
#define SF64_TASK_PENDING_H

#define MAIN_TASK_PENDING_CAPACITY 16
#define MAIN_TASK_PENDING_AUDIO 1
#define MAIN_TASK_PENDING_GFX 2
#define MAIN_TASK_PENDING_UNSUPPORTED 3

typedef struct {
    void* task;
    unsigned char type;
} MainTaskPendingEntry;

typedef struct {
    MainTaskPendingEntry entries[MAIN_TASK_PENDING_CAPACITY];
    unsigned char first;
    unsigned char count;
} MainTaskPendingQueue;

static inline void MainTaskPending_Init(MainTaskPendingQueue* queue) {
    queue->first = 0;
    queue->count = 0;
}

static inline int MainTaskPending_Push(MainTaskPendingQueue* queue, void* task, unsigned char type) {
    unsigned char index;
    if (queue->count >= MAIN_TASK_PENDING_CAPACITY || task == 0 ||
        (type != MAIN_TASK_PENDING_AUDIO && type != MAIN_TASK_PENDING_GFX &&
         type != MAIN_TASK_PENDING_UNSUPPORTED)) return 0;
    index = (queue->first + queue->count) % MAIN_TASK_PENDING_CAPACITY;
    queue->entries[index].task = task;
    queue->entries[index].type = type;
    queue->count++;
    return 1;
}

static inline int MainTaskPending_Peek(const MainTaskPendingQueue* queue, MainTaskPendingEntry* entry) {
    if (!queue->count) return 0;
    *entry = queue->entries[queue->first];
    return 1;
}

static inline int MainTaskPending_Pop(MainTaskPendingQueue* queue, MainTaskPendingEntry* entry) {
    if (!MainTaskPending_Peek(queue, entry)) return 0;
    queue->first = (queue->first + 1) % MAIN_TASK_PENDING_CAPACITY;
    queue->count--;
    return 1;
}

static inline int MainTaskPending_CanAdmit(unsigned char type, unsigned audioCount, unsigned audioCapacity,
                                           unsigned gfxCount, unsigned gfxCapacity) {
    if (type == MAIN_TASK_PENDING_AUDIO) return audioCount < audioCapacity;
    if (type == MAIN_TASK_PENDING_GFX) return gfxCount < gfxCapacity;
    return 0;
}

#endif
