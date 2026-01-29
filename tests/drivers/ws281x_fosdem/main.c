#include <stdio.h>
#include <string.h>
#include "xtimer.h"
#include "color.h"
#include "ws281x.h"
#include "ws281x_params.h"
#include "mineplex.h"

/* ---- Matrix configuration ---- */
#define MAT_W          32
#define MAT_H          8
#define NUM_LEDS       (MAT_W * MAT_H)

/* Set to 1 if your matrix is wired "serpentine" (every other row reversed) */
#ifndef SERPENTINE
#define SERPENTINE     1
#endif

/* ---- Text & color ---- */
static const char *TEXT = " RIOT + WS281x  ";
static const color_rgb_t FG = { .r = 0x80, .g = 0x80, .b = 0xFF };  /* bluish */
static const color_rgb_t BG = { .r = 0x00, .g = 0x00, .b = 0x00 };  /* off */

/* ---- Sine wave controls ----
 * amplitude: vertical pixels (try 1..2)
 * wavelength: pixels per full sine period along x
 * speed: LUT index steps per frame (1..4)
 */
#define WAV_AMP_PX     1
#define WAV_LEN_PX     24
#define WAV_SPEED      1

/* ---- 64-step integer sine LUT, scaled to [-127, +127] ----
 * sin(2*pi*n/64)*127 rounded to nearest integer.
 */
#define LUT_LEN 64
static const int8_t SIN_LUT[LUT_LEN] = {
     0,  12,  25,  37,  49,  60,  71,  81,
    90,  98, 106, 112, 117, 122, 125, 126,
   127, 126, 125, 122, 117, 112, 106,  98,
    90,  81,  71,  60,  49,  37,  25,  12,
     0, -12, -25, -37, -49, -60, -71, -81,
   -90, -98,-106,-112,-117,-122,-125,-126,
  -127,-126,-125,-122,-117,-112,-106, -98,
   -90, -81, -71, -60, -49, -37, -25, -12
};

static uint8_t g_phase;  /* 0..63 */

/* WS281x device */
static ws281x_t leds;

/* Map (x,y) -> LED index in the 1D chain */
static inline uint16_t map_xy_to_index(unsigned x, unsigned y)
{
    if (x >= MAT_W || y >= MAT_H) {
        return 0;
    }
#if SERPENTINE
    if (y & 1) {
        return (y * MAT_W) + (MAT_W - 1 - x);
    }
    else {
        return (y * MAT_W) + x;
    }
#else
    return (y * MAT_W) + x;
#endif
}

static inline void set_px(unsigned x, unsigned y, color_rgb_t c)
{
    uint16_t idx = map_xy_to_index(x, y);
    if (idx < leds.params.numof) {
        ws281x_set(&leds, idx, c);
    }
}

static void clear_matrix(void)
{
    for (unsigned y = 0; y < MAT_H; y++) {
        for (unsigned x = 0; x < MAT_W; x++) {
            set_px(x, y, BG);
        }
    }
}

/* Draw a 5x5 Mineplex glyph at (x0,y0).  LSB of each byte is leftmost pixel. */
static void draw_glyph_mineplex(int x0, int y0, char ch, color_rgb_t col)
{
    const uint8_t *g = mineplex_char(ch);
    for (int row = 0; row < (int)MINEPLEX_CHAR_H; row++) {
        uint8_t rbits = g[row];
        for (int col_x = 0; col_x < (int)MINEPLEX_CHAR_W; col_x++) {
            if (rbits & (1u << col_x)) {
                int x = x0 + col_x;
                int y = y0 + row;
                if ((unsigned)x < MAT_W && (unsigned)y < MAT_H) {
                    set_px((unsigned)x, (unsigned)y, col);
                }
            }
        }
    }
}

/* Convert an x-position (in pixels) into a small vertical offset (in pixels)
   using the sine LUT and the current phase. */
static inline int wave_offset_for_x(int xpix)
{
    /* Map x to LUT index via wavelength; add phase; wrap by LUT_LEN */
    unsigned idx = (unsigned)(g_phase + ((xpix * LUT_LEN) / WAV_LEN_PX)) & (LUT_LEN - 1);

    /* Scale [-127..127] to pixel offset with rounding */
    int v = SIN_LUT[idx]; /* -127..127 */
    int off = (WAV_AMP_PX * v + (v >= 0 ? 63 : -63)) / 127;
    return off;
}

/* Draw string with sine-wave vertical motion (per-letter). */
static void draw_text_sine(int x_left, int y_base, const char *s, color_rgb_t col)
{
    const int glyph_w = (int)MINEPLEX_CHAR_W;
    const int spacing = 1;
    const int char_step = glyph_w + spacing;

    int cursor = x_left;
    while (*s) {
        int glyph_center_x = cursor + glyph_w / 2;
        int y = y_base + wave_offset_for_x(glyph_center_x);
        draw_glyph_mineplex(cursor, y, *s++, col);
        cursor += char_step;
        if (cursor >= (int)MAT_W) {
            break;
        }
    }
}

int main(void)
{
    /* Use default ws281x_params, but override the number of LEDs */
    ws281x_params_t p = ws281x_params[0];
    p.numof = NUM_LEDS;

    if (ws281x_init(&leds, &p) != 0) {
        puts("ws281x_init failed");
        return 1;
    }

    const int glyph_w = (int)MINEPLEX_CHAR_W;
    const int spacing = 1;
    const int char_step = glyph_w + spacing;

    const int text_px = (int)strlen(TEXT) * char_step;

    /* center the 5-pixel font vertically in 8 pixels */
    const int y_center = (MAT_H - (int)MINEPLEX_CHAR_H) / 2;

    int scroll_x = MAT_W;  /* start from right edge */

    while (1) {
        clear_matrix();

        /* draw text with sine-wave vertical offsets */
        draw_text_sine(scroll_x, y_center, TEXT, FG);

        /* push frame to LEDs */
        ws281x_write(&leds);

        /* advance horizontal scroll and sine phase */
        scroll_x--;
        if (scroll_x <= -text_px) {
            scroll_x = MAT_W;
        }
        g_phase = (g_phase + WAV_SPEED) & (LUT_LEN - 1);

        /* frame time -> horizontal speed; tune together with WAV_SPEED */
        xtimer_usleep(60 * 1000);
    }

    return 0;
}
