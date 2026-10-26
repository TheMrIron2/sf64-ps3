#include "src/psp/timer_core.h"

int PspTimerCore_Register(PspTimerCore* core, void* owner, unsigned long long deadline,
                          unsigned long long interval, void* queue, void* message) {
    unsigned i, slot = PSP_TIMER_CORE_CAPACITY;
    if (!core || !owner) return -1;
    for (i = 0; i < PSP_TIMER_CORE_CAPACITY; i++) {
        PspTimerCoreSlot* entry = &core->slots[i];
        if (entry->owner == owner && entry->state != PSP_TIMER_CORE_FREE) {
            entry->state = PSP_TIMER_CORE_FREE;
            entry->owner = entry->queue = entry->message = 0;
            entry->generation++;
        }
    }
    for (i = 0; i < PSP_TIMER_CORE_CAPACITY; i++) if (core->slots[i].state == PSP_TIMER_CORE_FREE) { slot = i; break; }
    if (slot == PSP_TIMER_CORE_CAPACITY) return -1;
    core->slots[slot].owner = owner;
    core->slots[slot].deadline = deadline;
    core->slots[slot].interval = interval;
    core->slots[slot].queue = queue;
    core->slots[slot].message = message;
    core->slots[slot].generation++;
    core->slots[slot].state = PSP_TIMER_CORE_ACTIVE;
    return (int) slot;
}

int PspTimerCore_Cancel(PspTimerCore* core, void* owner) {
    unsigned i;
    int found = 0;
    if (!core || !owner) return 0;
    for (i = 0; i < PSP_TIMER_CORE_CAPACITY; i++) {
        PspTimerCoreSlot* entry = &core->slots[i];
        if (entry->owner == owner && entry->state != PSP_TIMER_CORE_FREE) {
            entry->state = PSP_TIMER_CORE_FREE;
            entry->owner = entry->queue = entry->message = 0;
            entry->generation++;
            found = 1;
        }
    }
    return found;
}

int PspTimerCore_NextDeadline(const PspTimerCore* core, unsigned long long* deadline) {
    unsigned i;
    int found = 0;
    if (!core || !deadline) return 0;
    *deadline = ~0ULL;
    for (i = 0; i < PSP_TIMER_CORE_CAPACITY; i++) {
        const PspTimerCoreSlot* entry = &core->slots[i];
        if (entry->state == PSP_TIMER_CORE_ACTIVE && (!found || entry->deadline < *deadline)) {
            *deadline = entry->deadline;
            found = 1;
        }
    }
    return found;
}

int PspTimerCore_TakeDue(PspTimerCore* core, unsigned long long now, PspTimerCoreDispatch* dispatch) {
    unsigned i, slot = PSP_TIMER_CORE_CAPACITY;
    if (!core || !dispatch) return 0;
    for (i = 0; i < PSP_TIMER_CORE_CAPACITY; i++) {
        PspTimerCoreSlot* entry = &core->slots[i];
        if (entry->state == PSP_TIMER_CORE_ACTIVE && entry->deadline <= now &&
            (slot == PSP_TIMER_CORE_CAPACITY || entry->deadline < core->slots[slot].deadline)) slot = i;
    }
    if (slot == PSP_TIMER_CORE_CAPACITY) return 0;
    dispatch->slot = slot;
    dispatch->generation = core->slots[slot].generation;
    dispatch->queue = core->slots[slot].queue;
    dispatch->message = core->slots[slot].message;
    dispatch->oneShot = core->slots[slot].interval == 0;
    if (dispatch->oneShot) core->slots[slot].state = PSP_TIMER_CORE_FIRING;
    else {
        unsigned long long interval = core->slots[slot].interval;
        core->slots[slot].deadline = ~0ULL - now < interval ? ~0ULL : now + interval;
    }
    return 1;
}

int PspTimerCore_DispatchValid(const PspTimerCore* core, const PspTimerCoreDispatch* dispatch) {
    const PspTimerCoreSlot* entry;
    if (!core || !dispatch || dispatch->slot >= PSP_TIMER_CORE_CAPACITY) return 0;
    entry = &core->slots[dispatch->slot];
    return entry->generation == dispatch->generation &&
           entry->state == (dispatch->oneShot ? PSP_TIMER_CORE_FIRING : PSP_TIMER_CORE_ACTIVE);
}

void PspTimerCore_Complete(PspTimerCore* core, const PspTimerCoreDispatch* dispatch) {
    if (dispatch && dispatch->oneShot && PspTimerCore_DispatchValid(core, dispatch)) {
        core->slots[dispatch->slot].state = PSP_TIMER_CORE_FREE;
        core->slots[dispatch->slot].owner = 0;
        core->slots[dispatch->slot].queue = 0;
        core->slots[dispatch->slot].message = 0;
    }
}
