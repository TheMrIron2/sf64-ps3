#include "sf64_ps3/title_starfield.h"

#include "global.h"

#define SF64_GFX_COMMAND_BYTES 8U
#define SF64_GFX_PIPESYNC 0xe7000000U
#define SF64_GFX_SETFILLCOLOR 0xf7000000U
#define SF64_GFX_FILLRECT 0xf6000000U
#define SF64_GFX_ENDDL 0xb8000000U

extern void sf64_original_title_starfield_init(void);
extern void sf64_original_title_starfield_update(void);
extern void sf64_original_title_starfield_draw(void);

Gfx *gMasterDisp;

static unsigned char *s_output;
static unsigned int s_output_capacity;
static unsigned int s_output_size;
static unsigned int s_star_count;
static int s_output_overflow;
static int s_original_initialized;
static Gfx s_marker_command;

static void sf64_title_starfield_write_word(unsigned char *output,
                                            unsigned int value)
{
    output[0] = (unsigned char)(value >> 24);
    output[1] = (unsigned char)(value >> 16);
    output[2] = (unsigned char)(value >> 8);
    output[3] = (unsigned char)value;
}

static void sf64_title_starfield_append(unsigned int word0,
                                        unsigned int word1)
{
    unsigned char *command;

    if (s_output_overflow ||
        s_output_size > s_output_capacity - SF64_GFX_COMMAND_BYTES) {
        s_output_overflow = 1;
        return;
    }

    command = s_output + s_output_size;
    sf64_title_starfield_write_word(command, word0);
    sf64_title_starfield_write_word(command + 4U, word1);
    s_output_size += SF64_GFX_COMMAND_BYTES;
}

void PspStarfield_Begin(void)
{
    s_output_size = 0U;
    s_star_count = 0U;
    s_output_overflow = 0;
    sf64_title_starfield_append(SF64_GFX_PIPESYNC, 0U);
}

void PspStarfield_Add(u16 source_index,
                      s16 x,
                      s16 y,
                      u32 n64_fill_color)
{
    unsigned int x_pixel = (unsigned int)(unsigned short)x;
    unsigned int y_pixel = (unsigned int)(unsigned short)y;
    unsigned int rectangle = SF64_GFX_FILLRECT |
                             (x_pixel << 14) |
                             (y_pixel << 2);

    (void)source_index;
    sf64_title_starfield_append(SF64_GFX_PIPESYNC, 0U);
    sf64_title_starfield_append(SF64_GFX_SETFILLCOLOR,
                                (unsigned int)n64_fill_color);
    sf64_title_starfield_append(rectangle,
                                (x_pixel << 14) | (y_pixel << 2));
    s_star_count++;
}

void PspStarfield_End(void)
{
    sf64_title_starfield_append(SF64_GFX_PIPESYNC, 0U);
    sf64_title_starfield_append(SF64_GFX_ENDDL, 0U);
}

int sf64_title_starfield_init(void)
{
    if (!s_original_initialized) {
        sf64_original_title_starfield_init();
        s_original_initialized = 1;
    }
    return 0;
}

static int sf64_title_starfield_build_internal(unsigned char *output,
                                               unsigned int output_capacity,
                                               unsigned int *output_size,
                                               unsigned int *star_count,
                                               int update_state)
{
    if (!output || !output_size || !star_count ||
        output_capacity > SF64_TITLE_STARFIELD_DISPLAY_LIST_CAPACITY ||
        output_capacity < 2U * SF64_GFX_COMMAND_BYTES) {
        return -1;
    }

    s_output = output;
    s_output_capacity = output_capacity;
    s_output_size = 0U;
    s_star_count = 0U;
    s_output_overflow = 0;
    gMasterDisp = &s_marker_command;

    if (sf64_title_starfield_init() != 0) {
        return -1;
    }
    if (update_state) {
        sf64_original_title_starfield_update();
    } else {
        sf64_original_title_starfield_draw();
    }

    s_output = NULL;
    s_output_capacity = 0U;
    if (s_output_overflow || s_output_size == 0U) {
        return -1;
    }

    *output_size = s_output_size;
    *star_count = s_star_count;
    return 0;
}

int sf64_title_starfield_build(unsigned char *output,
                               unsigned int output_capacity,
                               unsigned int *output_size,
                               unsigned int *star_count)
{
    return sf64_title_starfield_build_internal(output, output_capacity,
                                               output_size, star_count, 1);
}

int sf64_title_starfield_build_current(unsigned char *output,
                                       unsigned int output_capacity,
                                       unsigned int *output_size,
                                       unsigned int *star_count)
{
    return sf64_title_starfield_build_internal(output, output_capacity,
                                               output_size, star_count, 0);
}
