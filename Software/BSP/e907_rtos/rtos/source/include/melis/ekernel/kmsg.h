/*
 * ===========================================================================================
 *
 *       Filename:  kmsg.h
 *
 *    Description:  the definition of key message types for kernel and driver module communic-
 *                  aticion through mailbox.
 *
 *        Version:  Melis3.0
 *         Create:  2018-08-22 18:58:08
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2018-08-22 19:04:03
 *
 * ===========================================================================================
 */

#ifndef _KMSG_H_
#define _KMSG_H_

/****************************************************/
/* definition for usr msg id                        */
/****************************************************/
#define KMSG_USR_CLS_SYSTEM           0x00000000
#define KMSG_USR_CLS_KEY              0x01000000

#define KMSG_USR_SYSTEM_ALARM_COMMING (KMSG_USR_CLS_SYSTEM | 0x01)

/* hotplug message */
#define KMSG_USR_SYSTEM_USBD_PLUGIN   (KMSG_USR_CLS_SYSTEM | 0x11)
#define KMSG_USR_SYSTEM_USBD_PLUGOUT  (KMSG_USR_CLS_SYSTEM | 0x12)
#define KMSG_USR_SYSTEM_USBH_PLUGIN   (KMSG_USR_CLS_SYSTEM | 0x13)
#define KMSG_USR_SYSTEM_USBH_PLUGOUT  (KMSG_USR_CLS_SYSTEM | 0x14)
#define KMSG_USR_SYSTEM_USBH1_PLUGIN  (KMSG_USR_CLS_SYSTEM | 0x15)
#define KMSG_USR_SYSTEM_USBH1_PLUGOUT (KMSG_USR_CLS_SYSTEM | 0x16)
#define KMSG_USR_SYSTEM_USBH2_PLUGIN  (KMSG_USR_CLS_SYSTEM | 0x17)
#define KMSG_USR_SYSTEM_USBH2_PLUGOUT (KMSG_USR_CLS_SYSTEM | 0x18)

#define KMSG_USR_SYSTEM_SDMMC_PLUGIN  (KMSG_USR_CLS_SYSTEM | 0x21)
#define KMSG_USR_SYSTEM_SDMMC_PLUGOUT (KMSG_USR_CLS_SYSTEM | 0x22)
#define KMSG_USR_SYSTEM_TVDAC_PLUGIN  (KMSG_USR_CLS_SYSTEM | 0x23)
#define KMSG_USR_SYSTEM_TVDAC_PLUGOUT (KMSG_USR_CLS_SYSTEM | 0x24)
#define KMSG_USR_SYSTEM_HDMI_PLUGIN   (KMSG_USR_CLS_SYSTEM | 0x25)
#define KMSG_USR_SYSTEM_HDMI_PLUGOUT  (KMSG_USR_CLS_SYSTEM | 0x26)
#define KMSG_USR_SYSTEM_MOUSE_PLUGIN  (KMSG_USR_CLS_SYSTEM | 0x27)
#define KMSG_USR_SYSTEM_MOUSE_PLUGOUT (KMSG_USR_CLS_SYSTEM | 0x28)

#define KMSG_USR_SYSTEM_FS_PARA_MASK  0xffff0000
#define KMSG_USR_SYSTEM_FS_PLUGIN     (KMSG_USR_CLS_SYSTEM | 0x31)
#define KMSG_USR_SYSTEM_FS_PLUGOUT    (KMSG_USR_CLS_SYSTEM | 0x32)
#define KMSG_USR_SYSTEM_WAKEUP        (KMSG_USR_CLS_SYSTEM | 0x33)
#define KMSG_USR_SYSTEM_LOWPOWER      (KMSG_USR_CLS_SYSTEM | 0x34)
#define KMSG_USR_SYSTEM_USBD_NOT_CONNECT      (KMSG_USR_CLS_SYSTEM | 0x35)

#define KMSG_USR_SYSTEM_TVD0_PLUGIN  (KMSG_USR_CLS_SYSTEM | 0x40)
#define KMSG_USR_SYSTEM_TVD1_PLUGIN  (KMSG_USR_CLS_SYSTEM | 0x41)
#define KMSG_USR_SYSTEM_TVD0_PLUGOUT (KMSG_USR_CLS_SYSTEM | 0x42)
#define KMSG_USR_SYSTEM_TVD1_PLUGOUT (KMSG_USR_CLS_SYSTEM | 0x43)
#define KMSG_USR_SYSTEM_TVD0_SYSTEM_CHANGE  (KMSG_USR_CLS_SYSTEM | 0x44)
#define KMSG_USR_SYSTEM_TVD1_SYSTEM_CHANGE  (KMSG_USR_CLS_SYSTEM | 0x45)

#define KMSG_USR_SYSTEM_PHONE_MSG_BASE (KMSG_USR_CLS_SYSTEM | 0x100)

/* key message */
#define KMSG_USR_KEY_POWER            (KMSG_USR_CLS_KEY | 30      )
#define KMSG_USR_KEY_SOURCE           (KMSG_USR_CLS_KEY | 31      )
#define KMSG_USR_KEY_CLEAR            (KMSG_USR_CLS_KEY | 32      )
#define KMSG_USR_KEY_DISPLAY          (KMSG_USR_CLS_KEY | 33      )
#define KMSG_USR_KEY_NUM0             (KMSG_USR_CLS_KEY | 48      )
#define KMSG_USR_KEY_NUM1             (KMSG_USR_CLS_KEY | 49      )
#define KMSG_USR_KEY_NUM2             (KMSG_USR_CLS_KEY | 50      )
#define KMSG_USR_KEY_NUM3             (KMSG_USR_CLS_KEY | 51      )
#define KMSG_USR_KEY_NUM4             (KMSG_USR_CLS_KEY | 52      )
#define KMSG_USR_KEY_NUM5             (KMSG_USR_CLS_KEY | 53      )
#define KMSG_USR_KEY_NUM6             (KMSG_USR_CLS_KEY | 54      )
#define KMSG_USR_KEY_NUM7             (KMSG_USR_CLS_KEY | 55      )
#define KMSG_USR_KEY_NUM8             (KMSG_USR_CLS_KEY | 56      )
#define KMSG_USR_KEY_NUM9             (KMSG_USR_CLS_KEY | 57      )
#define KMSG_USR_KEY_REPEATE          (KMSG_USR_CLS_KEY | 34      )
#define KMSG_USR_KEY_BLOCK            (KMSG_USR_CLS_KEY | 0       )
#define KMSG_USR_KEY_PLAY_PAUSE       (KMSG_USR_CLS_KEY | 35      )
#define KMSG_USR_KEY_TITLE            (KMSG_USR_CLS_KEY | 36      )
#define KMSG_USR_KEY_MENU             (KMSG_USR_CLS_KEY | 37      )
#define KMSG_USR_KEY_ENTER            (KMSG_USR_CLS_KEY | 13      )
#define KMSG_USR_KEY_SETUP            (KMSG_USR_CLS_KEY | 38      )
#define KMSG_USR_KEY_GOTO             (KMSG_USR_CLS_KEY | 39      )
#define KMSG_USR_KEY_LEFT             (KMSG_USR_CLS_KEY | 16      )
#define KMSG_USR_KEY_RIGHT            (KMSG_USR_CLS_KEY | 18      )
#define KMSG_USR_KEY_UP               (KMSG_USR_CLS_KEY | 17      )
#define KMSG_USR_KEY_DOWN             (KMSG_USR_CLS_KEY | 19      )
#define KMSG_USR_KEY_SEL              (KMSG_USR_CLS_KEY | 40      )
#define KMSG_USR_KEY_SHIFT            (KMSG_USR_CLS_KEY | 41      )
#define KMSG_USR_KEY_DISC             (KMSG_USR_CLS_KEY | 42      )
#define KMSG_USR_KEY_ATT              (KMSG_USR_CLS_KEY | 43      )
#define KMSG_USR_KEY_VOICE_UP         (KMSG_USR_CLS_KEY | 117     )
#define KMSG_USR_KEY_VOICE_DOWN       (KMSG_USR_CLS_KEY | 100     )
#define KMSG_USR_KEY_POWEROFF         (KMSG_USR_CLS_KEY | 255     )
#define KMSG_USR_KEY_RST              (KMSG_USR_CLS_KEY | 254     )
#define KMSG_USR_KEY_CANCLE           (KMSG_USR_CLS_KEY | 95      )
#define KMSG_USR_KEY_ZOOM_UP          (KMSG_USR_CLS_KEY | 96      )
#define KMSG_USR_KEY_ZOOM_DOWN        (KMSG_USR_CLS_KEY | 97      )
#define KMSG_USR_KEY_RISE             (KMSG_USR_CLS_KEY | 65535   )
#define KMSG_USR_KEY_HOLD             (KMSG_USR_CLS_KEY | 99      )



/****************************************************/
/* definition for msg target description */
/****************************************************/
#define KMSG_TGT_PROC_LOW           0x00000001
#define KMSG_TGT_PROC_HIGH          0x0000ffff
#define KMSG_TGT_INT_TAIL           0xfffffffd
#define KMSG_TGT_CALLBACK           0xfffffffe
#define KMSG_TGT_BROADCST           0xffffffff

#define KMSG_TGT_IS_PROC(x)         (x >= KMSG_TGT_PROC_LOW && x<= KMSG_TGT_PROC_HIGH)
#define KMSG_TGT_IS_INTTAIL(x)      (x == KMSG_TGT_INT_TAIL)
#define KMSG_TGT_IS_CB(x)           (x == KMSG_TGT_CALLBACK)
#define KMSG_TGT_IS_BC(x)           (x == KMSG_TGT_BROADCST)

/* definition for KMSG value */
#define KMSG_VAL_SYS_LOW            0x00000001
#define KMSG_VAL_SYS_HIGH           0x0000ffff
#define KMSG_VAL_USER_LOW           0x00010000
#define KMSG_VAL_USER_HIGH          0x0001ffff

#define KMSG_VAL_IS_SYS(x)          (x >= KMSG_VAL_SYS_LOW && x <= KMSG_VAL_SYS_HIGH)
#define KMSG_VAL_IS_USER(x)         (x >= KMSG_VAL_USER_LOW && x <= KMSG_VAL_USER_HIGH)

/* for device KMSG value define */
#define KMSG_VAL_DEV_BASE           (KMSG_VAL_SYS_LOW + 0x80)           /* 64 KMSG */
#define KMSG_VAL_DEV_PLUGIN         (KMSG_VAL_DEV_BASE + 0)
#define KMSG_VAL_DEV_PLUGOUT        (KMSG_VAL_DEV_BASE + 1)
#define KMSG_VAL_DEV_CFG            (KMSG_VAL_DEV_BASE + 2)

#define KMSG_VAL_PHONE_CMD_BASE     (KMSG_VAL_DEV_BASE+0x100)
#define KMSG_VAL_PHONE_CMD          (KMSG_VAL_PHONE_CMD_BASE + 0)


#define KMSG_VAL_DEV_USBD            0
#define KMSG_VAL_DEV_USBH            1
#define KMSG_VAL_DEV_SDMMC           2

/* for part KMSG value define */
#define KMSG_VAL_PART_BASE          (KMSG_VAL_SYS_LOW + 0xC0)           /* 32 KMSG */
#define KMSG_VAL_PART_PLUGIN        (KMSG_VAL_PART_BASE +0)
#define KMSG_VAL_PART_PLUGOUT       (KMSG_VAL_PART_BASE +1)
#define KMSG_VAL_PART_PLUGCFG       (KMSG_VAL_PART_BASE +2)

/* for fs KMSG valude define */
#define KMSG_VAL_FS_BASE            (KMSG_VAL_SYS_LOW + 0xE0)           /* 64 KMSG */
#define KMSG_VAL_FS_PLUGIN          (KMSG_VAL_FS_BASE +0)
#define KMSG_VAL_FS_PLUGOUT         (KMSG_VAL_FS_BASE +1)
#define KMSG_VAL_FS_PLUGCFG         (KMSG_VAL_FS_BASE +2)
#define KMSG_VAL_FS_OBJADD          (KMSG_VAL_FS_BASE +3)
#define KMSG_VAL_FS_OBJDEL          (KMSG_VAL_FS_BASE +4)
#define KMSG_VAL_USBD_CONNECT       (KMSG_VAL_FS_BASE +5)


/* definition for KMSG process priorate */
#define KMSG_PRIO_LOW               0
#define KMSG_PRIO_HIGH              1

/* definition for KMSG para max bytes len */
#define KMSG_PARA_BYTES_MAX         20

typedef struct __EPOS_KMSG
{
    /*
     *0x00000000: reserve
     *0x00000001~0x0000ffff: proc_ID directive type, to applications and drivers
     *0x00010000~0xfffffffd: reserve
     *0xfffffffe: call back type, for Bottem half realization
     *0xffffffff: broadcast type, broad to all GUI application win proccees
     */
    uint32_t       target;

    /*
     *0x00000000: reserve
     *0x00000001~0x0000ffff: system derived KMSG
     *0x00010000~0x0001ffff: user define KMSG
     *0x00020000~0xffffffff: reserve
     */
    uint32_t       message;

    /*
     *0: low prio, will be pushed into KMSG fifo queue
     *1: high prio, will be deal imediately
     */
    uint32_t       prio;

    /* five words para, defined in varied usage */
    union
    {
        int32_t   para;
        void (*cb)(int64_t cb_u_arg/*application*/, int64_t cb_s_arg/*system*/);
    } l;

    union
    {
        int32_t   para;
        int64_t   cb_u_arg;
    } h;

    int32_t       rdata[3];
} __epos_kmsg_t;

typedef struct
{
    int32_t       boot_card_num;
} __ksrv_add_para;

#endif /* #ifndef _KMSG_H_ */
