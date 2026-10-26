#ifndef PSP_TIME_CONVERSION_H
#define PSP_TIME_CONVERSION_H

#define PSP_N64_TICKS_PER_SECOND 46875000ULL

static inline unsigned long long PspTime_UsecsToN64Ticks(unsigned long long usecs) {
    unsigned long long seconds = usecs / 1000000ULL;
    unsigned long long remainder = usecs % 1000000ULL;
    unsigned long long ticks, fraction;
    if (seconds > (~0ULL) / PSP_N64_TICKS_PER_SECOND) return ~0ULL;
    ticks = seconds * PSP_N64_TICKS_PER_SECOND;
    fraction = (remainder * PSP_N64_TICKS_PER_SECOND) / 1000000ULL;
    return (~0ULL - ticks < fraction) ? ~0ULL : ticks + fraction;
}

static inline unsigned long long PspTime_N64TicksToUsecs(unsigned long long ticks) {
    unsigned long long seconds = ticks / PSP_N64_TICKS_PER_SECOND;
    unsigned long long remainder = ticks % PSP_N64_TICKS_PER_SECOND;
    unsigned long long usecs, fraction;
    if (seconds > (~0ULL) / 1000000ULL) return ~0ULL;
    usecs = seconds * 1000000ULL;
    fraction = (remainder * 1000000ULL) / PSP_N64_TICKS_PER_SECOND;
    return (~0ULL - usecs < fraction) ? ~0ULL : usecs + fraction;
}

#endif
