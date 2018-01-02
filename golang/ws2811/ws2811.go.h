/***********************************************************************************
 * Copyright (c) 2015, Jacques Supcik, HEIA-FR
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ***********************************************************************************/

#include <stdint.h>
#include <string.h>
#include <ws2811.h>

ws2811_t ledstring = {
   .freq = 800000,
   .dmanum = 10,
   .channel = {
	   [0] = {
		   .gpionum = 18,
		   .count = 256,
		   .invert = 0,
		   .brightness = 32,
	   },
	   [1] = {
		   .gpionum = 0,
		   .count = 0,
		   .invert = 0,
		   .brightness = 0,
	   },
   },
};

void ws2811_set_led(ws2811_t *ws2811, int index, uint32_t value) {
	ws2811->channel[0].leds[index] = value;
}

void ws2811_clear(ws2811_t *ws2811) {
	for (int chan = 0; chan < RPI_PWM_CHANNELS; chan++) {
		ws2811_channel_t *channel = &ws2811->channel[chan];
		memset(channel->leds, 0, sizeof(ws2811_led_t) * channel->count);
	}
}

void ws2811_set_bitmap(ws2811_t *ws2811, void* a, int len) {
	memcpy(ws2811->channel[0].leds, a, len);
}
