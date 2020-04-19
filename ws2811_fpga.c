/*
 * ws2811_Fpga.c
 *
 * Copyright (c) 2014-2020 Jeremy Garff <jer @ jers.net>
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

/*
 * WORK IN PROGRESS
 *
 * This is a hardware driver for a raspberry pi hat implementation currently in progress.  The
 * new device supports up to 8 channels of LED strings operating simultaneously.
 *
 * The reason for this new hardware design and associated driver are to solve the following
 * common complaints about the RPI DMA implementation:
 *
 * - Increase the number of simultaneous channels to 8.  This is only a limitation of the current
 *   hardware.  More can be added in later board versions.
 * - Allow the user to run as non-root, which is done by giving access to the spi and gpio device
 *   by changing the device file permissions or user groups.
 * - Increase performance.  All bit shifting and timing now occurs in the hat hardware, thereby
 *   reducing the rendering time in software.  This is important for multi thousand chain LED
 *   strings.
 * - Wider hardware support.  This device should be easily applicable to non-raspberry pi
 *   boards assuming they also support SPI and GPIOs.  Various different hat implementations 
 *   are possible.
 *
 *
 * Current status:
 *
 * - Prototype board and logic up and running!
 * - Single channel tested and running.
 * - Lots of TODOs as noted in the code.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <signal.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <time.h>

#include "fpga.h"
#include "gpio.h"

#include "ws2811.h"


#define  SPI_DEVICE                              "/dev/spidev0.0"
#define  SPI_MODE                                0
#define  SPI_BITS                                8
#define  SPI_SPEED                               40000000 // 40Mhz
#define  SPI_DELAY                               0        // uS

#define  GPIO_INT                                4

#define  CHANNEL_REFRESH_CLOCKS                  50       // uS


typedef struct ws2811_device
{
    spidev_t *spidev;
    volatile gpio_t *gpio;
    ws2811_led_t *leds_raw[RPI_FPGA_CHANNELS];
    int next_bank;
    int max_count;
	int running;
} ws2811_device_t;


uint32_t fpga_channel_addr[] = {
    WS281X0_BASE,
};


/**
 * Display the controller registers for debugging purposes.
 *
 * @param    spidev  SPI device instance pointer.
 * @param    base    Address base for channel instance.
 *
 * @returns  None
 */
void ws281x_regs(spidev_t *spidev, uint32_t base) {
    uint32_t data;

    read_reg(spidev, WS281X_ADDR(base, version), &data);
    printf("version    :  %08x\n", data);
    read_reg(spidev, WS281X_ADDR(base, conf), &data);
    printf("conf       :  %08x\n", data);
    read_reg(spidev, WS281X_ADDR(base, divide), &data);
    printf("divide     :  %08x\n", data);
    read_reg(spidev, WS281X_ADDR(base, stop_count), &data);
    printf("stop_count :  %08x\n", data);
    read_reg(spidev, WS281X_ADDR(base, int_status), &data);
    printf("int_status :  %08x\n", data);
    read_reg(spidev, WS281X_ADDR(base, int_mask), &data);
    printf("int_mask   :  %08x\n", data);
    read_reg(spidev, WS281X_ADDR(base, mem0_len), &data);
    printf("mem0_len   :  %08x\n", data);
    read_reg(spidev, WS281X_ADDR(base, mem1_len), &data);
    printf("mem1_len   :  %08x\n", data);
}

/**
 * Shut down FPGA logic and cleanup memory.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  None
 */
const char * ws2811_get_return_t_str(const ws2811_return_t state)
{
    const int index = -state;
    static const char * const ret_state_str[] = { WS2811_RETURN_STATES(WS2811_RETURN_STATES_STRING) };

    if (index < (int)(sizeof(ret_state_str) / sizeof(ret_state_str[0])))
    {
        return ret_state_str[index];
    }

    return "";
}

/**
 * Shut down FPGA logic and cleanup memory.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  None
 */
void ws2811_fini(ws2811_t *ws2811) {
    int i;

    for (i = 0; i < RPI_FPGA_CHANNELS; i++) {
        ws2811_channel_t *channel = &ws2811->channel[i];

        if (channel->leds) {
            free(channel->leds);
        }
        channel->leds = NULL;

        if (channel->gamma) {
            free(channel->gamma);
        }
        channel->gamma = NULL;

        channel->strip_type = 0;
    }

    if (ws2811->device) {
        free(ws2811->device);
    }

    if (ws2811->device->gpio) {
        munmap((void *)ws2811->device->gpio, sizeof(gpio_t));
    }
}

void *gpiomap() {
    int mem_fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
    void *mem;

    if (mem_fd < 0) {
       perror("Can't open /dev/gpiomem");
       return NULL;
    }

    mem = mmap(0, sizeof(gpio_t), PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, GPIO_OFFSET);
    if (mem == MAP_FAILED) {
        perror("mmap error\n");
        return NULL;
    }

    close(mem_fd);

    return (char *)mem;
}

/**
 * Initialize the SPI interface and GPIO pin used for interrupt signalling.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  0 on success, -1 on unsupported pin
 */
ws2811_return_t ws2811_init(ws2811_t *ws2811) {
    spidev_t *spidev;
    int i;

    ws2811->device = malloc(sizeof(ws2811_device_t));
    if (!ws2811->device) {
        ws2811_fini(ws2811);
        return WS2811_ERROR_OUT_OF_MEMORY;
    }

    spidev = spi_init(SPI_DEVICE, SPI_MODE, SPI_BITS, SPI_SPEED, SPI_DELAY);
    if (!spidev) {
        ws2811_fini(ws2811);
        return WS2811_ERROR_OUT_OF_MEMORY;
    }
    ws2811->device->spidev = spidev;

    for (i = 0; i < RPI_FPGA_CHANNELS; i++) {
        ws2811_channel_t *channel = &ws2811->channel[i];

        channel->leds = malloc((sizeof(ws2811_channel_t) * channel->count));
        if (!channel->leds) {
            ws2811_fini(ws2811);
            return WS2811_ERROR_OUT_OF_MEMORY;
        }
        memset(channel->leds, 0, sizeof(ws2811_led_t) * channel->count);

        channel->leds = malloc((sizeof(ws2811_channel_t) * channel->count));
        if (!channel->leds) {
            ws2811_fini(ws2811);
            return WS2811_ERROR_OUT_OF_MEMORY;
        }
        memset(channel->leds, 0, sizeof(ws2811_led_t) * channel->count);

        ws2811->device->leds_raw[i] = malloc((sizeof(ws2811_led_t) * channel->count));
        if (!ws2811->device->leds_raw[i]) {
            ws2811_fini(ws2811);
            return WS2811_ERROR_OUT_OF_MEMORY;
        }
        memset(ws2811->device->leds_raw[i], 0, sizeof(ws2811_led_t) * channel->count);

        if (!channel->strip_type)
        {
            channel->strip_type = WS2811_STRIP_RGB;
        }

        // Set default uncorrected gamma table
        if (!channel->gamma)
        {
            channel->gamma = malloc(sizeof(uint8_t) * 256);
            int x;

            for(x = 0; x < 256; x++){
                channel->gamma[x] = x;
            }
        }

        channel->wshift = (channel->strip_type >> 24) & 0xff;
        channel->rshift = (channel->strip_type >> 16) & 0xff;
        channel->gshift = (channel->strip_type >> 8)  & 0xff;
        channel->bshift = (channel->strip_type >> 0)  & 0xff;
    }

    // Map the gpio memory
    ws2811->device->gpio = gpiomap();
    if (!ws2811->device->gpio)
    {
        ws2811_fini(ws2811);
        return WS2811_ERROR_MMAP;
    }
    gpio_output_set(ws2811->device->gpio, GPIO_INT, 0);  // Setup as input

    for (i = 0; i < RPI_FPGA_CHANNELS; i++) {
        ws2811_channel_t *channel = &ws2811->channel[i];
        int bpp = (channel->strip_type >> 24) ? 32 : 24;   // Check for white shift, if so, 32 bits

        // TODO:  Do the intialization for multiple channels
        // TODO:  Check the FPGA firmware version

        // Perform a logic reset
        write_reg(spidev, WS281X_ADDR(fpga_channel_addr[i], conf), WS281X_REGS_CONF_RESET);

        // Configure the channel speed, level translater output enable, and bits per pixel
        write_reg(spidev, WS281X_ADDR(fpga_channel_addr[i], conf), WS281X_REGS_CONF_BPW(bpp) | 
                                                                   WS281X_REGS_CONF_OUTPUT_ENABLE);

        // Configure the stop idle time to complete the refresh.  Convert from uS to clock counts.
        write_reg(spidev, WS281X_ADDR(fpga_channel_addr[i], stop_count), CHANNEL_REFRESH_CLOCKS * (LOGIC_FREQ / 1000000));

        // Configure the clock divider to generate the output PWM clock
        write_reg(spidev, WS281X_ADDR(fpga_channel_addr[i], divide), LOGIC_FREQ / FPGA_TARGET_FREQ);

        // Set the next bank to 0
        ws2811->device->next_bank = 0;
        ws2811->device->running = 0;
    }

    return WS2811_SUCCESS;
}

/**
 * Wait for any executing hardware operations to complete before returning.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  0 on success, -1 on DMA competion error
 */
ws2811_return_t ws2811_wait(ws2811_t *ws2811) {
    ws2811_device_t *device = ws2811->device;
    spidev_t *spidev = device->spidev;
    int i;

    while (ws2811->device->running && !gpio_level_get(ws2811->device->gpio, GPIO_INT))
        ;

    for (i = 0; i < RPI_FPGA_CHANNELS; i++) {
        uint32_t len_addr = WS281X_ADDR(fpga_channel_addr[i], mem0_len);

        if (!device->next_bank) {
            len_addr = WS281X_ADDR(fpga_channel_addr[i], mem1_len);
        }

        write_reg(spidev, len_addr, 0);
    }

    ws2811->device->running = 0;

    // TODO:  Setup next bank of data if needed
    // TODO:  Figure out how to make this actually interrupt driven and not burn cpu

    return WS2811_SUCCESS;
}

/**
 * Render the buffer from the user supplied LED arrays.  Bit rendering is handled
 * in hardware, so all we need to do here is remap the colors and apply the gamma.
 *
 * @param    ws2811  ws2811 instance pointer.
 *
 * @returns  None
 */
ws2811_return_t  ws2811_render(ws2811_t *ws2811) {
    int chnum;

    ws2811_wait(ws2811);

    for (chnum = 0; chnum < RPI_FPGA_CHANNELS; chnum++) {
        ws2811_channel_t *channel = &ws2811->channel[chnum];
        ws2811_device_t *device = ws2811->device;
        ws2811_led_t *leds_raw = ws2811->device->leds_raw[chnum];
        const int scale = (channel->brightness & 0xff) + 1;
        uint32_t bank_addr, len_addr, count;
        int i;

        // TODO:  Implement hardware based LED strip color type, in the meantime do it in software
        for (i = 0; i < channel->count; i++) {
            uint8_t color[] =  {
                channel->gamma[(((channel->leds[i] >> channel->rshift) & 0xff) * scale) >> 8], // red
                channel->gamma[(((channel->leds[i] >> channel->gshift) & 0xff) * scale) >> 8], // green
                channel->gamma[(((channel->leds[i] >> channel->bshift) & 0xff) * scale) >> 8], // blue
                channel->gamma[(((channel->leds[i] >> channel->wshift) & 0xff) * scale) >> 8], // white
            };

            // TODO:  The following should take endianness into account
            memcpy(&leds_raw[i], color, sizeof(leds_raw[i]));
        }

        // TODO:  Write the data into the device, but only the first buffers entries.
        count = channel->count;
        if (count > BUF_LEN) {
            count = BUF_LEN;
        }

        // Get the buffer address and length
        bank_addr = WS281X_ADDR(fpga_channel_addr[chnum], mem0);
        len_addr = WS281X_ADDR(fpga_channel_addr[chnum], mem0_len);
        if (device->next_bank) {
            bank_addr = WS281X_ADDR(fpga_channel_addr[chnum], mem1);
            len_addr = WS281X_ADDR(fpga_channel_addr[chnum], mem1_len);
        }

        // Setup for next bank
        device->next_bank = !device->next_bank;
        ws2811->device->running = 1;

        // Write into the destination buffer
        write_data(device->spidev, bank_addr, count, leds_raw);

        // Fire it up
        write_reg(device->spidev, len_addr, count | WS281X_REGS_MEM0_LEN_STOP);
    }

    return WS2811_SUCCESS;
}


