/* Copyright 2020 Nico Sonack
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* minimp3 example
 * This example decodes an mp3 file and
 * streams it to PulseAudio for playback.
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

//#define MINIMP3_FLOAT_OUTPUT
//#define MINIMP3_NO_SIMD
#define MINIMP3_IMPLEMENTATION
#include "minimp3/minimp3_ex.h"

#include <pulse/simple.h>
#include <pulse/error.h>

int main(argc, argv)
     int argc;
     char** argv;
{
    if (argc < 4)
    {
	fprintf(stderr, "ERR : seek requires 4 arguments: <file> <start [sec]> <end [sec]>\n");
	exit(EXIT_FAILURE);
    }

    static mp3dec_ex_t dec;

    if (mp3dec_ex_open(&dec, argv[1], MP3D_SEEK_TO_SAMPLE)) {
      fprintf(stderr, "ERR : Cannot open file.\n");
      exit(EXIT_FAILURE);
    }

    pa_simple *pa = NULL;
    int error,
	start = atoi(argv[2]),
	end = atoi(argv[3]);

    const pa_sample_spec ss = {
       .format = PA_SAMPLE_S16LE,
       .rate = dec.info.hz,
       .channels = dec.info.channels
    };

    fprintf(stdout, "INF : Encoder gave sample rate of %d Hz. We set it to %d Hz\n", dec.info.hz, ss.rate);
    pa = pa_simple_new(NULL, "minimp3player",
		       PA_STREAM_PLAYBACK, NULL,
		       "Playback", &ss, NULL, NULL, &error);
    if (!pa)	{
      fprintf(stderr, "ERR : Couldn't connect to PulseAudio: %s\n", pa_strerror(error));
      goto err_pa;
    }

    size_t expected_samples = dec.info.hz * dec.info.channels * (end - start);
    size_t offset_samples = dec.info.hz * dec.info.channels * start;

    mp3d_sample_t *buffer = malloc(expected_samples * sizeof(mp3d_sample_t));

    if (mp3dec_ex_seek(&dec, offset_samples)) {
      fprintf(stderr, "ERR : Unable to seek in mp3 stream\n");
      goto err;
    }

    size_t read_samples = mp3dec_ex_read(&dec, buffer, expected_samples);

    if (read_samples != expected_samples) {
      if (dec.last_error) {
	fprintf(stderr, "ERR : Couldn't decode mp3 stream.");
	goto err;
      }
    }

    if (pa_simple_write(pa, buffer, (size_t)(expected_samples * sizeof(mp3d_sample_t)), &error) < 0) {
      fprintf(stderr, "ERR : Unable to send data to PulseAudio: %s\n", pa_strerror(error));
      goto err;
    }

    if (pa_simple_drain(pa, &error) < 0) {
      fprintf(stderr, "ERR : Unable to drain data to PulseAudio: %s\n", pa_strerror(error));
      goto err;
    }

err:
    free(buffer);
err_pa:
    mp3dec_ex_close(&dec);
    pa_simple_free(pa);
}
