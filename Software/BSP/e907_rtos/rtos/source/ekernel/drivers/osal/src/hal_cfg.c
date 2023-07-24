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
#include "hal_cfg.h"
/*
*********************************************************************************************************
*                                       INITIALIZE CONFIG SYSTEM
*
* Description:
*
* Arguments  :
*
* Returns    :
*
* Note       :
*********************************************************************************************************
*/
int32_t Hal_Cfg_Init(uint8_t *CfgVAddr, uint32_t size)
{
	return esCFG_Init(CfgVAddr, size);
}

/*
*********************************************************************************************************
*                                       EXIT CONFIG SYSTEM
*
* Description:
*
* Arguments  :
*
* Returns    :
*
* Note       :
*********************************************************************************************************
*/
int32_t Hal_Cfg_Exit(void)
{
	return esCFG_Exit();
}
/*
*********************************************************************************************************
*                                       根据主键名称和子键名称获取脚本数据
*
* Description:
*
* Arguments  : main_name    主键名称
*
*              sub_name     子键名称
*
*              value        存放数据的buffer
*
*              count        buffer的最大个数
*
*
* Returns    : 获取数据是否成功
*
* Notes      :
*********************************************************************************************************
*/
int32_t Hal_Cfg_GetKeyValue(char *SecName, char *KeyName, int32_t Value[], int32_t Count)
{
    return  esCFG_GetKeyValue(SecName, KeyName, Value, Count);
}

/*
*********************************************************************************************************
*                                       根据主键名称，获取主键下的子键总共个数
*
* Description:
*
* Arguments  : main_name    主键名称
*
*
*
* Returns    : 如果成功，返回子键个数
*              如果失败，返回负数
*
* Notes      :
*********************************************************************************************************
*/
int32_t Hal_Cfg_GetSecKeyCount(char *SecName)
{
    return  esCFG_GetSecKeyCount(SecName);
}

/*
*********************************************************************************************************
*                                       获取总共主键的个数
*
* Description:
*
* Arguments  :
*
*
*
* Returns    : 如果成功，返回主键个数
*              如果失败，返回负数
*
* Notes      :
*********************************************************************************************************
*/
int32_t Hal_Cfg_GetSecCount(void)
{
    return  esCFG_GetSecCount();
}

/*
*********************************************************************************************************
*                                       根据主键名称，获取主键下的GPIO类型总共个数
*
* Description:
*
* Arguments  : main_name    主键名称
*
*
*
* Returns    : 如果成功，返回主键个数
*              如果失败，返回负数
*
* Notes      :
*********************************************************************************************************
*/
int32_t Hal_Cfg_GetGPIOSecKeyCount(char *GPIOSecName)
{
    return  esCFG_GetGPIOSecKeyCount(GPIOSecName);
}

/*
*********************************************************************************************************
*                                       根据主键名称，获取主键下的GPIO类型的所有数据
*
* Description:
*
* Arguments  : main_name    主键名称
*
*              gpio_cfg     存放GPIO数据信息的buffer
*
*              gpio_count   GPIO的总共个数
*
*
* Returns    : 如果成功，返回成功标志
*              如果失败，返回负数
*
* Notes      :
*********************************************************************************************************
*/
int32_t Hal_Cfg_GetGPIOSecData(char *GPIOSecName, void *pGPIOCfg, int32_t GPIONum)
{
    return  esCFG_GetGPIOSecData(GPIOSecName, pGPIOCfg, GPIONum);
}

/*
*********************************************************************************************************
*                                       根据主键名称、子键名称及数据类型获取对应数据
*
* Description:
*
* Arguments  : MainKeyName    主键名称
*
               SubKeyName   子键名称

*              value     子键存放的数据
*
*              type   数据类型
*
*
* Returns    : 如果成功，返回成功标志
*              如果失败，返回负数
*
* Notes      :
*********************************************************************************************************
*/
int32_t Hal_Cfg_GetSubKeyValue(char *MainKeyName, char *SubKeyName, void *value, int32_t type)
{
    return  esCFG_GetSubKeyData(MainKeyName, SubKeyName, value, type);
}

void Hal_Cfg_Dump(void)
{
	esCFG_Dump();
}
