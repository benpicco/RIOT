# c2wave

A standalone Linux command line tool built on top of the `codec2lite`
package sources (`pkg/codec2lite`), for converting between WAV files and
Codec 2 raw bitstreams.

## Building

`c2wave` links against the codec2lite sources, so they must already have
been fetched by building any RIOT application that uses `USEPKG +=
codec2lite`. Then build the tool with:

```sh
make -C dist/tools/codec2lite
```

This produces the `c2wave` binary in this directory.

## Usage

```
c2wave enc <mode> <in.wav> <out.c2>
c2wave dec <mode> <in.c2>  <out.wav>
```

`<mode>` is one of the Codec 2 mode names:
`3200 2400 1600 1400 1300 1200 700 700b 700c 450 450pwb`

Input WAV files for `enc` must be **mono, 16 bit signed PCM, sampled at
8000 Hz**. Files decoded with `dec` are written in the same format.

## Preparing an input WAV file

Codec 2 expects raw mono 16 bit PCM audio at 8 kHz. Use `ffmpeg` to convert
any input audio file (e.g. an MP3) into a compatible WAV file:

```sh
ffmpeg -i input.mp3 -ac 1 -ar 8000 -sample_fmt s16 -c:a pcm_s16le output.wav
```

- `-ac 1` downmixes to mono
- `-ar 8000` resamples to 8000 Hz
- `-sample_fmt s16` / `-c:a pcm_s16le` selects 16 bit signed PCM

## Examples

Encode a WAV file at 1300 bit/s mode:

```sh
./c2wave enc 1300 output.wav output.c2
```

Decode it back to WAV:

```sh
./c2wave dec 1300 output.c2 decoded.wav
```

Play the result, e.g. with `ffplay` or `aplay`:

```sh
ffplay decoded.wav
```
