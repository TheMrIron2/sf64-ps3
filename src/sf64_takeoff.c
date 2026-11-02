#include "sf64_ps3/takeoff.h"

#include <math.h>
#include <string.h>

#define SF64_TAKEOFF_TWO_PI 6.28318530717958647692f
#define SF64_TAKEOFF_DTOR (SF64_TAKEOFF_TWO_PI / 360.0f)
#define SF64_TAKEOFF_RTOD (360.0f / SF64_TAKEOFF_TWO_PI)
#define SF64_TAKEOFF_SQ(x) ((x) * (x))
#define SF64_TAKEOFF_CUBE(x) ((x) * (x) * (x))

typedef struct Sf64TakeoffVec3 {
    float x;
    float y;
    float z;
} Sf64TakeoffVec3;

typedef struct Sf64TakeoffCameraPoint {
    Sf64TakeoffVec3 eye;
    Sf64TakeoffVec3 at;
} Sf64TakeoffCameraPoint;

/* Original D_menu_801B6C60 camera path. */
static const Sf64TakeoffCameraPoint s_takeoff_camera[10] = {
    { { 230.0f, -20.0f, -0.000004f }, { 90.0f, -20.0f, 0.000002f } },
    { { 217.92657f, 45.0272f, -0.006961f }, { 96.70746f, -25.01511f, 0.003867f } },
    { { 161.75992f, 66.4626f, 50.403748f }, { 73.1124f, -6.102585f, -28.316711f } },
    { { 117.362366f, 2.619884f, 27.208603f }, { 84.39606f, -4.366472f, -45.34767f } },
    { { 19.168537f, 2.445203f, 27.57978f }, { 7.556181f, -4.366434f, -49.249607f } },
    { { -116.45978f, 1.270139f, 27.862812f }, { -109.60754f, -2.268106f, -49.75502f } },
    { { -117.43788f, 1.272571f, 27.864624f }, { -110.646645f, -2.272449f, -49.758255f } },
    { { -118.43771f, 1.314066f, 27.862713f }, { -111.646935f, -2.346546f, -49.754845f } },
    { { -118.44501f, 1.314066f, 27.862074f }, { -111.63391f, -2.346546f, -49.753704f } },
    { { -118.46446f, 1.314066f, 27.860361f }, { -111.59918f, -2.346546f, -49.750645f } },
};

static float takeoff_smooth_step(float *value, float target, float scale,
                                 float max_step, float min_step)
{
    float step = target - *value;

    if (step != 0.0f) {
        step *= scale;
        if ((step >= min_step) || (-min_step >= step)) {
            if (step > max_step) {
                step = max_step;
            } else if (step < -max_step) {
                step = -max_step;
            }
            *value += step;
        } else if (step < min_step) {
            step = min_step;
            *value += step;
            if (*value > target) {
                *value = target;
            }
        } else if (step > -min_step) {
            step = -min_step;
            *value += step;
            if (*value < target) {
                *value = target;
            }
        }
    }
    return step;
}

static void takeoff_get_cam_rot(const Sf64TakeoffState *state,
                                float *x_rot,
                                float *y_rot)
{
    float x = state->eye[0] - state->at[0];
    float y = state->eye[1] - state->at[1];
    float z = state->eye[2] - state->at[2];

    *x_rot = -atan2f(y, sqrtf(SF64_TAKEOFF_SQ(x) + SF64_TAKEOFF_SQ(z))) *
             SF64_TAKEOFF_RTOD;
    *y_rot = atan2f(x, z) * SF64_TAKEOFF_RTOD;
}

static void takeoff_set_cam_up(Sf64TakeoffState *state,
                               float x_rot,
                               float y_rot)
{
    float x = x_rot * SF64_TAKEOFF_DTOR;
    float y = y_rot * SF64_TAKEOFF_DTOR;

    state->up[0] = sinf(y) * sinf(x);
    state->up[1] = cosf(x);
    state->up[2] = cosf(y) * sinf(x);
}

static void takeoff_set_cam_up2(Sf64TakeoffState *state,
                                float translate_x,
                                float translate_y,
                                float translate_z,
                                float eye_distance,
                                float at_distance,
                                float x_rot,
                                float y_rot)
{
    float x = x_rot * SF64_TAKEOFF_DTOR;
    float y = y_rot * SF64_TAKEOFF_DTOR;
    float cosine_x = cosf(x);
    float sine_x = sinf(x);
    float cosine_y = cosf(y);
    float sine_y = sinf(y);
    float direction[3];

    direction[0] = sine_y * cosine_x;
    direction[1] = -sine_x;
    direction[2] = cosine_y * cosine_x;

    state->eye[0] = translate_x + eye_distance * direction[0];
    state->eye[1] = translate_y + eye_distance * direction[1];
    state->eye[2] = translate_z + eye_distance * direction[2];
    state->at[0] = translate_x + at_distance * direction[0];
    state->at[1] = translate_y + at_distance * direction[1];
    state->at[2] = translate_z + at_distance * direction[2];

    state->up[0] = sine_y * sine_x;
    state->up[1] = cosine_x;
    state->up[2] = cosine_y * sine_x;
}

static void takeoff_set_light_rot(Sf64TakeoffState *state,
                                  float x_rot,
                                  float y_rot,
                                  float z_source)
{
    float x_rot_target;
    float y_rot_target;
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = z_source;
    float angle;
    float cosine;
    float sine;

    takeoff_get_cam_rot(state, &x_rot_target, &y_rot_target);

    angle = (-x_rot_target) * SF64_TAKEOFF_DTOR;
    cosine = cosf(angle);
    sine = sinf(angle);
    {
        float old_y = dy;
        dy = old_y * cosine - dz * sine;
        dz = old_y * sine + dz * cosine;
    }
    angle = (-y_rot_target) * SF64_TAKEOFF_DTOR;
    cosine = cosf(angle);
    sine = sinf(angle);
    {
        float old_x = dx;
        dx = old_x * cosine + dz * sine;
        dz = -old_x * sine + dz * cosine;
    }
    angle = y_rot * SF64_TAKEOFF_DTOR;
    cosine = cosf(angle);
    sine = sinf(angle);
    {
        float old_x = dx;
        dx = old_x * cosine + dz * sine;
        dz = -old_x * sine + dz * cosine;
    }
    angle = x_rot * SF64_TAKEOFF_DTOR;
    cosine = cosf(angle);
    sine = sinf(angle);
    {
        float old_y = dy;
        dy = old_y * cosine - dz * sine;
        dz = old_y * sine + dz * cosine;
    }

    state->light_dir[0] = dx;
    state->light_dir[1] = dy;
    state->light_dir[2] = dz;
}

static void takeoff_camera_calc(Sf64TakeoffVec3 *eye,
                                Sf64TakeoffVec3 *at,
                                const Sf64TakeoffCameraPoint *points,
                                float weight,
                                int mode)
{
    float t1;
    float t2;
    float t3;
    float t4;

    switch (mode) {
        case 0:
            t1 = SF64_TAKEOFF_CUBE(1.0f - weight);
            t2 = 21.0f * SF64_TAKEOFF_CUBE(weight) / 12.0f -
                 9.0f * SF64_TAKEOFF_SQ(weight) / 2.0f + 3.0f * weight;
            t3 = -11.0f * SF64_TAKEOFF_CUBE(weight) / 12.0f +
                 3.0f * SF64_TAKEOFF_SQ(weight) / 2.0f;
            t4 = SF64_TAKEOFF_CUBE(weight) / 6.0f;
            break;
        case 1:
            t1 = SF64_TAKEOFF_CUBE(1.0f - weight) / 4.0f;
            t2 = 7.0f * SF64_TAKEOFF_CUBE(weight) / 12.0f -
                 5.0f * SF64_TAKEOFF_SQ(weight) / 4.0f + weight / 4.0f +
                 7.0f / 12.0f;
            t3 = -SF64_TAKEOFF_CUBE(weight) / 2.0f +
                 SF64_TAKEOFF_SQ(weight) / 2.0f + weight / 2.0f + 1.0f / 6.0f;
            t4 = SF64_TAKEOFF_CUBE(weight) / 6.0f;
            break;
        case 2:
            t1 = SF64_TAKEOFF_CUBE(1.0f - weight) / 6.0f;
            t2 = (0.5f * weight - 1.0f) * SF64_TAKEOFF_SQ(weight) + 2.0f / 3.0f;
            t3 = ((-weight + 1.0f) * weight + 1.0f) * 0.5f * weight + 1.0f / 6.0f;
            t4 = SF64_TAKEOFF_CUBE(weight) / 6.0f;
            break;
        case 3:
            t1 = SF64_TAKEOFF_CUBE(1.0f - weight) / 6.0f;
            t2 = -SF64_TAKEOFF_CUBE(1.0f - weight) / 2.0f +
                 SF64_TAKEOFF_SQ(1.0f - weight) / 2.0f + (1.0f - weight) / 2.0f +
                 1.0f / 6.0f;
            t3 = 7.0f * SF64_TAKEOFF_CUBE(1.0f - weight) / 12.0f -
                 5.0f * SF64_TAKEOFF_SQ(1.0f - weight) / 4.0f +
                 (1.0f - weight) / 4.0f + 7.0f / 12.0f;
            t4 = SF64_TAKEOFF_CUBE(weight) / 4.0f;
            break;
        default:
            t1 = SF64_TAKEOFF_CUBE(1.0f - weight) / 6.0f;
            t2 = -11.0f * SF64_TAKEOFF_CUBE(1.0f - weight) / 12.0f +
                 3.0f * SF64_TAKEOFF_SQ(1.0f - weight) / 2.0f;
            t3 = 21.0f * SF64_TAKEOFF_CUBE(1.0f - weight) / 12.0f -
                 9.0f * SF64_TAKEOFF_SQ(1.0f - weight) / 2.0f +
                 3.0f * (1.0f - weight);
            t4 = SF64_TAKEOFF_CUBE(weight);
            break;
    }

    eye->x = t1 * points[0].eye.x + t2 * points[1].eye.x +
             t3 * points[2].eye.x + t4 * points[3].eye.x;
    eye->y = t1 * points[0].eye.y + t2 * points[1].eye.y +
             t3 * points[2].eye.y + t4 * points[3].eye.y;
    eye->z = t1 * points[0].eye.z + t2 * points[1].eye.z +
             t3 * points[2].eye.z + t4 * points[3].eye.z;
    at->x = t1 * points[0].at.x + t2 * points[1].at.x +
            t3 * points[2].at.x + t4 * points[3].at.x;
    at->y = t1 * points[0].at.y + t2 * points[1].at.y +
            t3 * points[2].at.y + t4 * points[3].at.y;
    at->z = t1 * points[0].at.z + t2 * points[1].at.z +
            t3 * points[2].at.z + t4 * points[3].at.z;
}

static void takeoff_cutscene_set_camera(Sf64TakeoffState *state, int count)
{
    Sf64TakeoffVec3 eye;
    Sf64TakeoffVec3 at;
    int mode;

    if (state->cam_segment == -1) {
        return;
    }

    if (state->cam_segment == 0) {
        mode = 0;
    } else if (state->cam_segment == 1) {
        mode = 1;
        if (count == 5) {
            mode = 4;
        }
    } else if (count == state->cam_segment + 5) {
        mode = 3;
    } else if (count == state->cam_segment + 4) {
        mode = 4;
    } else {
        mode = 2;
    }

    takeoff_camera_calc(&eye, &at, &s_takeoff_camera[state->cam_segment],
                        state->cam_weight, mode);
    state->eye[0] = eye.x;
    state->eye[1] = eye.y;
    state->eye[2] = eye.z;
    state->at[0] = at.x;
    state->at[1] = at.y;
    state->at[2] = at.z;

    state->cam_weight += state->cam_speed;
    if (state->cam_weight > 1.0f) {
        state->cam_segment++;
        state->cam_weight = 0.0f;
        if ((count - 4) < state->cam_segment) {
            state->cam_segment = -1;
        }
    }
}

static void takeoff_setup(Sf64TakeoffState *state)
{
    float x = -120.0f;
    int i;

    state->platform_visible = 1;
    state->fill_alpha = 0;
    state->fill_red = 255;
    state->fill_green = 255;
    state->fill_blue = 255;
    state->star_count = 0;
    state->card_state = 0;

    state->light_x = -360.0f;
    state->light_y = 131.0f;
    state->light1_r = 90;
    state->light1_g = 75;
    state->light1_b = 90;
    state->ambient_r = 45;
    state->ambient_g = 34;
    state->ambient_b = 56;

    state->deck_offset_y = 84.0f;
    state->deck_offset_z = -20.0f;
    state->deck_shadow = -33.0f;
    state->deck_launcher_zpos = 25.0f;

    for (i = 0; i < 4; i++) {
        state->arwing[i].position[0] = x;
        state->arwing[i].position[1] = 0.0f;
        state->arwing[i].position[2] = 0.0f;
        state->arwing[i].scale = 0.5f;
        state->arwing[i].rotation_degrees[0] = 0.0f;
        state->arwing[i].rotation_degrees[1] = 0.0f;
        state->arwing[i].rotation_degrees[2] = 0.0f;
        state->arwing[i].cockpit_glass_xrot = -60.0f;
        state->arwing[i].laser_guns_ypos = -10.0f;
        state->arwing[i].vel = 0.05f;
        state->arwing[i].draw = 1;
        state->arwing[i].draw_shadow = 1;
        state->arwing[i].timer = 0;
        state->arwing[i].engine_glow = 0;
        state->arwing[i].engine_particle = 0;
        state->arwing[i].engine_glow_scale = 0.0f;
        x += 75.0f;
    }

    state->cam_x = 140.0f;
    state->cam_y = -20.0f;
    state->cam_z = 0.0f;
    state->cam_eye_dist = 90.0f;
    state->cam_at_dist = -50.0f;
    state->cam_xrot = 0.0f;
    state->cam_yrot = 90.0f;
    takeoff_set_cam_up2(state, state->cam_x, state->cam_y, state->cam_z,
                        state->cam_eye_dist, state->cam_at_dist,
                        state->cam_xrot, state->cam_yrot);
}

void sf64_takeoff_init(Sf64TakeoffState *state)
{
    if (!state) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->scene_state = 0;
    state->cam_segment = 0;
    state->cam_weight = 0.0f;
}

void sf64_takeoff_update(Sf64TakeoffState *state)
{
    int i;

    if (!state || state->finished) {
        return;
    }
    if (state->timer1 > 0) {
        state->timer1--;
    }

    switch (state->scene_state) {
        case 0:
            takeoff_setup(state);
            state->timer1 = 30;
            state->arwing[3].timer = 70;  /* SLIPPY */
            state->arwing[2].timer = 102; /* PEPPY */
            state->arwing[1].timer = 132; /* FALCO */
            state->arwing[0].timer = 171; /* FOX */
            state->cam_segment = 0;
            state->cam_weight = 0.0f;
            state->cam_speed = 0.024f;
            state->scene_state = 10;
            break;

        case 10:
            if (state->timer1 == 0) {
                state->timer3 = 0;
                state->scene_state = 1;
            }
            break;

        case 1:
            takeoff_get_cam_rot(state, &state->cam_xrot, &state->cam_yrot);
            takeoff_set_cam_up(state, state->cam_xrot, state->cam_yrot);
            takeoff_cutscene_set_camera(state, 10);

            if (state->deck_offset_y > 0.0f) {
                state->deck_offset_y -= 0.41f;
                if (state->deck_offset_y < 0.0f) {
                    state->deck_offset_y = 0.0f;
                }
            }

            if (state->timer3 == 60) {
                state->card_state = 1;
            }

            for (i = 3; i >= 0; i--) {
                if (state->arwing[i].timer > 0) {
                    state->arwing[i].timer--;
                }
                if (state->arwing[i].timer == 0) {
                    takeoff_smooth_step(&state->arwing[i].laser_guns_ypos, 0.0f,
                                        state->arwing[i].vel, 100.0f, 0.01f);
                    takeoff_smooth_step(&state->arwing[i].cockpit_glass_xrot,
                                        0.0f, state->arwing[i].vel, 100.0f,
                                        0.01f);
                    state->arwing[i].vel *= 1.04f;
                }
            }

            if (state->timer3 == 70) {
                state->platform_visible = 0;
                state->arwing[0].draw = 0;
            }
            if (state->timer3 == 80) {
                state->arwing[1].draw = 0;
                for (i = 0; i < 4; i++) {
                    state->arwing[i].draw_shadow = 0;
                }
            }
            if (state->timer3 == 115) {
                state->arwing[0].draw = 1;
                state->arwing[1].draw = 1;
            }
            if (state->timer3 == 226) {
                state->card_state = 0;
                state->cam_x = 105.0f;
                state->cam_y = 0.0f;
                state->cam_z = -12.0f;
                state->cam_xrot = 9.4f;
                state->cam_yrot = 110.0f;
                state->cam_eye_dist = 36.0f;
                state->cam_at_dist = -70.0f;
                takeoff_set_cam_up2(state, state->cam_x, state->cam_y,
                                    state->cam_z, state->cam_eye_dist,
                                    state->cam_at_dist, state->cam_xrot,
                                    state->cam_yrot);
                state->timer3 = 0;
                state->arwing[3].engine_glow_scale = 0.0f;
                state->light_x = 60.0f;
                state->light_y = 0.0f;
                state->light1_r = 60.0f;
                state->light1_g = 45.0f;
                state->light1_b = 60.0f;
                state->ambient_r = 35;
                state->ambient_g = 24;
                state->ambient_b = 46;
                state->scene_state = 2;
            }
            state->timer3++;
            break;

        case 2:
            if (state->timer3 == 30) {
                state->arwing[3].engine_particle = 1;
            }
            if (state->timer3 == 40) {
                state->arwing[3].engine_glow = 1;
            }
            if (state->timer3 > 40) {
                state->arwing[3].engine_glow_scale += 0.002f;
                if (state->arwing[3].engine_glow_scale > 0.2f) {
                    state->arwing[3].engine_glow_scale = 0.2f;
                }
                takeoff_smooth_step(&state->light1_r, 240.0f, 0.06f, 2.0f,
                                    0.01f);
                takeoff_smooth_step(&state->light1_g, 240.0f, 0.06f, 2.0f,
                                    0.01f);
                takeoff_smooth_step(&state->light1_b, 255.0f, 0.06f, 2.0f,
                                    0.01f);
            }
            if (state->timer3 > 60 && state->fill_alpha < 255) {
                state->fill_alpha++;
            }
            if (state->fill_alpha == 128) {
                state->timer1 = 7;
                state->arwing[3].engine_glow = 0;
                state->arwing[3].engine_particle = 0;
                state->fill_alpha = 255;
                state->scene_state = 3;
            }
            if (state->timer3 == 50) {
                state->arwing[0].draw = 0;
            }
            if (state->timer3 == 60) {
                state->arwing[1].draw = 0;
            }
            takeoff_smooth_step(&state->cam_eye_dist, 16.0f, 0.01f, 100.0f,
                                0.01f);
            takeoff_smooth_step(&state->cam_yrot, 138.0f, 0.01f, 100.0f,
                                0.01f);
            takeoff_set_cam_up2(state, state->cam_x, state->cam_y,
                                state->cam_z, state->cam_eye_dist,
                                state->cam_at_dist, state->cam_xrot,
                                state->cam_yrot);
            state->timer3++;
            break;

        case 3:
            if (state->timer1 == 0) {
                state->timer1 = 4;
                state->fill_alpha = 0;
                for (i = 0; i < 4; i++) {
                    state->arwing[i].engine_glow = 1;
                    state->arwing[i].engine_glow_scale = 0.9f;
                }
                state->timer3 = 0;
                state->star_count = 800;
                state->scene_state = 4;
            }
            break;

        case 4:
            if (state->timer1 == 0) {
                state->arwing[3].engine_glow_scale = 1.2f;
                takeoff_smooth_step(&state->arwing[3].position[2], 900.0f,
                                    0.06f, 100.0f, 0.0001f);
                takeoff_smooth_step(&state->cam_yrot, 154.0f, 0.2f, 100.0f,
                                    0.01f);
                takeoff_smooth_step(&state->cam_xrot, -5.0f, 0.2f, 100.0f,
                                    0.01f);
                takeoff_smooth_step(&state->cam_x, 100.0f, 0.2f, 100.0f,
                                    0.01f);
                takeoff_smooth_step(&state->cam_eye_dist, 40.0f, 0.2f, 100.0f,
                                    0.01f);
                takeoff_set_cam_up2(state, state->cam_x, state->cam_y,
                                    state->cam_z, state->cam_eye_dist,
                                    state->cam_at_dist, state->cam_xrot,
                                    state->cam_yrot);
                if (state->timer3 > 8) {
                    state->scene_state = 0;
                    state->finished = 1;
                }
                state->timer3++;
            }
            break;

        default:
            break;
    }

    takeoff_set_light_rot(state, state->light_x, state->light_y, 100.0f);
}

/* Original D_menu_801ADF84 camera path for the space fly-by. */
static const Sf64TakeoffCameraPoint s_space_camera[9] = {
    { { -223.5775f, -61.156242f, -26.375984f }, { -53.867554f, -42.139626f, 44.77107f } },
    { { -160.05501f, -67.74775f, 85.750084f }, { -61.486446f, -44.164986f, 29.322315f } },
    { { -22.228004f, -79.54139f, 121.01903f }, { -15.555059f, -64.843475f, 85.51631f } },
    { { 94.76258f, -100.20792f, 96.57968f }, { 56.850273f, -76.324486f, 62.990417f } },
    { { 187.5704f, -114.560425f, 12.510195f }, { 111.428185f, -84.394104f, 8.465982f } },
    { { 244.8392f, -135.20625f, -212.59172f }, { 124.296425f, -87.25594f, -43.656223f } },
    { { 279.38947f, -216.01698f, -401.49118f }, { 122.86145f, -89.64914f, -105.359215f } },
    { { 440.83572f, -305.19196f, -598.5676f }, { 89.57457f, -23.879997f, -4.847023f } },
    { { 481.63818f, -313.2934f, -628.9213f }, { 96.2103f, -5.343582f, -4.23885f } },
};

static void space_get_cam_rot(const Sf64SpaceState *state,
                              float *x_rot,
                              float *y_rot)
{
    float x = state->eye[0] - state->at[0];
    float y = state->eye[1] - state->at[1];
    float z = state->eye[2] - state->at[2];

    *x_rot = -atan2f(y, sqrtf(SF64_TAKEOFF_SQ(x) + SF64_TAKEOFF_SQ(z))) *
             SF64_TAKEOFF_RTOD;
    *y_rot = atan2f(x, z) * SF64_TAKEOFF_RTOD;
}

static void space_set_cam_up(Sf64SpaceState *state, float x_rot, float y_rot)
{
    float x = x_rot * SF64_TAKEOFF_DTOR;
    float y = y_rot * SF64_TAKEOFF_DTOR;

    state->up[0] = sinf(y) * sinf(x);
    state->up[1] = cosf(x);
    state->up[2] = cosf(y) * sinf(x);
}

static void space_set_cam_up2(Sf64SpaceState *state,
                              float translate_x,
                              float translate_y,
                              float translate_z,
                              float eye_distance,
                              float at_distance,
                              float x_rot,
                              float y_rot)
{
    float x = x_rot * SF64_TAKEOFF_DTOR;
    float y = y_rot * SF64_TAKEOFF_DTOR;
    float cosine_x = cosf(x);
    float sine_x = sinf(x);
    float cosine_y = cosf(y);
    float sine_y = sinf(y);
    float direction[3];

    direction[0] = sine_y * cosine_x;
    direction[1] = -sine_x;
    direction[2] = cosine_y * cosine_x;

    state->eye[0] = translate_x + eye_distance * direction[0];
    state->eye[1] = translate_y + eye_distance * direction[1];
    state->eye[2] = translate_z + eye_distance * direction[2];
    state->at[0] = translate_x + at_distance * direction[0];
    state->at[1] = translate_y + at_distance * direction[1];
    state->at[2] = translate_z + at_distance * direction[2];

    state->up[0] = sine_y * sine_x;
    state->up[1] = cosine_x;
    state->up[2] = cosine_y * sine_x;
}

static void space_set_light_rot(Sf64SpaceState *state,
                                float x_rot,
                                float y_rot,
                                float z_source)
{
    float x_rot_target;
    float y_rot_target;
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = z_source;
    float angle;
    float cosine;
    float sine;

    space_get_cam_rot(state, &x_rot_target, &y_rot_target);

    angle = (-x_rot_target) * SF64_TAKEOFF_DTOR;
    cosine = cosf(angle);
    sine = sinf(angle);
    {
        float old_y = dy;
        dy = old_y * cosine - dz * sine;
        dz = old_y * sine + dz * cosine;
    }
    angle = (-y_rot_target) * SF64_TAKEOFF_DTOR;
    cosine = cosf(angle);
    sine = sinf(angle);
    {
        float old_x = dx;
        dx = old_x * cosine + dz * sine;
        dz = -old_x * sine + dz * cosine;
    }
    angle = y_rot * SF64_TAKEOFF_DTOR;
    cosine = cosf(angle);
    sine = sinf(angle);
    {
        float old_x = dx;
        dx = old_x * cosine + dz * sine;
        dz = -old_x * sine + dz * cosine;
    }
    angle = x_rot * SF64_TAKEOFF_DTOR;
    cosine = cosf(angle);
    sine = sinf(angle);
    {
        float old_y = dy;
        dy = old_y * cosine - dz * sine;
        dz = old_y * sine + dz * cosine;
    }

    state->light_dir[0] = dx;
    state->light_dir[1] = dy;
    state->light_dir[2] = dz;
}

static void space_camera_set(Sf64SpaceState *state, int count)
{
    Sf64TakeoffVec3 eye;
    Sf64TakeoffVec3 at;
    int mode;

    if (state->cam_segment == -1) {
        return;
    }

    if (state->cam_segment == 0) {
        mode = 0;
    } else if (state->cam_segment == 1) {
        mode = 1;
        if (count == 5) {
            mode = 4;
        }
    } else if (count == state->cam_segment + 5) {
        mode = 3;
    } else if (count == state->cam_segment + 4) {
        mode = 4;
    } else {
        mode = 2;
    }

    takeoff_camera_calc(&eye, &at, &s_space_camera[state->cam_segment],
                        state->cam_weight, mode);
    state->eye[0] = eye.x;
    state->eye[1] = eye.y;
    state->eye[2] = eye.z;
    state->at[0] = at.x;
    state->at[1] = at.y;
    state->at[2] = at.z;

    state->cam_weight += state->cam_speed;
    if (state->cam_weight > 1.0f) {
        state->cam_segment++;
        state->cam_weight = 0.0f;
        if ((count - 4) < state->cam_segment) {
            state->cam_segment = -1;
        }
    }
}

static void space_setup(Sf64SpaceState *state)
{
    int i;

    state->star_count = 1;
    state->fill_alpha = 0;
    state->fill_mode = 255;
    state->corneria_position[0] = 130.0f;
    state->corneria_position[1] = -30.0f;
    state->corneria_position[2] = 2000.0f;
    state->corneria_scale = 8.0f;
    state->corneria_draw = 0;

    state->great_fox_position[0] = 0.0f;
    state->great_fox_position[1] = 0.0f;
    state->great_fox_position[2] = 0.0f;
    state->great_fox_rotation[0] = 0.0f;
    state->great_fox_rotation[1] = 0.0f;
    state->great_fox_rotation[2] = 0.0f;
    state->great_fox_scale = 0.1f;

    state->light1_r = 101;
    state->light1_g = 106;
    state->light1_b = 92;
    state->ambient_r = 3;
    state->ambient_g = 4;
    state->ambient_b = 10;

    for (i = 0; i < 4; i++) {
        state->arwing[i].position[0] = 0.0f;
        state->arwing[i].position[1] = -40.0f;
        state->arwing[i].position[2] = -30.0f;
        state->arwing[i].scale = 0.1f;
        state->arwing[i].rotation_degrees[0] = 0.0f;
        state->arwing[i].rotation_degrees[1] = 0.0f;
        state->arwing[i].rotation_degrees[2] = 0.0f;
        state->arwing[i].cockpit_glass_xrot = 0.0f;
        state->arwing[i].laser_guns_ypos = 0.0f;
        state->arwing[i].vel = 0.05f;
        state->arwing[i].timer = 0;
        state->arwing[i].draw = 0;
        state->arwing[i].draw_shadow = 0;
    }

    state->cam_weight = 0.0f;
    state->cam_segment = 0;
    space_set_cam_up2(state, -8.0f, -37.0f, 64.0f, 235.0f, 50.0f, 5.9f,
                      -112.74477f);
}

void sf64_space_init(Sf64SpaceState *state)
{
    if (!state) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->scene_state = 0;
    state->cam_segment = 0;
    state->cam_weight = 0.0f;
}

void sf64_space_update(Sf64SpaceState *state)
{
    static const float target_x[4] = { -15.0f, -5.0f, 5.0f, 10.0f };
    static const float target_y[4] = { -50.0f, -45.0f, -55.0f, -60.0f };
    static const float target_z[4] = { 265.0f, 260.0f, 255.0f, 230.0f };
    int i;

    if (!state || state->finished) {
        return;
    }
    if (state->timer1 > 0) {
        state->timer1--;
    }

    switch (state->scene_state) {
        case 0:
            space_setup(state);
            state->timer3 = 0;
            state->corneria_draw = 1;
            state->arwing[0].timer = 3;
            state->arwing[1].timer = 23;
            state->arwing[2].timer = 43;
            state->arwing[3].timer = 63;
            state->cam_segment = 0;
            state->cam_weight = 0.0f;
            state->cam_speed = 0.042f;
            state->scene_state = 1;
            break;

        case 1:
            for (i = 0; i < 4; i++) {
                if (state->arwing[i].timer > 0) {
                    state->arwing[i].timer--;
                } else {
                    float x;
                    float y;
                    float z;
                    if (state->arwing[i].timer == 0) {
                        state->arwing[i].timer = -1;
                        state->arwing[i].draw = 1;
                    }
                    takeoff_smooth_step(&state->arwing[i].position[1],
                                        target_y[i], state->arwing[i].vel,
                                        100.0f, 0.001f);
                    takeoff_smooth_step(&state->arwing[i].position[0],
                                        target_x[i], state->arwing[i].vel,
                                        100.0f, 0.001f);
                    takeoff_smooth_step(&state->arwing[i].position[2],
                                        target_z[i], state->arwing[i].vel,
                                        100.0f, 0.001f);
                    state->arwing[i].vel *= 1.05f;

                    if (state->arwing[i].position[2] == target_z[i]) {
                        state->arwing[i].scale -= 0.002f;
                        if (state->arwing[i].scale < 0.0f) {
                            state->arwing[i].scale = 0.0f;
                            state->arwing[i].draw = 0;
                        }
                    }

                    x = target_x[i] - state->arwing[i].position[0];
                    y = target_y[i] - state->arwing[i].position[1];
                    z = target_z[i] + 10.0f - state->arwing[i].position[2];
                    state->arwing[i].rotation_degrees[0] =
                        SF64_TAKEOFF_RTOD *
                        -atan2f(y, sqrtf(SF64_TAKEOFF_SQ(x) +
                                         SF64_TAKEOFF_SQ(z)));
                    state->arwing[i].rotation_degrees[1] =
                        SF64_TAKEOFF_RTOD * atan2f(x, z);
                }
            }

            if (state->timer3 == 125) {
                state->fill_mode = 1;
            }
            if (state->fill_alpha == 255) {
                state->finished = 1;
                break;
            }
            if (state->fill_mode == 1) {
                state->fill_alpha += 15;
                if (state->fill_alpha > 255) {
                    state->fill_alpha = 255;
                    state->fill_mode = 255;
                }
            }
            state->timer3++;
            break;

        default:
            break;
    }

    {
        float x_rot;
        float y_rot;
        space_get_cam_rot(state, &x_rot, &y_rot);
        space_set_cam_up(state, x_rot, y_rot);
    }
    space_camera_set(state, 9);
    space_set_light_rot(state, 0.0f, 0.0f, 100.0f);
}
