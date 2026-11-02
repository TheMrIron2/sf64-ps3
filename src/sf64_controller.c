#include "sf64_ps3/controller.h"

#include <string.h>

/* Digital bits follow the PSL1GHT ioPadGetData packet. */
static uint16_t sf64_controller_buttons(const Sf64ControllerSample *sample)
{
    uint16_t buttons = 0;

    if (sample->digital1 & 0x08U) buttons |= 0x1000U;
    if (sample->digital1 & 0x10U) buttons |= 0x0008U;
    if (sample->digital1 & 0x20U) buttons |= 0x0001U;
    if (sample->digital1 & 0x40U) buttons |= 0x0004U;
    if (sample->digital1 & 0x80U) buttons |= 0x0002U;
    if (sample->digital2 & 0x40U) buttons |= 0x8000U;
    if (sample->digital2 & 0x20U) buttons |= 0x4000U;
    if (sample->digital2 & 0x80U) buttons |= 0x0004U;
    if (sample->digital2 & 0x10U) buttons |= 0x0002U;
    if (sample->digital2 & 0x04U) buttons |= 0x2000U;
    if (sample->digital2 & 0x08U) buttons |= 0x0010U;
    if (sample->digital2 & 0x01U) buttons |= 0x0020U;
    return buttons;
}

static int8_t sf64_controller_axis(uint8_t raw, int invert)
{
    int centered = (int)raw - 128;
    int value = centered < 0 ? centered * 80 / 128 : centered * 80 / 127;

    if (invert) value = -value;
    if (value >= -16 && value <= 16) return 0;
    value += value > 0 ? -16 : 16;
    if (value > 60) value = 60;
    if (value < -60) value = -60;
    return (int8_t)value;
}

void sf64_controller_update(Sf64ControllerState *state,
                            const Sf64ControllerSample *sample)
{
    uint16_t previous = state->held;

    if (!sample) {
        memset(state, 0, sizeof(*state));
        return;
    }
    state->held = sf64_controller_buttons(sample);
    state->pressed = state->held & (uint16_t)~previous;
    state->stick_x = sf64_controller_axis(sample->left_x, 0);
    state->stick_y = sf64_controller_axis(sample->left_y, 1);
}
