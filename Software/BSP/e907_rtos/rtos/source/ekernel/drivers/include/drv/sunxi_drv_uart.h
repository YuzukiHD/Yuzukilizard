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
#ifndef SUNXI_DRV_UART_H
#define SUNXI_DRV_UART_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <rtthread.h>
#include <hal_uart.h>

typedef struct sunxi_driver_uart
{
    struct rt_device   base;
    int32_t            dev_id;
    const void        *hal_drv;
} sunxi_driver_uart_t;

/*
 *brief Access structure of the UART Driver.
 */
typedef struct sunxi_hal_driver_uart
{

    ///< Pointer to \ref SUNXI_HAL_UART_GetVersion : Get driver version.
    sunxi_hal_version_t (*get_version)(int32_t dev);

    ///< Pointer to \ref SUNXI_HAL_UART_GetCapabilities : Get driver capabilities.
    sunxi_hal_uart_capabilities_t (*get_capabilities)(int32_t dev);

    ///< Pointer to \ref SUNXI_HAL_UART_Initialize : Initialize UART Interface.
    int32_t (*initialize)(int32_t uart_port);

    ///< Pointer to \ref SUNXI_HAL_UART_Uninitialize : De-initialize UART Interface.
    int32_t (*uninitialize)(int32_t uart_port);

    ///< Pointer to \ref SUNXI_HAL_UART_PowerControl : Control UART Interface Power.
    int32_t (*power_control)(int32_t dev, sunxi_hal_power_state_e state);

    ///< Pointer to \ref SUNXI_HAL_UART_Send : Start sending data to UART transmitter.
    int32_t (*send)(int32_t dev, const uint8_t *data, uint32_t num);

    ///< Pointer to \ref SUNXI_HAL_UART_Receive : Start receiving data from UART receiver.
    int32_t (*receive)(int32_t dev, uint8_t *data, uint32_t num);

    ///< Pointer to \ref SUNXI_HAL_UART_Transfer : Start sending/receiving data to/from UART.
    int32_t (*transfer)(int32_t dev, const void *data_out, void *data_in, uint32_t    num);

    ///< Pointer to \ref SUNXI_HAL_UART_GetTxCount : Get transmitted data count.
    uint32_t (*get_tx_count)(int32_t dev);

    ///< Pointer to \ref SUNXI_HAL_UART_GetRxCount : Get received data count.
    uint32_t (*get_rx_count)(int32_t dev);

    ///< Pointer to \ref SUNXI_HAL_UART_Control : Control UART Interface.
    int32_t (*control)(int32_t uart_port, int cmd, void *args);

    ///< Pointer to \ref SUNXI_HAL_UART_GetStatus : Get UART status.
    sunxi_hal_uart_status_t (*get_status)(int32_t dev);
    ///< Pointer to \ref SUNXI_HAL_UART_SetModemControl : Set UART Modem Control line state.
    int32_t (*set_modem_control)(int32_t dev, sunxi_hal_uart_modem_control_e control);

    ///< Pointer to \ref SUNXI_HAL_UART_GetModemStatus : Get UART Modem Status lines state.
    sunxi_hal_uart_modem_status_t (*get_modem_status)(int32_t dev);

    int32_t (*receive_polling)(int32_t dev, uint8_t *data, uint32_t num);
    sunxi_hal_poll_ops *poll_ops;
} const sunxi_hal_driver_uart_t;

void sunxi_driver_uart_init(void);

#ifdef __cplusplus
}
#endif

#endif
