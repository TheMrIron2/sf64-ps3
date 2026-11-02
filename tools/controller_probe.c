#include "sf64_ps3/controller.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    Sf64ControllerState state = { 0 };
    Sf64ControllerSample sample = { 0, 0, 128, 128 };

    sf64_controller_update(&state, &sample);
    assert(state.held == 0 && state.pressed == 0);
    assert(state.stick_x == 0 && state.stick_y == 0);

    sample.digital1 = 0x08;
    sample.digital2 = 0x40;
    sample.left_x = 255;
    sample.left_y = 0;
    sf64_controller_update(&state, &sample);
    assert(state.held == 0x9000 && state.pressed == 0x9000);
    assert(state.stick_x == 60 && state.stick_y == 60);

    sf64_controller_update(&state, &sample);
    assert(state.held == 0x9000 && state.pressed == 0);
    sample.digital2 = 0x20;
    sample.left_x = 0;
    sample.left_y = 255;
    sf64_controller_update(&state, &sample);
    assert(state.held == 0x5000 && state.pressed == 0x4000);
    assert(state.stick_x == -60 && state.stick_y == -60);

    sample.digital1 = 0xf0;
    sample.digital2 = 0;
    sample.left_x = 150;
    sample.left_y = 106;
    sf64_controller_update(&state, &sample);
    assert(state.held == 0x000f && state.pressed == 0x000f);
    assert(state.stick_x == 0 && state.stick_y == 0);

    sf64_controller_update(&state, NULL);
    assert(state.held == 0 && state.pressed == 0);
    assert(state.stick_x == 0 && state.stick_y == 0);
    sf64_controller_update(&state, &sample);
    assert(state.pressed == 0x000f);
    puts("controller mapping and input transitions passed");
    return 0;
}
