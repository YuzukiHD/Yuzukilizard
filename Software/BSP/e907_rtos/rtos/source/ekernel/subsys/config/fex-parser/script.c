/*
 * ===========================================================================================
 *
 *       Filename:  script.c
 *
 *    Description:  porting from legacy melis code.
 *
 *        Version:  Melis3.0
 *         Create:  2020-08-21 10:44:57
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-08-21 17:10:09
 *
 * ===========================================================================================
 */

#include <script.h>
#include <string.h>
#include <stdlib.h>
#include <debug.h>
#include <log.h>

static void *phandle_fex;

static int get_str_len(char *str)
{
    int length = 0;

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

void *script_get_handle(void)
{
    if (phandle_fex)
    {
        return phandle_fex;
    }
    else
    {
        __err("script not initialized.");
        software_break();
        return NULL;
    }
}

void *script_parser_init(char *script_buf, unsigned long size)
{
    script_parser_t *parser = NULL;
    script_head_t   *script_head = NULL;

    if (phandle_fex)
    {
        goto EXIST;
    }

    if ((script_buf == NULL) || !size)
    {
        __err("parameter invalid.");
        software_break();
        return NULL;
    }

    parser = malloc(sizeof(script_parser_t));
    if (NULL == parser)
    {
        __err("heap malloc failure.");
        software_break();
        return NULL;
    }

    memset(parser, 0x00, sizeof(script_parser_t));

    parser->script_mod_buf = malloc(size);
    if (NULL == parser->script_mod_buf)
    {
        __err("heap malloc failure.");
        software_break();
        return 0;
    }

    memset(parser->script_mod_buf, 0x00, size);

    memcpy(parser->script_mod_buf, script_buf, size);
    parser->script_mod_buf_size = size;

    script_head = (script_head_t *)(parser->script_mod_buf);

    parser->script_main_key_count = script_head->main_key_count;

    phandle_fex = parser;

EXIST:
    return phandle_fex;
}

int32_t script_parser_exit(void *hparser)
{
    script_parser_t *parser = NULL;

    parser = (script_parser_t *)hparser;
    if (NULL == parser)
    {
        __err("invalid parameter.");
        software_break();
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    if (parser->script_mod_buf)
    {
        free(parser->script_mod_buf);
        parser->script_mod_buf = NULL;
    }

    parser->script_main_key_count = 0;
    parser->script_mod_buf_size = 0;

    free(parser);

    parser = NULL;
    phandle_fex = NULL;

    return SCRIPT_PARSER_OK;
}

int32_t script_parser_fetch(void *hparser, char *main_name, char *sub_name, int value[], int count)
{
    char main_bkname[32] = {0}, sub_bkname[32] = {0};
    char *main_char = NULL, *sub_char = NULL;
    script_sub_key_t *sub_key = NULL;
    int i, j;
    int pattern, word_count;
    script_parser_t *parser;

    parser = (script_parser_t *)hparser;
    if (NULL == parser)
    {
        __err("invalid parameter.");
        software_break();
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    if (!parser->script_mod_buf)
    {
        __err("script buffer not exist.");
        software_break();
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    if ((main_name == NULL) || (sub_name == NULL))
    {
        __err("paramer invalid.");
        software_break();
        return SCRIPT_PARSER_KEYNAME_NULL;
    }

    if (value == NULL)
    {
        __err("paramer invalid.");
        software_break();
        return SCRIPT_PARSER_DATA_VALUE_NULL;
    }

    main_char = main_name;
    if (get_str_len(main_name) > 31)
    {
        memset(main_bkname, 0, 32);
        strncpy(main_bkname, main_name, 31);
        main_char = main_bkname;
    }
    sub_char = sub_name;
    if (get_str_len(sub_name) > 31)
    {
        memset(sub_bkname, 0, 32);
        strncpy(sub_bkname, sub_name, 31);
        sub_char = sub_bkname;
    }

    for (i = 0; i < parser->script_main_key_count; i++)
    {
        script_main_key_t  *main_key = NULL;
        main_key = (script_main_key_t *)(parser->script_mod_buf + (sizeof(script_head_t)) + i * sizeof(script_main_key_t));
        if (strcmp(main_key->main_name, main_char))
        {
            continue;
        }

        for (j = 0; j < main_key->lenth; j++)
        {
            sub_key = (script_sub_key_t *)(parser->script_mod_buf + (main_key->offset << 2) + (j * sizeof(script_sub_key_t)));
            if (strcmp(sub_key->sub_name, sub_char))
            {
                continue;
            }
            pattern    = (sub_key->pattern >> 16) & 0xffff;
            word_count = (sub_key->pattern >> 0)  & 0xffff;

            switch (pattern)
            {
                case DATA_TYPE_SINGLE_WORD:
                    value[0] = *(int *)(parser->script_mod_buf + (sub_key->offset << 2));
                    break;

                case DATA_TYPE_STRING:
                    if (count < word_count)
                    {
                        word_count = count;
                    }
                    memcpy((char *)value, parser->script_mod_buf + (sub_key->offset << 2), word_count << 2);
                    break;

                case DATA_TYPE_MULTI_WORD:
                    break;
                case DATA_TYPE_GPIO_WORD:
                {
                    script_gpio_set_t  *user_gpio_cfg = (script_gpio_set_t *)value;

                    if (sizeof(script_gpio_set_t) > (count << 2))
                    {
                        return SCRIPT_PARSER_BUFFER_NOT_ENOUGH;
                    }

                    strcpy(user_gpio_cfg->gpio_name, sub_char);
                    memcpy(&user_gpio_cfg->port, parser->script_mod_buf + (sub_key->offset << 2),  sizeof(script_gpio_set_t) - 32);
                    break;
                }
            }

            return SCRIPT_PARSER_OK;
        }
    }

    return SCRIPT_PARSER_KEY_NOT_FIND;
}

int32_t script_parser_subkey_count(void *hparser, char *main_name)
{
    char main_bkname[32] = {0};
    char *main_char = NULL;
    int i;
    script_parser_t *parser;

    parser = (script_parser_t *)hparser;
    if (NULL == parser)
    {
        __err("paramer invalid.");
        software_break();
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    if (!parser->script_mod_buf)
    {
        __err("script binary buffer not exist.");
        software_break();
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    if (main_name == NULL)
    {
        __err("paramer invalid.");
        software_break();
        return SCRIPT_PARSER_KEYNAME_NULL;
    }

    main_char = main_name;
    if (get_str_len(main_name) > 31)
    {
        memset(main_bkname, 0, 32);
        strncpy(main_bkname, main_name, 31);
        main_char = main_bkname;
    }

    for (i = 0; i < parser->script_main_key_count; i++)
    {
        script_main_key_t  *main_key = NULL;
        main_key = (script_main_key_t *)(parser->script_mod_buf + (sizeof(script_head_t)) + i * sizeof(script_main_key_t));
        if (strcmp(main_key->main_name, main_char))
        {
            continue;
        }

        return main_key->lenth;
    }

    return -1;
}

int32_t script_parser_mainkey_count(void *hparser)
{
    script_parser_t *parser;

    parser = (script_parser_t *)hparser;
    if (NULL == parser)
    {
        __err("paramer invalid.");
        software_break();
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    if (!parser->script_mod_buf)
    {
        __err("script binary buffer not exist.");
        software_break();
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    return  parser->script_main_key_count;
}

int32_t script_parser_mainkey_get_gpio_count(void *hparser, char *main_name)
{
    char   main_bkname[32];
    char   *main_char;
    script_sub_key_t   *sub_key = NULL;
    int    i, j;
    int    pattern, gpio_count = 0;
    script_parser_t *parser;

    parser = (script_parser_t *)hparser;
    if (NULL == parser)
    {
        __err("paramer invalid.");
        software_break();
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    if (!parser->script_mod_buf)
    {
        __err("script binary buffer not exist.");
        software_break();
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    if (main_name == NULL)
    {
        __err("paramer invalid.");
        software_break();
        return SCRIPT_PARSER_KEYNAME_NULL;
    }

    main_char = main_name;
    if (get_str_len(main_name) > 31)
    {
        memset(main_bkname, 0, 32);
        strncpy(main_bkname, main_name, 31);
        main_char = main_bkname;
    }

    for (i = 0; i < parser->script_main_key_count; i++)
    {
        script_main_key_t  *main_key = NULL;
        main_key = (script_main_key_t *)(parser->script_mod_buf + (sizeof(script_head_t)) + i * sizeof(script_main_key_t));
        if (strcmp(main_key->main_name, main_char))
        {
            continue;
        }

        for (j = 0; j < main_key->lenth; j++)
        {
            sub_key = (script_sub_key_t *)(parser->script_mod_buf + (main_key->offset << 2) + (j * sizeof(script_sub_key_t)));

            pattern    = (sub_key->pattern >> 16) & 0xffff;

            if (DATA_TYPE_GPIO_WORD == pattern)
            {
                gpio_count ++;
            }
        }
    }

    return gpio_count;
}

int32_t script_parser_mainkey_get_gpio_cfg(void *hparser, char *main_name, void *gpio_cfg, int gpio_count)
{
    char   main_bkname[32];
    char   *main_char;
    script_sub_key_t   *sub_key = NULL;
    script_gpio_set_t  *user_gpio_cfg = (script_gpio_set_t *)gpio_cfg;
    int    i, j;
    int    pattern, user_index;
    script_parser_t *parser;

    parser = (script_parser_t *)hparser;
    if (NULL == parser)
    {
        __err("paramer invalid.");
        software_break();
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    if (!parser->script_mod_buf)
    {
        __err("script binary buffer not exist.");
        software_break();
        return SCRIPT_PARSER_EMPTY_BUFFER;
    }

    if (main_name == NULL)
    {
        __err("paramer invalid.");
        software_break();
        return SCRIPT_PARSER_KEYNAME_NULL;
    }

    memset(user_gpio_cfg, 0, sizeof(script_gpio_set_t) * gpio_count);

    main_char = main_name;
    if (get_str_len(main_name) > 31)
    {
        memset(main_bkname, 0, 32);
        strncpy(main_bkname, main_name, 31);
        main_char = main_bkname;
    }

    for (i = 0; i < parser->script_main_key_count; i++)
    {
        script_main_key_t  *main_key = NULL;
        main_key = (script_main_key_t *)(parser->script_mod_buf + (sizeof(script_head_t)) + i * sizeof(script_main_key_t));
        if (strcmp(main_key->main_name, main_char))
        {
            continue;
        }

        //match the mainkey, find subkey
        user_index = 0;
        for (j = 0; j < main_key->lenth; j++)
        {
            sub_key = (script_sub_key_t *)(parser->script_mod_buf + (main_key->offset << 2) + (j * sizeof(script_sub_key_t)));
            pattern    = (sub_key->pattern >> 16) & 0xffff;           //get data type
            // get the data.
            if (DATA_TYPE_GPIO_WORD == pattern)
            {
                strcpy(user_gpio_cfg[user_index].gpio_name, sub_key->sub_name);
                memcpy(&user_gpio_cfg[user_index].port, parser->script_mod_buf + (sub_key->offset << 2), sizeof(script_gpio_set_t) - 32);
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
