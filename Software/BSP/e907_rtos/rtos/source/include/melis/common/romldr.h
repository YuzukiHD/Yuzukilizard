/*
 * =====================================================================================
 *
 *       Filename:  romldr.h
 *
 *    Description:  elf modules loader used type.
 *
 *        Version:  2.0
 *         Create:  2017-11-03 11:32:32
 *       Revision:  none
 *       Compiler:  gcc version 6.3.0 (crosstool-NG crosstool-ng-1.23.0)
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2017-11-03 11:33:01
 *
 * =====================================================================================
 */

#ifndef __ROMLDR_H__
#define __ROMLDR_H__
#include <typedef.h>

//IOCTL commmands for melis system loader
#define ROMLDR_IOC_BASE                0x100
#define ROMLDR_IOC_GET_MAGIC_INDEX     (ROMLDR_IOC_BASE + 1)
#define ROMLDR_IOC_GET_SECTION_NUM     (ROMLDR_IOC_BASE + 2)
#define ROMLDR_IOC_GET_SECTION_HDR     (ROMLDR_IOC_BASE + 3)
#define ROMLDR_IOC_GET_SECTION_DATA    (ROMLDR_IOC_BASE + 4)
#define ROMLDR_IOC_LAST                (ROMLDR_IOC_BASE + 50)
#define ROMLDR_IOC_CMD(cmd)            ((ROMLDR_IOC_BASE <= cmd) && (ROMLDR_IOC_LAST >= cmd))

//the section header structure.
typedef struct __SECTION_ROM_HDR
{
    uint32_t    Size;
    unsigned long   VAddr;
    uint32_t    Type;
    uint32_t    Flags;
} __section_rom_hdr_t;

#endif  //__ROMLDR_H__
