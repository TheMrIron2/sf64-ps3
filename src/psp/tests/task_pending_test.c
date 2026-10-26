#include "src/sys/task_pending.h"
#include <assert.h>
#include <string.h>

#define INPUT_CAPACITY 16
#define AUDIO_CAPACITY 1
#define GFX_CAPACITY 2
#define TASK_COUNT 40

typedef struct {
    unsigned id;
    unsigned char type;
} TestTask;

typedef struct {
    TestTask* entries[INPUT_CAPACITY];
    unsigned count;
} TestInputQueue;

static int enqueue(TestInputQueue* queue, TestTask* task) {
    if (queue->count == INPUT_CAPACITY) return 0;
    queue->entries[queue->count++] = task;
    return 1;
}

static TestTask* dequeue(TestInputQueue* queue) {
    TestTask* task;
    unsigned i;
    if (!queue->count) return 0;
    task = queue->entries[0];
    for (i = 1; i < queue->count; i++) queue->entries[i - 1] = queue->entries[i];
    queue->count--;
    return task;
}

static unsigned active_count(TestTask** tasks, unsigned capacity) {
    unsigned i, count = 0;
    for (i = 0; i < capacity; i++) if (tasks[i]) count++;
    return count;
}

static void admit(MainTaskPendingQueue* pending, TestTask** audio, TestTask** gfx,
                  unsigned admitted[], unsigned* admittedCount) {
    MainTaskPendingEntry entry;
    for (;;) {
        unsigned audioCount = active_count(audio, AUDIO_CAPACITY);
        unsigned gfxCount = active_count(gfx, GFX_CAPACITY);
        if (!MainTaskPending_Peek(pending, &entry)) return;
        if (!MainTaskPending_CanAdmit(entry.type, audioCount, AUDIO_CAPACITY, gfxCount, GFX_CAPACITY)) return;
        if (entry.type == MAIN_TASK_PENDING_AUDIO) {
            audio[audioCount] = (TestTask*) entry.task;
        } else {
            gfx[gfxCount] = (TestTask*) entry.task;
        }
        admitted[((TestTask*) entry.task)->id]++;
        (*admittedCount)++;
        MainTaskPending_Pop(pending, &entry);
    }
}

static void scheduler_iteration(MainTaskPendingQueue* pending, TestInputQueue* input,
                                TestTask** audio, TestTask** gfx,
                                unsigned admitted[], unsigned* admittedCount) {
    TestTask* task;
    MainTaskPendingEntry entry;
    admit(pending, audio, gfx, admitted, admittedCount);
    while (pending->count < MAIN_TASK_PENDING_CAPACITY && (task = dequeue(input)) != 0) {
        assert(task->type == MAIN_TASK_PENDING_AUDIO || task->type == MAIN_TASK_PENDING_GFX ||
               task->type == MAIN_TASK_PENDING_UNSUPPORTED);
        assert(MainTaskPending_Push(pending, task, task->type));
        if (task->type == MAIN_TASK_PENDING_UNSUPPORTED) {
            assert(MainTaskPending_Peek(pending, &entry) && entry.task == task);
            MainTaskPending_Pop(pending, &entry);
        } else {
            admit(pending, audio, gfx, admitted, admittedCount);
        }
    }
    admit(pending, audio, gfx, admitted, admittedCount);
}

int main(void) {
    MainTaskPendingQueue pending;
    TestInputQueue input = { { 0 }, 0 };
    TestTask tasks[TASK_COUNT];
    TestTask* audio[AUDIO_CAPACITY] = { 0 };
    TestTask* gfx[GFX_CAPACITY] = { 0 };
    unsigned admitted[TASK_COUNT] = { 0 }, admittedCount = 0, submitted = 0, i, iterations = 0;
    MainTaskPendingEntry entry;

    MainTaskPending_Init(&pending);
    assert(!MainTaskPending_Peek(&pending, &entry));
    assert(!MainTaskPending_Pop(&pending, &entry));
    assert(!MainTaskPending_CanAdmit(MAIN_TASK_PENDING_UNSUPPORTED, 0, 1, 0, 2));
    assert(!MainTaskPending_Push(&pending, 0, MAIN_TASK_PENDING_AUDIO));
    assert(!MainTaskPending_Push(&pending, &tasks[0], 99));

    for (i = 0; i < TASK_COUNT; i++) {
        tasks[i].id = i;
        tasks[i].type = (i < 10) ? MAIN_TASK_PENDING_AUDIO :
                        ((i & 1) ? MAIN_TASK_PENDING_GFX : MAIN_TASK_PENDING_AUDIO);
    }

    tasks[0].type = MAIN_TASK_PENDING_AUDIO;
    assert(MainTaskPending_Push(&pending, &tasks[0], MAIN_TASK_PENDING_AUDIO));
    admit(&pending, audio, gfx, admitted, &admittedCount);
    assert(audio[0] == &tasks[0]);
    audio[0] = 0;
    tasks[1].type = MAIN_TASK_PENDING_GFX;
    assert(MainTaskPending_Push(&pending, &tasks[1], MAIN_TASK_PENDING_GFX));
    admit(&pending, audio, gfx, admitted, &admittedCount);
    assert(gfx[0] == &tasks[1]);
    gfx[0] = 0;

    MainTaskPending_Init(&pending);
    memset(admitted, 0, sizeof(admitted));
    admittedCount = 0;
    /* Start with active graphics work so both queues fill */
    tasks[0].type = tasks[1].type = MAIN_TASK_PENDING_GFX;
    gfx[0] = &tasks[0];
    gfx[1] = &tasks[1];
    admitted[0] = admitted[1] = 1;
    admittedCount = 2;
    submitted = 2;
    while (admittedCount < TASK_COUNT) {
        while (submitted < TASK_COUNT && enqueue(&input, &tasks[submitted])) submitted++;
        if (++iterations == 3) {
            assert(input.count == INPUT_CAPACITY);
            assert(pending.count == MAIN_TASK_PENDING_CAPACITY);
            assert(!MainTaskPending_Push(&pending, &tasks[39], MAIN_TASK_PENDING_GFX));
            gfx[0] = gfx[1] = 0;
        } else if (iterations > 3) {
            /* Simulate SP and DP completion */
            audio[0] = 0;
            gfx[0] = gfx[1] = 0;
        }
        scheduler_iteration(&pending, &input, audio, gfx, admitted, &admittedCount);
        assert(iterations < 100);
    }
    assert(pending.count == 0 && input.count == 0 && submitted == TASK_COUNT);
    for (i = 0; i < TASK_COUNT; i++) assert(admitted[i] == 1);

    MainTaskPending_Init(&pending);
    assert(MainTaskPending_Push(&pending, &tasks[10], MAIN_TASK_PENDING_GFX));
    assert(MainTaskPending_Push(&pending, &tasks[11], MAIN_TASK_PENDING_AUDIO));
    assert(MainTaskPending_Peek(&pending, &entry) && entry.task == &tasks[10]);
    MainTaskPending_Pop(&pending, &entry);
    assert(MainTaskPending_Push(&pending, &tasks[12], MAIN_TASK_PENDING_GFX));
    assert(MainTaskPending_Pop(&pending, &entry) && entry.task == &tasks[11]);
    assert(MainTaskPending_Pop(&pending, &entry) && entry.task == &tasks[12]);
    assert(!MainTaskPending_Pop(&pending, &entry));
    return 0;
}
