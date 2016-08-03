/*
 * ws2811.c
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

#include "mailbox.h"
#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
#include "rpihw.h"

#include "gamma.h"

#include "ws2811.h"


#define BUS_TO_PHYS(x)                           ((x)&~0xC0000000)

#define OSC_FREQ                                 19200000   // crystal frequency

/* 4 colors (R, G, B + W), 8 bits per byte, 3 symbols per bit + 55uS low for reset signal */
#define LED_COLOURS                              4
#define LED_RESET_uS                             55
#define LED_BIT_COUNT(leds, freq)                ((leds * LED_COLOURS * 8 * 3) + ((LED_RESET_uS * \
                                                  (freq * 3)) / 1000000))

// Pad out to the nearest uint32 + 32-bits for idle low/high times the number of channels
#define PWM_BYTE_COUNT(leds, freq)               (((((LED_BIT_COUNT(leds, freq) >> 3) & ~0x7) + 4) + 4) * \
                                                  RPI_PWM_CHANNELS)

#define SYMBOL_HIGH                              0x6  // 1 1 0
#define SYMBOL_LOW                               0x4  // 1 0 0


// We use the mailbox interface to request memory from the VideoCore.
// This lets us request one physically contiguous chunk, find its
// physical address, and map it 'uncached' so that writes from this
// code are immediately visible to the DMA controller.  This struct
// holds data relevant to the mailbox interface.
typedef struct videocore_mbox {
    int handle;             /* From mbox_open() */
    unsigned mem_ref;       /* From mem_alloc() */
    unsigned bus_addr;      /* From mem_lock() */
    unsigned size;          /* Size of allocation */
    uint8_t *virt_addr;     /* From mapmem() */
} videocore_mbox_t;

typedef struct ws2811_device
{
    volatile uint8_t *pwm_raw;
    volatile dma_t *dma;
    volatile pwm_t *pwm;
    volatile dma_cb_t *dma_cb;
    uint32_t dma_cb_addr;
    volatile gpio_t *gpio;
    volatile cm_pwm_t *cm_pwm;
    videocore_mbox_t mbox;
    int max_count;
} ws2811_device_t;

/**
 * Iterate through the channels and find the largest led count.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  Maximum number of LEDs in all channels.
 */
static int max_channel_led_count(ws2811_t *ws2811)
{
    int chan, max = 0;

    for (chan = 0; chan < RPI_PWM_CHANNELS; chan++)
    {
        if (ws2811->channel[chan].count > max)
        {
            max = ws2811->channel[chan].count;
        }
    }

    return max;
}

/**
 * Map all devices into userspace memory.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  0 on success, -1 otherwise.
 */
static int map_registers(ws2811_t *ws2811)
{
    ws2811_device_t *device = ws2811->device;
    const rpi_hw_t *rpi_hw = ws2811->rpi_hw;
    uint32_t base = ws2811->rpi_hw->periph_base;
    uint32_t dma_addr;

    dma_addr = dmanum_to_offset(ws2811->dmanum);
    if (!dma_addr)
    {
        return -1;
    }
    dma_addr += rpi_hw->periph_base;

    device->dma = mapmem(dma_addr, sizeof(dma_t));
    if (!device->dma)
    {
        return -1;
    }

    device->pwm = mapmem(PWM_OFFSET + base, sizeof(pwm_t));
    if (!device->pwm)
    {
        return -1;
    }

    device->gpio = mapmem(GPIO_OFFSET + base, sizeof(gpio_t));
    if (!device->gpio)
    {
        return -1;
    }

    device->cm_pwm = mapmem(CM_PWM_OFFSET + base, sizeof(cm_pwm_t));
    if (!device->cm_pwm)
    {
        return -1;
    }

    return 0;
}

/**
 * Unmap all devices from virtual memory.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  None
 */
static void unmap_registers(ws2811_t *ws2811)
{
    ws2811_device_t *device = ws2811->device;

    if (device->dma)
    {
        unmapmem((void *)device->dma, sizeof(dma_t));
    }

    if (device->pwm)
    {
        unmapmem((void *)device->pwm, sizeof(pwm_t));
    }

    if (device->cm_pwm)
    {
        unmapmem((void *)device->cm_pwm, sizeof(cm_pwm_t));
    }

    if (device->gpio)
    {
        unmapmem((void *)device->gpio, sizeof(gpio_t));
    }
}

/**
 * Given a userspace address pointer, return the matching bus address used by DMA.
 *     Note: The bus address is not the same as the CPU physical address.
 *
 * @param    addr   Userspace virtual address pointer.
 *
 * @returns  Bus address for use by DMA.
 */
static uint32_t addr_to_bus(ws2811_device_t *device, const volatile void *virt)
{
    videocore_mbox_t *mbox = &device->mbox;

    uint32_t offset = (uint8_t *)virt - mbox->virt_addr;

    return mbox->bus_addr + offset;
}

/**
 * Stop the PWM controller.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  None
 */
static void stop_pwm(ws2811_t *ws2811)
{
    ws2811_device_t *device = ws2811->device;
    volatile pwm_t *pwm = device->pwm;
    volatile cm_pwm_t *cm_pwm = device->cm_pwm;

    // Turn off the PWM in case already running
    pwm->ctl = 0;
    usleep(10);

    // Kill the clock if it was already running
    cm_pwm->ctl = CM_PWM_CTL_PASSWD | CM_PWM_CTL_KILL;
    usleep(10);
    while (cm_pwm->ctl & CM_PWM_CTL_BUSY)
        ;
}

/**
 * Setup the PWM controller in serial mode on both channels using DMA to feed the PWM FIFO.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  None
 */
static int setup_pwm(ws2811_t *ws2811)
{
    ws2811_device_t *device = ws2811->device;
    volatile dma_t *dma = device->dma;
    volatile dma_cb_t *dma_cb = device->dma_cb;
    volatile pwm_t *pwm = device->pwm;
    volatile cm_pwm_t *cm_pwm = device->cm_pwm;
    int maxcount = max_channel_led_count(ws2811);
    uint32_t freq = ws2811->freq;
    int32_t byte_count;

    stop_pwm(ws2811);

    // Setup the PWM Clock - Use OSC @ 19.2Mhz w/ 3 clocks/tick
    cm_pwm->div = CM_PWM_DIV_PASSWD | CM_PWM_DIV_DIVI(OSC_FREQ / (3 * freq));
    cm_pwm->ctl = CM_PWM_CTL_PASSWD | CM_PWM_CTL_SRC_OSC;
    cm_pwm->ctl = CM_PWM_CTL_PASSWD | CM_PWM_CTL_SRC_OSC | CM_PWM_CTL_ENAB;
    usleep(10);
    while (!(cm_pwm->ctl & CM_PWM_CTL_BUSY))
        ;

    // Setup the PWM, use delays as the block is rumored to lock up without them.  Make
    // sure to use a high enough priority to avoid any FIFO underruns, especially if
    // the CPU is busy doing lots of memory accesses, or another DMA controller is
    // busy.  The FIFO will clock out data at a much slower rate (2.6Mhz max), so
    // the odds of a DMA priority boost are extremely low.

    pwm->rng1 = 32;  // 32-bits per word to serialize
    usleep(10);
    pwm->ctl = RPI_PWM_CTL_CLRF1;
    usleep(10);
    pwm->dmac = RPI_PWM_DMAC_ENAB | RPI_PWM_DMAC_PANIC(7) | RPI_PWM_DMAC_DREQ(3);
    usleep(10);
    pwm->ctl = RPI_PWM_CTL_USEF1 | RPI_PWM_CTL_MODE1 |
               RPI_PWM_CTL_USEF2 | RPI_PWM_CTL_MODE2;
    if (ws2811->channel[0].invert)
    {
        pwm->ctl |= RPI_PWM_CTL_POLA1;
    }
    if (ws2811->channel[1].invert)
    {
        pwm->ctl |= RPI_PWM_CTL_POLA2;
    }
    usleep(10);
    pwm->ctl |= RPI_PWM_CTL_PWEN1 | RPI_PWM_CTL_PWEN2;

    // Initialize the DMA control block
    byte_count = PWM_BYTE_COUNT(maxcount, freq);
    dma_cb->ti = RPI_DMA_TI_NO_WIDE_BURSTS |  // 32-bit transfers
                 RPI_DMA_TI_WAIT_RESP |       // wait for write complete
                 RPI_DMA_TI_DEST_DREQ |       // user peripheral flow control
                 RPI_DMA_TI_PERMAP(5) |       // PWM peripheral
                 RPI_DMA_TI_SRC_INC;          // Increment src addr

    dma_cb->source_ad = addr_to_bus(device, device->pwm_raw);

    dma_cb->dest_ad = (uint32_t)&((pwm_t *)PWM_PERIPH_PHYS)->fif1;
    dma_cb->txfr_len = byte_count;
    dma_cb->stride = 0;
    dma_cb->nextconbk = 0;

    dma->cs = 0;
    dma->txfr_len = 0;

    return 0;
}

/**
 * Start the DMA feeding the PWM FIFO.  This will stream the entire DMA buffer out of both
 * PWM channels.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  None
 */
static void dma_start(ws2811_t *ws2811)
{
    ws2811_device_t *device = ws2811->device;
    volatile dma_t *dma = device->dma;
    uint32_t dma_cb_addr = device->dma_cb_addr;

    dma->cs = RPI_DMA_CS_RESET;
    usleep(10);

    dma->cs = RPI_DMA_CS_INT | RPI_DMA_CS_END;
    usleep(10);

    dma->conblk_ad = dma_cb_addr;
    dma->debug = 7; // clear debug error flags
    dma->cs = RPI_DMA_CS_WAIT_OUTSTANDING_WRITES |
              RPI_DMA_CS_PANIC_PRIORITY(15) |
              RPI_DMA_CS_PRIORITY(15) |
              RPI_DMA_CS_ACTIVE;
}

/**
 * Initialize the application selected GPIO pins for PWM operation.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  0 on success, -1 on unsupported pin
 */
static int gpio_init(ws2811_t *ws2811)
{
    volatile gpio_t *gpio = ws2811->device->gpio;
    int chan;

    for (chan = 0; chan < RPI_PWM_CHANNELS; chan++)
    {
        int pinnum = ws2811->channel[chan].gpionum;

        if (pinnum)
        {
            int altnum = pwm_pin_alt(chan, pinnum);

            if (altnum < 0)
            {
                return -1;
            }

            gpio_function_set(gpio, pinnum, altnum);
        }
    }

    return 0;
}

/**
 * Initialize the PWM DMA buffer with all zeros, inverted operation will be
 * handled by hardware.  The DMA buffer length is assumed to be a word
 * multiple.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  None
 */
void pwm_raw_init(ws2811_t *ws2811)
{
    volatile uint32_t *pwm_raw = (uint32_t *)ws2811->device->pwm_raw;
    int maxcount = max_channel_led_count(ws2811);
    int wordcount = (PWM_BYTE_COUNT(maxcount, ws2811->freq) / sizeof(uint32_t)) /
                    RPI_PWM_CHANNELS;
    int chan;

    for (chan = 0; chan < RPI_PWM_CHANNELS; chan++)
    {
        int i, wordpos = chan;

        for (i = 0; i < wordcount; i++)
        {
            pwm_raw[wordpos] = 0x0;
            wordpos += 2;
        }
    }
}

/**
 * Cleanup previously allocated device memory and buffers.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  None
 */
void ws2811_cleanup(ws2811_t *ws2811)
{
    ws2811_device_t *device = ws2811->device;
    int chan;

    for (chan = 0; chan < RPI_PWM_CHANNELS; chan++)
    {
        if (ws2811->channel[chan].leds)
        {
            free(ws2811->channel[chan].leds);
        }
        ws2811->channel[chan].leds = NULL;
    }

    if (device->mbox.handle != -1)
    {
        videocore_mbox_t *mbox = &device->mbox;

        unmapmem(mbox->virt_addr, mbox->size);
        mem_unlock(mbox->handle, mbox->mem_ref);
        mem_free(mbox->handle, mbox->mem_ref);
        mbox_close(mbox->handle);

        mbox->handle = -1;
    }

    if (device) {
        free(device);
    }
    ws2811->device = NULL;
}


/*
 *
 * Application API Functions
 *
 */


/**
 * Allocate and initialize memory, buffers, pages, PWM, DMA, and GPIO.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  0 on success, -1 otherwise.
 */
int ws2811_init(ws2811_t *ws2811)
{
    ws2811_device_t *device;
    const rpi_hw_t *rpi_hw;
    int chan;
    int error = -1;

    ws2811->rpi_hw = rpi_hw_detect();
    if (!ws2811->rpi_hw)
    {
        return WS2811_ERR_HW_DETECT;
    }
    rpi_hw = ws2811->rpi_hw;

    ws2811->device = malloc(sizeof(*ws2811->device));
    if (!ws2811->device)
    {
        return WS2811_ERR_DEVICE_MALLOC;
    }
    device = ws2811->device;

    // Determine how much physical memory we need for DMA
    device->mbox.size = PWM_BYTE_COUNT(max_channel_led_count(ws2811), ws2811->freq) +
                        sizeof(dma_cb_t);
    // Round up to page size multiple
    device->mbox.size = (device->mbox.size + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);

    device->mbox.handle = mbox_open();
    if (device->mbox.handle == -1)
    {
        return WS2811_ERR_MAILBOX_OPEN;
    }

    device->mbox.mem_ref = mem_alloc(device->mbox.handle, device->mbox.size, PAGE_SIZE,
                                     rpi_hw->videocore_base == 0x40000000 ? 0xC : 0x4);
    if (device->mbox.mem_ref == 0)
    {
       return WS2811_ERR_MAILBOX_ALLOC;
    }

    device->mbox.bus_addr = mem_lock(device->mbox.handle, device->mbox.mem_ref);
    if (device->mbox.bus_addr == (uint32_t) ~0UL)
    {
       mem_free(device->mbox.handle, device->mbox.size);
       return WS2811_ERR_MAILBOX_LOCK;
    }

    device->mbox.virt_addr = mapmem(BUS_TO_PHYS(device->mbox.bus_addr), device->mbox.size);
    if (!device->mbox.virt_addr)
    {
        mem_unlock(device->mbox.handle, device->mbox.mem_ref);
        mem_free(device->mbox.handle, device->mbox.size);
        error = WS2811_ERR_MAP_MEM;
        goto err;
    }

    // Initialize all pointers to NULL.  Any non-NULL pointers will be freed on cleanup.
    device->pwm_raw = NULL;
    device->dma_cb = NULL;
    for (chan = 0; chan < RPI_PWM_CHANNELS; chan++)
    {
        ws2811->channel[chan].leds = NULL;
    }

    // Allocate the LED buffers
    for (chan = 0; chan < RPI_PWM_CHANNELS; chan++)
    {
        ws2811_channel_t *channel = &ws2811->channel[chan];

        channel->leds = malloc(sizeof(ws2811_led_t) * channel->count);
        if (!channel->leds)
        {
            error = WS2811_ERR_LEDS_MALLOC;
            goto err;
        }

        memset(channel->leds, 0, sizeof(ws2811_led_t) * channel->count);

        if (!channel->strip_type)
        {
          channel->strip_type=WS2811_STRIP_RGB;
        }
    }

    device->dma_cb = (dma_cb_t *)device->mbox.virt_addr;
    device->pwm_raw = (uint8_t *)device->mbox.virt_addr + sizeof(dma_cb_t);

    pwm_raw_init(ws2811);

    memset((dma_cb_t *)device->dma_cb, 0, sizeof(dma_cb_t));

    // Cache the DMA control block bus address
    device->dma_cb_addr = addr_to_bus(device, device->dma_cb);

    // Map the physical registers into userspace
    if (map_registers(ws2811))
    {
        error = WS2811_ERR_MAP_REGISTERS;
        goto err;
    }

    // Initialize the GPIO pins
    if (gpio_init(ws2811))
    {
        unmap_registers(ws2811);
        error = WS2811_ERR_GPIO_INIT;
        goto err;
    }

    // Setup the PWM, clocks, and DMA
    if (setup_pwm(ws2811))
    {
        unmap_registers(ws2811);
        error = WS2811_ERR_SETUP_PWM;
        goto err;
    }

    return 0;

err:
    ws2811_cleanup(ws2811);

    return error;
}

/**
 * Shut down DMA, PWM, and cleanup memory.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  None
 */
void ws2811_fini(ws2811_t *ws2811)
{
    ws2811_wait(ws2811);
    stop_pwm(ws2811);

    unmap_registers(ws2811);

    ws2811_cleanup(ws2811);
}

/**
 * Wait for any executing DMA operation to complete before returning.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  0 on success, -1 on DMA competion error
 */
int ws2811_wait(ws2811_t *ws2811)
{
    volatile dma_t *dma = ws2811->device->dma;

    while ((dma->cs & RPI_DMA_CS_ACTIVE) &&
           !(dma->cs & RPI_DMA_CS_ERROR))
    {
        usleep(10);
    }

    if (dma->cs & RPI_DMA_CS_ERROR)
    {
        fprintf(stderr, "DMA Error: %08x\n", dma->debug);
        return -1;
    }

    return 0;
}

/**
 * Render the PWM DMA buffer from the user supplied LED arrays and start the DMA
 * controller.  This will update all LEDs on both PWM channels.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  None
 */
int ws2811_render(ws2811_t *ws2811)
{
    volatile uint8_t *pwm_raw = ws2811->device->pwm_raw;
    int bitpos = 31;
    int i, k, l, chan;
    unsigned j;

    for (chan = 0; chan < RPI_PWM_CHANNELS; chan++)         // Channel
    {
        ws2811_channel_t *channel = &ws2811->channel[chan];
        int wordpos = chan;
        int scale   = (channel->brightness & 0xff) + 1;
        int wshift  = (channel->strip_type >> 24) & 0xff;
        int rshift  = (channel->strip_type >> 16) & 0xff;
        int gshift  = (channel->strip_type >> 8)  & 0xff;
        int bshift  = (channel->strip_type >> 0)  & 0xff;

        for (i = 0; i < channel->count; i++)                // Led
        {
            uint8_t color[] =
            {
                ws281x_gamma[(((channel->leds[i] >> rshift) & 0xff) * scale) >> 8], // red
                ws281x_gamma[(((channel->leds[i] >> gshift) & 0xff) * scale) >> 8], // green
                ws281x_gamma[(((channel->leds[i] >> bshift) & 0xff) * scale) >> 8], // blue
                ws281x_gamma[(((channel->leds[i] >> wshift) & 0xff) * scale) >> 8], // white
            };
            uint8_t array_size = 3; // Assume 3 color LEDs, RGB

            // If our shift mask includes the highest nibble, then we have 4
            // LEDs, RBGW.
            if (channel->strip_type & SK6812_SHIFT_WMASK)
            {
                array_size = 4;
            }

            for (j = 0; j < array_size; j++)               // Color
            {
                for (k = 7; k >= 0; k--)                   // Bit
                {
                    uint8_t symbol = SYMBOL_LOW;

                    if (color[j] & (1 << k))
                    {
                        symbol = SYMBOL_HIGH;
                    }

                    for (l = 2; l >= 0; l--)               // Symbol
                    {
                        uint32_t *wordptr = &((uint32_t *)pwm_raw)[wordpos];

                        *wordptr &= ~(1 << bitpos);
                        if (symbol & (1 << l))
                        {
                            *wordptr |= (1 << bitpos);
                        }

                        bitpos--;
                        if (bitpos < 0)
                        {
                            // Every other word is on the same channel
                            wordpos += 2;

                            bitpos = 31;
                        }
                    }
                }
            }
        }
    }

    // Wait for any previous DMA operation to complete.
    if (ws2811_wait(ws2811))
    {
        return -1;
    }

    dma_start(ws2811);

    return 0;
}

