#include "sf64_ps3/platform.h"
#include "sf64_ps3/port_config.h"

#include <stdio.h>
#include <string.h>

#ifndef OS_MESG_NOBLOCK
#define OS_MESG_NOBLOCK 0
#endif

#ifndef OS_MESG_BLOCK
#define OS_MESG_BLOCK 1
#endif

typedef void *OSMesg;

typedef struct OSMesgQueue {
    void *opaque[8];
} OSMesgQueue;

typedef struct OSContPad {
    unsigned short button;
    signed char stick_x;
    signed char stick_y;
    unsigned char errno_;
} OSContPad;

typedef struct OSTask {
    unsigned int type;
    unsigned int flags;
    void *ucode_boot;
    unsigned int ucode_boot_size;
    void *ucode;
    unsigned int ucode_size;
    void *dram_stack;
    unsigned int dram_stack_size;
    void *output_buff;
    void *output_buff_size;
    void *data_ptr;
    unsigned int data_size;
    void *yield_data_ptr;
    unsigned int yield_data_size;
} OSTask;

extern int n64ps3_init(void) SF64_PS3_WEAK;
extern int n64ps3Init(void) SF64_PS3_WEAK;
extern void n64ps3_shutdown(void) SF64_PS3_WEAK;
extern void n64ps3Shutdown(void) SF64_PS3_WEAK;

extern void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, int count) SF64_PS3_WEAK;
extern int osSendMesg(OSMesgQueue *mq, OSMesg msg, int flag) SF64_PS3_WEAK;
extern int osJamMesg(OSMesgQueue *mq, OSMesg msg, int flag) SF64_PS3_WEAK;
extern int osRecvMesg(OSMesgQueue *mq, OSMesg *msg, int flag) SF64_PS3_WEAK;
extern void osViSetEvent(OSMesgQueue *mq, OSMesg msg, unsigned int retrace_count) SF64_PS3_WEAK;
extern int osContInit(OSMesgQueue *mq, unsigned char *bitpattern, OSContPad *status) SF64_PS3_WEAK;
extern void osContStartReadData(OSMesgQueue *mq) SF64_PS3_WEAK;
extern void osContGetReadData(OSContPad *pad) SF64_PS3_WEAK;
extern void osSpTaskStartGo(OSTask *task) SF64_PS3_WEAK;

static OSMesgQueue s_vi_queue;
static OSMesg s_vi_messages[4];
static int s_queue_created;
static int s_vi_registered;

void sf64_platform_print_banner(void)
{
    printf("[sf64-ps3] smoke target: %s\n", SF64_PS3_BUILD_FLAVOR);
    printf("[sf64-ps3] probing n64ps3/libultra-shaped APIs\n");
}

Sf64ApiState sf64_platform_init(void)
{
    if (n64ps3_init != 0) {
        return n64ps3_init() == 0 ? SF64_API_WORKED : SF64_API_FAILED;
    }

    if (n64ps3Init != 0) {
        return n64ps3Init() == 0 ? SF64_API_WORKED : SF64_API_FAILED;
    }

    printf("[sf64-ps3] TODO(n64ps3): expose a documented init entry point\n");
    return SF64_API_MISSING;
}

Sf64ApiState sf64_platform_create_vi_queue(void)
{
    memset(&s_vi_queue, 0, sizeof(s_vi_queue));
    memset(s_vi_messages, 0, sizeof(s_vi_messages));

    if (osCreateMesgQueue == 0) {
        printf("[sf64-ps3] TODO(n64ps3): provide osCreateMesgQueue\n");
        return SF64_API_MISSING;
    }

    osCreateMesgQueue(&s_vi_queue, s_vi_messages, 4);
    s_queue_created = 1;
    return SF64_API_WORKED;
}

Sf64ApiState sf64_platform_register_vi_event(void)
{
    if (!s_queue_created) {
        return SF64_API_FAILED;
    }

    if (osViSetEvent == 0) {
        printf("[sf64-ps3] TODO(n64ps3): provide osViSetEvent\n");
        return SF64_API_MISSING;
    }

    osViSetEvent(&s_vi_queue, SF64_PS3_VI_MESSAGE_VALUE, 1);
    s_vi_registered = 1;
    return SF64_API_WORKED;
}

Sf64ApiState sf64_platform_trigger_vi_retrace(void)
{
    if (!s_queue_created || !s_vi_registered) {
        return SF64_API_FAILED;
    }

    if (osSendMesg != 0) {
        int rc = osSendMesg(&s_vi_queue, SF64_PS3_VI_MESSAGE_VALUE, OS_MESG_NOBLOCK);
        if (rc == 0) {
            printf("[sf64-ps3] VI retrace simulated through osSendMesg\n");
            return SF64_API_INCOMPLETE;
        }
        return SF64_API_FAILED;
    }

    printf("[sf64-ps3] TODO(n64ps3): expose VI retrace pump or usable osSendMesg\n");
    return SF64_API_MISSING;
}

Sf64ApiState sf64_platform_receive_vi_message(void)
{
    OSMesg msg = 0;

    if (!s_queue_created) {
        return SF64_API_FAILED;
    }

    if (osRecvMesg == 0) {
        printf("[sf64-ps3] TODO(n64ps3): provide osRecvMesg with OS_MESG_BLOCK semantics\n");
        return SF64_API_MISSING;
    }

    if (osRecvMesg(&s_vi_queue, &msg, OS_MESG_BLOCK) != 0) {
        return SF64_API_FAILED;
    }

    if (msg != SF64_PS3_VI_MESSAGE_VALUE) {
        printf("[sf64-ps3] VI message payload mismatch\n");
        return SF64_API_FAILED;
    }

    return SF64_API_WORKED;
}

Sf64ApiState sf64_platform_poll_controller(void)
{
    OSContPad pad;
    unsigned char bitpattern = 0;

    memset(&pad, 0, sizeof(pad));

    if (osContInit == 0 || osContStartReadData == 0 || osContGetReadData == 0) {
        printf("[sf64-ps3] TODO(n64ps3): provide controller init/read APIs\n");
        return SF64_API_MISSING;
    }

    if (!s_queue_created) {
        return SF64_API_FAILED;
    }

    if (osContInit(&s_vi_queue, &bitpattern, &pad) != 0) {
        return SF64_API_FAILED;
    }

    osContStartReadData(&s_vi_queue);
    osContGetReadData(&pad);
    printf("[sf64-ps3] controller bitpattern=0x%02x buttons=0x%04x stick=(%d,%d)\n",
           bitpattern,
           pad.button,
           pad.stick_x,
           pad.stick_y);
    return SF64_API_WORKED;
}

Sf64ApiState sf64_platform_submit_dummy_gfx_task(void)
{
    OSTask task;

    memset(&task, 0, sizeof(task));

    if (osSpTaskStartGo == 0) {
        printf("[sf64-ps3] TODO(n64ps3): provide dummy SP/DP task submission\n");
        return SF64_API_MISSING;
    }

    task.type = 1;
    osSpTaskStartGo(&task);
    return SF64_API_INCOMPLETE;
}

void sf64_platform_shutdown(void)
{
    if (n64ps3_shutdown != 0) {
        n64ps3_shutdown();
        return;
    }

    if (n64ps3Shutdown != 0) {
        n64ps3Shutdown();
        return;
    }

    printf("[sf64-ps3] TODO(n64ps3): expose a documented shutdown entry point\n");
}
