/*
 * ws2811.h
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


#ifndef __WS2811_H__
#define __WS2811_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "rpihw.h"
#include "pwm.h"


#define WS2811_TARGET_FREQ                       800000   // Can go as low as 400000

#define WS2811_STRIP_RGB                         0x100800
#define WS2811_STRIP_RBG                         0x100008
#define WS2811_STRIP_GRB                         0x081000
#define WS2811_STRIP_GBR                         0x080010
#define WS2811_STRIP_BRG                         0x001008
#define WS2811_STRIP_BGR                         0x000810

struct ws2811_device;

typedef uint32_t ws2811_led_t;                   //< 0x00RRGGBB
typedef struct
{
    int gpionum;                                 //< GPIO Pin with PWM alternate function, 0 if unused
    int invert;                                  //< Invert output signal
    int count;                                   //< Number of LEDs, 0 if channel is unused
    int brightness;                              //< Brightness value between 0 and 255
    int strip_type;                              //< Strip color layout -- one of WS2811_STRIP_xxx constants
    ws2811_led_t *leds;                          //< LED buffers, allocated by driver based on count
} ws2811_channel_t;

typedef struct
{
    struct ws2811_device *device;                //< Private data for driver use
    const rpi_hw_t *rpi_hw;                      //< RPI Hardware Information
    uint32_t freq;                               //< Required output frequency
    int dmanum;                                  //< DMA number _not_ already in use
    ws2811_channel_t channel[RPI_PWM_CHANNELS];
} ws2811_t;


int ws2811_init(ws2811_t *ws2811);               //< Initialize buffers/hardware
void ws2811_fini(ws2811_t *ws2811);              //< Tear it all down
int ws2811_render(ws2811_t *ws2811);             //< Send LEDs off to hardware
int ws2811_wait(ws2811_t *ws2811);               //< Wait for DMA completion

#ifdef __cplusplus
}
#endif

#endif /* __WS2811_H__ */

