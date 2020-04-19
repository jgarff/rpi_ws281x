/*
 * fpga.c
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


#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <arpa/inet.h>

#include "fpga.h"


/**
 * Initialize the SPI interface using the proper SPI parameters.
 *
 * @param    device  SPI device file name pointer.
 * @param    mode    SPI protocol mode number.
 * @param    bits    Number of bits per word.
 * @param    speed   SPI frequency in hz.
 * @param    delay   SPI CS delay in uS.
 *
 * @returns  Allocated and initialized SPI device structure pointer.
 */
spidev_t *spi_init(char *device, uint8_t mode, uint8_t bits, uint32_t speed, uint16_t delay) {
    spidev_t *spidev;
    int result, fd;

    spidev = malloc(sizeof(*spidev));
    if (!spidev) {
        return NULL;
    }

    strncpy(spidev->device, device, sizeof(spidev->device) - 1);
    spidev->delay = delay;

    fd = open(spidev->device, O_RDWR);
    if (fd < 0) {
        perror("open");
        goto unwind1;
    }
    spidev->fd = fd;

    // Mode
    result = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if (result == -1) {
        goto unwind2;
    }

    result = ioctl(fd, SPI_IOC_RD_MODE, &mode);
    if (result == -1) {
        goto unwind2;
    }
    spidev->mode = mode;

    // BPW
    result = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (result == -1) {
        goto unwind2;
    }

    result = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (result == -1) {
        goto unwind2;
    }
    spidev->bits = bits;

    // Speed
    result = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (result == -1) {
        goto unwind2;
    }

    result = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (result == -1) {
        goto unwind2;
    }
    spidev->speed = speed;

    return spidev;

unwind2:
    close(fd);

unwind1:
    free(spidev);

    return NULL;
}

/**
 * Given a RX and TX byte buffer, perform the SPI transfer.
 *
 * @param    device  SPI device file name pointer.
 * @param    len     Buffer length in bytes.
 * @param    tx      Transmit buffer pointer.
 * @param    rx      Receive buffer pointer.
 *
 * @returns  Length in bytes of completed transfer.  -1 on failure.
 */
int spi_xfer(spidev_t *device, int len, uint8_t *tx, uint8_t *rx)
{
	struct spi_ioc_transfer xfer[] = {
        {
            .tx_buf = (unsigned long)tx,
            .rx_buf = (unsigned long)rx,
            .delay_usecs = device->delay,
            .speed_hz = device->speed,
            .bits_per_word = device->bits,
            .len = len,
        },
	};
    int result;

    result = ioctl(device->fd, SPI_IOC_MESSAGE(ARRAY_SIZE(xfer)), &xfer);  // Do 1 SPI transfer of the above
    if (result < 1) {
        return -1;
    }

    return len;
}

/**
 * Write a 32-bit data value to a register at the provided address.
 *
 * @param    device  SPI device file name pointer.
 * @param    addr    Register address.
 * @param    data    Data to write to register.
 *
 * @returns  0 on success.  -1 on failure.
 */
int write_reg(spidev_t *spidev, uint32_t addr, uint32_t data) {
    spiseq_wr_t cmd = {
        .cmdaddr = SPI_CMD_WRITE | (addr & SPI_CMD_ADDR_MASK),
        .data = data,
    };
    spiseq_wr_t reply;
    int result;

    result = spi_xfer(spidev, sizeof(cmd), (uint8_t *)&cmd, (uint8_t *)&reply);
    if (result == -1) {
        return -1;
    }

    return 0;
}

/**
 * Read a 32-bit data value from a register at the provided address.
 *
 * @param    device  SPI device file name pointer.
 * @param    addr    Register address.
 * @param    data    Data pointer for read data.
 *
 * @returns  0 on success.  -1 on failure.
 */
int read_reg(spidev_t *spidev, uint32_t addr, uint32_t *data) {
    spiseq_rd_t cmd = {
        .cmdaddr = SPI_CMD_READ | (addr & SPI_CMD_ADDR_MASK),
    };
    spiseq_rd_t reply;
    int result;

    result = spi_xfer(spidev, sizeof(cmd), (uint8_t *)&cmd, (uint8_t *)&reply);
    if (result == -1) {
        return -1;
    }

    *data = reply.data;

    return 0;
}

/**
 * Write a array of 32-bit values starting at the provided address.
 *
 * @param    device  SPI device file name pointer.
 * @param    addr    Register address to start from.
 * @param    len     Length in 32-bit words for the write.
 * @param    data    Data pointer for write data.
 *
 * @returns  0 on success.  -1 on failure.
 */
int write_data(spidev_t *spidev, uint32_t addr, uint32_t len, uint32_t *data) {
    uint32_t txdata[len + 1];
    uint32_t rxdata[len + 1];
    int result;

    txdata[0] = SPI_CMD_WRITE | SPI_CMD_INCREMENT | (addr & SPI_CMD_ADDR_MASK);
    memcpy(&txdata[1], data, sizeof(uint32_t) * len);

    result = spi_xfer(spidev, sizeof(uint32_t) * (len + 1), (uint8_t *)txdata, (uint8_t *)rxdata);
    if (result == -1) {
        return -1;
    }
    
    return 0;
}

