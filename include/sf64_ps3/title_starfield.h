#ifndef SF64_PS3_TITLE_STARFIELD_H
#define SF64_PS3_TITLE_STARFIELD_H

#define SF64_TITLE_STARFIELD_DISPLAY_LIST_CAPACITY 0x00006000U

/* Keep the original SF64 starfield state across calls and run its camera and
 * background update for each frame. Output is an N64 big-endian display list
 * accepted by n64ps3. */
int sf64_title_starfield_build(unsigned char *output,
                               unsigned int output_capacity,
                               unsigned int *output_size,
                               unsigned int *star_count);

/* Initialize once, then draw after the original Title_Screen_Update ran. */
int sf64_title_starfield_init(void);
int sf64_title_starfield_build_current(unsigned char *output,
                                       unsigned int output_capacity,
                                       unsigned int *output_size,
                                       unsigned int *star_count);

#endif
