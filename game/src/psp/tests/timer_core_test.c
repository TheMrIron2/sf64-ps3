#include "src/psp/timer_core.h"
#include <assert.h>

int main(void) {
    PspTimerCore core = { 0 };
    PspTimerCoreDispatch d;
    int owners[PSP_TIMER_CORE_CAPACITY + 2];
    unsigned i;
    for (i = 0; i < 10000; i++) {
        int owner = (int) i;
        int slot = PspTimerCore_Register(&core, &owner, 10, 0, (void*) 1, (void*) 2);
        assert(slot >= 0);
        assert(PspTimerCore_TakeDue(&core, 9, &d) == 0);
        assert(PspTimerCore_TakeDue(&core, 10, &d) == 1 && d.oneShot);
        assert(PspTimerCore_DispatchValid(&core, &d));
        PspTimerCore_Complete(&core, &d);
        assert(!PspTimerCore_DispatchValid(&core, &d));
    }
    for (i = 0; i < PSP_TIMER_CORE_CAPACITY; i++)
        assert(PspTimerCore_Register(&core, &owners[i], 100, 0, 0, 0) >= 0);
    assert(PspTimerCore_Register(&core, &owners[PSP_TIMER_CORE_CAPACITY], 100, 0, 0, 0) < 0);
    assert(PspTimerCore_Cancel(&core, &owners[7]));
    assert(PspTimerCore_Register(&core, &owners[PSP_TIMER_CORE_CAPACITY], 100, 0, 0, 0) >= 0);
    for (i = 0; i < PSP_TIMER_CORE_CAPACITY + 1; i++) PspTimerCore_Cancel(&core, &owners[i]);
    assert(PspTimerCore_Register(&core, &owners[0], 20, 10, (void*) 3, (void*) 4) >= 0);
    assert(PspTimerCore_Register(&core, &owners[1], 30, 0, 0, 0) >= 0);
    assert(PspTimerCore_TakeDue(&core, 29, &d) == 1 && d.queue == (void*) 3 && !d.oneShot);
    {
        unsigned long long deadline;
        assert(PspTimerCore_NextDeadline(&core, &deadline) && deadline == 30);
    }
    assert(PspTimerCore_TakeDue(&core, 1000, &d) == 1 && d.oneShot == 1);
    assert(PspTimerCore_Cancel(&core, &owners[1]));
    assert(!PspTimerCore_DispatchValid(&core, &d));
    assert(PspTimerCore_Register(&core, &owners[1], 1001, 0, 0, 0) >= 0);
    assert(!PspTimerCore_DispatchValid(&core, &d));
    assert(PspTimerCore_TakeDue(&core, 1000, &d) == 1 && !d.oneShot);
    assert(PspTimerCore_Cancel(&core, &owners[1]));
    {
        unsigned long long deadline;
        assert(PspTimerCore_NextDeadline(&core, &deadline) && deadline == 1010);
    }
    assert(PspTimerCore_Cancel(&core, &owners[0]));
    assert(PspTimerCore_Register(&core, &owners[0], 0, 0, 0, 0) >= 0);
    assert(PspTimerCore_TakeDue(&core, 0, &d) == 1 && d.oneShot);
    PspTimerCore_Complete(&core, &d);
    assert(PspTimerCore_Register(&core, &owners[0], ~0ULL, 0, 0, 0) >= 0);
    {
        unsigned long long deadline;
        assert(PspTimerCore_NextDeadline(&core, &deadline) && deadline == ~0ULL);
    }
    assert(PspTimerCore_Cancel(&core, &owners[0]));
    return 0;
}
