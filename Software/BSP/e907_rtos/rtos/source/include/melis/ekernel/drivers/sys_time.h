#ifndef _SYS_TIME_H_MELIS_
#define _SYS_TIME_H_MELIS_
#include <typedef.h>
#include <kconfig.h>
#include <ktype.h>


#define ALARM_INTERRUPT_NORMAL          0
#define ALARM_INTERRUPT_EXTNMI          1
#define TMR_SHOT_MODE_ONLYONE           0/*CSP_TMRC_TMR_MODE_ONE_SHOOT  //Timer will not automatically restart when it expires*/
#define TMR_SHOT_MODE_PERIODIC          1/*CSP_TMRC_TMR_MODE_CONTINU    //Timer will     automatically restart when it expires*/

/*define the precision of the timer:ns us ms s */
//#define TMR_PRECISION_NANO_SECOND       0
#define TMR_PRECISION_MICRO_SECOND      1
#define TMR_PRECISION_MILLI_SECOND      2
#define TMR_PRECISION_SECOND            3

typedef enum
{
    TMR_CONTINUE_MODE,
    TMR_SINGLE_MODE,
} __csp_timer_mode;

typedef struct _CSP_TMRC_tmr_type
{
    uint32_t precision;//This precision cannot be changed after you set successful!
    uint32_t nPeriod;//This precision cannot be changed after you set successful!
    __csp_timer_mode isOneShot;//The timer can count down from >=least count to 0.
} __csp_timer_req_type_t;

typedef struct __DATE_T
{
    __u16   year;
    __u8    month;
    __u8    day;
} __awos_date_t;

typedef struct __TIME_T
{
    __u8    hour;
    __u8    minute;
    __u8    second;
} __awos_time_t;

typedef enum __TMR_CNTR_STAT
{
    TMR_CNT_STAT_INVALID = 0,
    TMR_CNT_STAT_STOP   = 1,
    TMR_CNT_STAT_RUN    = 2,
    TMR_CNT_STAT_PAUSE  = 3,
    TMR_CNT_STAT_
} __tmr_cntr_stat_e;

typedef enum __RTC_INT_TYPE
{
    RTC_INT_TYPE_ALARM,     /* alarm interrupt  */
    RTC_INT_TYPE_TIMER,     /* timer interrupt  */
    RTC_INT_TYPE_CNTER,     /* conter interrupt */
    RTC_INT_TYPE_
} __rtc_int_type_e;

typedef enum __RTC_DEVICE_CMD_SET
{
    RTC_CMD_GET_TIME,       /* get current time */
    RTC_CMD_SET_TIME,       /* set current time */
    RTC_CMD_GET_DATE,       /* get current date */
    RTC_CMD_SET_DATE,       /* set current date */
    RTC_CMD_REQUEST_ALARM,  /* request alarm    */
    RTC_CMD_RELEASE_ALARM,  /* release alarm    */
    RTC_CMD_START_ALARM,    /* start alarm      */
    RTC_CMD_STOP_ALARM,     /* stop alarm       */
    RTC_CMD_QUERY_ALARM,    /* query alarm      */
    RTC_CMD_QUERY_INT,      /* qurey rtc interrupt  */
    RTC_CMD_SETCLOCKOUT,    /* set rtc clock out*/
    RTC_CMD_
} __rtc_device_cmd_set_e;

int32_t TIME_Init(void);
int32_t TIME_Exit(void);
int32_t TMR_Init(void);
int32_t TMR_Exit(void);
int32_t rtc_init(void);
int32_t rtc_exit(void);
int32_t CNTR_Init(void);
int32_t CNTR_Exit(void);
int32_t RTC_QueryInt(int32_t type);
int32_t TIME_KRNLTickInit(void);
int32_t TIME_SetKRNLTick(__pISR_t kerntick, __u32 period);
int32_t TIME_SetSTMRTick(__pISR_t stmrtick, __u32 period);



#endif  //define _SYS_TIME_H_MELIS_
