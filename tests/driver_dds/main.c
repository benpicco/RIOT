/*
 * Copyright (C) 2019 Otto-von-Guericke-Universität Magdeburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Test application for the DDS driver
 *
 * @author      Marian Buschsieweke <marian.buschsieweke@ovgu.de>
 *
 * @}
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "shell_commands.h"

#include "dds.h"
#include "dds_params.h"

enum {
    NOTE_C4         = 0,
    NOTE_CIS4       = 1,
    NOTE_DES4       = 1,
    NOTE_D4         = 2,
    NOTE_DIS4       = 3,
    NOTE_ES4        = 3,
    NOTE_E4         = 4,
    NOTE_F4         = 5,
    NOTE_FIS4       = 6,
    NOTE_GES4       = 6,
    NOTE_G4         = 7,
    NOTE_GIS4       = 8,
    NOTE_AS4        = 8,
    NOTE_A4         = 9,
    NOTE_AIS4       = 10,
    NOTE_BB4        = 10,
    NOTE_B4         = 11,
    NOTE_C5         = 12,
    NOTE_CIS5       = 13,
    NOTE_DES5       = 13,
    NOTE_D5         = 14,
    /* entries below not usable in compact format */
    NOTE_DIS5       = 15,
    NOTE_ES5        = 15,
    NOTE_E5         = 16,
    NOTE_F5         = 17,
    NOTE_FIS5       = 18,
    NOTE_GES5       = 18,
    NOTE_G5         = 19,
    NOTE_GIS5       = 20,
    NOTE_AS5        = 20,
    NOTE_A5         = 21,
    NOTE_AIS5       = 22,
    NOTE_BB5        = 22,
    NOTE_B5         = 23,
    NOTE_C6         = 24,
};

enum {
    NOTE_LEN_1      = 0 << 4,
    NOTE_LEN_2      = 1 << 4,
    NOTE_LEN_4      = 2 << 4,
    NOTE_LEN_8      = 3 << 4,
    NOTE_LEN_16     = 4 << 4,
};

enum {
    NOTE_LEN_DOT    = 1 << 7,
    NOTE_PAUSE      = 0xf,
};

static int sc_play(int argc, char **argv);
static int sc_music(int argc, char **argv);

/* One 32 sample sine wave (from 0 to 2 pi) */
static const uint8_t sine[] = {
    0x80, 0x99, 0xb1, 0xc7,  0xdb, 0xea, 0xf6, 0xfe,
    0xff, 0xfe, 0xf6, 0xea,  0xdb, 0xc7, 0xb1, 0x99,
    0x80, 0x67, 0x4f, 0x39,  0x25, 0x16, 0x0a, 0x02,
    0x00, 0x02, 0x0a, 0x16,  0x25, 0x39, 0x4f, 0x67
};

static const uint8_t constant_wave[] = { 0x00 };

static const shell_command_t shell_commands[] = {
    { "play", "Play a sine wave", sc_play },
    { "music", "Play music", sc_music },
    { NULL, NULL, NULL }
};

static const uint16_t freqs[] = {
    [NOTE_C4]       = 262,
    [NOTE_CIS4]     = 277,
    [NOTE_D4]       = 294,
    [NOTE_DIS4]     = 311,
    [NOTE_E4]       = 330,
    [NOTE_F4]       = 349,
    [NOTE_FIS4]     = 370,
    [NOTE_G4]       = 392,
    [NOTE_GIS4]     = 415,
    [NOTE_A4]       = 440,
    [NOTE_AIS4]     = 466,
    [NOTE_B4]       = 494,
    [NOTE_C5]       = 523,
    [NOTE_CIS5]     = 554,
    [NOTE_D5]       = 587,
    [NOTE_DIS5]     = 622,
    [NOTE_E5]       = 659,
    [NOTE_F5]       = 698,
    [NOTE_FIS5]     = 740,
    [NOTE_G5]       = 784,
    [NOTE_GIS5]     = 831,
    [NOTE_A5]       = 880,
    [NOTE_AIS5]     = 932,
    [NOTE_B5]       = 988,
    [NOTE_C6]       = 1047,
};

static const uint8_t notes[] = {
    NOTE_C4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_G4     | NOTE_LEN_8,
    NOTE_F4     | NOTE_LEN_8,
    NOTE_C4     | NOTE_LEN_4    | NOTE_LEN_DOT,
    NOTE_C4     | NOTE_LEN_16,
    NOTE_C4     | NOTE_LEN_16,
    /* --- */
    NOTE_C4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_G4     | NOTE_LEN_8,
    NOTE_F4     | NOTE_LEN_8,
    NOTE_D4     | NOTE_LEN_4    | NOTE_LEN_DOT,
    NOTE_PAUSE  | NOTE_LEN_4,
    /* --- */
    NOTE_D4     | NOTE_LEN_8,
    NOTE_BB4    | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_G4     | NOTE_LEN_8,
    NOTE_E4     | NOTE_LEN_2,
    /* --- */
    NOTE_C5     | NOTE_LEN_8,
    NOTE_C5     | NOTE_LEN_8,
    NOTE_BB4    | NOTE_LEN_8,
    NOTE_G4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_4    | NOTE_LEN_DOT,
    NOTE_PAUSE  | NOTE_LEN_4,
    /* --- */
    NOTE_C4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_G4     | NOTE_LEN_8,
    NOTE_F4     | NOTE_LEN_8,
    NOTE_C4     | NOTE_LEN_2,
    /* --- */
    NOTE_C4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_G4     | NOTE_LEN_8,
    NOTE_F4     | NOTE_LEN_8,
    NOTE_D4     | NOTE_LEN_4    | NOTE_LEN_DOT,
    NOTE_D4     | NOTE_LEN_8,
    /* --- */
    NOTE_D4     | NOTE_LEN_8,
    NOTE_BB4    | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_G4     | NOTE_LEN_8,
    NOTE_C5     | NOTE_LEN_8,
    NOTE_C5     | NOTE_LEN_8,
    NOTE_C5     | NOTE_LEN_8,
    NOTE_C5     | NOTE_LEN_8,
    /* --- */
    NOTE_D5     | NOTE_LEN_8,
    NOTE_C5     | NOTE_LEN_8,
    NOTE_BB4    | NOTE_LEN_8,
    NOTE_G4     | NOTE_LEN_8,
    NOTE_F4     | NOTE_LEN_4    | NOTE_LEN_DOT,
    NOTE_PAUSE  | NOTE_LEN_8,
    /* --- */
    NOTE_A4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_4,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_4,
    /* --- */
    NOTE_A4     | NOTE_LEN_8,
    NOTE_C5     | NOTE_LEN_8,
    NOTE_F4     | NOTE_LEN_8    | NOTE_LEN_DOT,
    NOTE_G4     | NOTE_LEN_16,
    NOTE_A4     | NOTE_LEN_4    | NOTE_LEN_DOT,
    NOTE_PAUSE  | NOTE_LEN_4,
    /* --- */
    NOTE_BB4    | NOTE_LEN_8,
    NOTE_BB4    | NOTE_LEN_8,
    NOTE_BB4    | NOTE_LEN_8    | NOTE_LEN_DOT,
    NOTE_BB4    | NOTE_LEN_16,
    NOTE_BB4    | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_16,
    NOTE_A4     | NOTE_LEN_16,
    /* --- */
    NOTE_A4     | NOTE_LEN_8,
    NOTE_G4     | NOTE_LEN_8,
    NOTE_G4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_G4     | NOTE_LEN_8,
    NOTE_C4     | NOTE_LEN_4    | NOTE_LEN_DOT,
    NOTE_PAUSE  | NOTE_LEN_8,
    /* --- */
    NOTE_A4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_4,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_4,
    /* --- */
    NOTE_A4     | NOTE_LEN_8,
    NOTE_C5     | NOTE_LEN_8,
    NOTE_F4     | NOTE_LEN_8    | NOTE_LEN_DOT,
    NOTE_G4     | NOTE_LEN_16,
    NOTE_A4     | NOTE_LEN_4    | NOTE_LEN_DOT,
    NOTE_PAUSE  | NOTE_LEN_4,
    /* --- */
    NOTE_BB4    | NOTE_LEN_8,
    NOTE_BB4    | NOTE_LEN_8,
    NOTE_BB4    | NOTE_LEN_8    | NOTE_LEN_DOT,
    NOTE_BB4    | NOTE_LEN_16,
    NOTE_BB4    | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_8,
    NOTE_A4     | NOTE_LEN_16,
    NOTE_A4     | NOTE_LEN_16,
    /* --- */
    NOTE_C5     | NOTE_LEN_8,
    NOTE_C5     | NOTE_LEN_8,
    NOTE_BB4    | NOTE_LEN_8,
    NOTE_G4     | NOTE_LEN_8,
    NOTE_F4     | NOTE_LEN_4    | NOTE_LEN_DOT,
    NOTE_PAUSE  | NOTE_LEN_4,
};

static dds_t dds;

static void pause(uint16_t duration_ms)
{
    dds_play(&dds, constant_wave, sizeof(constant_wave), 440, duration_ms,
             DDS_MODE_BLOCK);
}

static int sc_play(int argc, char **argv)
{
    uint16_t freq = 440;
    uint16_t duration_ms = 1000;

    if (argc > 2) {
        duration_ms = atoi(argv[2]);
        if (!duration_ms) {
            puts("Invalid duration");
            return 1;
        }
    }

    if (argc > 1) {
        freq = atoi(argv[1]);
        if (!freq) {
            puts("Invalid frequency");
            return 1;
        }
    }

    dds_play(&dds, sine, sizeof(sine), freq, duration_ms, DDS_MODE_ASYNC);
    return 0;
}

static int sc_music(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const uint16_t duration_one = 2048;
    const uint8_t transpose = 7;
    for (size_t i = 0; i < sizeof(notes); i++) {
        uint8_t note = (notes[i] & 0xf);
        uint8_t note_len = (notes[i] >> 4) & 0x7;
        uint8_t note_dot = notes[i] & NOTE_LEN_DOT;
        uint16_t duration = duration_one >> note_len;
        if (note_dot) {
            duration += duration >> 1;
        }
        if (note != NOTE_PAUSE) {
            uint16_t freq = freqs[note + transpose];
            dds_play(&dds, sine, sizeof(sine), freq, duration,
                     DDS_MODE_BLOCK);
        }
        else {
            pause(duration);
        }
    }

    return 0;
}

int main(void)
{

    if (dds_init(&dds, &dds_params[0])) {
        puts("Initialization of DDS failed");
        return 1;
    }

    puts(
        "Run \"play [f [d]]\"\n"
        "\n"
        "  f = Frequency in Hz\n"
        "  d = Duration in ms"
    );

    sc_music(0, NULL);

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
