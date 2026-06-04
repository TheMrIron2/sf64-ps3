#include "sf64_ps3/platform.h"
#include "sf64_ps3/port_config.h"

#include "n64ps3/n64ps3.h"

#include <stdio.h>
#include <string.h>

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
    N64ps3Config config = n64ps3_config_default();

    config.app_name = "sf64-ps3-smoke";
    config.asset_root = "assets";
    config.save_root = "saves";
    config.vi_refresh_hz = 60;

    return n64ps3_runtime_init(&config) == 0 ? SF64_API_WORKED : SF64_API_FAILED;
}

Sf64ApiState sf64_platform_create_vi_queue(void)
{
    memset(&s_vi_queue, 0, sizeof(s_vi_queue));
    memset(s_vi_messages, 0, sizeof(s_vi_messages));

    osCreateMesgQueue(&s_vi_queue, s_vi_messages, 4);
    s_queue_created = 1;
    return SF64_API_WORKED;
}

Sf64ApiState sf64_platform_register_vi_event(void)
{
    if (!s_queue_created) {
        return SF64_API_FAILED;
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

    n64ps3_vi_raise_retrace_for_host();
    return SF64_API_WORKED;
}

Sf64ApiState sf64_platform_receive_vi_message(void)
{
    OSMesg msg = 0;
    OSMesg duplicate = 0;

    if (!s_queue_created) {
        return SF64_API_FAILED;
    }

    if (osRecvMesg(&s_vi_queue, &msg, OS_MESG_BLOCK) != 0) {
        return SF64_API_FAILED;
    }

    if (msg != SF64_PS3_VI_MESSAGE_VALUE) {
        printf("[sf64-ps3] VI message payload mismatch\n");
        return SF64_API_FAILED;
    }

    if (osRecvMesg(&s_vi_queue, &duplicate, OS_MESG_NOBLOCK) == 0) {
        printf("[sf64-ps3] duplicate VI message received for one retrace\n");
        return SF64_API_FAILED;
    }

    return SF64_API_WORKED;
}

Sf64ApiState sf64_platform_poll_controller(void)
{
    N64ps3ControllerState pad;

    memset(&pad, 0, sizeof(pad));
    n64ps3_pad_init();
    if (n64ps3_pad_read(0, &pad) != 0) {
        return SF64_API_FAILED;
    }

    printf("[sf64-ps3] controller port=0 buttons=0x%04x stick=(%d,%d)\n",
           pad.buttons,
           pad.stick_x,
           pad.stick_y);
    return SF64_API_INCOMPLETE;
}

Sf64ApiState sf64_platform_probe_pi_read(void)
{
    unsigned char byte = 0;

    if (n64ps3_pi_read_rom(0, &byte, (u32)sizeof(byte)) == 0) {
        return SF64_API_WORKED;
    }

    printf("[sf64-ps3] TODO(n64ps3): provide ROM-free PI/asset read smoke path\n");
    return SF64_API_MISSING;
}

Sf64ApiState sf64_platform_submit_dummy_gfx_task(void)
{
    static const unsigned long long fake_display_list[] = {
        0xDEADBEEF00000000ull,
        0x0000000000000000ull
    };
    N64ps3GfxTask task;

    memset(&task, 0, sizeof(task));
    task.kind = N64PS3_GFX_TASK_DISPLAY_LIST;
    task.display_list = fake_display_list;
    task.display_list_size_bytes = (u32)sizeof(fake_display_list);
    task.debug_name = "sf64-ps3 dummy display list";

    n64ps3_gfx_begin_frame();
    n64ps3_gfx_submit_task(&task);
    n64ps3_gfx_end_frame();
    return SF64_API_INCOMPLETE;
}

void sf64_platform_shutdown(void)
{
    n64ps3_runtime_shutdown();
}
