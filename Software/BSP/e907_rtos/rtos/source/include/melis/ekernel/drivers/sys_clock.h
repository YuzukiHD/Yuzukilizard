#ifndef __SYS_CLOCK__
#define __SYS_CLOCK__
#include <typedef.h>
#include <ktype.h>
#include <csp_ccm_para.h>

//command for call-back function of clock change
typedef enum __CLK_CMD
{
    CLK_CMD_SCLKCHG_REQ = 0,    //command for notify that clock will change
    CLK_CMD_SCLKCHG_DONE,       //command for notify that clock change finish
    CLK_CMD_
} __clk_cmd_t;

//command or status of clock on/off
typedef enum __CLK_ONOFF
{
    CLK_OFF = 0,                //clock off
    CLK_ON = 1,                 //clock on
} __CLK_onoff_t;

//status of clock, busy or free
typedef enum __CLK_FREE
{
    CLK_FREE = 0,               //clock is not used
    CLK_USED = 1,               //clock is used by someone
} __clk_free_t;

typedef struct __WAKEUP_SYS_SOURCE
{
    __u8    touch_panel;
    __u8    ext_nmi;
    __u8    keyboard;
    __u8    usb;
    __u8    alarm;
    __u8    lradc;
} __wakeup_sys_source;

typedef enum __CLK_MODE
{
    CLK_MODE_0 = 0,               //clock mode 0
    CLK_MODE_1 = 1,               //clock mode 1
    CLK_MODE_2 = 2,               //clock mode 2
    CLK_MODE_3 = 3,               //clock mode 3
} __clk_mode_t;


long CLK_Init(void);

#endif
