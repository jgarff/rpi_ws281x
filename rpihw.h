#ifndef __RPIHW_H__
#define __RPIHW_H__


typdef struct {
    uint32_t hwver;
    uint32_t baseaddr;
    uint32_t type;
#define RPI_HWVER_TYPE_UNKNOWN                   0
#define RPI_HWVER_TYPE_PI1                       1
#define RPI_HWVER_TYPE_PI2                       2
} rpi_hwver_t;


#endif /* __RPIHW_H__ */
