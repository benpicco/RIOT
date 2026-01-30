#include <stdio.h>
#include <string.h>
#include "xtimer.h"
#include "fmt.h"
#include "color.h"
#include "walltime.h"
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

/* ---- Text ---- */
static const char *TEXT = " FOSDEM Beer Event ";

/* ---- Sine wave controls ---- */
#define WAV_AMP_PX     1
#define WAV_LEN_PX     8
#define WAV_SPEED      1

/* ---- 64-step integer sine LUT ---- */
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

static uint8_t g_phase;  /* sine phase 0..63 */

/* ---- Rainbow parameters ---- */
#define HUE_SPEED           2
#define HUE_PER_LETTER     16
#define HUE_PER_COL         8
#define RAINBOW_BRIGHTNESS 200
static uint8_t g_hue_phase;

/* ---- Global brightness pulse (breathing) ---- */
#define BRIGHT_MIN        120
#define BRIGHT_MAX        255
#define PULSE_SPEED         2
static uint8_t g_pulse_phase;

/* WS281x device */
static ws281x_t leds;

/* Map (x,y) -> LED index */
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

/* Clear matrix to off */
static void clear_matrix(void)
{
    color_rgb_t off = {0,0,0};
    for (unsigned y = 0; y < MAT_H; y++) {
        for (unsigned x = 0; x < MAT_W; x++) {
            set_px(x, y, off);
        }
    }
}

static inline int wave_offset_for_x(int xpix)
{
    unsigned idx = (unsigned)(g_phase + ((xpix * LUT_LEN) / WAV_LEN_PX)) & (LUT_LEN - 1);
    int v = SIN_LUT[idx]; /* -127..127 */
    int off = (WAV_AMP_PX * v + (v >= 0 ? 63 : -63)) / 127;
    return off;
}

/* Rainbow wheel hue->RGB */
static inline color_rgb_t wheel(uint8_t pos, uint8_t bright)
{
    uint8_t r, g, b;
    if (pos < 85) {
        r = (uint8_t)(255 - pos * 3);
        g = (uint8_t)(pos * 3);
        b = 0;
    }
    else if (pos < 170) {
        pos -= 85;
        r = 0;
        g = (uint8_t)(255 - pos * 3);
        b = (uint8_t)(pos * 3);
    }
    else {
        pos -= 170;
        r = (uint8_t)(pos * 3);
        g = 0;
        b = (uint8_t)(255 - pos * 3);
    }
    color_rgb_t c;
    c.r = (uint8_t)(((uint16_t)r * bright) >> 8);
    c.g = (uint8_t)(((uint16_t)g * bright) >> 8);
    c.b = (uint8_t)(((uint16_t)b * bright) >> 8);
    return c;
}

/* Current global brightness from pulse */
static inline uint8_t pulse_brightness_now(void)
{
    int v = SIN_LUT[g_pulse_phase];            /* -127..127 */
    int u = v + 127;                           /* 0..254 */
    uint8_t base = (uint8_t)u;                 /* 0..254 */

    unsigned span = (unsigned)(BRIGHT_MAX - BRIGHT_MIN);
    uint8_t scaled = (uint8_t)((base * span + 127) / 255);
    return (uint8_t)(BRIGHT_MIN + scaled);
}

/* Draw a Mineplex glyph with rainbow gradient across columns */
static void draw_glyph_mineplex_rainbow(int x0, int y0, char ch,
                                        uint8_t base_hue, uint8_t cur_bright)
{
    const uint8_t *g = mineplex_char(ch);
    for (int row = 0; row < (int)MINEPLEX_CHAR_H; row++) {
        uint8_t rbits = g[row];
        for (int col_x = 0; col_x < (int)MINEPLEX_CHAR_W; col_x++) {
            if (rbits & (1u << col_x)) {
                int x = x0 + col_x;
                int y = y0 + row;
                if ((unsigned)x < MAT_W && (unsigned)y < MAT_H) {
                    uint8_t h = (uint8_t)(base_hue + (uint8_t)(col_x * HUE_PER_COL));
                    set_px((unsigned)x, (unsigned)y, wheel(h, cur_bright));
                }
            }
        }
    }
}

/* Draw rainbow text riding the sine wave, modulated by the pulse brightness */
static void draw_text_sine_rainbow(int x_left, int y_base, const char *s, uint8_t cur_bright)
{
    const int glyph_w = (int)MINEPLEX_CHAR_W;
    const int spacing = 1;
    const int char_step = glyph_w + spacing;

    int cursor = x_left;
    int letter_index = 0;

    while (*s) {
        int glyph_center_x = cursor + glyph_w / 2;
        int y = y_base + wave_offset_for_x(glyph_center_x);

        uint8_t base_hue = (uint8_t)(g_hue_phase + (uint8_t)(letter_index * HUE_PER_LETTER));
        draw_glyph_mineplex_rainbow(cursor, y, *s++, base_hue, cur_bright);

        cursor += char_step;
        letter_index++;
        if (cursor >= (int)MAT_W) {
            break;
        }
    }
}

#include "sht3x.h"
#include "sht3x_params.h"

int main(void)
{
    ws281x_params_t p = ws281x_params[0];
    p.numof = NUM_LEDS;

    if (ws281x_init(&leds, &p) != 0) {
        puts("ws281x_init failed");
        return 1;
    }

    const int glyph_w = (int)MINEPLEX_CHAR_W;
    const int spacing = 1;
    const int char_step = glyph_w + spacing;
    const int y_center = (MAT_H - (int)MINEPLEX_CHAR_H) / 2;

    gpio_init(BTN0_PIN, BTN0_MODE);
    gpio_init(BTN1_PIN, BTN1_MODE);

    int scroll_x = MAT_W;

    sht3x_dev_t sht3x;
    sht3x_init(&sht3x, &sht3x_params[0]);

    enum {
        MODE_TEXT,
        MODE_TIME,
        MODE_TEMP,
    } op_mode = MODE_TEXT;

    xtimer_ticks32_t last_wakeup = 0;
    char custom_text[64];
    unsigned update_interval = 0;

    while (1) {
        clear_matrix();

        const char *text;

        switch (op_mode) {
        case MODE_TEXT:
            text = TEXT;
            break;
        case MODE_TIME:
            text = custom_text;

            if (update_interval--) {
                break;
            }
            update_interval = 15;

            struct tm now;
            if (!walltime_get(&now, NULL)) {
                snprintf(custom_text, sizeof(custom_text), "%02u:%02u:%02u",
                         now.tm_hour, now.tm_min, now.tm_sec);
            }

            break;
        case MODE_TEMP:
            text = custom_text;

            if (update_interval--) {
                break;
            }
            update_interval = 40;

            int16_t temp;
            int16_t hum;
            sht3x_read(&sht3x, &temp, &hum);
            snprintf(custom_text, sizeof(custom_text), "%u.%02uC %u.%02u%% hum",
                     temp / 100, temp % 100,
                     hum / 100, hum % 100);
            break;
        }

        if (!gpio_read(BTN0_PIN)) {
            op_mode = MODE_TEXT;
        } else if (!gpio_read(BTN1_PIN)) {
            op_mode = MODE_TIME;
            update_interval = 0;
        } else if (!gpio_read(BTN2_PIN)) {
            op_mode = MODE_TEMP;
            update_interval = 0;
        }

        int text_px = (int)strlen(text) * char_step;

        uint8_t cur_bright = pulse_brightness_now();
        draw_text_sine_rainbow(scroll_x, y_center, text, cur_bright);
        ws281x_write(&leds);

        scroll_x--;
        if (scroll_x <= -text_px) {
            scroll_x = MAT_W;
        }
        g_phase       = (uint8_t)((g_phase + WAV_SPEED) & (LUT_LEN - 1));
        g_hue_phase   = (uint8_t)(g_hue_phase + HUE_SPEED);
        g_pulse_phase = (uint8_t)((g_pulse_phase + PULSE_SPEED) & (LUT_LEN - 1));

        xtimer_periodic_wakeup(&last_wakeup, 60 * 1000);
    }
    return 0;
}
