#ifndef _SYS_PWRMAN_H_
#define _SYS_PWRMAN_H_
#include <typedef.h>
#include <ktype.h>
#include <kconfig.h>
#include <csp_dram_para.h>

#define PWRMAN_PARA_REVERSED_NUM    (2)
#define STANDBY_EVENT_POWEROFF      (1<<0)
#define STANDBY_EVENT_LOWPOWER      (1<<1)

//define count of power mode
#define PWRMAN_MODE_CFG_CNT         (8)

#define PWRMAN_CHECK_PRESCALE       (100)
#define IDLE_LOWEST_PERCENT         (10)
#define IDLE_HIGHEST_PERCENT        (30)
#define CORE_CLOCK_FREQ_PRESCALE    (6 *1000*1000)
#define LOWEST_CORE_CLOCK_FREQ      (30*1000*1000)
#define BAD_FREQUENCY_LIMIT         (30*1000*1000)
#define IDLE_RUN_PRESCALE           (25)

typedef enum __SYS_PWRMAN_DEV
{
    PWRMAN_DEV_NONE = 0,

    //level1 pwm devices
    PWRMAN_DEV_TP   = 0x100,
    PWRMAN_DEV_IR,
    PWRMAN_DEV_KEY,
    PWRMAN_DEV_FM,
    PWRMAN_DEV_POWER,
    PWRMAN_DEV_HDMI,
    PWRMAN_DEV_RTC,
    PWRMAN_DEV_CSI,

    //level2 pwm devices
    PWRMAN_DEV_MONITOR = 0x200,
    PWRMAN_DEV_USBD,
    PWRMAN_DEV_USBH,
    PWRMAN_DEV_AUDIO,
    PWRMAN_DEV_DISPLAY,
    PWRMAN_DEV_MP,
    PWRMAN_DEV_NAND,
    PWRMAN_DEV_MS,
    PWRMAN_DEV_SDMMC,

    //level3 pwm devices
    PWRMAN_DEV_TWI = 0x300,
    PWRMAN_DEV_SPI,
    PWRMAN_DEV_UART,

    PWRMAN_DEV_

} __sys_pwrman_dev_e;

typedef struct __PWRMAN_MODE_CFG
{
    __bool          valid;          //flag to mark that if current parameter is valid
    uint32_t        core_pll_hi;    //core pll hi的频率，必须为(6*n +30)*1000000hz
    uint32_t        core_pll_lo;    //core pll lo的频率，必须为(6*n +30)*1000000hz
    uint32_t        dram_pll;       //dram pll的频率，必须为(12*n+60)*1000000hz
    uint32_t        vdd;            //vdd的电压值，必须确保在以上参数配置下能稳定运行
} __pwrman_mode_cfg_t;

typedef struct __PWRMAN_MODE_LIST
{
    struct __PWRMAN_MODE_LIST   *next;
    int32_t                     mode;
} __pwrman_mode_list_t;

typedef struct __PWRMAN_DEV_NODE
{
    __sys_pwrman_dev_e          devid;      // power user device id
    __pCB_DPMCtl_t              cb;         // call-back function
    void                        *parg;      // argument for call-back
    struct __PWRMAN_DEV_NODE    *prev;      // pointer for keep device tree list
    struct __PWRMAN_DEV_NODE    *next;      // pointer for keep device tree list
} __pwrman_dev_node_t;

typedef struct __SYS_PWRMAN_CFG
{
    __hdle                      task_prio;                  //pwm main task priority
    uint8_t                     start_flag;                 //start flag
    int8_t                      usbd_flag;                  //usb device flag
    int8_t                      usbh_flag;                  //usb host flag
    __hdle                      lock;                       //lock to avoid adjust frequency conflict
    __hdle                      h_power;                    //power driver handle
    int32_t                     cpu_lock;                   //flag to mark that if need lock cpu frequency
    __pwrman_mode_cfg_t         mode[PWRMAN_MODE_CFG_CNT];  //system define power mode
    __pwrman_mode_list_t        *mode_list;                 //mode list for manage current pwm mode
    __pwrman_mode_cfg_t         cur_para;                   //current pwm mode parameter
    void                        *pStandbyBin;               //buffer for store standby file
    __pwrman_dev_node_t         *pDevList;                  //power user device list
} __sys_pwrman_cfg_t;

typedef enum __SYS_PWRMAN_MODE
{
    SYS_PWRMAN_MODE_LEVEL0      = 0x00, /* level0 power mode (full speed)       */
    SYS_PWRMAN_MODE_LEVEL1      = 0x01, /* level1 power mode (high speed)       */
    SYS_PWRMAN_MODE_LEVEL2      = 0x02, /* level2 power mode (normal speed)     */
    SYS_PWRMAN_MODE_LEVEL3      = 0x03, /* level3 power mode (low speed)        */

    SYS_PWRMAN_MODE_USBD        = 0x20, /* usb device mode                      */
    SYS_PWRMAN_MODE_USBH        = 0x21, /* usb host mode                        */

    SYS_PWRMAN_MODE_

} __sys_pwrman_mode_t;

typedef enum __SYS_PWRMAN_CMD
{
    PWRMAN_CMD_ENTER_NONE       = 0,
    PWRMAN_CMD_ENTER_STANDBY,  //request device to enter standby
    PWRMAN_CMD_EXIT_STANDBY,   //wake up device
    PWRMAN_CMD_ENTER_

} __sys_pwrman_cmd_e;

typedef struct  __BOARD_POWER_CFG
{
    uint32_t        keyvalue        : 8;
    uint32_t        twi_controller  : 4;
    uint32_t        power_exist     : 4;
    uint32_t        reserved        : 16;
}
__board_power_cfg_t;

typedef struct __SYS_PWRMAN_PARA
{
    int32_t                 PowerOffTime;       //timer value when power off
    uint32_t                CurVddVal;          //current vdd value

    uint32_t                IrMask;             //ir code mask
    uint32_t                IrPowerVal;         //value of the power key of ir

    uint32_t                HKeyMin;            //min value of the hold-key
    uint32_t                HKeyMax;            //max value of the hold-key
    uint8_t                 WakeUpMode;         //mode of event wake up the system
    uint8_t                 EventFlag;          //event flag that need system process when exit standby
    uint8_t                 TvdFlag;            //flag to mark that if current board is tvd board

    __board_power_cfg_t     PowerCfg;           //power configuration
    __dram_para_t           dram_para;          // dram para
    //uint8_t                 IrBuffer[128];      //ir data buffer
    uint32_t                eint_no;
    uint32_t                pio_int_trigger;    /* trigger condition*/
    __hdle                  gpio_hdl;
    uint32_t                port;
    uint32_t                port_num;
    uint32_t                standby_cnt;
    uint32_t                standby_mode;
    uint32_t                sumin_value;
    uint32_t                sumout_value;
    uint32_t                power_off_flag;
} __sys_pwrman_para_t;


#define STANDBY_ENTRY       0xffffa000
#define STANDBY_SIZE        0x3000

extern __sys_pwrman_para_t     pwrmanStandbyPar;
typedef void (*standby_entry_t)(__sys_pwrman_para_t *pVar);

extern int32_t  initPwrmanIniCfg(__sys_pwrman_cfg_t *pSysPwrmanCfg);
extern int32_t  pwrman_init(__dram_para_t *para);
extern int32_t  pwrman_exit(void);
extern int32_t  pwrman_start(void);
extern int32_t  pwrman_stop(void);


#endif //#ifndef _SYS_PWRMAN_H_
