/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the People's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef SUNXI_DRV_SPI_H
#define SUNXI_DRV_SPI_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sunxi_hal_spi.h>
#include <rtthread.h>

typedef struct sunxi_driver_spi
{
    struct rt_device   base;
    int32_t            dev_id;
    const void        *hal_drv;
} sunxi_driver_spi_t;

#if 0
typedef enum
{
	SPI_WRITE_READ = 0, /**< SPI master is busy. */
	SPI_CONFIG = 1  /**< SPI master is idle. */
} hal_spi_transfer_cmd_t;

typedef struct
{
	const uint8_t *tx_buf; /**< Data buffer to send, */
	uint32_t tx_len; /**< The total number of bytes to send. */
	uint32_t
		tx_single_len; /**< The number of bytes to send in single mode. */
	uint8_t *rx_buf;   /**< Received data buffer, */
	uint32_t rx_len;   /**< The valid number of bytes received. */
	uint8_t tx_nbits : 3;  /**< Data buffer to send in nbits mode */
	uint8_t rx_nbits : 3;  /**< Data buffer to received in nbits mode */
	uint8_t dummy_byte;    /**< Flash send dummy byte, default 0*/
#define SPI_NBITS_SINGLE 0x01  /* 1bit transfer */
#define SPI_NBITS_DUAL 0x02    /* 2bit transfer */
#define SPI_NBITS_QUAD 0x04    /* 4bit transfer */
	uint8_t bits_per_word; /**< transfer bit_per_word */
} hal_spi_master_transfer_t;
#endif

typedef struct sunxi_hal_driver_spi
{
    spi_master_status_t (*initialize)(hal_spi_master_port_t port);
    spi_master_status_t (*uninitialize)(hal_spi_master_port_t port);
    spi_master_status_t (*send)(hal_spi_master_port_t port,
                                const void *buf, uint32_t size);
    spi_master_status_t (*receive)(hal_spi_master_port_t port,
                                   void *buf, uint32_t size);
    spi_master_status_t (*hw_config)(hal_spi_master_port_t port, hal_spi_master_config_t *spi_config);
    spi_master_status_t (*transfer)(hal_spi_master_port_t port, hal_spi_master_transfer_t *transfer);
} const sunxi_hal_driver_spi_t;

#ifdef __cplusplus
}
#endif

#endif