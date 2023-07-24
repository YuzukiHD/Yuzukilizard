/*
*********************************************************************************************************
*                                                   ePDK
*                                   the Easy Portable/Player Develop Kits
*                                              hello world sample
*
*                               (c) Copyright 2006-2007, Steven.ZGJ China
*                                           All Rights Reserved
*
* File    : pfhead.h
* By      : Steven.ZGJ
* Version : V1.00
*********************************************************************************************************
*/
#include <typedef.h>
#include <kapi.h>

int finsh_system_init(void);
const __exec_mgsec_t exfinfo __attribute__((section(".magic"))) =
{
    {'e', 'P', 'D', 'K', '.', 'e', 'x', 'f'}, //.magic
    0x01000000,                             //.version
    0,                                      //.type
    0x1F00000,                              //.heapaddr
    0x400,                                  //.heapsize
    finsh_system_init,                              //.maintask
    0x4000,                                 //.mtskstksize
    KRNL_priolevel6                         //.mtskpriolevel
};

