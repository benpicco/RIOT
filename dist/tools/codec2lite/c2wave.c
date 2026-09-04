/*
 * c2wave - convert between 16 bit PCM WAV files and Codec 2 raw bit streams
 *
 * Uses the codec2lite library (pkg/codec2lite) to encode a mono, 8 kHz,
 * 16 bit signed PCM WAV file into a packed Codec 2 bitstream, or to decode
 * such a bitstream back into a WAV file.
 *
 * Usage:
 *   c2wave enc <mode> <in.wav>  <out.c2>
 *   c2wave dec <mode> <in.c2>   <out.wav>
 *
 * <mode> is one of the codec2 mode names, e.g. 3200, 2400, 1600, 1400,
 * 1300, 1200, 700, 700b, 700c, 450, 450pwb
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "codec2.h"

#define WAV_SAMPLE_RATE 8000

/* --- minimal WAV reader/writer (canonical PCM, mono, 16 bit) --- */

static uint32_t rd_u32le(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rd_u16le(const unsigned char *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static void wr_u32le(FILE *f, uint32_t v)
{
    unsigned char b[4] = { v & 0xff, (v >> 8) & 0xff,
                            (v >> 16) & 0xff, (v >> 24) & 0xff };
    fwrite(b, 1, 4, f);
}

static void wr_u16le(FILE *f, uint16_t v)
{
    unsigned char b[2] = { v & 0xff, (v >> 8) & 0xff };
    fwrite(b, 1, 2, f);
}

/* Reads a mono 16 bit PCM WAV file, returns malloc'd sample buffer and
 * sets *n_samples. Returns NULL on error. */
static short *wav_read(const char *filename, uint32_t *n_samples,
                        uint32_t *sample_rate)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "c2wave: cannot open '%s': %s\n", filename, strerror(errno));
        return NULL;
    }

    unsigned char hdr[12];
    if (fread(hdr, 1, 12, f) != 12 ||
        memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        fprintf(stderr, "c2wave: '%s' is not a RIFF/WAVE file\n", filename);
        fclose(f);
        return NULL;
    }

    uint16_t channels = 0, bits_per_sample = 0;
    uint32_t rate = 0;
    short *samples = NULL;
    uint32_t nsamp = 0;

    for (;;) {
        unsigned char chdr[8];
        if (fread(chdr, 1, 8, f) != 8) {
            break;
        }
        char id[5] = { chdr[0], chdr[1], chdr[2], chdr[3], 0 };
        uint32_t size = rd_u32le(chdr + 4);

        if (memcmp(id, "fmt ", 4) == 0) {
            unsigned char fmt[16];
            if (size < 16 || fread(fmt, 1, 16, f) != 16) {
                fprintf(stderr, "c2wave: truncated fmt chunk in '%s'\n", filename);
                fclose(f);
                free(samples);
                return NULL;
            }
            uint16_t format = rd_u16le(fmt + 0);
            channels = rd_u16le(fmt + 2);
            rate = rd_u32le(fmt + 4);
            bits_per_sample = rd_u16le(fmt + 14);
            if (format != 1 /* PCM */) {
                fprintf(stderr, "c2wave: '%s' is not integer PCM (format=%u)\n",
                        filename, format);
                fclose(f);
                free(samples);
                return NULL;
            }
            /* skip any extra fmt bytes */
            if (size > 16 && fseek(f, size - 16, SEEK_CUR) != 0) {
                break;
            }
        }
        else if (memcmp(id, "data", 4) == 0) {
            if (channels != 1 || bits_per_sample != 16) {
                fprintf(stderr,
                        "c2wave: '%s' must be mono 16 bit PCM (channels=%u, bits=%u)\n",
                        filename, channels, bits_per_sample);
                fclose(f);
                free(samples);
                return NULL;
            }
            nsamp = size / sizeof(short);
            samples = malloc(nsamp * sizeof(short));
            if (!samples || fread(samples, sizeof(short), nsamp, f) != nsamp) {
                fprintf(stderr, "c2wave: truncated data chunk in '%s'\n", filename);
                fclose(f);
                free(samples);
                return NULL;
            }
            if (size & 1) {
                fseek(f, 1, SEEK_CUR); /* padding byte */
            }
        }
        else {
            /* skip unknown chunk */
            if (fseek(f, size + (size & 1), SEEK_CUR) != 0) {
                break;
            }
        }
    }

    fclose(f);

    if (!samples) {
        fprintf(stderr, "c2wave: no data chunk found in '%s'\n", filename);
        return NULL;
    }

    if (rate != WAV_SAMPLE_RATE) {
        fprintf(stderr,
                "c2wave: warning: '%s' has sample rate %u Hz, codec2 expects %u Hz\n",
                filename, rate, WAV_SAMPLE_RATE);
    }

    *n_samples = nsamp;
    *sample_rate = rate;
    return samples;
}

static int wav_write(const char *filename, const short *samples, uint32_t n_samples,
                      uint32_t sample_rate)
{
    FILE *f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "c2wave: cannot create '%s': %s\n", filename, strerror(errno));
        return -1;
    }

    uint32_t data_bytes = n_samples * sizeof(short);
    uint16_t block_align = 2; /* mono, 16 bit */
    uint32_t byte_rate = sample_rate * block_align;

    fwrite("RIFF", 1, 4, f);
    wr_u32le(f, 36 + data_bytes);
    fwrite("WAVE", 1, 4, f);

    fwrite("fmt ", 1, 4, f);
    wr_u32le(f, 16);
    wr_u16le(f, 1);           /* PCM */
    wr_u16le(f, 1);           /* mono */
    wr_u32le(f, sample_rate);
    wr_u32le(f, byte_rate);
    wr_u16le(f, block_align);
    wr_u16le(f, 16);          /* bits per sample */

    fwrite("data", 1, 4, f);
    wr_u32le(f, data_bytes);
    fwrite(samples, sizeof(short), n_samples, f);

    fclose(f);
    return 0;
}

/* --- mode name lookup --- */

static const struct {
    const char *name;
    int mode;
} modes[] = {
    { "3200",    CODEC2_MODE_3200 },
    { "2400",    CODEC2_MODE_2400 },
    { "1600",    CODEC2_MODE_1600 },
    { "1400",    CODEC2_MODE_1400 },
    { "1300",    CODEC2_MODE_1300 },
    { "1200",    CODEC2_MODE_1200 },
    { "700",     CODEC2_MODE_700  },
    { "700b",    CODEC2_MODE_700B },
    { "700c",    CODEC2_MODE_700C },
    { "450",     CODEC2_MODE_450  },
    { "450pwb",  CODEC2_MODE_450PWB },
};

static int mode_from_name(const char *name)
{
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        if (strcmp(modes[i].name, name) == 0) {
            return modes[i].mode;
        }
    }
    return -1;
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s enc <mode> <in.wav> <out.c2>\n"
            "       %s dec <mode> <in.c2>  <out.wav>\n"
            "\n"
            "<mode> is one of: 3200 2400 1600 1400 1300 1200 700 700b 700c 450 450pwb\n"
            "\n"
            "Input WAV files must be mono, 16 bit signed PCM, sampled at %u Hz.\n",
            prog, prog, WAV_SAMPLE_RATE);
}

static int do_encode(int mode, const char *in_wav, const char *out_c2)
{
    uint32_t n_samples = 0, sample_rate = 0;
    short *samples = wav_read(in_wav, &n_samples, &sample_rate);
    if (!samples) {
        return 1;
    }

    struct CODEC2 *c2 = codec2_create(mode);
    if (!c2) {
        fprintf(stderr, "c2wave: codec2_create() failed\n");
        free(samples);
        return 1;
    }

    int samples_per_frame = codec2_samples_per_frame(c2);
    int bytes_per_frame = (codec2_bits_per_frame(c2) + 7) / 8;

    FILE *out = fopen(out_c2, "wb");
    if (!out) {
        fprintf(stderr, "c2wave: cannot create '%s': %s\n", out_c2, strerror(errno));
        codec2_destroy(c2);
        free(samples);
        return 1;
    }

    unsigned char *bits = malloc(bytes_per_frame);
    short *frame = calloc(samples_per_frame, sizeof(short));
    uint32_t off = 0;
    uint32_t n_frames = 0;

    while (off + samples_per_frame <= n_samples) {
        codec2_encode(c2, bits, samples + off);
        fwrite(bits, 1, bytes_per_frame, out);
        off += samples_per_frame;
        n_frames++;
    }

    /* pad and encode the final partial frame, if any, so no audio is lost */
    if (off < n_samples) {
        memset(frame, 0, samples_per_frame * sizeof(short));
        memcpy(frame, samples + off, (n_samples - off) * sizeof(short));
        codec2_encode(c2, bits, frame);
        fwrite(bits, 1, bytes_per_frame, out);
        n_frames++;
    }

    fclose(out);
    free(bits);
    free(frame);
    free(samples);
    codec2_destroy(c2);

    fprintf(stderr, "c2wave: encoded %u frames (%d samples/frame, %d bytes/frame) to '%s'\n",
            n_frames, samples_per_frame, bytes_per_frame, out_c2);
    return 0;
}

static int do_decode(int mode, const char *in_c2, const char *out_wav)
{
    FILE *in = fopen(in_c2, "rb");
    if (!in) {
        fprintf(stderr, "c2wave: cannot open '%s': %s\n", in_c2, strerror(errno));
        return 1;
    }

    struct CODEC2 *c2 = codec2_create(mode);
    if (!c2) {
        fprintf(stderr, "c2wave: codec2_create() failed\n");
        fclose(in);
        return 1;
    }

    int samples_per_frame = codec2_samples_per_frame(c2);
    int bytes_per_frame = (codec2_bits_per_frame(c2) + 7) / 8;

    unsigned char *bits = malloc(bytes_per_frame);
    short *out_samples = NULL;
    uint32_t n_samples = 0, capacity = 0;
    uint32_t n_frames = 0;

    while (fread(bits, 1, bytes_per_frame, in) == (size_t)bytes_per_frame) {
        if (n_samples + samples_per_frame > capacity) {
            capacity = (capacity == 0) ? 65536 : capacity * 2;
            out_samples = realloc(out_samples, capacity * sizeof(short));
        }
        codec2_decode(c2, out_samples + n_samples, bits);
        n_samples += samples_per_frame;
        n_frames++;
    }

    fclose(in);
    free(bits);
    codec2_destroy(c2);

    int ret = wav_write(out_wav, out_samples, n_samples, WAV_SAMPLE_RATE);
    free(out_samples);

    if (ret == 0) {
        fprintf(stderr, "c2wave: decoded %u frames (%d samples/frame) to '%s'\n",
                n_frames, samples_per_frame, out_wav);
    }
    return ret == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc != 5) {
        usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];
    int mode = mode_from_name(argv[2]);
    if (mode < 0) {
        fprintf(stderr, "c2wave: unknown mode '%s'\n", argv[2]);
        usage(argv[0]);
        return 1;
    }

    if (strcmp(cmd, "enc") == 0) {
        return do_encode(mode, argv[3], argv[4]);
    }
    else if (strcmp(cmd, "dec") == 0) {
        return do_decode(mode, argv[3], argv[4]);
    }

    fprintf(stderr, "c2wave: unknown command '%s'\n", cmd);
    usage(argv[0]);
    return 1;
}
