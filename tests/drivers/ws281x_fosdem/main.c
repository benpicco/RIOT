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

/* Choose a text and color */
static const char *TEXT = " RIOT + WS281x  ";
static const color_rgb_t FG = { .r = 0x80, .g = 0x80, .b = 0xFF };  /* bluish */
static const color_rgb_t BG = { .r = 0x00, .g = 0x00, .b = 0x00 };  /* off */

/* WS281x device */
static ws281x_t leds;

/* Map (x,y) -> LED index in the 1D chain */
static inline uint16_t map_xy_to_index(unsigned x, unsigned y)
{
    if (x >= MAT_W || y >= MAT_H) {
        return 0; /* shouldn’t happen */
    }
#if SERPENTINE
    /* Even rows left->right, odd rows right->left */
    if (y & 1) {
        return (y * MAT_W) + (MAT_W - 1 - x);
    }
    else {
        return (y * MAT_W) + x;
    }
#else
    /* Simple left->right, top->bottom */
    return (y * MAT_W) + x;
#endif
}

/* Set a pixel in the internal WS281x buffer */
static inline void set_px(unsigned x, unsigned y, color_rgb_t c)
{
    uint16_t idx = map_xy_to_index(x, y);
    if (idx < leds.params.numof) {
        ws281x_set(&leds, idx, c);
    }
}

/* Clear whole matrix to background color */
static void clear_matrix(void)
{
    for (unsigned y = 0; y < MAT_H; y++) {
        for (unsigned x = 0; x < MAT_W; x++) {
            set_px(x, y, BG);
        }
    }
}

/* Draw a 5x5 Mineplex glyph at (x0,y0).
   According to the docs, mineplex_char() returns 5 bytes (rows top->bottom),
   and each row uses the LSB 5 bits, with bit 0 == leftmost pixel. */
static void draw_glyph_mineplex(int x0, int y0, char ch, color_rgb_t col)
{
    const uint8_t *g = mineplex_char(ch);
    for (int row = 0; row < (int)MINEPLEX_CHAR_H; row++) {
        uint8_t rbits = g[row];
        for (int col_x = 0; col_x < (int)MINEPLEX_CHAR_W; col_x++) {
            if (rbits & (1u << col_x)) {
                int x = x0 + col_x;
                int y = y0 + row;
                if (x >= 0 && x < (int)MAT_W && y >= 0 && y < (int)MAT_H) {
                    set_px((unsigned)x, (unsigned)y, col);
                }
            }
        }
    }
}

/* Draw a string using Mineplex; add 1px spacing after each char */
static void draw_text(int x, int y, const char *s, color_rgb_t col)
{
    int cursor = x;
    while (*s) {
        draw_glyph_mineplex(cursor, y, *s++, col);
        cursor += (int)MINEPLEX_CHAR_W + 1;
        if (cursor >= (int)MAT_W) {
            break; /* off the right edge */
        }
    }
}

int main(void)
{
    /* ---- Initialize WS281x with overrides from ws281x_params.h ----
       We’ll override the LED count to 32x8 in the Makefile.
       The buffer comes from ws281x_params (or the default).
    */
    ws281x_params_t p = ws281x_params[0];
    p.numof = NUM_LEDS;                 /* make sure numof matches matrix */

    if (ws281x_init(&leds, &p) != 0) {
        puts("ws281x_init failed");
        return 1;
    }

    /* Simple scrolling demo */
    const int glyph_w = (int)MINEPLEX_CHAR_W;
    const int spacing = 1;
    const int char_step = glyph_w + spacing;

    /* Total text width in pixels */
    const int text_px = (int)strlen(TEXT) * char_step;

    /* Vertically center the 5-pixel-tall font in an 8-pixel-tall matrix */
    const int y0 = (MAT_H - (int)MINEPLEX_CHAR_H) / 2;

    int offset = MAT_W;  /* start scrolling in from the right edge */
    while (1) {
        clear_matrix();
        /* draw text with its left edge at x = offset */
        draw_text(offset, y0, TEXT, FG);

        /* push frame to LEDs */
        ws281x_write(&leds);

        /* update scroll offset */
        offset--;
        if (offset <= -(text_px)) {
            offset = MAT_W;  /* restart */
        }

        /* adjust speed as you like */
        xtimer_usleep(60 * 1000);  /* ~60ms per step */
    }

    return 0;
}
