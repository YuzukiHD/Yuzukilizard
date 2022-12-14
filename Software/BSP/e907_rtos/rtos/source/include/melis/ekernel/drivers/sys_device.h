/*
**********************************************************************************************************************
*                                                   ePOS
*                               the Easy Portable/Player Operation System
*                                              Krnl sub-system
*
*                               (c) Copyright 2006-2007, Steven.ZGJ China
*                                           All Rights Reserved
*
*                                File   : sys_dev.h
*                                Version: V1.0
*                                By     : steven.zgj
**********************************************************************************************************************
*/

#ifndef _SYS_DEV_H_
#define _SYS_DEV_H_

#include <typedef.h>

#define DEV_DEVTYPE_MAXNUM              32

/* maximum size of device class name */
#define MAX_DEV_CLASS_NAME_LEN          32

/* maximum size of device node name */
#define MAX_DEV_NODE_NAME_LEN           32

#define DEV_CLASS_NOP                   "NOP"
#define DEV_CLASS_DISK                  "DISK"
#define DEV_CLASS_USBD                  "USBD"
#define DEV_CLASS_KEY                   "KEY"
#define DEV_CLASS_MOUSE                 "MOUSE"
#define DEV_CLASS_FS                    "FSDRV"
#define DEV_CLASS_PD                    "PDDRV"
#define DEV_CLASS_DMS                   "DMS"
#define DEV_CLASS_HWSC                  "HWSC"

#define DEV_CLASS_USERDEF               "USERDEF"


#define DEV_NAME_RAMDISK                "RAMDISK"
#define DEV_NAME_SDCARD                 "SDMMC-DISK"
#define DEV_NAME_SDCARD0                "SDMMC-DISK:0"
#define DEV_NAME_SDCARD1                "SDMMC-DISK:1"
#define DEV_NAME_SCSI_DISK_00           "SCSI_DISK_00"
#define DEV_NAME_USB_CDROM_00           "USB_CDROM_00"
#define DEV_NAME_ROOTFS                 "ROOTFS"
#define DEV_NAME_PROVIDERFS             "PROVIDERFS"
#define DEV_NAME_SYSDATAFS              "SYSDATAFS"
#define DEV_NAME_SYSBOOTFS              "SYSBOOTFS"
#define DEV_NAME_BOOTFS                 "BOOTFS"
#define DEV_NAME_USERDISK               "USERDISK"
#define DEV_NAME_HWSC                   "hwsc"
#define DEV_NAME_UDISK                  "UDISK"
#define DEV_NAME_VIR_DISK               "VIRDISK"

#define DEV_NODE_ATTR_BLK               (1<<0)
#define DEV_NODE_ATTR_CHR               (1<<1)
#define DEV_NODE_ATTR_CTL               (1<<2)
#define DEV_NODE_ATTR_PART              (1<<3)
#define DEV_NODE_ATTR_FS                (1<<4)
#define DEV_NODE_ATTR_MOVABLE           (1<<5)
#define DEV_NODE_ATTR_USBSLOT           (1<<6)
#define DEV_NODE_ATTR_RD                (1<<7)
#define DEV_NODE_ATTR_WR                (1<<8)
#define DEV_NODE_ATTR_SYNMNT            (1<<9)

#define DEV_CMD_GET_INFO                0
#define DEV_CMD_GET_INFO_AUX_SIZE       0
#define DEV_CMD_GET_STATUS              1
#define DEV_CMD_GET_OFFSET              2


/* user defined device ioctrol cmd: 0x00000000~0x7FFFFFFF */
/* system defined device ioctrol cmd: 0x80000000~0xFFFFFFFF */
#define DEV_IOC_USR_BASE                0x00000000
#define DEV_IOC_SYS_BASE                0x80000000
#define IS_DEVIOCSYS(cmd)               (cmd>=DEV_IOC_SYS_BASE)
/* sys */
#define DEV_IOC_SYS_GET_DEVNAME         (DEV_IOC_SYS_BASE + 0)
#define DEV_IOC_SYS_GET_CLSNAME         (DEV_IOC_SYS_BASE + 1)
#define DEV_IOC_SYS_GET_ATTRIB          (DEV_IOC_SYS_BASE + 2)
#define DEV_IOC_SYS_GET_LETTER          (DEV_IOC_SYS_BASE + 3)
#define DEV_IOC_SYS_GET_OPENARGS        (DEV_IOC_SYS_BASE + 4)

/* dms */
#define DEV_IOC_USR_GET_DEVITEM         (DEV_IOC_USR_BASE + 102)
#define DEV_IOC_USR_GET_DEVROOT         (DEV_IOC_USR_BASE + 103)
/* blk */
#define DEV_IOC_USR_GET_GEO             (DEV_IOC_USR_BASE + 104)
#define DEV_IOC_USR_FLUSH_CACHE         (DEV_IOC_USR_BASE + 105)
#define DEV_IOC_USR_PHY_READ            (DEV_IOC_USR_BASE + 106)
#define DEV_IOC_USR_PHY_WRITE           (DEV_IOC_USR_BASE + 107)
#define DEV_IOC_USR_PHY_ERASE           (DEV_IOC_USR_BASE + 108)
#define DEV_IOC_USR_LOGIC_WRITE         (DEV_IOC_USR_BASE + 109)
#define DEV_IOC_USR_LOGIC_READ          (DEV_IOC_USR_BASE + 110)
#define DEV_IOC_USR_GET_HW_INFO         (DEV_IOC_USR_BASE + 111)
#define DEV_IOC_USR_BLK_ERASE           (DEV_IOC_USR_BASE + 112)
#define DEV_IOC_USR_SECTOR_ERASE        (DEV_IOC_USR_BASE + 113)
#define DEV_IOC_USR_FLUSH_BLOCK_CACHE   (DEV_IOC_USR_BASE + 114)


/* cd-rom */
#define DEV_CDROM_LAST_WRITTEN          (DEV_IOC_USR_BASE + 120)  /* get last block written on disc */
#define DEV_CDROM_MULTISESSION          (DEV_IOC_USR_BASE + 121)  /* Obtain the start-of-last-session
                                                               * address of multi session disks,
                                                               * address type is logical block.*/

#define DRV_CMD_PLUGIN                  0xffff0000
#define DRV_CMD_PLUGOUT                 0xffff0001
#define DRV_CMD_GET_STATUS              0xffff0002

typedef enum __DRV_STA
{
    DRV_STA_FREE = 0,
    DRV_STA_BUSY
} __drv_sta_t;

typedef enum __DEV_ERR
{
    DEV_NO_ERR = 0,
    DEV_ERR,
    DEV_HANDLE_ERR,                                 /* handle error, maybe handle equ. null, or handle not a opened */
    /* handle                                                       */
    DEV_CLK_NOT_EN,                                 /* clock not enable                                             */
    DEV_CLK_SCLK_ERR,
    DEV_CLK_DIV_ERR,
    DEV_CLK_HANDLE_FREE,
} __dev_err_t;

typedef struct  __DEV_DEVOP
{
    __hdle      (*Open)(void *open_arg, uint32_t mode);
    int32_t     (*Close)(__hdle hDev);
    uint32_t    (*Read)(void * /*pBuffer*/, uint32_t /*chrdev: 1,blkdev: sector pos*/,
                 uint32_t /*chrdev: byte cnt, blkdev: sector cnt*/, __hdle/*hDev*/);
    uint32_t    (*Write)(const void * /*pBuffer*/, uint32_t /*chrdev: 1, blkdev: sector pos*/,
                  uint32_t /*chrdev: byte cnt, blkdev: sector cnt*/, __hdle/*hDev*/);
    int32_t     (*Ioctl)(__hdle hDev, uint32_t Cmd, long Aux, void *pBuffer);
} __dev_devop_t;

typedef  struct __DEV_BLKINFO
{
    uint32_t            hiddennum;
    uint32_t            headnum;
    uint32_t            secnum;
    uint32_t            partsize;
    uint32_t            secsize;
} __dev_blkinfo_t;

/* disk info            */
typedef  struct __DEV_DSKINFO
{
    int8_t              name[16];
    uint32_t            n;
    __dev_blkinfo_t     *info;
} __dev_dskinfo_t;

typedef enum __DEV_HOTPLUG_MSG_TARGET
{
    DEV_MSG_TARGET_HOTPLUG_USBMONITOR,
} __dev_msgtarget_t;

typedef struct __BLK_DEV_RW_ATTR
{
    uint32_t            blk;
    int32_t             cnt;
    void                *buf;
    uint32_t            reserved;
} __blk_dev_rw_attr_t;

struct DmsNodeInfo_dev
{
    int32_t             *name;
    uint32_t            key;
    int32_t             type;
    __hdle              hnode;
};

struct DmsDirInfo_dev
{
    int32_t             itemPos;
    __hdle              dir;
};

int32_t dev_init(void);
int32_t dev_exit(void);

#endif //#ifndef _SYS_DEV_H_
