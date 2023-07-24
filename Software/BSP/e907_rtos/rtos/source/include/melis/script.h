/*
 * ===========================================================================================
 *
 *       Filename:  script.h
 *
 *    Description:  porting from melis legacy code.
 *
 *        Version:  Melis3.0 
 *         Create:  2020-08-21 11:01:52
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-08-21 13:49:35
 *
 * ===========================================================================================
 */

#ifndef __SCRIPT_H__
#define __SCRIPT_H__

#include <stdint.h>

#define DATA_TYPE_SINGLE_WORD            ( 1)
#define DATA_TYPE_STRING                 ( 2)
#define DATA_TYPE_MULTI_WORD             ( 3)
#define DATA_TYPE_GPIO_WORD              ( 4)
#define SCRIPT_PARSER_OK                 ( 0)
#define SCRIPT_PARSER_EMPTY_BUFFER       (-1)
#define SCRIPT_PARSER_KEYNAME_NULL       (-2)
#define SCRIPT_PARSER_DATA_VALUE_NULL    (-3)
#define SCRIPT_PARSER_KEY_NOT_FIND       (-4)
#define SCRIPT_PARSER_BUFFER_NOT_ENOUGH  (-5)

typedef struct
{
    int main_key_count;
    int version[3];
} script_head_t;

typedef struct
{
    char main_name[32];
    int  lenth;
    int  offset;
} script_main_key_t;

typedef struct
{
    char sub_name[32];
    int  offset;
    int  pattern;
} script_sub_key_t;

typedef struct
{
    char gpio_name[32];
    int port;
    int port_num;
    int mul_sel;
    int pull;
    int drv_level;
    int data;
} script_gpio_set_t;

typedef struct
{
    char  *script_mod_buf;           //指向第一个主键//不能是malloc出来的
    int    script_mod_buf_size;
    int    script_main_key_count;       //保存主键的个数    
} script_parser_t;

typedef struct
{
    char gpio_name[32];
    int port;
    int port_num;
    int mul_sel;
    int pull;
    int drv_level;
    int data;
} user_gpio_set_t;

void* script_parser_init(char *script_buf, unsigned long size);
int32_t script_parser_exit(void *hparser);
int32_t script_parser_fetch(void *hparser, char *main_name, char *sub_name, int value[], int count);
int32_t script_parser_mainkey_count(void *hparser);
int32_t script_parser_subkey_get_gpio_cfg(void *hparser, char *main_name, char *sub_name, void *data_cfg, int cfg_type);
int32_t script_parser_subkey_get_gpio_cfg(void *hparser, char *main_name, char *sub_name, void *data_cfg, int cfg_type);
int32_t script_parser_mainkey_get_gpio_cfg(void *hparser, char *main_name, void *gpio_cfg, int gpio_count);
int32_t script_parser_mainkey_get_gpio_count(void *hparser, char *main_name);
int32_t script_parser_subkey_count(void *hparser, char *main_name);
void script_dump(void *parser);
void *script_get_handle(void);

#endif // __SCRIPT_H__
