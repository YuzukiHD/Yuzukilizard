/*
**********************************************************************************************************************
*                                                   ePOS
*                               the Easy Portable/Player Operation System
*                                              Krnl sub-system
*
*                               (c) Copyright 2006-2007, Steven.ZGJ China
*                                           All Rights Reserved
*
* File   : sys_hwsc.h
* Version: V1.0
* By     : steven.zgj
**********************************************************************************************************************
*/
#ifndef _SYS_HWSC_H_
#define _SYS_HWSC_H_

#include <sys_device.h>

/* hwsc - USB */
#define DEV_IOC_USR_HWSC_USBD_SUSPEND                   (DEV_IOC_USR_BASE + 200)
#define DEV_IOC_USR_HWSC_USBD_RESET                     (DEV_IOC_USR_BASE + 201)
#define DEV_IOC_USR_HWSC_USBD_START                     (DEV_IOC_USR_BASE + 202)
#define DEV_IOC_USR_HWSC_USB_AUDIO_START                (DEV_IOC_USR_BASE + 203)
#define DEV_IOC_USR_HWSC_USB_HID_START                  (DEV_IOC_USR_BASE + 204)
#define DEV_IOC_USR_HWSC_USB_HID_REMOVE                 (DEV_IOC_USR_BASE + 205)


#define DEV_IOC_USR_HWSC_USBH_DISCONNECT                (DEV_IOC_USR_BASE + 208)
#define DEV_IOC_USR_HWSC_USBH_CONNECT                   (DEV_IOC_USR_BASE + 209)
#define DEV_IOC_USR_HWSC_USBH_REMOVE                    (DEV_IOC_USR_BASE + 210)

#define DEV_IOC_USR_HWSC_APP_INSMOD_USBH                (DEV_IOC_USR_BASE + 216)
#define DEV_IOC_USR_HWSC_APP_RMMOD_USBH                 (DEV_IOC_USR_BASE + 217)
#define DEV_IOC_USR_HWSC_APP_INSMOD_USBD                (DEV_IOC_USR_BASE + 218)
#define DEV_IOC_USR_HWSC_APP_RMMOD_USBD                 (DEV_IOC_USR_BASE + 219)
#define DEV_IOC_USR_HWSC_APP_DRV_NULL                   (DEV_IOC_USR_BASE + 220)
#define DEV_IOC_USR_HWSC_TV_INSMOD_USBH                 (DEV_IOC_USR_BASE + 221)
#define DEV_IOC_USR_HWSC_TV_RMMOD_USBH                  (DEV_IOC_USR_BASE + 222)

#define DEV_IOC_USR_HWSC_GET_USB_INFO                   (DEV_IOC_USR_BASE + 224)
#define DEV_IOC_USR_HWSC_USBH_INSMOD_STATUS             (DEV_IOC_USR_BASE + 225)

#define DEV_IOC_USR_HWSC_USBH_MSC_DEV_REG_SET           (DEV_IOC_USR_BASE + 233)
#define DEV_IOC_USR_HWSC_USBH_MSC_DEV_REG_GET           (DEV_IOC_USR_BASE + 234)
#define DEV_IOC_USR_START_USB_MONITOR                   (DEV_IOC_USR_BASE + 235)
#define DEV_IOC_USR_STOP_USB_MONITOR                    (DEV_IOC_USR_BASE + 236)
#define DEV_IOC_USR_GET_USB_CHARGE_SOURCE               (DEV_IOC_USR_BASE + 237)

#define DEV_IOC_USR_HWSC_SET_USBH_WORK_STATUS           (DEV_IOC_USR_BASE + 238)
#define DEV_IOC_USR_HWSC_GET_USBH_WORK_STATUS           (DEV_IOC_USR_BASE + 239)
#define DEV_IOC_USR_IS_NEED_APP_INSMOD_USBH             (DEV_IOC_USR_BASE + 240)
#define DEV_IOC_USR_GET_CURRENT_PORT                    (DEV_IOC_USR_BASE + 241)

#define DEV_IOC_USR_HWSC_APP_INSMOD_USBH_1              (DEV_IOC_USR_BASE + 242)
#define DEV_IOC_USR_HWSC_APP_RMMOD_USBH_1               (DEV_IOC_USR_BASE + 243)
#define DEV_IOC_USR_HWSC_APP_INSMOD_USBH_2              (DEV_IOC_USR_BASE + 244)
#define DEV_IOC_USR_HWSC_APP_RMMOD_USBH_2               (DEV_IOC_USR_BASE + 245)
#define DEV_IOC_USR_HWSC_SET_USBH1_WORK_STATUS          (DEV_IOC_USR_BASE + 246)
#define DEV_IOC_USR_HWSC_GET_USBH1_WORK_STATUS          (DEV_IOC_USR_BASE + 247)
#define DEV_IOC_USR_HWSC_SET_USBH2_WORK_STATUS          (DEV_IOC_USR_BASE + 248)
#define DEV_IOC_USR_HWSC_GET_USBH2_WORK_STATUS          (DEV_IOC_USR_BASE + 249)

//define command to enable or disable device monitor
#define DEV_IOC_USR_HWSC_ENABLE_MONITOR                 (DEV_IOC_USR_BASE + 250)
#define DEV_IOC_USR_HWSC_DISABLE_MONITOR                (DEV_IOC_USR_BASE + 251)

#define DEV_IOC_USR_SET_USBD_MODE                       (DEV_IOC_USR_BASE + 252)
#define DEV_IOC_USR_ADD_USBD_MODE                       (DEV_IOC_USR_BASE + 253)
#define DEV_IOC_USR_RMMOD_USBD_DEV                      (DEV_IOC_USR_BASE + 254)

#define DEV_IOC_USR_HWSC_SET_USB_HOST_SPEED             (DEV_IOC_USR_BASE + 300)
#define DEV_IOC_USR_HWSC_QUERY_USB_HOST_SPEED           (DEV_IOC_USR_BASE + 301)

#define DEV_IOC_USR_HWSC_SET_USB_AUDIO_WORK_STATUS      (DEV_IOC_USR_BASE + 302)

#define DEV_IOC_USR_INIT_HOST_CONTROLER                 (DEV_IOC_USR_BASE + 303)
#define DEV_IOC_USR_DEINIT_HOST_CONTROLER               (DEV_IOC_USR_BASE + 304)
#define DEV_IOC_USR_ENABLE_SD_MMC_CHECK                 (DEV_IOC_USR_BASE + 305)



//-----------------------------------------------------------------------------
//  usb device aux
//  mass storage: 0x00 ~ 0x99
//  vedio class:  0x100 ~ 0x199
//  audio class:  0x200 ~ 0x299
//-----------------------------------------------------------------------------
#define DEV_IOC_USR_USBD_AUX_MSC                        0x00
#define DEV_IOC_USR_USBD_AUX_UVC                        0x100
#define DEV_IOC_USR_USBD_AUX_UAC                        0x200
#define DEV_IOC_USR_USBD_AUX_HID                        0x300

//-----------------------------------------------
//   sd add by weiziheng 2011-5-4 16:24
//-----------------------------------------------
#define DEV_IOC_USR_HWSC_SD_DRV_INSMOD_OK               (DEV_IOC_USR_BASE + 500)
#define DEV_IOC_USR_HWSC_SD_DRV_RMMOD_OK                (DEV_IOC_USR_BASE + 501)
#define DEV_IOC_USR_HWSC_SD_DRV_SET_READ_WRITE_FLAG     (DEV_IOC_USR_BASE + 502)


//-----------------------------------------------
//   usb audio add by Bayden 2011-12-12
//-----------------------------------------------
#define DEV_IOC_USR_HWSC_APP_INSMOD_USB_AUDIO           (DEV_IOC_USR_BASE + 510)
#define DEV_IOC_USR_HWSC_APP_RMMOD_USB_AUDIO            (DEV_IOC_USR_BASE + 511)
#define DEV_IOC_USR_HWSC_IS_ALLOW_APP_INSMOD_USB_AUDIO  (DEV_IOC_USR_BASE + 512)
#define DEV_IOC_USR_HWSC_GET_USB_AUDIO_WORK_STATUS      (DEV_IOC_USR_BASE + 513)


#define DEV_IOC_USR_HWSC_APP_INSMOD_USB_HID             (DEV_IOC_USR_BASE + 514)
#define DEV_IOC_USR_HWSC_APP_RMMOD_USB_HID              (DEV_IOC_USR_BASE + 515)

typedef enum usbd_mode
{
    USBD_UDISK      = 0,
    USBD_UVC,
    USBD_CHARGE,
} usbm_usbd_mode_t;


typedef struct tag_usbm_ioctrl_para
{
    uint32_t    usb_drv;
    uint32_t    usbh_inmod_info;
    uint32_t    usbh_msc_dev_reg;
    uint32_t    usb_cattle;
} usbm_ioctrl_para_t;

typedef enum usbm_usbh_insmod
{
    USBH_INSMOD_FAIL = 0,
    USBH_INSMOD_BUSY,
    USBH_INSMOD_SUCCESS
} usbm_usbh_insmod_t;


typedef struct __nand_hwsc_info
{
    uint8_t     id[8];
    uint8_t     chip_cnt;
    uint8_t     chip_connect;
    uint8_t     rb_cnt;
    uint8_t     rb_connect;
    uint32_t    good_block_ratio;
} __nand_hwsc_info_t;

#endif //#ifndef _SYS_HWSC_H_
