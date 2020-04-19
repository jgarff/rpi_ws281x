/*
 * fpga.h
 *
 * Copyright (c) 2020 Jeremy Garff <jer @ jers.net>
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


#ifndef __FPGA_H__
#define __FPGA_H__


#define ARRAY_SIZE(val)                          (sizeof(val) / sizeof(val[0]))
#define DEVICE_NAME_LEN                          120

#define LOGIC_FREQ                               60000000
#define FPGA_TARGET_FREQ                         (800000 * 3)  // 3 transitions per PWM clock

#define RPI_FPGA_CHANNELS                        1

#define SPI_CMD_WRITE                            (1 << 31)
#define SPI_CMD_READ                             (1 << 30)
#define SPI_CMD_INCREMENT                        (1 << 29)
#define SPI_CMD_ADDR_MASK                        0xffffff

#define BUF_LEN                                  1024

#define WS281X0_BASE                             0x000000
#define WS281X1_BASE                             0x010000
#define WS281X2_BASE                             0x020000
#define WS281X3_BASE                             0x030000
#define PLL_BASE                                 0x0d0000
#define DUAL_BOOT_BASE                           0x0e0000
#define FLASH_CSR                                0x0f0000
#define FLASH_BASE                               0x100000
#define FLASH_ICB                                FLASH_BASE
#define FLASH_UFM                                (FLASH_BASE + 0x800)
#define FLASH_CFM1                               (FLASH_BASE + 0x8800)
#define FLASH_CFM0                               (FLASH_BASE + 0x2b800)

#define FLASH_SECTOR_UFM0                        0
#define FLASH_SECTOR_UFM1                        1
#define FLASH_SECTOR_CFM2                        2
#define FLASH_SECTOR_CFM1                        3
#define FLASH_SECTOR_CFM0                        4

typedef struct {
    char device[DEVICE_NAME_LEN];
    int fd;
    uint8_t mode;
    uint8_t bits;       // 8 or 9
    uint32_t speed;     // In Khz
    uint16_t delay;
} spidev_t; 

typedef struct {
    uint32_t cmdaddr;
    uint32_t data;
} __attribute__ ((packed)) spiseq_wr_t;

typedef struct {
    uint32_t cmdaddr;
    uint32_t cmd_echo;
    uint32_t data;
} __attribute__ ((packed)) spiseq_rd_t;


spidev_t *spi_init(char *device, uint8_t mode, uint8_t bits, uint32_t speed, uint16_t delay);

int spi_xfer(spidev_t *device, int len, uint8_t *tx, uint8_t *rx);

int read_reg(spidev_t *spidev, uint32_t addr, uint32_t *data);
int write_reg(spidev_t *spidev, uint32_t addr, uint32_t data);
int write_data(spidev_t *spidev, uint32_t addr, uint32_t len, uint32_t *data);


#define WS281X_BUF_COUNT                         BUF_LEN
typedef struct {
    uint32_t version;
    uint32_t conf;
#define WS281X_REGS_CONF_BPW(val)                ((val & 0x3f) << 0)
#define WS281X_REGS_CONF_IDLE_HIGH               (1 << 8)
#define WS281X_REGS_CONF_OUTPUT_ENABLE           (1 << 9)
#define WS281X_REGS_CONF_RESET                   (1 << 31)
    uint32_t divide;
    uint32_t stop_count;
    uint32_t int_status;
#define WS281X_REGS_INT_STATUS_BANK0             (1 << 0)
#define WS281X_REGS_INT_STATUS_BANK1             (1 << 1)
#define WS281X_REGS_INT_STATUS_BANK_NEXT         (1 << 31)
    uint32_t int_mask;
#define WS281X_REGS_INT_MASK_BANK0               (1 << 0)
#define WS281X_REGS_INT_MASK_BANK1               (1 << 1)
    uint32_t mem0_len;
#define WS281X_REGS_MEM0_LEN_STOP                (1 << 31)
    uint32_t mem1_len;
#define WS281X_REGS_MEM1_LEN_STOP                (1 << 31)
    uint32_t resvd_0x1c[0x3f8];
    uint32_t mem0[WS281X_BUF_COUNT];
    uint32_t mem1[WS281X_BUF_COUNT];
} __attribute__ ((packed)) ws281x_regs_t;

#define WS281X_ADDR(base, field)                 (uint32_t)&(((ws281x_regs_t *)base)->field)


typedef struct {
    uint32_t status;
#define MAX10_CSR_STATUS_MASK                    0x3
#define MAX10_CSR_STATUS_SHIFT                   0
#define MAX10_CSR_STATUS_IDLE                    (0x0 < MAX10_CSR_STATUS_SHIFT)
#define MAX10_CSR_STATUS_BUSY_ERASE              (0x1 < MAX10_CSR_STATUS_SHIFT)
#define MAX10_CSR_STATUS_BUSY_WRITE              (0x2 < MAX10_CSR_STATUS_SHIFT)
#define MAX10_CSR_STATUS_BUSY_READ               (0x3 < MAX10_CSR_STATUS_SHIFT)
#define MAX10_CSR_STATUS_READ_SUCCESS            (1 << 2)
#define MAX10_CSR_STATUS_WRITE_SUCCESS           (1 << 3)
#define MAX10_CSR_STATUS_ERASE_SUCCESS           (1 << 4)
#define MAX10_CSR_STATUS_SECTOR_PROT(val)        (val << 5)
    uint32_t control;
#define MAX10_CSR_CONTROL_PAGE_MASK              0xfffff
#define MAX10_CSR_CONTROL_PAGE_ERASE(page)       (page & MAX10_CSR_CONTROL_MASK)   // 32-bit word addr
#define MAX10_CSR_CONTROL_SECTOR_MASK            0x7
#define MAX10_CSR_CONTROL_SECTOR_SHIFT           20
#define MAX10_CSR_CONTROL_SECTOR_ERASE(sector)   ((sector & MAX10_CSR_CONTROL_SECTOR_MASK) << \
                                                  MAX10_CSR_CONTROL_SECTOR_SHIFT)
#define MAX10_CSR_CONTROL_WP_MASK                0x1f
#define MAX10_CSR_CONTROL_WP_SHIFT               23
#define MAX10_CSR_CONTROL_WP(sector)             ((1 << sector) << MAX10_CSR_CONTROL_WP_SHIFT)
} __attribute__ ((packed)) max10_ufw_t;

#define MAX10_CSR_ADDR(field)                    (uint32_t)&(((max10_ufw_t *)FLASH_CSR)->field)


#endif /* __FPGA_H__ */
