#ifndef SF64_PS3_PLATFORM_H
#define SF64_PS3_PLATFORM_H

typedef enum Sf64ApiState {
    SF64_API_WORKED = 0,
    SF64_API_MISSING = 1,
    SF64_API_INCOMPLETE = 2,
    SF64_API_FAILED = 3
} Sf64ApiState;

void sf64_platform_print_banner(void);
Sf64ApiState sf64_platform_init(void);
Sf64ApiState sf64_platform_create_vi_queue(void);
Sf64ApiState sf64_platform_register_vi_event(void);
Sf64ApiState sf64_platform_trigger_vi_retrace(void);
Sf64ApiState sf64_platform_receive_vi_message(void);
Sf64ApiState sf64_platform_poll_controller(void);
Sf64ApiState sf64_platform_probe_pi_read(void);
Sf64ApiState sf64_platform_submit_f3dex1_gfx_task(void);
void sf64_platform_shutdown(void);

void sf64_stub_game_run_frame(int frame_index);

#endif
