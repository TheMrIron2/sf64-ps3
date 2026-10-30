#include "src/psp/time_conversion.h"
#include <assert.h>

int main(void) {
    assert(PspTime_UsecsToN64Ticks(1000000) == 46875000);
    assert(PspTime_N64TicksToUsecs(46875000) == 1000000);
    assert(PspTime_N64TicksToUsecs(0) == 0);
    assert(PspTime_UsecsToN64Ticks(0) == 0);
    assert(PspTime_N64TicksToUsecs(~0ULL) != 0);
    assert(PspTime_UsecsToN64Ticks(~0ULL) == ~0ULL);
    assert(PspTime_N64TicksToUsecs(PspTime_UsecsToN64Ticks(987654321)) >= 987654320);
    assert(PspTime_N64TicksToUsecs(PspTime_UsecsToN64Ticks(987654321)) <= 987654321);
    return 0;
}
