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
#include <typedef.h>
#include <hal_mem.h>
#include <rtthread.h>
#include "log.h"
#include "ktype.h"
#include "kapi.h"
#include <stdio.h>
#include <string.h>

#define SCRIPT_DEBUG_PRINTF(fmt, arg...)    printf("\e[35m[script]\e[0m"fmt"\n", ##arg)

#define DATA_TYPE(x)    ((x->pattern>>16) & 0xffff)
#define DATA_WORD(x)    ((x.pattern>> 0) & 0xffff)

#define SCRIPT_GET_MAIN_KEY(x, i) (script_main_key_t *)(x->script_mod_buf + \
        (sizeof(script_head_t)) + i * sizeof(script_main_key_t))

#define SCRIPT_GET_SUB_KEY(x, m, i) (script_sub_key_t *)(x->script_mod_buf + \
        (m->offset<<2) + (i * sizeof(script_sub_key_t)))


/*
************************************************************************************************************
*
*                                             _test_str_length
*
*    函数名称：测试传进的字符串的长度
*
*    参数列表：str
*
*    返回值  ：
*
*    说明    ：
*
*
************************************************************************************************************
*/
static int32_t _test_str_length(char *str)
{
    int32_t length = 0;

    while (str[length++])
    {
        if (length > 32)
        {
            length = 32;
            break;
        }
    }

    return length;
}
/*
************************************************************************************************************
*
*                                             script_parser_init
*
*    函数名称：
*
*    参数列表：script_buf: 脚本数据池不能是malloc出来的 size:脚本数据字节数
*
*    返回值  ：
*
*    说明    ：
*
*
************************************************************************************************************
*/
void *script_parser_init(char *script_buf, unsigned long size)
{
    script_parser_t *parser;
    script_head_t   *script_head;

    if (script_buf)
    {
        parser = hal_malloc(sizeof(script_parser_t));

        if (NULL == parser)
        {
            return NULL;
        }
#ifndef CONFIG_SYSCONF_BUILDIN
        parser->script_mod_buf = hal_malloc(size);
        if (NULL == parser->script_mod_buf)
        {
            return NULL;
        }

        parser->script_mod_buf_size = size;

        {
            char *p;

            p = parser->script_mod_buf;
            while (size--)
            {
                *p++ = *script_buf++;
            }
        }

#else
        parser->script_mod_buf = script_buf;
        parser->script_mod_buf_size = size;
#endif
        script_head = (script_head_t *)(parser->script_mod_buf);
        parser->script_main_key_count = script_head->main_key_count;

        return (void *)parser;
    }
    else
    {
        return NULL;
    }
}
/*
************************************************************************************************************
*
*                                             script_parser_exit
*
*    函数名称：
*
*    参数列表：NULL
*
*    返回值  ：
*
*    说明    ：
*
*
************************************************************************************************************
*/
int32_t script_parser_exit(void *hparser)
{
    script_parser_t *parser;

    parser = (script_parser_t *)hparser;
    if (NULL == parser)
    {
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    if (parser->script_mod_buf)
    {

#ifndef CONFIG_SYSCONF_BUILDIN
        hal_free(parser->script_mod_buf);
#endif
        parser->script_mod_buf = NULL;
    }
    parser->script_main_key_count = 0;
    parser->script_mod_buf_size = 0;

#ifndef CONFIG_SYSCONF_BUILDIN
    hal_free(parser);
#endif

    return SCRIPT_PARSER_OK;
}

/*
************************************************************************************************************
*
*                                             script_parser_fetch
*
*    函数名称：
*
*    参数列表：
*
*    返回值  ：
*
*    说明    ：根据传进的主键，子键，取得对应的数值
*
*
************************************************************************************************************
*/
int32_t script_parser_fetch(void *hparser, char *main_name, char *sub_name, int value[], int count)
{
    char   main_bkname[32], sub_bkname[32];
    char   *main_char, *sub_char;
    script_main_key_t  *main_key = NULL;
    script_sub_key_t   *sub_key = NULL;
    int32_t    i, j;
    int32_t    pattern, word_count;
    script_parser_t *parser;

    parser = (script_parser_t *)hparser;
    if (NULL == parser)
    {
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    //检查脚本buffer是否存在
    if (!parser->script_mod_buf)
    {
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }
    //检查主键名称和子键名称是否为空
    if ((main_name == NULL) || (sub_name == NULL))
    {
        return SCRIPT_PARSER_KEYNAME_NULL;
    }
    //检查数据buffer是否为空
    if (value == NULL)
    {
        return SCRIPT_PARSER_DATA_VALUE_NULL;
    }
    //保存主键名称和子键名称，如果超过31字节则截取31字节
    main_char = main_name;
    if (_test_str_length(main_name) > 31)
    {
        rt_memset(main_bkname, 0, 32);
        rt_strncpy(main_bkname, main_name, 31);
        main_char = main_bkname;
    }
    sub_char = sub_name;
    if (_test_str_length(sub_name) > 31)
    {
        rt_memset(sub_bkname, 0, 32);
        rt_strncpy(sub_bkname, sub_name, 31);
        sub_char = sub_bkname;
    }

    for (i = 0; i < parser->script_main_key_count; i++)
    {
        main_key = (script_main_key_t *)(parser->script_mod_buf + (sizeof(script_head_t)) + i * sizeof(script_main_key_t));
        if (rt_strcmp(main_key->main_name, main_char))   //如果主键不匹配，寻找下一个主键
        {
            continue;
        }
        //主键匹配，寻找子键名称匹配
        for (j = 0; j < main_key->lenth; j++)
        {
            sub_key = (script_sub_key_t *)(parser->script_mod_buf + (main_key->offset << 2) + (j * sizeof(script_sub_key_t)));
            if (rt_strcmp(sub_key->sub_name, sub_char))   //如果主键不匹配，寻找下一个主键
            {
                continue;
            }
            pattern    = (sub_key->pattern >> 16) & 0xffff;           //获取数据的类型
            word_count = (sub_key->pattern >> 0) & 0xffff;            //获取所占用的word个数
            //取出数据
            switch (pattern)
            {
                case DATA_TYPE_SINGLE_WORD:                           //单word数据类型
                    value[0] = *(int *)(parser->script_mod_buf + (sub_key->offset << 2));
                    break;

                case DATA_TYPE_STRING:                                //字符串数据类型
                    if (count < word_count)
                    {
                        word_count = count;
                    }
                    rt_memcpy((char *)value, parser->script_mod_buf + (sub_key->offset << 2), word_count << 2);
                    break;

                case DATA_TYPE_MULTI_WORD:
                    break;
                case DATA_TYPE_GPIO_WORD:                            //多word数据类型
                {
                    user_gpio_set_t  *user_gpio_cfg = (user_gpio_set_t *)value;
                    //发现是GPIO类型，检查是否足够存放用户数据
                    if (sizeof(user_gpio_set_t) > (count << 2))
                    {
                        return SCRIPT_PARSER_BUFFER_NOT_ENOUGH;
                    }
                    rt_strncpy(user_gpio_cfg->gpio_name, sub_char, rt_strlen(sub_char) + 1);
                    rt_memcpy(&user_gpio_cfg->port, parser->script_mod_buf + (sub_key->offset << 2),  sizeof(user_gpio_set_t) - 32);

                    break;
                }
            }

            return SCRIPT_PARSER_OK;
        }
    }

    return SCRIPT_PARSER_KEY_NOT_FIND;
}
/*
************************************************************************************************************
*
*                                             script_parser_subkey_count
*
*    函数名称：
*
*    参数列表：
*
*    返回值  ：
*
*    说明    ：根据传进的主键，取得主键下的子键个数
*
*
************************************************************************************************************
*/
int32_t script_parser_subkey_count(void *hparser, char *main_name)
{
    char   main_bkname[32];
    char   *main_char;
    script_main_key_t  *main_key = NULL;
    int32_t    i;
    script_parser_t *parser;

    parser = (script_parser_t *)hparser;
    if (NULL == parser)
    {
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    //检查脚本buffer是否存在
    if (!parser->script_mod_buf)
    {
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }
    //检查主键名称和子键名称是否为空
    if (main_name == NULL)
    {
        return SCRIPT_PARSER_KEYNAME_NULL;
    }
    //保存主键名称和子键名称，如果超过31字节则截取31字节
    main_char = main_name;
    if (_test_str_length(main_name) > 31)
    {
        rt_memset(main_bkname, 0, 32);
        rt_strncpy(main_bkname, main_name, 31);
        main_char = main_bkname;
    }

    for (i = 0; i < parser->script_main_key_count; i++)
    {
        main_key = (script_main_key_t *)(parser->script_mod_buf + (sizeof(script_head_t)) + i * sizeof(script_main_key_t));
        if (rt_strcmp(main_key->main_name, main_char))   //如果主键不匹配，寻找下一个主键
        {
            continue;
        }

        return main_key->lenth;    //返回当前主键下的子键个数
    }

    return -1;                     //-1 表示没有对应的主键
}
/*
************************************************************************************************************
*
*                                             script_parser_mainkey_count
*
*    函数名称：
*
*    参数列表：
*
*    返回值  ：
*
*    说明    ：根据传进的主键，取得主键的个数
*
*
************************************************************************************************************
*/
int32_t script_parser_mainkey_count(void *hparser)
{
    script_parser_t *parser;

    parser = (script_parser_t *)hparser;
    if (NULL == parser)
    {
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    //检查脚本buffer是否存在
    if (!parser->script_mod_buf)
    {
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    return  parser->script_main_key_count;
}
/*
************************************************************************************************************
*
*                                             script_parser_mainkey_get_gpio_count
*
*    函数名称：
*
*    参数列表：
*
*    返回值  ：
*
*    说明    ：根据传进的主键，取得主键的GPIO个数
*
*
************************************************************************************************************
*/
int32_t script_parser_mainkey_get_gpio_count(void *hparser, char *main_name)
{
    char   main_bkname[32];
    char   *main_char;
    script_main_key_t  *main_key = NULL;
    script_sub_key_t   *sub_key = NULL;
    int32_t    i, j;
    int32_t    pattern, gpio_count = 0;
    script_parser_t *parser;

    parser = (script_parser_t *)hparser;
    if (NULL == parser)
    {
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    //检查脚本buffer是否存在
    if (!parser->script_mod_buf)
    {
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }
    //检查主键名称和子键名称是否为空
    if (main_name == NULL)
    {
        return SCRIPT_PARSER_KEYNAME_NULL;
    }
    //保存主键名称和子键名称，如果超过31字节则截取31字节
    main_char = main_name;
    if (_test_str_length(main_name) > 31)
    {
        rt_memset(main_bkname, 0, 32);
        rt_strncpy(main_bkname, main_name, 31);
        main_char = main_bkname;
    }

    for (i = 0; i < parser->script_main_key_count; i++)
    {
        main_key = (script_main_key_t *)(parser->script_mod_buf + (sizeof(script_head_t)) + i * sizeof(script_main_key_t));
        if (rt_strcmp(main_key->main_name, main_char))   //如果主键不匹配，寻找下一个主键
        {
            continue;
        }
        //主键匹配，寻找子键名称匹配
        for (j = 0; j < main_key->lenth; j++)
        {
            sub_key = (script_sub_key_t *)(parser->script_mod_buf + (main_key->offset << 2) + (j * sizeof(script_sub_key_t)));

            pattern    = (sub_key->pattern >> 16) & 0xffff;           //获取数据的类型
            //取出数据
            if (DATA_TYPE_GPIO_WORD == pattern)
            {
                gpio_count ++;
            }
        }
    }

    return gpio_count;
}

/*
************************************************************************************************************
*
*                                             script_parser_mainkey_get_gpio_cfg
*
*    函数名称：
*
*    参数列表：
*
*    返回值  ：
*
*    说明    ：根据传进的主键，取得主键下GPIO的配置信息
*
*
************************************************************************************************************
*/
int32_t script_parser_mainkey_get_gpio_cfg(void *hparser, char *main_name, void *gpio_cfg, int gpio_count)
{
    char   main_bkname[32];
    char   *main_char;
    script_main_key_t  *main_key = NULL;
    script_sub_key_t   *sub_key = NULL;
    user_gpio_set_t  *user_gpio_cfg = (user_gpio_set_t *)gpio_cfg;
    int32_t    i, j;
    int32_t    pattern, user_index;
    script_parser_t *parser;

    parser = (script_parser_t *)hparser;
    if (NULL == parser)
    {
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    //检查脚本buffer是否存在
    if (!parser->script_mod_buf)
    {
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }
    //检查主键名称和子键名称是否为空
    if (main_name == NULL)
    {
        return SCRIPT_PARSER_KEYNAME_NULL;
    }
    //首先清空用户buffer
    rt_memset(user_gpio_cfg, 0, sizeof(user_gpio_set_t) * gpio_count);
    //保存主键名称和子键名称，如果超过31字节则截取31字节
    main_char = main_name;
    if (_test_str_length(main_name) > 31)
    {
        rt_memset(main_bkname, 0, 32);
        rt_strncpy(main_bkname, main_name, 31);
        main_char = main_bkname;
    }

    for (i = 0; i < parser->script_main_key_count; i++)
    {
        main_key = (script_main_key_t *)(parser->script_mod_buf + (sizeof(script_head_t)) + i * sizeof(script_main_key_t));
        if (rt_strcmp(main_key->main_name, main_char))   //如果主键不匹配，寻找下一个主键
        {
            continue;
        }
        //主键匹配，寻找子键名称匹配
        user_index = 0;
        for (j = 0; j < main_key->lenth; j++)
        {
            sub_key = (script_sub_key_t *)(parser->script_mod_buf + (main_key->offset << 2) + (j * sizeof(script_sub_key_t)));
            pattern    = (sub_key->pattern >> 16) & 0xffff;           //获取数据的类型
            //取出数据
            if (DATA_TYPE_GPIO_WORD == pattern)
            {
                rt_strncpy(user_gpio_cfg[user_index].gpio_name, sub_key->sub_name, rt_strlen(sub_key->sub_name) + 1);
                rt_memcpy(&user_gpio_cfg[user_index].port, parser->script_mod_buf + (sub_key->offset << 2), sizeof(user_gpio_set_t) - 32);
                user_index++;
                if (user_index >= gpio_count)
                {
                    break;
                }
            }
        }
        return SCRIPT_PARSER_OK;
    }

    return SCRIPT_PARSER_KEY_NOT_FIND;
}

void script_dump(void *hparser)
{
    script_main_key_t  *main_key = NULL;
    script_sub_key_t   *sub_key = NULL;
    int value[64];
    int32_t i, j;
    int32_t pattern, word_count;
    script_parser_t *parser;

    static const char *type_name[] =
    {
        "Null",
        "SINGLE_WORD",
        "STRING",
        "MULTI_WORD",
        "GPIO_WORD",
    };

    parser = (script_parser_t *)hparser;
    if (NULL == parser)
    {
        return;
    }

    if (!parser->script_mod_buf)
    {
        return;
    }

    SCRIPT_DEBUG_PRINTF("main_key cnt:%d", parser->script_main_key_count);
    for (i = 0; i < parser->script_main_key_count; i++)
    {
        main_key = SCRIPT_GET_MAIN_KEY(parser, i);
        SCRIPT_DEBUG_PRINTF("main_key: %s, sub key cnt:%d", main_key->main_name,
                            main_key->lenth);

        for (j = 0; j < main_key->lenth; j++)
        {
            sub_key = SCRIPT_GET_SUB_KEY(parser, main_key, j);
            pattern    = DATA_TYPE(sub_key);
            word_count = (sub_key->pattern >> 0) & 0xffff;

            switch (pattern)
            {
                case DATA_TYPE_SINGLE_WORD:                           //单word数据类型
                    value[0] = *(int *)(parser->script_mod_buf + (sub_key->offset << 2));
                    SCRIPT_DEBUG_PRINTF("\t%s = 0x%x", sub_key->sub_name, value[0]);
                    break;
                case DATA_TYPE_STRING:                                //字符串数据类型
                    memcpy((char *)value, parser->script_mod_buf + (sub_key->offset << 2),
                           word_count << 2);
                    SCRIPT_DEBUG_PRINTF("\t%s = %s", sub_key->sub_name, (char *)value);
                    break;
                case DATA_TYPE_MULTI_WORD:
                    SCRIPT_DEBUG_PRINTF("\t%s = ???(not support)", sub_key->sub_name);
                    break;
                case DATA_TYPE_GPIO_WORD:                            //多word数据类型
                {
                    user_gpio_set_t  *gpio_cfg = (user_gpio_set_t *)value;
                    //发现是GPIO类型，检查是否足够存放用户数据
                    if (sizeof(user_gpio_set_t) > 64)
                    {
                        return;
                    }
                    memcpy(&gpio_cfg->port, parser->script_mod_buf +
                           (sub_key->offset << 2), sizeof(user_gpio_set_t) - 32);
                    SCRIPT_DEBUG_PRINTF("\t%s = P%c%d", sub_key->sub_name,
                                        gpio_cfg->port + 0x40, gpio_cfg->port_num);
                    break;
                }
                default:
                    SCRIPT_DEBUG_PRINTF("\t%s = %ld(not support)", sub_key->sub_name, pattern);
            }
        }
    }
}
