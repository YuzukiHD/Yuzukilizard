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
#include <string.h>
#include <stdio.h>
#include <port.h>
#include <log.h>
#include <script.h>

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
intptr_t *esCFG_Init_Ex(char *path)
{
    intptr_t   *parser = NULL;
    __hdle      fp = NULL;
    char        *CfgVAddr = NULL;
    uint32_t    size = 0;
    uint32_t    readnum = 0;

    __wrn("esCFG_Init_Ex, path=%s", path);

    fp = fopen(path, "rb");
    if (NULL == fp)
    {
        __wrn("file does not exist:%s", path);
        parser = NULL;
        goto end;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    CfgVAddr = (char *)k_malloc(size);
    if (NULL == CfgVAddr)
    {
        __wrn("esMEMS_Balloc fail...");
        parser = NULL;
        goto end;
    }

    readnum = fread(CfgVAddr, 1, size, fp);
    if (readnum != size)
    {
        __wrn("esFSYS_fread fail...");
        parser = NULL;
        goto end;
    }

    parser = script_parser_init((char *)(CfgVAddr), size);
    if (NULL == parser)
    {
        __wrn("script_parser_init fail...");
        parser = NULL;
        goto end;
    }

end:
    if (fp)
    {
        fclose((__hdle)fp);
        fp = NULL;
    }

    if (CfgVAddr && size > 0)
    {
        k_free(CfgVAddr);
        CfgVAddr = NULL;
    }

    __wrn("parser=0x%x", parser);

    return parser;
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
int32_t esCFG_Exit_Ex(intptr_t *parser)
{
    if (NULL != parser)
    {
        script_parser_exit(parser);
        //parser = NULL; //do not change the outside parameter
    }

    return EPDK_OK;
}

/*
*********************************************************************************************************
*                                       根据主键名称和子键名称获取脚本数据
*
* Description:
*
* Arguments  :    KeyName : sec_name/key_name  主键名称/子键名称
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
int32_t esCFG_GetKeyValue_Ex(intptr_t *parser, char *KeyName, int32_t Value[], int32_t Count)
{
    char    sec_name[32];
    char    key_name[32];
    char    *pblash;

    if (NULL == parser)
    {
        return EPDK_FAIL;
    }

    memset(sec_name, 0, sizeof(sec_name));
    memset(key_name, 0, sizeof(key_name));
    pblash = (char *)strchr(KeyName, '/');
    if (NULL == pblash)
    {
        __wrn("KeyName format incorrect, / not found...");
        return EPDK_FAIL;
    }
    strncpy(sec_name, KeyName, pblash - KeyName);
    strcpy(key_name, pblash + 1);
    __wrn("sec_name=%s", sec_name);
    __wrn("key_name=%s", key_name);
    return  script_parser_fetch(parser, sec_name, key_name, (int *)Value, Count);
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
int32_t esCFG_GetSecKeyCount_Ex(intptr_t *parser, char *SecName)
{
    if (NULL == parser)
    {
        return EPDK_FAIL;
    }

    return  script_parser_subkey_count(parser, SecName);
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
int32_t esCFG_GetSecCount_Ex(intptr_t *parser)
{
    if (NULL == parser)
    {
        return EPDK_FAIL;
    }

    return  script_parser_mainkey_count(parser);
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
int32_t esCFG_GetGPIOSecKeyCount_Ex(intptr_t *parser, char *GPIOSecName)
{
    if (NULL == parser)
    {
        return EPDK_FAIL;
    }

    return  script_parser_mainkey_get_gpio_count(parser, GPIOSecName);
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
int32_t esCFG_GetGPIOSecData_Ex(intptr_t *parser, char *GPIOSecName, void *pGPIOCfg, int32_t GPIONum)
{
    if (NULL == parser)
    {
        return EPDK_FAIL;
    }

    return  script_parser_mainkey_get_gpio_cfg(parser, GPIOSecName, pGPIOCfg, GPIONum);
}
