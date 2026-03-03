
#ifndef __RP1_WS281X_PWM_H__
#define __RP1_WS281X_PWM_H__


#define DEVICE_NAME                              "ws281x_pwm"

#define RP1_WS281X_PWM_IOCTL_MAGIC               0x6a67
#define RP1_WS281X_PWM_IOCTL_VERSION             _IOR(RP1_WS281X_PWM_IOCTL_MAGIC, 0x0, uint32_t *)
#define RP1_WS281X_PWM_IOCTL_REG_READ            _IOWR(RP1_WS281X_PWM_IOCTL_MAGIC, 0x1, rp1_ws281x_pwm_ioctl_reg_t *)
#define RP1_WS281X_PWM_IOCTL_REG_WRITE           _IOW(RP1_WS281X_PWM_IOCTL_MAGIC, 0x2, rp1_ws281x_pwm_ioctl_reg_t *)


//
// Ioctl Structures
//
typedef struct {
    uint32_t flags;
    uint32_t reg_offset;
    uint32_t reg_value;
} rp1_ws281x_pwm_ioctl_reg_t;

typedef struct {
    void *addr;
    uint64_t len;
} rp1_ws281x_pwm_ioctl_xfer_t;


//
// Kernel module function declarations
//
void rp1_ws281x_pwm_chan(int channel, int invert);
void rp1_ws281x_pwm_init(int channel, int invert);
void rp1_ws281x_pwm_cleanup(void);
int rp1_ws281x_pwm_open(struct inode *inode, struct file *file);
int rp1_ws281x_pwm_release(struct inode *inode, struct file *file);
long rp1_ws281x_pwm_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
void rp1_ws281x_dma_callback(void *param);
ssize_t rp1_ws281x_dma(const char *buf, ssize_t len);
ssize_t rp1_ws281x_pwm_write(struct file *file, const char *buf, size_t total, loff_t *loff);
int rp1_ws281x_pwm_probe(struct platform_device *pdev);
void rp1_ws281x_pwm_remove(struct platform_device *pdev);


#endif // __RP1_WS281X_PWM_H__

