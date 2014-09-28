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

#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"

#include "ws2811.h"


#define OSC_FREQ                                 19200000   // crystal frequency

#define PAGE_SIZE                                (1 << 12)
#define PAGE_MASK                                (~(PAGE_SIZE - 1))
#define PAGE_OFFSET(page)                        (page & (PAGE_SIZE - 1))
#define MAX_PAGES                                (PAGE_SIZE / sizeof(dma_cb_t))

/* 3 colors, 8 bits per byte, 3 symbols per bit + 55uS low for reset signal */
#define LED_RESET_uS                             55
#define LED_BIT_COUNT(leds, freq)                ((leds * 3 * 8 * 3) + ((LED_RESET_uS * \
                                                  (freq * 3)) / 1000000))

// Pad out to the nearest uint32 + 32-bits for idle low
#define PWM_BYTE_COUNT(leds, freq)               ((((LED_BIT_COUNT(leds, freq) >> 3) & ~0x7) + 4) + 4)

#define SYMBOL_HIGH                              0x6  // 1 1 0
#define SYMBOL_LOW                               0x4  // 1 0 0

#define ARRAY_SIZE(stuff)                        (sizeof(stuff) / sizeof(stuff[0]))


typedef struct dma_page
{
    struct dma_page *next;
    struct dma_page *prev;
    void *addr;
} dma_page_t;

typedef struct ws2811_device
{
    volatile uint8_t *pwm_raw;
    volatile dma_t *dma;
    volatile pwm_t *pwm;
    volatile dma_cb_t *dma_cb;
    uint32_t dma_cb_addr;
    dma_page_t page_head;
    volatile gpio_t *gpio;
    volatile cm_pwm_t *cm_pwm;
} ws2811_device_t;


typedef struct
{
    int pin;
    int altnum;
} pwm_pin_table_t;

// Mapping of Pin to alternate function for PWM channel 0
const static pwm_pin_table_t pwm_pin_table[] =
{
    {
        .pin = 12,
        .altnum = 0,
    },
    {
        .pin = 18,
        .altnum = 5,
    },
    {
        .pin = 40,
        .altnum = 0,
    },
    {
        .pin = 52,
        .altnum = 1,
    },
};

// DMA address mapping by DMA number index
const static uint32_t dma_addr[] =
{
    DMA0,
    DMA1,
    DMA2,
    DMA3,
    DMA4,
    DMA5,
    DMA6,
    DMA7,
    DMA8,
    DMA9,
    DMA10,
    DMA11,
    DMA12,
    DMA13,
    DMA14,
    DMA15,
};


// ARM gcc built-in function, fortunately works when root w/virtual addrs
void __clear_cache(char *begin, char *end);


static dma_page_t *dma_page_add(dma_page_t *head, void *addr)
{
    dma_page_t *page = malloc(sizeof(dma_page_t));

    if (!page)
    {
        return NULL;
    }

    page->next = head;
    page->prev = head->prev;

    head->prev->next = page;
    head->prev = page;

    page->addr = addr;

    return page;
}

static void dma_page_remove(dma_page_t *page)
{
    page->prev->next = page->next;
    page->next->prev = page->prev;

    free(page);
}

static void dma_page_remove_all(dma_page_t *head)
{
    while (head->next != head)
    {
        dma_page_remove(head->next);
    }
}

static dma_page_t *dma_page_next(dma_page_t *head, dma_page_t *page)
{
    if (page->next != head)
    {
        return page->next;
    }

    return NULL;
}

static void dma_page_init(dma_page_t *page)
{
    memset(page, 0, sizeof(*page));

    page->next = page;
    page->prev = page;
}

static void *dma_alloc(dma_page_t *head, uint32_t size)
{
    uint32_t pages = (size / PAGE_SIZE) + 1;
    void *vaddr;
    int i;

    vaddr = mmap(NULL, pages * PAGE_SIZE,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS | MAP_NORESERVE |
                 MAP_LOCKED, -1, 0);
    if (vaddr == MAP_FAILED)
    {
        perror("dma_alloc() mmap() failed");
        return NULL;
    }

    for (i = 0; i < pages; i++)
    {
        if (!dma_page_add(head, &((uint8_t *)vaddr)[PAGE_SIZE * i]))
        {
            dma_page_remove_all(head);
            munmap(vaddr, pages * PAGE_SIZE);
            return NULL;
        }
    }

    return vaddr;
}

static dma_cb_t *dma_desc_alloc(uint32_t descriptors)
{
    uint32_t pages = ((descriptors * sizeof(dma_cb_t)) / PAGE_SIZE);
    dma_cb_t *vaddr;

    if (pages > 1)
    {
        return NULL;
    }

    vaddr = mmap(NULL, pages * PAGE_SIZE,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS | MAP_NORESERVE |
                 MAP_LOCKED, -1, 0);
    if (vaddr == MAP_FAILED)
    {
        perror("dma_desc_alloc() mmap() failed");
        return NULL;
    }

    return vaddr;
}

static void dma_page_free(void *buffer, const uint32_t size)
{
    uint32_t pages = (size / PAGE_SIZE) + 1;

    munmap(buffer, pages * PAGE_SIZE);
}

static void *map_device(const uint32_t phys, const uint32_t len)
{
    uint32_t start_page_addr = phys & PAGE_MASK;
    uint32_t end_page_addr = (phys + len) & PAGE_MASK;
    uint32_t pages = end_page_addr - start_page_addr + 1;
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    void *virt;

    if (fd < 0)
    {
        perror("Can't open /dev/mem");
        close(fd);
        return NULL;
    }

    virt = mmap(NULL, PAGE_SIZE * pages, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                start_page_addr);
    if (virt == MAP_FAILED)
    {
        perror("map_device() mmap() failed");
        close(fd);
        return NULL;
    }

    close(fd);

    return (void *)(((uint8_t *)virt) + PAGE_OFFSET(phys));
}

static void unmap_device(volatile void *addr, const uint32_t len)
{
    uint32_t virt = (uint32_t)addr;
    uint32_t start_page_addr = virt & PAGE_MASK;
    uint32_t end_page_addr = (virt + len) & PAGE_MASK;
    uint32_t pages = end_page_addr - start_page_addr + 1;

    munmap((void *)addr, PAGE_SIZE * pages);
}

static int map_registers(ws2811_t *ws2811)
{
    ws2811_device_t *device = ws2811->device;

    if (ws2811->dmanum >= ARRAY_SIZE(dma_addr))
    {
        return -1;
    }

    device->dma = map_device(dma_addr[ws2811->dmanum], sizeof(dma_t));
    if (!device->dma)
    {
        return -1;
    }

    device->pwm = map_device(PWM, sizeof(pwm_t));
    if (!device->pwm)
    {
        return -1;
    }

    device->gpio = map_device(GPIO, sizeof(gpio_t));
    if (!device->gpio)
    {
        return -1;
    }

    device->cm_pwm = map_device(CM_PWM, sizeof(cm_pwm_t));
    if (!device->cm_pwm)
    {
        return -1;
    }

    return 0;
}

static void unmap_registers(ws2811_t *ws2811)
{
    ws2811_device_t *device = ws2811->device;

    if (device->dma)
    {
        unmap_device(device->dma, sizeof(dma_t));
    }

    if (device->pwm)
    {
        unmap_device(device->pwm, sizeof(pwm_t));
    }

    if (device->cm_pwm)
    {
        unmap_device(device->cm_pwm, sizeof(cm_pwm_t));
    }

    if (device->gpio)
    {
        unmap_device(device->gpio, sizeof(gpio_t));
    }
}

static uint32_t addr_to_bus(const volatile void *addr)
{
    char filename[40];
    uint64_t pfn;
    int fd;

    sprintf(filename, "/proc/%d/pagemap", getpid());
    fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        perror("addr_to_bus() can't open pagemap");
        return ~0UL;
    }

    if (lseek(fd, (uint32_t)addr >> 9, SEEK_SET) != 
        (uint32_t)addr >> 9)
    {
        perror("addr_to_bus() lseek() failed");
        close(fd);
        return ~0UL;
    }

    if (read(fd, &pfn, sizeof(pfn)) != sizeof(pfn))
    {
        perror("addr_to_bus() read() failed");
        close(fd);
        return ~0UL;
    }

    close(fd);

    return ((uint32_t)pfn << 12) | 0x40000000 | ((uint32_t)addr & 0xfff);
}

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

static int setup_pwm(ws2811_t *ws2811)
{
    ws2811_device_t *device = ws2811->device;
    volatile dma_t *dma = device->dma;
    volatile dma_cb_t *dma_cb = device->dma_cb;
    volatile pwm_t *pwm = device->pwm;
    volatile cm_pwm_t *cm_pwm = device->cm_pwm;
    int count = ws2811->count;
    uint32_t freq = ws2811->freq;
    int invert = ws2811->invert;
    dma_page_t *page;
    int32_t byte_count;

    stop_pwm(ws2811);

    // Setup the PWM Clock - Use OSC @ 19.2Mhz w/ 3 clocks/tick
    cm_pwm->div = CM_PWM_DIV_PASSWD | CM_PWM_DIV_DIVI(OSC_FREQ / (3 * freq));
    cm_pwm->ctl = CM_PWM_CTL_PASSWD | CM_PWM_CTL_SRC_OSC;
    cm_pwm->ctl = CM_PWM_CTL_PASSWD | CM_PWM_CTL_SRC_OSC | CM_PWM_CTL_ENAB;
    usleep(10);
    while (!(cm_pwm->ctl & CM_PWM_CTL_BUSY))
        ;

    // Setup the PWM, use delays as the block is rumored to lock without them
    pwm->rng1 = 32;  // 32-bits per word to serialize
    usleep(10);
    pwm->ctl = RPI_PWM_CTL_CLRF1;
    usleep(10);
    pwm->dmac = RPI_PWM_DMAC_ENAB | RPI_PWM_DMAC_PANIC(7) | RPI_PWM_DMAC_DREQ(3);
    usleep(10);
    pwm->ctl = RPI_PWM_CTL_USEF1 | RPI_PWM_CTL_MODE1;
    if (invert)
    {
        pwm->ctl |= RPI_PWM_CTL_POLA1;
    }
    pwm->ctl |= RPI_PWM_CTL_PWEN1;
    usleep(10);

    // Initialize the DMA control blocks to chain together all the DMA pages
    page = &device->page_head;
    byte_count = PWM_BYTE_COUNT(count, freq);
    while ((page = dma_page_next(&device->page_head, page)) &&
           byte_count)
    {
        int32_t page_bytes = PAGE_SIZE < byte_count ? PAGE_SIZE : byte_count;

        dma_cb->ti = RPI_DMA_TI_NO_WIDE_BURSTS |  // 32-bit transfers
                     RPI_DMA_TI_WAIT_RESP |       // wait for write complete
                     RPI_DMA_TI_DEST_DREQ |       // user peripheral flow control
                     RPI_DMA_TI_PERMAP(5) |       // PWM peripheral
                     RPI_DMA_TI_SRC_INC;          // Increment src addr

        dma_cb->source_ad = addr_to_bus(page->addr);
        if (dma_cb->source_ad == ~0L)
        {
            return -1;
        }

        dma_cb->dest_ad = (uint32_t)&((pwm_t *)PWM_PERIPH)->fif1;
        dma_cb->txfr_len = page_bytes;
        dma_cb->stride = 0;
        dma_cb->nextconbk = addr_to_bus(dma_cb + 1);

        byte_count -= page_bytes;
        if (!dma_page_next(&device->page_head, page))
        {
            break;
        }

        dma_cb++;
    }

    // Terminate the final control block to stop DMA
    dma_cb->nextconbk = 0;

    dma->cs = 0;
    dma->txfr_len = 0;

    return 0;
}

static void dma_start(ws2811_t *ws2811)
{
    ws2811_device_t *device = ws2811->device;
    volatile dma_t *dma = device->dma;
    uint32_t dma_cb_addr = device->dma_cb_addr;

    dma->conblk_ad = dma_cb_addr;
    dma->cs = RPI_DMA_CS_WAIT_OUTSTANDING_WRITES |
              RPI_DMA_CS_PANIC_PRIORITY(15) | 
              RPI_DMA_CS_PRIORITY(15) |
              RPI_DMA_CS_ACTIVE;
}

static int gpio_init(ws2811_t *ws2811)
{
    volatile gpio_t *gpio = ws2811->device->gpio;
    int i;

    for (i = 0; i < ARRAY_SIZE(pwm_pin_table); i++)
    {
        if (pwm_pin_table[i].pin == ws2811->gpionum)
        {
            gpio_function_set(gpio, ws2811->gpionum, pwm_pin_table[i].altnum);

            return 0;
        }
    }

    return -1;
}

// TODO:  Make nasty initialization code more elegant
int ws2811_init(ws2811_t *ws2811)
{
    ws2811->device = malloc(sizeof(*ws2811->device));
    if (!ws2811->device)
    {
        return -1;
    }

    dma_page_init(&ws2811->device->page_head);

    // Allocate the LED buffers
    ws2811->leds = malloc(sizeof(*ws2811->leds) * ws2811->count);
    if (!ws2811->leds)
    {
        free(ws2811->device);
        return -1;
    }
    memset(ws2811->leds, 0, sizeof(*ws2811->leds) * ws2811->count);

    // Allocate the DMA buffer
    ws2811->device->pwm_raw = dma_alloc(&ws2811->device->page_head, PWM_BYTE_COUNT(ws2811->count, ws2811->freq));
    if (!ws2811->device->pwm_raw)
    {
        free(ws2811->leds);
        free(ws2811->device);
        return -1;
    }
    memset((uint8_t *)ws2811->device->pwm_raw, 0, PWM_BYTE_COUNT(ws2811->count, ws2811->freq));

    // Allocate the DMA control block
    ws2811->device->dma_cb = dma_desc_alloc(MAX_PAGES);
    if (!ws2811->device->dma_cb)
    {
        dma_page_free((uint8_t *)ws2811->device->pwm_raw, PWM_BYTE_COUNT(ws2811->count, ws2811->freq));
        free(ws2811->leds);
        free(ws2811->device);
        return -1;
    }
    memset((dma_cb_t *)ws2811->device->dma_cb, 0, sizeof(dma_cb_t));

    // Cache the DMA control block bus address
    ws2811->device->dma_cb_addr = addr_to_bus(ws2811->device->dma_cb);
    if (ws2811->device->dma_cb_addr == ~0L)
    {
        dma_page_free((uint8_t *)ws2811->device->pwm_raw, PWM_BYTE_COUNT(ws2811->count, ws2811->freq));
        dma_page_free((dma_cb_t *)ws2811->device->dma_cb, sizeof(dma_cb_t));
        free(ws2811->leds);
        free(ws2811->device);
        return -1;
    }

    // Map the physical registers into userspace
    if (map_registers(ws2811))
    {
        dma_page_free((uint8_t *)ws2811->device->pwm_raw, PWM_BYTE_COUNT(ws2811->count, ws2811->freq));
        dma_page_free((dma_cb_t *)ws2811->device->dma_cb, sizeof(dma_cb_t));
        free(ws2811->leds);
        free(ws2811->device);
        return -1;
    }

    // Map the GPIO pin
    if (gpio_init(ws2811))
    {
        unmap_registers(ws2811);
        dma_page_free((uint8_t *)ws2811->device->pwm_raw, PWM_BYTE_COUNT(ws2811->count, ws2811->freq));
        dma_page_free((dma_cb_t *)ws2811->device->dma_cb, sizeof(dma_cb_t));
        free(ws2811->leds);
        free(ws2811->device);
        return -1;
    }

    // Setup the PWM and clocks
    if (setup_pwm(ws2811))
    {
        unmap_registers(ws2811);
        dma_page_free((uint8_t *)ws2811->device->pwm_raw, PWM_BYTE_COUNT(ws2811->count, ws2811->freq));
        dma_page_free((dma_cb_t *)ws2811->device->dma_cb, sizeof(dma_cb_t));
        free(ws2811->leds);
        free(ws2811->device);
        return -1;
    }

    return 0;
}

void ws2811_fini(ws2811_t *ws2811)
{
    ws2811_wait(ws2811);
    stop_pwm(ws2811);

    unmap_registers(ws2811);

    dma_page_remove_all(&ws2811->device->page_head);
    dma_page_free((dma_cb_t *)ws2811->device->dma_cb, sizeof(*ws2811->device->dma_cb));
    dma_page_free((uint8_t *)ws2811->device->pwm_raw, PWM_BYTE_COUNT(ws2811->count,
                   ws2811->freq));

    free(ws2811->leds);
    free(ws2811->device);
}

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

int ws2811_render(ws2811_t *ws2811)
{
    volatile uint8_t *pwm_raw = ws2811->device->pwm_raw;
    int bitpos = 31, wordpos = 0;
    int i, j, k, l;

    for (i = 0; i < ws2811->count; i++)          // Led
    {
        uint8_t color[] =
        {
            (ws2811->leds[i] >> 8) & 0xff,       // green
            (ws2811->leds[i] >> 16) & 0xff,      // red
            (ws2811->leds[i] >> 0) & 0xff,       // blue
        };

        for (j = 0; j < ARRAY_SIZE(color); j++)  // Color
        {
            for (k = 7; k >= 0; k--)             // Bit
            {
                uint8_t symbol = SYMBOL_LOW;

                if (color[j] & (1 << k))
                {
                    symbol = SYMBOL_HIGH;
                }

                for (l = 2; l >= 0; l--)         // Symbol
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
                        wordpos++;
                        bitpos = 31;
                    }
                }
            }
        }
    }

    __clear_cache((char *)pwm_raw,
                  (char *)&pwm_raw[PWM_BYTE_COUNT(ws2811->count, ws2811->freq)]);

    if (ws2811_wait(ws2811))
    {
        return -1;
    }

    dma_start(ws2811);

    return 0;
}

