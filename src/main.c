#include "sf64_ps3/platform.h"
#include "sf64_ps3/port_config.h"

#include <stdio.h>

#if defined(SF64_PS3_TARGET)
#include <sys/process.h>
#include <sysutil/sysutil.h>

SYS_PROCESS_PARAM(1001, 0x100000);

static int s_running = 1;

static void sf64_sysutil_callback(u64 status, u64 param, void *userdata)
{
    (void)param;
    (void)userdata;
    if (status == SYSUTIL_EXIT_GAME) {
        s_running = 0;
    }
}
#endif

static int sf64_note_status(const char *name, Sf64ApiState state, int required)
{
    const char *label = "unknown";

    switch (state) {
        case SF64_API_WORKED:
            label = "worked";
            break;
        case SF64_API_MISSING:
            label = "missing";
            break;
        case SF64_API_INCOMPLETE:
            label = "incomplete";
            break;
        case SF64_API_FAILED:
            label = "failed";
            break;
    }

    printf("[sf64-ps3] %-28s %s%s\n", name, label, required ? "" : " (optional)");
    return required && state != SF64_API_WORKED ? 1 : 0;
}

int main(void)
{
    int issues = 0;
    int frame;

    sf64_platform_print_banner();

    if (sf64_platform_init() != SF64_API_WORKED) {
        (void)sf64_note_status("n64ps3 init", SF64_API_FAILED, 1);
        sf64_platform_shutdown();
        return 1;
    }
    (void)sf64_note_status("n64ps3 init", SF64_API_WORKED, 1);
#if !defined(SF64_PS3_ARWING_PROBE)
    issues += sf64_note_status("OSMesgQueue create", sf64_platform_create_vi_queue(), 1);
    issues += sf64_note_status("VI event register", sf64_platform_register_vi_event(), 1);
#endif

#if defined(SF64_PS3_TARGET)
    sysUtilRegisterCallback(0, sf64_sysutil_callback, NULL);
    frame = 0;
    while (s_running) {
        sysUtilCheckCallback();
#else
    for (frame = 0; frame < SF64_PS3_SMOKE_FRAME_COUNT; frame++) {
#endif
#if defined(SF64_PS3_ARWING_PROBE)
        if (frame == 0) {
            printf("[sf64-ps3] rendering original Arwing geometry through n64ps3\n");
        }
        issues += sf64_note_status("original Arwing title task",
                                   sf64_platform_submit_f3dex1_gfx_task(), 1);
        if (issues != 0) {
            s_running = 0;
        }
#else
        printf("[sf64-ps3] frame %d begin\n", frame);
        issues += sf64_note_status("VI retrace trigger", sf64_platform_trigger_vi_retrace(), 1);
        issues += sf64_note_status("VI message receive", sf64_platform_receive_vi_message(), 1);
        issues += sf64_note_status("controller poll", sf64_platform_poll_controller(), 0);
        issues += sf64_note_status("PI read", sf64_platform_probe_pi_read(), 0);
        issues += sf64_note_status("F3DEX1 gfx task", sf64_platform_submit_f3dex1_gfx_task(), 1);
        sf64_stub_game_run_frame(frame);
#endif
#if defined(SF64_PS3_TARGET)
        frame++;
#endif
    }

    sf64_platform_shutdown();

    if (issues != 0) {
        printf("[sf64-ps3] smoke completed with %d missing/incomplete/failed probes\n", issues);
        return 1;
    }

    printf("[sf64-ps3] smoke completed cleanly\n");
    return 0;
}
