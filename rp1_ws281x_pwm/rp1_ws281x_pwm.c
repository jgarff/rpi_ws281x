/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024 Jeremy Garff <jer @ jers.net>
 *
 * All rights reserved.
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


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_dma.h>
#include <linux/dma-mapping.h>

#include "rp1_ws281x_pwm.h"


#define RP1_WS281X_PWM_DRIVER_VERSION            0x00000100

//
// PWM Hardware Register Structures
// See https://datasheets.raspberrypi.com/rp1/rp1-peripherals.pdf
//
typedef struct {
    uint32_t ctrl;
#define RP1_PWM_REGS_CHAN_CTRL_MODE_0            ((0x0 & 0x7) << 0)
#define RP1_PWM_REGS_CHAN_CTRL_MODE_TEMS         ((0x1 & 0x7) << 0)
#define RP1_PWM_REGS_CHAN_CTRL_MODE_PCMS         ((0x2 & 0x7) << 0)
#define RP1_PWM_REGS_CHAN_CTRL_MODE_PDEO         ((0x3 & 0x7) << 0)
#define RP1_PWM_REGS_CHAN_CTRL_MODE_MSBS         ((0x4 & 0x7) << 0)
#define RP1_PWM_REGS_CHAN_CTRL_MODE_PPMO         ((0x5 & 0x7) << 0)
#define RP1_PWM_REGS_CHAN_CTRL_MODE_LEMS         ((0x6 & 0x7) << 0)
#define RP1_PWM_REGS_CHAN_CTRL_MODE_LSBO         ((0x7 & 0x7) << 0)
#define RP1_PWM_REGS_CHAN_CTRL_INVERT            (1 << 3)
#define RP1_PWM_REGS_CHAN_CTRL_BIND              (1 << 4)
#define RP1_PWM_REGS_CHAN_CTRL_USEFIFO           (1 << 5)
#define RP1_PWM_REGS_CHAN_CTRL_SDM               (1 << 6)
#define RP1_PWM_REGS_CHAN_CTRL_DITHER            (1 << 7)
#define RP1_PWM_REGS_CHAN_CTRL_FIFO_POP          (1 << 8)
#define RP1_PWM_REGS_CHAN_CTRL_SDM_BITWIDTH(val) (((val) & 0xf) << 12)
#define RP1_PWM_REGS_CHAN_CTRL_SDM_BIAS(val)     (((val) & 0xffff) << 16)
    uint32_t range;
    uint32_t phase;
    uint32_t duty;
} __attribute__((packed)) rp1_pwm_chan_t;

typedef struct {
    uint32_t global_ctrl;
#define RP1_PWM_REGS_GLOBAL_CTRL_CHAN_EN(chan)   (1 << chan)
#define RP1_PWM_REGS_GLOBAL_CTRL_SET_UPDATE      (1 << 31)
    uint32_t fifo_ctrl;
#define RP1_PWM_REGS_FIFO_CTRL_LEVEL(reg)        ((reg & 0x1f) >> 0)
#define RP1_PWM_REGS_FIFO_CTRL_FLUSH             (1 << 5)
#define RP1_PWM_REGS_FIFO_CTRL_FLUSH_DONE        (1 << 6)
#define RP1_PWM_REGS_FIFO_CTRL_THRESHOLD(val)    (((val) & 0x1f) << 11)
#define RP1_PWM_REGS_FIFO_CTRL_DWELL_TIME(val)   (((val) & 0x1f) << 16)
#define RP1_PWM_REGS_FIFO_CTRL_DREQ_EN           (1 << 31)
    uint32_t common_range;
    uint32_t common_duty;
    uint32_t duty_fifo;
    rp1_pwm_chan_t chan[4];
    uint32_t intr;                               // Raw interrupts
    uint32_t inte;                               // Interrupt Enable
    uint32_t intf;                               // Interrupt Force
    uint32_t ints;                               // Interrupt status after mask and forcing
} __attribute__((packed)) rp1_pwm_regs_t;


//
// WS281X_PWM Device Driver Structure
//
typedef struct rp1_pwm_device {
    volatile uint32_t flags;
#define RP1_WS281X_PWM_DEVICE_FLAGS_BUSY         (1 << 0)
    volatile uint32_t active;
#define RP1_WS281X_PWM_DEVICE_ACTIVE             (1 << 0)
    struct miscdevice mdev;
    rp1_pwm_regs_t __iomem *regs;
	struct clk *clk;
    struct platform_device *pdev;
    struct resource *res;
    struct dma_chan *chan;
    void *dmabuf;
    struct mutex *lock;
    wait_queue_head_t *wq;
    struct sg_table dma_table;
    int page_count;
    struct page *pages[SG_CHUNK_SIZE];
} rp1_ws281x_pwm_device_t;


//
// Global Variables
//
DECLARE_WAIT_QUEUE_HEAD(rp1_ws281x_pwm_wq);
DEFINE_MUTEX(rp1_ws281x_pwm_lock);
rp1_ws281x_pwm_device_t rp1_ws281x_pwm = {
    .lock = &rp1_ws281x_pwm_lock,
    .wq = &rp1_ws281x_pwm_wq,
    .flags = 0,
    .active = 0,
};

//
// PWM Channel Setup
//
void rp1_ws281x_pwm_chan(int channel, int invert) {
    uint32_t tmp;

    tmp = RP1_PWM_REGS_CHAN_CTRL_MODE_MSBS |
          RP1_PWM_REGS_CHAN_CTRL_USEFIFO |
          RP1_PWM_REGS_CHAN_CTRL_BIND |
          RP1_PWM_REGS_CHAN_CTRL_FIFO_POP;

    if (invert) {
        tmp |= RP1_PWM_REGS_CHAN_CTRL_INVERT;
    }
          
    iowrite32(tmp, &rp1_ws281x_pwm.regs->chan[channel].ctrl);
}

//
// PWM Controller Setup
//
void rp1_ws281x_pwm_init(int channel, int invert) {
    rp1_ws281x_pwm_chan(channel, invert);

    // Set the range to 32-bits since we're sending data through
    // the FIFO 32-bits per DMA cycle.
    iowrite32(31, &rp1_ws281x_pwm.regs->common_range);

    iowrite32(RP1_PWM_REGS_FIFO_CTRL_DWELL_TIME(2) |
              RP1_PWM_REGS_FIFO_CTRL_THRESHOLD(8) |
              RP1_PWM_REGS_FIFO_CTRL_DREQ_EN,
              &rp1_ws281x_pwm.regs->fifo_ctrl);

    iowrite32(RP1_PWM_REGS_GLOBAL_CTRL_CHAN_EN(channel) |
              RP1_PWM_REGS_GLOBAL_CTRL_SET_UPDATE, 
              &rp1_ws281x_pwm.regs->global_ctrl);

    // Set the level to zero
    iowrite32(0, &rp1_ws281x_pwm.regs->duty_fifo);
}

void rp1_ws281x_pwm_cleanup(void) {
    // Set level back to zero
    iowrite32(0, &rp1_ws281x_pwm.regs->duty_fifo);

    iowrite32(RP1_PWM_REGS_GLOBAL_CTRL_SET_UPDATE, 
              &rp1_ws281x_pwm.regs->global_ctrl);
}


//
// Character device file operations
//
int rp1_ws281x_pwm_open(struct inode *inode, struct file *file) {
    if (mutex_lock_interruptible(rp1_ws281x_pwm.lock)) {
        return -EINTR;
    }

    // Only one user at a time
    if (rp1_ws281x_pwm.flags & RP1_WS281X_PWM_DEVICE_FLAGS_BUSY) {
        mutex_unlock(rp1_ws281x_pwm.lock);
        return -EBUSY;
    }
    rp1_ws281x_pwm.flags |= RP1_WS281X_PWM_DEVICE_FLAGS_BUSY;

    file->private_data = &rp1_ws281x_pwm;

    mutex_unlock(rp1_ws281x_pwm.lock);

    rp1_ws281x_pwm_init(2, 0);

    return 0;
}

int rp1_ws281x_pwm_release(struct inode *inode, struct file *file) {
    int retval;

    if (mutex_lock_interruptible(rp1_ws281x_pwm.lock)) {
        return -EINTR;
    }

    rp1_ws281x_pwm.flags &= ~RP1_WS281X_PWM_DEVICE_FLAGS_BUSY;

    retval = wait_event_interruptible(*rp1_ws281x_pwm.wq, !(rp1_ws281x_pwm.active & RP1_WS281X_PWM_DEVICE_ACTIVE));
    if (retval) {  // Ctrl-C / Kill
        dmaengine_terminate_sync(rp1_ws281x_pwm.chan);
        rp1_ws281x_pwm.flags &= ~RP1_WS281X_PWM_DEVICE_ACTIVE;
    }

    mutex_unlock(rp1_ws281x_pwm.lock);

    return 0;
}

long rp1_ws281x_pwm_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    uint32_t ver = RP1_WS281X_PWM_DRIVER_VERSION;
    rp1_ws281x_pwm_ioctl_reg_t reg;

    switch (cmd) {
        case RP1_WS281X_PWM_IOCTL_VERSION:
            if (copy_to_user((uint32_t *)arg, &ver, sizeof(ver))) {
                return -EACCES;
            }
            break;

        case RP1_WS281X_PWM_IOCTL_REG_READ:
            if (copy_from_user(&reg, (rp1_ws281x_pwm_ioctl_reg_t *)arg, sizeof(reg))) {
                return -EACCES;
            }

            // Bounds check the register space
            if (reg.reg_offset >= sizeof(rp1_pwm_regs_t)) {
                return -EINVAL;
            }

            reg.reg_value = ioread32((uint8_t *)rp1_ws281x_pwm.regs + reg.reg_offset);

            if (copy_to_user((rp1_ws281x_pwm_ioctl_reg_t *)arg, &reg, sizeof(reg))) {
                return -EACCES;
            }

            break;

        case RP1_WS281X_PWM_IOCTL_REG_WRITE:
            if (copy_from_user(&reg, (rp1_ws281x_pwm_ioctl_reg_t *)arg, sizeof(reg))) {
                return -EACCES;
            }

            // Bounds check the register space
            if (reg.reg_offset >= sizeof(rp1_pwm_regs_t)) {
                return -EINVAL;
            }

            iowrite32(reg.reg_value, (uint8_t *)rp1_ws281x_pwm.regs + reg.reg_offset);

            break;

        default:
            return -EINVAL;
    }

    return 0;
}

void rp1_ws281x_dma_callback(void *param) {
    rp1_ws281x_pwm.active &= ~RP1_WS281X_PWM_DEVICE_ACTIVE;
    wake_up(rp1_ws281x_pwm.wq);
}

ssize_t rp1_ws281x_dma(const char *buf, ssize_t len) {
    uint64_t first = (uint64_t)buf >> PAGE_SHIFT;
    uint64_t last = ((uint64_t)buf + (len - 1)) >> PAGE_SHIFT;
    int offset = (uint64_t)buf % PAGE_SIZE;
    int count = last - first + 1;
    struct dma_async_tx_descriptor *desc;
    int retval = 0;

    if (count > ARRAY_SIZE(rp1_ws281x_pwm.pages)) {
        count = ARRAY_SIZE(rp1_ws281x_pwm.pages);
        len = (PAGE_SIZE - offset) + ((count - 1) * PAGE_SIZE);
    }

    retval = pin_user_pages_fast((uint64_t)buf, count, 0, rp1_ws281x_pwm.pages);
    if (retval != count) {
        if (retval) {
            unpin_user_pages(rp1_ws281x_pwm.pages, retval);
        }

        dev_err(&rp1_ws281x_pwm.pdev->dev, "Failed to map user pages %d %d\n", retval, count);
        return -ENOBUFS;
    }

    retval = sg_alloc_table_from_pages(&rp1_ws281x_pwm.dma_table, rp1_ws281x_pwm.pages,
                                       count, offset, len, GFP_KERNEL);
    if (retval) {
        unpin_user_pages(rp1_ws281x_pwm.pages, count);

        return -ENOBUFS;
    }

    retval = dma_map_sgtable(&rp1_ws281x_pwm.pdev->dev, &rp1_ws281x_pwm.dma_table, DMA_TO_DEVICE, 0);
    if (retval) {
        sg_free_table(&rp1_ws281x_pwm.dma_table); 
        unpin_user_pages(rp1_ws281x_pwm.pages, count);

        return -ENOBUFS;
    }

    rp1_ws281x_pwm.active |= RP1_WS281X_PWM_DEVICE_ACTIVE;

    desc = dmaengine_prep_slave_sg(rp1_ws281x_pwm.chan, rp1_ws281x_pwm.dma_table.sgl, 
                                   rp1_ws281x_pwm.dma_table.nents, DMA_MEM_TO_DEV, 0);
    if (!desc) {
        len = -ENOBUFS;
        goto cleanup;
    }

    desc->callback = rp1_ws281x_dma_callback;
    desc->callback_param = NULL;

    retval = dmaengine_submit(desc);
    if (retval < 0) {
        len = -ENOBUFS;

        goto cleanup;
    }
    dma_async_issue_pending(rp1_ws281x_pwm.chan);

    // Wait for the DMA to complete, this ensures the user can't mess with the pinned memory,
    // at least in the writer thread context.
    retval = wait_event_interruptible(*rp1_ws281x_pwm.wq, !(rp1_ws281x_pwm.active & RP1_WS281X_PWM_DEVICE_ACTIVE));
    if (retval) {  // Ctrl-C / Kill
        dmaengine_terminate_sync(rp1_ws281x_pwm.chan);
        rp1_ws281x_pwm.flags &= ~RP1_WS281X_PWM_DEVICE_ACTIVE;
        len = -ERESTARTSYS;
        goto cleanup;
    }

cleanup:
    dma_unmap_sgtable(&rp1_ws281x_pwm.pdev->dev, &rp1_ws281x_pwm.dma_table, DMA_TO_DEVICE, 0);
    sg_free_table(&rp1_ws281x_pwm.dma_table); 
    unpin_user_pages(rp1_ws281x_pwm.pages, count);

    return len;
}

ssize_t rp1_ws281x_pwm_write(struct file *file, const char *buf, size_t total, loff_t *loff) {
    ssize_t len = 0;

    if (mutex_lock_interruptible(rp1_ws281x_pwm.lock)) {
        return -EINTR;
    }

    while (len < total) {
        ssize_t retval = rp1_ws281x_dma(buf + len, total - len);
        if (retval <= 0) {
            mutex_unlock(rp1_ws281x_pwm.lock);
            return retval;
        }

        len += retval;
    }

    mutex_unlock(rp1_ws281x_pwm.lock);
 
    return len;
}


static struct file_operations rp1_ws281x_pwm_fops = {
    .owner = THIS_MODULE,
    .open = rp1_ws281x_pwm_open,
    .release = rp1_ws281x_pwm_release,
    .write = rp1_ws281x_pwm_write,
    .unlocked_ioctl = rp1_ws281x_pwm_ioctl,
};


/*
 * Driver Probe / Init
 */
int rp1_ws281x_pwm_probe(struct platform_device *pdev) {
    struct dma_slave_config dma_conf = {
        .dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES,
        .src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES,
        .direction = DMA_MEM_TO_DEV,
        .dst_maxburst = 8,
        .dst_port_window_size = 1,
        .device_fc = false,
    };
    int result;

    rp1_ws281x_pwm.pdev = pdev;

    rp1_ws281x_pwm.res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!rp1_ws281x_pwm.res) {
        dev_err(&pdev->dev, "%s: Failed to get platform memory region\n", DEVICE_NAME);
        return -EIO;
    }

    // Setup the target fifo register address for the DMA operations
    dma_conf.dst_addr = rp1_ws281x_pwm.res->start + offsetof(rp1_pwm_regs_t, duty_fifo);

    rp1_ws281x_pwm.regs = devm_ioremap_resource(&pdev->dev, rp1_ws281x_pwm.res);
    if (!rp1_ws281x_pwm.regs) {
        dev_err(&pdev->dev, "%s: Failed to request register space\n", DEVICE_NAME);
        return -EIO;
    }

    rp1_ws281x_pwm.clk = devm_clk_get(&pdev->dev, NULL);
    if (IS_ERR(rp1_ws281x_pwm.clk)) {
        dev_err(&pdev->dev, "Clock not provided\n");
        devm_iounmap(&pdev->dev, rp1_ws281x_pwm.regs);
        return -ENODEV;
    }

    result = clk_prepare_enable(rp1_ws281x_pwm.clk);
    if (result) {
        clk_disable_unprepare(rp1_ws281x_pwm.clk);
        devm_iounmap(&pdev->dev, rp1_ws281x_pwm.regs);
        dev_err(&pdev->dev, "Clock could not be enabled\n");
        return result;
    }

    rp1_ws281x_pwm.chan = dma_request_chan(&pdev->dev, "pwm0");
    if (IS_ERR(rp1_ws281x_pwm.chan)) {
        clk_disable_unprepare(rp1_ws281x_pwm.clk);
        devm_iounmap(&pdev->dev, rp1_ws281x_pwm.regs);
        dev_err(&pdev->dev, "Unable to allocate dma channel\n");
        return -ENODEV;
    }

    result = dmaengine_slave_config(rp1_ws281x_pwm.chan, &dma_conf);
    if (result) {
        dma_release_channel(rp1_ws281x_pwm.chan);
        clk_disable_unprepare(rp1_ws281x_pwm.clk);
        devm_iounmap(&pdev->dev, rp1_ws281x_pwm.regs);
        dev_err(&pdev->dev, "Unable to allocate dma channel\n");
        return -ENODEV;
    }

    result = dma_set_mask(&pdev->dev, DMA_BIT_MASK(64));
    if (result) {
        dma_release_channel(rp1_ws281x_pwm.chan);
        clk_disable_unprepare(rp1_ws281x_pwm.clk);
        devm_iounmap(&pdev->dev, rp1_ws281x_pwm.regs);
        dev_err(&pdev->dev, "Unable to allocate dma channel\n");
        return -ENODEV;
    }

    rp1_ws281x_pwm.mdev.minor = MISC_DYNAMIC_MINOR;
    rp1_ws281x_pwm.mdev.name = DEVICE_NAME;
    rp1_ws281x_pwm.mdev.fops = &rp1_ws281x_pwm_fops;

    if (misc_register(&rp1_ws281x_pwm.mdev)) {
        dma_release_channel(rp1_ws281x_pwm.chan);
        clk_disable_unprepare(rp1_ws281x_pwm.clk);
        devm_iounmap(&pdev->dev, rp1_ws281x_pwm.regs);
        return -ENODEV;
    }

    platform_set_drvdata(pdev, &rp1_ws281x_pwm);

    rp1_ws281x_pwm_init(2, 0);

    return 0;
}

int rp1_ws281x_pwm_remove(struct platform_device *pdev) {
    rp1_ws281x_pwm_cleanup();

    misc_deregister(&rp1_ws281x_pwm.mdev);
    dmaengine_terminate_sync(rp1_ws281x_pwm.chan);
    dma_release_channel(rp1_ws281x_pwm.chan);
    clk_disable_unprepare(rp1_ws281x_pwm.clk);
    devm_iounmap(&pdev->dev, rp1_ws281x_pwm.regs);

    rp1_ws281x_pwm.pdev = NULL;

    return 0;
}

static const struct of_device_id rp1_ws281x_pwm_of_match[] = {
	{ .compatible = "rp1-ws281x-pwm" },
	{ }
};
MODULE_DEVICE_TABLE(of, rp1_ws281x_pwm_of_match);

static struct platform_driver rp1_ws281x_pwm_driver = {
	.driver = {
		.name = "rp1-ws281x-pwm",
		.of_match_table = rp1_ws281x_pwm_of_match,
	},
	.probe = rp1_ws281x_pwm_probe,
	.remove = rp1_ws281x_pwm_remove,
};
module_platform_driver(rp1_ws281x_pwm_driver);

MODULE_AUTHOR("Jeremy Garff <jer @ jers.net>");
MODULE_LICENSE("GPL");
