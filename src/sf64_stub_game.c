#include "sf64_ps3/platform.h"

#include <stdio.h>

void sf64_stub_game_run_frame(int frame_index)
{
    printf("[sf64-ps3] SF64 stub frame %d: update, build display list, submit task\n", frame_index);
}
