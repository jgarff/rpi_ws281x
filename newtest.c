/*
 * main.c
 *
 * Copyright (c) 2014 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


static char VERSION[] = "testing...";

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdarg.h>
#include <getopt.h>


#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"

#include "ws2811.h"


#define ARRAY_SIZE(stuff)                        (sizeof(stuff) / sizeof(stuff[0]))

#define TARGET_FREQ                              WS2811_TARGET_FREQ
#define GPIO_PIN                                 18
#define DMA                                      5
#define STRIP_TYPE				WS2811_STRIP_RGB		// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE				WS2811_STRIP_GBR		// WS2812/SK6812RGB integrated chip+leds
//#define STRIP_TYPE				SK6812_STRIP_RGBW		// SK6812RGBW (NOT SK6812RGB)

#define WIDTH                                    8
#define HEIGHT                                   8
#define LED_COUNT                                (WIDTH * HEIGHT)

int gpio_pin = 18;
int dma = 5;
int strip_type = WS2811_STRIP_RGB;

ws2811_t ledstring =
{
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
    {
        [0] =
        {
            .gpionum = GPIO_PIN,
            .count = LED_COUNT,
            .invert = 0,
            .brightness = 255,
            .strip_type = STRIP_TYPE,
        },
        [1] =
        {
            .gpionum = 0,
            .count = 0,
            .invert = 0,
            .brightness = 0,
        },
    },
};

ws2811_led_t matrix[WIDTH][HEIGHT];

static uint8_t running = 1;

void matrix_render(void)
{
    int x, y;

    for (x = 0; x < WIDTH; x++)
    {
        for (y = 0; y < HEIGHT; y++)
        {
            ledstring.channel[0].leds[(y * WIDTH) + x] = matrix[x][y];
        }
    }
}

void matrix_raise(void)
{
    int x, y;

    for (y = 0; y < (HEIGHT - 1); y++)
    {
        for (x = 0; x < WIDTH; x++)
        {
            matrix[x][y] = matrix[x][y + 1];
        }
    }
}

int dotspos[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
ws2811_led_t dotcolors[] =
{
    0x200000,  // red
    0x201000,  // orange
    0x202000,  // yellow
    0x002000,  // green
    0x002020,  // lightblue
    0x000020,  // blue
    0x100010,  // purple
    0x200010,  // pink
};

void matrix_bottom(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(dotspos); i++)
    {
        dotspos[i]++;
        if (dotspos[i] > (WIDTH - 1))
        {
            dotspos[i] = 0;
        }

        matrix[dotspos[i]][HEIGHT - 1] = dotcolors[i];
    }
}

static void ctrl_c_handler(int signum)
{
    running = 0;
}

static void setup_handlers(void)
{
    struct sigaction sa =
    {
        .sa_handler = ctrl_c_handler,
    };

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}


void parseargs(int argc, char **argv, ws2811_t *ws2811)
{
	int index;
	int c;

	static struct option longopts[] =
	{
		{"help", no_argument, 0, 'h'},
		{"gpio", required_argument, 0, 'g'},
		{"invert", no_argument, 0, 'i'},
		{"pcm", no_argument, 0, 'p'},
		{"version", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};

	while (1)
	{

		index = 0;
		c = getopt_long(argc, argv, "Dg:hipv", longopts, &index);

		if (c == -1)
			break;

		switch (c)
		{
		case 0:
			/* handle flag options (array's 3rd field non-0) */
			break;

		case 'h':
			fprintf(stderr, "%s version %s\n", argv[0], VERSION);
			fprintf(stderr, "Usage: %s [-hDgipv]\n"
				"-h (--help)    - this information\n"
				"-D (--daemon)  - Don't daemonize\n"
				"-g (--gpio)    - comma separated list of GPIOs to use\n"
				"                 If omitted, default is \"4,17,18,27,21,22,23,24,25\"\n"
				"-i (--invert)  - invert pin output (pulse LOW)\n"
				"-p (--pcm)     - use pcm for dmascheduling\n"
				"-v (--version) - version information\n"
				, argv[0]);
			exit(-1);

		case 'D':
			break;

		case 'g':
			if (optarg) {
				const char s[2] = ",";
				char *token;
   
				/* get the first token */
				token = strtok(optarg, s);

				/* walk through other tokens */
				while( token != NULL ) 
				{
					token = strtok(NULL, s);
				}
			}
			break;

		case 'i':
			ws2811->channel[0].invert=1;
			break;

		case 'p':
			break;

		case 'v':
			fprintf(stderr, "%s version %s\n", argv[0], VERSION);
			exit(-1);

		case '?':
			/* getopt_long already reported error? */
			exit(-1);

		default:
			exit(-1);
		}
	}
}


int main(int argc, char *argv[])
{
    int ret = 0;

    parseargs(argc, argv, &ledstring);

    setup_handlers();

    if (ws2811_init(&ledstring))
    {
        return -1;
    }

    while (running)
    {
        matrix_raise();
        matrix_bottom();
        matrix_render();

        if (ws2811_render(&ledstring))
        {
            ret = -1;
            break;
        }

        // 15 frames /sec
        usleep(1000000 / 15);
    }

    ws2811_fini(&ledstring);

    return ret;
}

