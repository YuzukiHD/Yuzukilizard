#ifndef SUNXI_HAL_STATUS_H
#define SUNXI_HAL_STATUS_H

typedef enum {
    HAL_OK      = 0,	/* success */
    HAL_ERROR   = -1,	/* general error */
    HAL_BUSY    = -2,	/* device or resource busy */
    HAL_TIMEOUT = -3,	/* wait timeout */
    HAL_INVALID = -4,	/* invalid argument */
    HAL_NOMEM   = -5,	/* no memory */
} hal_status_t;

#endif
