
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


#endif // __RP1_WS281X_PWM_H__

