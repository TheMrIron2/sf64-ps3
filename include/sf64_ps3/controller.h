#ifndef SF64_PS3_CONTROLLER_H
#define SF64_PS3_CONTROLLER_H

#include <stdint.h>

typedef struct Sf64ControllerSample {
    uint16_t digital1;
    uint16_t digital2;
    uint8_t left_x;
    uint8_t left_y;
} Sf64ControllerSample;

typedef struct Sf64ControllerState {
    uint16_t held;
    uint16_t pressed;
    int8_t stick_x;
    int8_t stick_y;
} Sf64ControllerState;

void sf64_controller_update(Sf64ControllerState *state,
                            const Sf64ControllerSample *sample);

#endif
