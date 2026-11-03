/* Host software rasterizer for the n64ps3 F3DEX1 triangle stream.
 *
 * Renders a decoded boot/title scene frame to PNG so the renderer output can be
 * inspected without a PS3. It mirrors the RSX contract: clip-space vertices are
 * perspective-divided, the window has Y down, RGB is sqrt(texture * shade) and
 * alpha is texture alpha * shade alpha, with RDP FORCE_BL now covering text.
 *
 * Supported texture formats: RGBA16 and RGBA32 (the formats the decoder keeps).
 *
 * usage: sf64_frame_raster <space|takeoff|team> <frame> [out.png]
 */

#include "n64ps3/gfx.h"
#include "n64ps3/gfx_f3dex1.h"
#include "sf64_ps3/arwing.h"
#include "sf64_ps3/boot.h"
#include "sf64_ps3/title_starfield.h"
#include "sf64_ps3/takeoff.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned char sf64_arwing_segment_data[];
extern const unsigned int sf64_arwing_segment_size;
extern const unsigned char sf64_title_segment_data[];
extern const unsigned int sf64_title_segment_size;
extern const unsigned char sf64_great_fox_segment_data[];
extern const unsigned int sf64_great_fox_segment_size;

#define FB_W 320
#define FB_H 240
#define SS 2 /* supersample factor */

static unsigned char s_color[FB_W * SS * FB_H * SS][3];
static float s_depth[FB_W * SS * FB_H * SS];
static unsigned s_written;

static float s_vtx[3][4];
static float s_invw[3];
static unsigned char s_col[3][4];
static float s_u[3];
static float s_v[3];
static const unsigned char *s_tex;
static unsigned s_tex_bytes;
static unsigned s_tex_w;
static unsigned s_tex_h;
static unsigned s_wrap_s;
static unsigned s_wrap_t;
static unsigned s_blend;

static void clear_fb(void)
{
    unsigned i;

    for (i = 0; i < (unsigned)(FB_W * SS * FB_H * SS); i++) {
        s_color[i][0] = 0;
        s_color[i][1] = 0;
        s_color[i][2] = 0;
        s_depth[i] = 1e30f;
    }
}

static unsigned wrap_coord(int c, unsigned size, unsigned wrap)
{
    int s = (int)size;

    if (wrap == 2) { /* G_TX_CLAMP */
        if (c < 0) return 0;
        if (c >= s) return size - 1;
        return (unsigned)c;
    }
    c %= s;
    if (c < 0) c += s;
    return (unsigned)c;
}

static void sample_texel(float u, float v, float *out)
{
    int x, y;
    unsigned texel;
    float r, g, b, a;

    if (!s_tex || s_tex_w == 0 || s_tex_h == 0) {
        out[0] = out[1] = out[2] = 0.75f;
        out[3] = 1.0f;
        return;
    }
    x = (int)(u * (float)s_tex_w);
    y = (int)(v * (float)s_tex_h);
    x = (int)wrap_coord(x, s_tex_w, s_wrap_s);
    y = (int)wrap_coord(y, s_tex_h, s_wrap_t);

    if (s_tex_bytes == 2) {
        texel = ((unsigned)s_tex[(y * s_tex_w + x) * 2] << 8) |
                s_tex[(y * s_tex_w + x) * 2 + 1];
        r = (float)((texel >> 11) & 0x1f) / 31.0f;
        g = (float)((texel >> 6) & 0x1f) / 31.0f;
        b = (float)((texel >> 1) & 0x1f) / 31.0f;
        a = (texel & 1) ? 1.0f : 0.0f;
    } else {
        const unsigned char *p = s_tex + (y * s_tex_w + x) * 4;
        r = p[0] / 255.0f;
        g = p[1] / 255.0f;
        b = p[2] / 255.0f;
        a = p[3] / 255.0f;
    }
    out[0] = r;
    out[1] = g;
    out[2] = b;
    out[3] = a;
}

static void plot(int x, int y, float z, float r, float g, float b, float a)
{
    unsigned idx;
    unsigned char *dst;

    if (x < 0 || y < 0 || x >= FB_W * SS || y >= FB_H * SS) return;
    idx = (unsigned)y * (FB_W * SS) + (unsigned)x;
    if (z >= s_depth[idx]) return;
    dst = s_color[idx];
    if (s_blend && a < 1.0f) {
        dst[0] = (unsigned char)(r * a * 255.0f + dst[0] * (1.0f - a));
        dst[1] = (unsigned char)(g * a * 255.0f + dst[1] * (1.0f - a));
        dst[2] = (unsigned char)(b * a * 255.0f + dst[2] * (1.0f - a));
    } else {
        dst[0] = (unsigned char)(r * 255.0f);
        dst[1] = (unsigned char)(g * 255.0f);
        dst[2] = (unsigned char)(b * 255.0f);
    }
    s_depth[idx] = z;
    s_written++;
}

static int raster_triangle(void *user, const N64ps3GfxF3dex1Vertex *a,
                           const N64ps3GfxF3dex1Vertex *b,
                           const N64ps3GfxF3dex1Vertex *c)
{
    const N64ps3GfxF3dex1Vertex *src[3];
    float area;
    int i, minx, maxx, miny, maxy;
    (void)user;

    src[0] = a;
    src[1] = b;
    src[2] = c;
    if (a->clip_w <= 0.0f || b->clip_w <= 0.0f || c->clip_w <= 0.0f) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        float invw = 1.0f / src[i]->clip_w;

        s_vtx[i][0] = ((src[i]->clip_x * invw) * 0.5f + 0.5f) * (FB_W * SS);
        s_vtx[i][1] = (0.5f - (src[i]->clip_y * invw) * 0.5f) * (FB_H * SS);
        s_vtx[i][2] = src[i]->clip_z * invw;
        s_invw[i] = invw;
        s_col[i][0] = src[i]->color[0];
        s_col[i][1] = src[i]->color[1];
        s_col[i][2] = src[i]->color[2];
        s_col[i][3] = src[i]->color[3];
        if (src[i]->texture_enabled && src[i]->texture_width != 0U &&
            src[i]->texture_height != 0U) {
            s_u[i] = ((float)src[i]->s * (float)src[i]->texture_scale_s) /
                     (65536.0f * 32.0f * (float)src[i]->texture_width);
            s_v[i] = ((float)src[i]->t * (float)src[i]->texture_scale_t) /
                     (65536.0f * 32.0f * (float)src[i]->texture_height);
        } else {
            s_u[i] = 0.0f;
            s_v[i] = 0.0f;
        }
    }

    area = (s_vtx[1][0] - s_vtx[0][0]) * (s_vtx[2][1] - s_vtx[0][1]) -
           (s_vtx[2][0] - s_vtx[0][0]) * (s_vtx[1][1] - s_vtx[0][1]);
    if (area == 0.0f) {
        return 0;
    }

    s_tex = a->texture_enabled ? a->texture_data : NULL;
    s_tex_bytes = a->texture_bytes_per_texel;
    s_tex_w = a->texture_width;
    s_tex_h = a->texture_height;
    s_wrap_s = a->texture_wrap_s;
    s_wrap_t = a->texture_wrap_t;
    s_blend = a->blend_enabled;

    minx = (int)(s_vtx[0][0] < s_vtx[1][0] ? (s_vtx[0][0] < s_vtx[2][0] ? s_vtx[0][0] : s_vtx[2][0]) : (s_vtx[1][0] < s_vtx[2][0] ? s_vtx[1][0] : s_vtx[2][0]));
    miny = (int)(s_vtx[0][1] < s_vtx[1][1] ? (s_vtx[0][1] < s_vtx[2][1] ? s_vtx[0][1] : s_vtx[2][1]) : (s_vtx[1][1] < s_vtx[2][1] ? s_vtx[1][1] : s_vtx[2][1]));
    maxx = (int)(s_vtx[0][0] > s_vtx[1][0] ? (s_vtx[0][0] > s_vtx[2][0] ? s_vtx[0][0] : s_vtx[2][0]) : (s_vtx[1][0] > s_vtx[2][0] ? s_vtx[1][0] : s_vtx[2][0]));
    maxy = (int)(s_vtx[0][1] > s_vtx[1][1] ? (s_vtx[0][1] > s_vtx[2][1] ? s_vtx[0][1] : s_vtx[2][1]) : (s_vtx[1][1] > s_vtx[2][1] ? s_vtx[1][1] : s_vtx[2][1]));
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx > FB_W * SS - 1) maxx = FB_W * SS - 1;
    if (maxy > FB_H * SS - 1) maxy = FB_H * SS - 1;

    {
        int x, y;

        for (y = miny; y <= maxy; y++) {
            for (x = minx; x <= maxx; x++) {
                float px = (float)x + 0.5f;
                float py = (float)y + 0.5f;
                float w0 = ((s_vtx[1][0] - px) * (s_vtx[2][1] - py) -
                            (s_vtx[2][0] - px) * (s_vtx[1][1] - py)) / area;
                float w1 = ((s_vtx[2][0] - px) * (s_vtx[0][1] - py) -
                            (s_vtx[0][0] - px) * (s_vtx[2][1] - py)) / area;
                float w2 = 1.0f - w0 - w1;
                float invw, u, v, z;
                float tex[4];
                float r, g, b, al;

                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
                invw = w0 * s_invw[0] + w1 * s_invw[1] + w2 * s_invw[2];
                if (invw <= 0.0f) continue;
                z = w0 * s_vtx[0][2] + w1 * s_vtx[1][2] + w2 * s_vtx[2][2];
                u = (w0 * s_u[0] * s_invw[0] + w1 * s_u[1] * s_invw[1] +
                     w2 * s_u[2] * s_invw[2]) / invw;
                v = (w0 * s_v[0] * s_invw[0] + w1 * s_v[1] * s_invw[1] +
                     w2 * s_v[2] * s_invw[2]) / invw;
                r = (w0 * s_col[0][0] * s_invw[0] + w1 * s_col[1][0] * s_invw[1] +
                     w2 * s_col[2][0] * s_invw[2]) / invw / 255.0f;
                g = (w0 * s_col[0][1] * s_invw[0] + w1 * s_col[1][1] * s_invw[1] +
                     w2 * s_col[2][1] * s_invw[2]) / invw / 255.0f;
                b = (w0 * s_col[0][2] * s_invw[0] + w1 * s_col[1][2] * s_invw[1] +
                     w2 * s_col[2][2] * s_invw[2]) / invw / 255.0f;
                al = (w0 * s_col[0][3] * s_invw[0] + w1 * s_col[1][3] * s_invw[1] +
                      w2 * s_col[2][3] * s_invw[2]) / invw / 255.0f;

                sample_texel(u, v, tex);
                if (s_tex) {
                    float rr = tex[0] * r;
                    float gg = tex[1] * g;
                    float bb = tex[2] * b;

                    r = rr > 0.0f ? (float)sqrt((double)rr) : 0.0f;
                    g = gg > 0.0f ? (float)sqrt((double)gg) : 0.0f;
                    b = bb > 0.0f ? (float)sqrt((double)bb) : 0.0f;
                    al *= tex[3];
                }
                plot(x, y, z, r, g, b, al);
            }
        }
    }
    return 0;
}

/* ---- minimal PNG writer (stored deflate blocks) ---- */
static unsigned long crc_table[256];

static void crc_init(void)
{
    unsigned long c;
    int n, k;

    for (n = 0; n < 256; n++) {
        c = (unsigned long)n;
        for (k = 0; k < 8; k++) {
            c = (c & 1) ? 0xedb88320UL ^ (c >> 1) : c >> 1;
        }
        crc_table[n] = c;
    }
}

static unsigned long crc_update(unsigned long crc, const unsigned char *buf,
                                size_t len)
{
    size_t n;

    for (n = 0; n < len; n++) {
        crc = crc_table[(crc ^ buf[n]) & 0xff] ^ (crc >> 8);
    }
    return crc;
}

static void put32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void write_chunk(FILE *f, const char *type, const unsigned char *data,
                        size_t len)
{
    unsigned char hdr[8];
    unsigned long crc;
    unsigned char tail[4];

    put32(hdr, (unsigned long)len);
    memcpy(hdr + 4, type, 4);
    fwrite(hdr, 1, 8, f);
    if (len) fwrite(data, 1, len, f);
    crc = crc_update(0xffffffffUL, (const unsigned char *)type, 4);
    if (len) crc = crc_update(crc, data, len);
    crc ^= 0xffffffffUL;
    put32(tail, crc);
    fwrite(tail, 1, 4, f);
}

static int write_png(const char *path)
{
    static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    unsigned char ihdr[13];
    unsigned w = FB_W * 2;
    unsigned h = FB_H * 2;
    size_t row = 1 + (size_t)w * 3;
    unsigned char *raw = (unsigned char *)calloc(1, row * h);
    unsigned char *z;
    size_t zlen = 0;
    unsigned y, x;
    FILE *f;

    if (!raw) return -1;
    z = (unsigned char *)malloc(row * h + (row * h / 65535 + 2) * 5 + 16);
    if (!z) {
        free(raw);
        return -1;
    }
    for (y = 0; y < h; y++) {
        unsigned char *dst = raw + y * row;

        dst[0] = 0;
        for (x = 0; x < w; x++) {
            unsigned idx = y * FB_W * SS + x;

            dst[1 + x * 3 + 0] = s_color[idx][0];
            dst[1 + x * 3 + 1] = s_color[idx][1];
            dst[1 + x * 3 + 2] = s_color[idx][2];
        }
    }
    z[zlen++] = 0x78;
    z[zlen++] = 0x01;
    {
        size_t pos = 0;
        size_t total = row * h;

        while (pos < total) {
            size_t n = total - pos;

            if (n > 65535) n = 65535;
            z[zlen++] = (pos + n == total) ? 1 : 0;
            z[zlen++] = (unsigned char)(n & 0xff);
            z[zlen++] = (unsigned char)(n >> 8);
            z[zlen++] = (unsigned char)((~n) & 0xff);
            z[zlen++] = (unsigned char)(((~n) >> 8) & 0xff);
            memcpy(z + zlen, raw + pos, n);
            zlen += n;
            pos += n;
        }
    }
    {
        unsigned long a = 1, b = 0;
        size_t i;

        for (i = 0; i < row * h; i++) {
            a = (a + raw[i]) % 65521;
            b = (b + a) % 65521;
        }
        put32(z + zlen, (b << 16) | a);
        zlen += 4;
    }

    put32(ihdr, w);
    put32(ihdr + 4, h);
    ihdr[8] = 8;
    ihdr[9] = 2;
    ihdr[10] = ihdr[11] = ihdr[12] = 0;

    crc_init();
    f = fopen(path, "wb");
    if (!f) {
        free(raw);
        free(z);
        return -1;
    }
    fwrite(sig, 1, 8, f);
    write_chunk(f, "IHDR", ihdr, 13);
    write_chunk(f, "IDAT", z, zlen);
    write_chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(z);
    return 0;
}

static int render_task(N64ps3GfxTask *task, const char *label,
                       const char *path)
{
    N64ps3GfxF3dex1Stats stats;
    N64ps3GfxF3dex1Status status;

    clear_fb();
    s_written = 0;
    memset(&stats, 0, sizeof(stats));
    status = n64ps3_gfx_decode_f3dex1(task, raster_triangle, NULL, &stats);
    if (status != N64PS3_GFX_F3DEX1_OK) {
        printf("%s: decode failed (%d)\n", label, (int)status);
        return 1;
    }
    if (write_png(path) != 0) {
        printf("%s: failed to write %s\n", label, path);
        return 1;
    }
    printf("%s: commands=%u triangles=%u written-dots=%u scissor=%s(%u,%u %ux%u) -> %s\n", label,
           stats.command_count, stats.triangle_count, s_written,
           stats.scissor_valid ? "yes" : "no", stats.scissor_x, stats.scissor_y,
           stats.scissor_width, stats.scissor_height, path);
    return 0;
}

static int render_space(int frames, const char *path)
{
    static uint8_t rdram[SF64_ARWING_RDRAM_REQUIRED_SIZE];
    Sf64SpaceState state;
    N64ps3GfxTask task;
    char label[64];
    int i;

    sf64_space_init(&state);
    for (i = 0; i < frames; i++) {
        sf64_space_update(&state);
    }
    if (sf64_space_build_task(&task, rdram, sizeof(rdram),
                              sf64_arwing_segment_data,
                              sf64_arwing_segment_size,
                              sf64_title_segment_data,
                              sf64_title_segment_size,
                              sf64_great_fox_segment_data,
                              sf64_great_fox_segment_size, &state) != 0) {
        printf("space %d: build failed\n", frames);
        return 1;
    }
    snprintf(label, sizeof(label), "space %d", frames);
    return render_task(&task, label, path);
}

static int render_takeoff(int frames, const char *path)
{
    static uint8_t rdram[SF64_ARWING_RDRAM_REQUIRED_SIZE];
    Sf64TakeoffState state;
    N64ps3GfxTask task;
    char label[64];
    int i;

    sf64_takeoff_init(&state);
    for (i = 0; i < frames; i++) {
        sf64_takeoff_update(&state);
    }
    if (sf64_takeoff_build_task(&task, rdram, sizeof(rdram),
                                sf64_arwing_segment_data,
                                sf64_arwing_segment_size,
                                sf64_title_segment_data,
                                sf64_title_segment_size, &state) != 0) {
        printf("takeoff %d: build failed\n", frames);
        return 1;
    }
    snprintf(label, sizeof(label),
             "takeoff %d eye=(%.1f,%.1f,%.1f) at=(%.1f,%.1f,%.1f) seg=%d w=%.2f",
             frames, state.eye[0], state.eye[1], state.eye[2], state.at[0],
             state.at[1], state.at[2], state.cam_segment, state.cam_weight);
    return render_task(&task, label, path);
}

static int render_opening(int frames, const char *path)
{
    static uint8_t rdram[SF64_OPENING_RDRAM_BYTES];
    static uint8_t stars[SF64_TITLE_STARFIELD_DISPLAY_LIST_CAPACITY];
    Sf64OpeningState opening;
    N64ps3GfxTask task;
    char label[64];
    unsigned stars_size = 0;
    unsigned star_count = 0;
    int i;

    sf64_opening_init(&opening);
    for (i = 0; i <= frames; i++) {
        if (i > 0) {
            sf64_opening_update(&opening);
        }
        if ((opening.phase == 0 || opening.phase == 2) &&
            sf64_title_starfield_build_opening(stars, sizeof(stars),
                                               &stars_size, &star_count,
                                               opening.eye, opening.at,
                                               opening.star_speed) != 0) {
            printf("opening %d: starfield build failed\n", frames);
            return 1;
        }
        /* Build every frame: the sun glare advances its own counters here. */
        if (sf64_opening_build_task(&task, rdram, sizeof(rdram),
                                    sf64_title_segment_data,
                                    sf64_title_segment_size, stars,
                                    stars_size, &opening,
                                    sf64_great_fox_segment_data,
                                    sf64_great_fox_segment_size) != 0) {
            printf("opening %d: build failed\n", frames);
            return 1;
        }
    }
    snprintf(label, sizeof(label), "opening %d (logo=%d sun=%d slide=%.0f)",
             frames, opening.logo_state, opening.show_sun, opening.n64_slide);
    return render_task(&task, label, path);
}

static int render_team(int frames, const char *path)
{
    static uint8_t rdram[SF64_ARWING_RDRAM_REQUIRED_SIZE];
    Sf64OpeningState opening;
    N64ps3GfxTask task;
    char label[64];
    unsigned f = 0;
    float fade;
    int i;

    sf64_opening_init(&opening);
    for (; f < 4000 && opening.phase == 0; f++) {
        sf64_opening_update(&opening);
    }
    for (; f < 4000 && opening.team_z_offset != 0.0f; f++) {
        sf64_opening_update(&opening);
    }
    for (i = 0; i < frames; i++) {
        sf64_opening_update(&opening);
    }
    fade = 1.0f - (float)opening.fill_alpha / 255.0f;
    if (sf64_team_running_build(&task, rdram, sizeof(rdram),
                                sf64_title_segment_data, sf64_title_segment_size,
                                opening.eye, opening.at, opening.up,
                                opening.team_z_offset, opening.passage_offset,
                                fade, opening.team_light_dir,
                                opening.team_ambient, opening.team_frames,
                                opening.team_name_visible,
                                opening.team_name_index) != 0) {
        printf("team %d: build failed\n", frames);
        return 1;
    }
    snprintf(label, sizeof(label), "team %d", frames);
    printf("team light dir=(%.1f,%.1f,%.1f) ambient=(%.0f,%.0f,%.0f)\n",
           opening.team_light_dir[0], opening.team_light_dir[1],
           opening.team_light_dir[2], opening.team_ambient[0],
           opening.team_ambient[1], opening.team_ambient[2]);
    return render_task(&task, label, path);
}

int main(int argc, char **argv)
{
    int frames;
    char path[512];

    if (argc < 3) {
        printf("usage: %s <opening|space|takeoff|team> <frame> [out.png]\n", argv[0]);
        return 2;
    }
    frames = atoi(argv[2]);
    if (argc > 3) {
        snprintf(path, sizeof(path), "%s", argv[3]);
    } else {
        snprintf(path, sizeof(path), "frame_%s_%d.png", argv[1], frames);
    }

    if (strcmp(argv[1], "opening") == 0) {
        return render_opening(frames, path);
    }
    if (strcmp(argv[1], "space") == 0) {
        return render_space(frames, path);
    }
    if (strcmp(argv[1], "takeoff") == 0) {
        return render_takeoff(frames, path);
    }
    if (strcmp(argv[1], "team") == 0) {
        return render_team(frames, path);
    }
    printf("unknown scene '%s'\n", argv[1]);
    return 2;
}
