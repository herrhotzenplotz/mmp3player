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
#include "minimp3/minimp3.h"

#include <pulse/simple.h>
#include <pulse/error.h>

int main(argc, argv)
     int argc;
     char** argv;
{
    if (argc < 2)
    {
	fprintf(stderr, "ERR : mp3pa requires 2 arguments\n");
	exit(EXIT_FAILURE);
    }

    struct stat statbuf;

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0)
    {
	fprintf(stderr, "ERR : Couldn't open file '%s'\n", argv[1]);
	exit(EXIT_FAILURE);
    }

    if (fstat(fd, &statbuf) < 0)
    {
	fprintf(stderr, "ERR : Couldn't fstat input file\n");
	goto err_fstat;
    }

    uint8_t* file_buffer = mmap(NULL, statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (!file_buffer)
    {
	fprintf(stderr, "ERR : Unable to mmap input file\n");
	goto err_mmap;
    }

    long stream_length = statbuf.st_size;

    static mp3dec_t mp3d;
    mp3dec_init(&mp3d);

    mp3dec_frame_info_t info;
    mp3d_sample_t pcm_data[MINIMP3_MAX_SAMPLES_PER_FRAME];

    long frame_number = 0;

    pa_simple *pa = NULL;
    int error;

    for (int samples = mp3dec_decode_frame(&mp3d, file_buffer, statbuf.st_size, pcm_data, &info);
	 (stream_length > 0) && ((samples > 0) || (samples == 0 && info.frame_bytes > 0));
	 samples = mp3dec_decode_frame(&mp3d, file_buffer, statbuf.st_size, pcm_data, &info))
    {
	if (!pa) {
	    const pa_sample_spec ss = {
	       .format = PA_SAMPLE_S16LE,
	       .rate = info.hz,
	       .channels = info.channels
	    };

	    fprintf(stdout, "INF : Encoder gave sample rate of %d Hz. We set it to %d Hz\n", info.hz, ss.rate);
	    pa = pa_simple_new(NULL, "minimp3player",
			       PA_STREAM_PLAYBACK, NULL,
			       "Playback", &ss, NULL, NULL, &error);
	    if (!pa)
	    {
		fprintf(stderr, "ERR : Couldn't connect to PulseAudio: %s\n", pa_strerror(error));
		goto err_pa_new;
	    }
	}

	fprintf(stdout, "\rINF : Decoding frame no. %5ld", ++frame_number);
	file_buffer += info.frame_bytes;
	stream_length -= info.frame_bytes;

	if (samples == 0) // some other informational data we don't care about
	    continue;

	if (pa_simple_write(pa, pcm_data, (size_t)(samples * info.channels * sizeof(mp3d_sample_t)), &error) < 0)
	{
	    fprintf(stderr, "ERR : Unable to send data to PulseAudio: %s\n", pa_strerror(error));
	    goto err_pa_write;
	}
    }

    if (pa_simple_drain(pa, &error) < 0)
    {
	fprintf(stderr, "ERR : Unable to drain data to PulseAudio: %s\n", pa_strerror(error));
	goto err_pa_drain;
    }


err_pa_drain:
err_pa_write:
    pa_simple_free(pa);
err_pa_new:
    munmap(file_buffer, statbuf.st_size);
err_mmap:
err_fstat:
    close(fd);
}
