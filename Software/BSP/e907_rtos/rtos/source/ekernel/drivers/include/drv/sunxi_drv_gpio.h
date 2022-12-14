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
#ifndef __DRV_GPIO_H__
#define __DRV_GPIO_H__

#include <stdint.h>
#include <stdio.h>
#include <interrupt.h>
#include "../hal/hal_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/** This enum defines the return type of GPIO API. */
typedef enum
{
    DRV_GPIO_STATUS_ERROR_POWER_MODE  = -5,     /**< The GPIO power mode  failed to execute.*/
    DRV_GPIO_STATUS_ERROR_MUXSEL      = -4,     /**< The GPIO function failed to execute.*/
    DRV_GPIO_STATUS_ERROR_PARAMETER   = -3,     /**< The GPIO parameter failed to execute.*/
    DRV_GPIO_STATUS_ERROR_PIN         = -2,     /**< Invalid input pin number. */
    DRV_GPIO_STATUS_ERROR             = -1,     /**< Invalid input parameter. */
    DRV_GPIO_STATUS_OK                = 0       /**< The GPIO function executed successfully. */
} gpio_status_t;

/**
 * @hua       This function configures the pinmux of target GPIO.
 *            Pin Multiplexer (pinmux) connects the pin and the onboard peripherals,
 *            so the pin will operate in a specific mode once the pin is programmed to a peripheral's function.
 *            The alternate functions of every pin are provided in hal_pinmux_define.h.
 * @param[in] gpio_pin specifies the pin number to configure.
 * @param[in] function_index specifies the function for the pin.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #HAL_PINMUX_STATUS_OK, the operation completed successfully.
 *            If the return value is #HAL_PINMUX_STATUS_INVALID_FUNCTION, a wrong alternate function is given, the parameter must be verified.
 *            If the return value is #HAL_PINMUX_STATUS_ERROR_PORT, invalid input pin number, the parameter must be verified.
 *            If the return value is #HAL_PINMUX_STATUS_ERROR, the operation failed.
 * @notes
 * @warning
 */
gpio_status_t drv_gpio_pinmux_set_function(gpio_pin_t gpio_pin, uint32_t function_index);


/**
 * @dui       This function configures the power mode of target GPIO.
 *            Pin Multiplexer (pinmux) connects the pin and the onboard peripherals,
 *            so the pin will operate in a specific mode once the pin is programmed to a peripheral's function.
 *            The alternate functions of every pin are provided in hal_pinmux_define.h.
 * @param[in] gpio_pin specifies the pin number to configure.
 * @param[in] function_index specifies the function for the pin.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #PINMUX_STATUS_OK, the operation completed successfully.
 *            If the return value is #PINMUX_STATUS_INVALID_FUNCTION, a wrong alternate function is given, the parameter must be verified.
 *            If the return value is #PINMUX_STATUS_ERROR_PORT, invalid input pin number, the parameter must be verified.
 *            If the return value is #PINMUX_STATUS_ERROR, the operation failed.
 * @notes
 * @warning
 */
gpio_status_t drv_gpio_sel_vol_mode(gpio_pin_t gpio_pin, uint32_t sel_num);
/**
 * @hua       This function gets the input data of target GPIO when the direction of the GPIO is input.
 * @param[in] gpio_pin specifies the pin number to operate.
 * @param[out] gpio_data is the input data received from the target GPIO.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_INVALID_PARAMETER, a wrong parameter (except for pin number) is given, the parameter must be verified.
 *            If the return value is #DRV_GPIO_STATUS_ERROR_PIN, invalid input pin number, the parameter must be verified.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_get_input(gpio_pin_t gpio_pin, gpio_data_t *gpio_data);

/**
 * @hua       This function sets the output data of the target GPIO.
 * @param[in] gpio_pin specifies the pin number to operate.
 * @param[in] gpio_data is the output data of the target GPIO.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_ERROR_PIN, invalid input pin number, the parameter must be verified.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_set_output(gpio_pin_t gpio_pin, gpio_data_t gpio_data);


/**
 * @hua       This function gets the output data of the target GPIO when the direction of the GPIO is output.
 * @param[in] gpio_pin specifies the pin number to operate.
 * @param[in] gpio_data is output data of the target GPIO.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_INVALID_PARAMETER, a wrong parameter (except for pin number) is given, the parameter must be verified.
 *            If the return value is #DRV_GPIO_STATUS_ERROR_PIN, invalid input pin number, the parameter must be verified.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_get_output(gpio_pin_t gpio_pin, gpio_data_t *gpio_data);

/**
 * @hua       This function sets the direction of the target GPIO.
 * @param[in] gpio_pin specifies the pin number to set.
 * @param[in] gpio_direction is the direction of the target GPIO, the direction can be input or output.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_ERROR_PIN, invalid input pin number, the parameter must be verified.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_set_direction(gpio_pin_t gpio_pin, gpio_direction_t gpio_direction);


/**
 * @hua       This function gets the direction of the target GPIO.
 * @param[in] gpio_pin specifies the pin number to operate.
 * @param[output] gpio_direction is the direction of target GPIO, the direction can be input or output.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_INVALID_PARAMETER, a wrong parameter (except for pin number) is given, the parameter must be verified.
 *            If the return value is #DRV_GPIO_STATUS_ERROR_PIN, invalid input pin number, the parameter must be verified.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_get_direction(gpio_pin_t gpio_pin, gpio_direction_t *gpio_direction);

/**
 * @hua       This function sets the pull or down state of the target GPIO.
 * @param[in] gpio_pin specifies the pin number to operate.
 * @param[in] gpio_pull_status is the pull state of target GPIO, the state can be disable(float),pull up,pull down.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_ERROR_PIN, invalid input pin number, the parameter must be verified.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_set_pull_status(gpio_pin_t gpio_pin, gpio_pull_status_t gpio_pull_status);

/**
 * @hua       This function gets the pull or down state of the target GPIO.
 * @param[in] gpio_pin specifies the pin number to operate.
 * @param[output] gpio_pull_status is the pull state of target GPIO, the state can be disable(float),pull up,pull down.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_INVALID_PARAMETER, a wrong parameter (except for pin number) is given, the parameter must be verified.
 *            If the return value is #DRV_GPIO_STATUS_ERROR_PIN, invalid input pin number, the parameter must be verified.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_get_pull_status(gpio_pin_t gpio_pin, gpio_pull_status_t *gpio_pull_status);

/**
 * @hua       This function sets the driving current of the target GPIO.
 * @param[in] gpio_pin specifies the pin number to configure.
 * @param[in] level specifies the driving current to set to target GPIO.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_ERROR_PIN, the operation failed.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_set_driving_level(gpio_pin_t gpio_pin, gpio_driving_level_t gpio_driving_level);

/**
 * @hua       This function gets the driving current of the target GPIO.
 * @param[in] gpio_pin specifies the pin number to configure.
 * @param[out] level specifies the driving current to be set to target GPIO.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_ERROR_PIN, the operation failed.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 *            If the return value is #DRV_GPIO_STATUS_INVALID_PARAMETER, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_get_driving_level(gpio_pin_t gpio_pin, gpio_driving_level_t *gpio_driving_level);

/**
 * @hua       This function sets the debounce of the bank which has the target GPIO.
 * @param[in] gpio_pin specifies the pin number to find the bank to configure.
 * @param[out] freq specifies the freq to be set to target bank of the gpio,max freq is 24M.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_ERROR_PIN, the operation failed.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_set_debounce(gpio_pin_t gpio_pin, unsigned value);

/**
 * @hua       This function gets the debounce of the bank which has the target GPIO.
 * @param[in] gpio_pin specifies the pin number to find the bank to get.
 * @param[out] freq specifies the freq to be get to target bank of the gpio,max freq is 24M.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_ERROR_PIN, the operation failed.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
  *           If the return value is #DRV_GPIO_STATUS_INVALID_PARAMETER, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_get_debounce(gpio_pin_t gpio_pin, uint32_t *freq);

/**
 * @hua       This function gets the irqnum of the gpio.
 * @param[in] gpio_pin specifies the pin number to find the irq num to get.
 * @param[out] freq specifies the irq to be set to target irq num of the gpio.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_ERROR_PIN, the operation failed.
 *            If the return value is #DRV_GPIO_STATUS_INVALID_PARAMETER, the operation failed.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_to_irq(gpio_pin_t gpio_pin, int32_t *irq);

/**
 * @hua       This function request an irq of gpio.
 * @param[in] irq specifies the irq to request.
 * @param[in] hdle specifies the irq handler
 * @param[in] flags specifiles the irq detach type
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_ERROR_PIN, the operation failed.
 *            If the return value is #DRV_GPIO_STATUS_INVALID_PARAMETER, the operation failed.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_irq_request(uint32_t irq, hal_irq_handler_t hdle, unsigned long flags, void *data);

/**
 * @hua       This function free an irq of gpio.
 * @param[in] irq specifies the irq to free.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_irq_free(uint32_t irq);

/**
 * @hua       This function enable an irq of the gpio.
 * @param[in] irq specifies the irq to enable.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_irq_enable(uint32_t irq);

/**
 * @hua       This function disable an irq of the gpio.
 * @param[in] irq specifies the irq to disable.
 * @return    To indicate whether this function call is successful or not.
 *            If the return value is #DRV_GPIO_STATUS_OK, the operation completed successfully.
 *            If the return value is #DRV_GPIO_STATUS_ERROR, the operation failed.
 * @note
 * @warning
 */
gpio_status_t drv_gpio_irq_disable(uint32_t irq);

/**
 * @dui       This function init the gpio.
 * @param     none
 * @warning
 */
gpio_status_t drv_gpio_init(void);

#ifdef __cplusplus
}
#endif

#endif