#ifndef SUNXI_HAL_CFG_H
#define SUNXI_HAL_CFG_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef CONFIG_KERNEL_FREERTOS
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
int32_t Hal_Cfg_GetGPIOSecKeyCount(char *GPIOSecName);
int32_t Hal_Cfg_GetGPIOSecData(char *GPIOSecName, void *pGPIOCfg, int32_t GPIONum);
#elif defined(CONFIG_OS_MELIS)
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <kapi.h>
int32_t Hal_Cfg_Init(uint8_t *CfgVAddr, uint32_t size);
int32_t Hal_Cfg_Exit(void);
int32_t Hal_Cfg_GetKeyValue(char *SecName, char *KeyName, int32_t Value[], int32_t Count);
int32_t Hal_Cfg_GetSecKeyCount(char *SecName);
int32_t Hal_Cfg_GetSecCount(void);
int32_t Hal_Cfg_GetGPIOSecKeyCount(char *GPIOSecName);
int32_t Hal_Cfg_GetGPIOSecData(char *GPIOSecName, void *pGPIOCfg, int32_t GPIONum);
#else
#error "can not support unknown platform"
#endif

#ifdef __cplusplus
}
#endif

#endif
