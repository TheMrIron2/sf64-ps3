#include "sf64_ps3/platform.h"
#include "sf64_ps3/port_config.h"

#include <stdio.h>

static int sf64_note_status(const char *name, Sf64ApiState state)
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

    printf("[sf64-ps3] %-28s %s\n", name, label);
    return state == SF64_API_WORKED ? 0 : 1;
}

int main(void)
{
    int issues = 0;
    int frame;

    sf64_platform_print_banner();

    issues += sf64_note_status("n64ps3 init", sf64_platform_init());
    issues += sf64_note_status("OSMesgQueue create", sf64_platform_create_vi_queue());
    issues += sf64_note_status("VI event register", sf64_platform_register_vi_event());

    for (frame = 0; frame < SF64_PS3_SMOKE_FRAME_COUNT; frame++) {
        printf("[sf64-ps3] frame %d begin\n", frame);
        issues += sf64_note_status("VI retrace trigger", sf64_platform_trigger_vi_retrace());
        issues += sf64_note_status("VI message receive", sf64_platform_receive_vi_message());
        issues += sf64_note_status("controller poll", sf64_platform_poll_controller());
        issues += sf64_note_status("dummy gfx task", sf64_platform_submit_dummy_gfx_task());
        sf64_stub_game_run_frame(frame);
    }

    sf64_platform_shutdown();

    if (issues != 0) {
        printf("[sf64-ps3] smoke completed with %d missing/incomplete/failed probes\n", issues);
        return 1;
    }

    printf("[sf64-ps3] smoke completed cleanly\n");
    return 0;
}
