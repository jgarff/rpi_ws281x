#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "rp1_ws281x_pwm.h"


#define VERSION                                  0x00000100
#define TESTBUF_SIZE                             1024


typedef struct {
    int fd;
} rp1_ws281x_pwm_dev_t;


int rp1_ws281x_pwm_init(rp1_ws281x_pwm_dev_t *dev) {
    int fd = open("/dev/" DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        perror("open()");
        return -1;
    }

    dev->fd = fd;

    return 0;
}

void rp1_ws281x_pwm_destroy(rp1_ws281x_pwm_dev_t *dev) {
    close(dev->fd);
}

int rp1_ws281x_pwm_reg_read(rp1_ws281x_pwm_dev_t *dev, uint32_t addr, uint32_t *data) {
    rp1_ws281x_pwm_ioctl_reg_t reg = {
        .reg_offset = addr,
    };
    int result;

    result = ioctl(dev->fd, RP1_WS281X_PWM_IOCTL_REG_READ, &reg);
    if (result) {
        perror("ioctl()");
        return result;
    }

    *data = reg.reg_value;

    return result;
}

int rp1_ws281x_pwm_reg_write(rp1_ws281x_pwm_dev_t *dev, uint32_t addr, uint32_t data) {
    rp1_ws281x_pwm_ioctl_reg_t reg = {
        .reg_offset = addr,
        .reg_value = data,
    };
    int result;

    result = ioctl(dev->fd, RP1_WS281X_PWM_IOCTL_REG_WRITE, &reg);
    if (result) {
        perror("ioctl()");
    }

    return result;
}

int rp1_ws281x_pwm_testpattern(rp1_ws281x_pwm_dev_t *dev) {
    uint32_t test[32];
    int i, result;
    int done = 0;

    // Incrementing test pattern
    for (i = 0; i < (sizeof(test) / sizeof(test[0])); i++) {
        test[i] = i;
    }
    test[32 - 1] = 0;

    while (!done) {
        result = write(dev->fd, test, sizeof(test));
        if (result != sizeof(test)) {
            perror("write()");
        }
    }

    return result;
}

char *short_opts = "vhwra:d:t";

void usage(char *exename) {
    printf("%s [-h] [-v] [-r <-a <addr>>] [-w <-a <addr> -d <value>>] [-t] \n", exename);
}

int main(int argc, char *argv[]) {
    int readcmd = 0, writecmd = 0, datacmd = 0, testpattern = 0;
    rp1_ws281x_pwm_dev_t rp1_ws281x_pwm_dev;
    uint32_t addr = -1;
    uint32_t data;
    int opt, result = 0;

    while ((opt = getopt(argc, argv, short_opts)) != -1) {
        switch (opt) {
            case 'h':
                usage(argv[0]);
                return 0;

            case 'a':
                addr = strtoul(optarg, NULL, 0);
                break;

            case 'd':
                data = strtoul(optarg, NULL, 0);
                datacmd = 1;
                break;

            case 'r':
                readcmd = 1;
                break;

            case 'w':
                writecmd = 1;
                break;

            case 't':
                testpattern = 1;
                break;

            case 'v':
                printf("Version: %08x\n", VERSION);
                return 0;

            default:
                usage(argv[0]);
                return -1;
        }
    }

    if (!writecmd && !readcmd && !testpattern) {
        fprintf(stderr, "Must specify read or write command\n");
        return -1;
    }

    if (writecmd && readcmd) {
        fprintf(stderr, "Must specify either read or write command\n");
        return -1;
    }

    if (rp1_ws281x_pwm_init(&rp1_ws281x_pwm_dev)) {
        return -1;
    }

    if (testpattern) {
        rp1_ws281x_pwm_testpattern(&rp1_ws281x_pwm_dev);
    }

    if (readcmd) {
        if (addr == -1) {
            fprintf(stderr, "Must specify address\n");
            result = -1;
            goto done;
        }

        if (rp1_ws281x_pwm_reg_read(&rp1_ws281x_pwm_dev, addr, &data)) {
            result = -1;
            goto done;
        }

        printf("%08x: %08x\n", addr, data);
    }

    if (writecmd) {
        if (addr == -1) {
            fprintf(stderr, "Must specify address\n");
            result = -1;
            goto done;
        }
        if (!datacmd) {
            fprintf(stderr, "Must specify data value\n");
            result = -1;
            goto done;
        }

        if (rp1_ws281x_pwm_reg_write(&rp1_ws281x_pwm_dev, addr, data)) {
            result = -1;
            goto done;
        }
    }

done:
    rp1_ws281x_pwm_destroy(&rp1_ws281x_pwm_dev);

    return result;
}

