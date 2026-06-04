#ifndef SF64_PS3_PORT_CONFIG_H
#define SF64_PS3_PORT_CONFIG_H

#define SF64_PS3_SMOKE_FRAME_COUNT 3
#define SF64_PS3_VI_MESSAGE_VALUE ((void *)0x53463634)

#if defined(SF64_PS3_TARGET)
#define SF64_PS3_BUILD_FLAVOR "ps3"
#else
#define SF64_PS3_BUILD_FLAVOR "host"
#endif

#if defined(__GNUC__)
#define SF64_PS3_WEAK __attribute__((weak))
#else
#define SF64_PS3_WEAK
#endif

#endif
