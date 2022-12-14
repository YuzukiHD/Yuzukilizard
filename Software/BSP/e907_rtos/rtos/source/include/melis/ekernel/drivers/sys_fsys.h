#ifndef __SYS_FSYS_H__
#define __SYS_FSYS_H__
#include <typedef.h>
#include <stdbool.h>
#include "sys_device.h"

//melis legacy fseek semantic. else posix semantic
//#define FSEEK_LEGACY

#define FS_ATTR_READONLY                0x00000001 /* readonly fs */
#define FSYS_VERSION                    0x01000000UL   /* 1.00.0000 */

/* Maximum size of a directory name */
#define FSYS_DIRNAME_MAX                1024

/* maximum size of volume name */
#define MAX_VOLUME_NAME_LEN             512

/* maximum size of part name */
#define MAX_PART_NAME_LEN               MAX_VOLUME_NAME_LEN

/* maximum size of file system name */
#define MAX_FS_NAME_LEN                 16

/* file attributes */
#define FSYS_ATTR_READONLY              0x00000001
#define FSYS_ATTR_HIDDEN                0x00000002
#define FSYS_ATTR_SYSTEM                0x00000004
#define FSYS_FAT_VOLUME_ID              0x00000008
#define FSYS_ATTR_DIRECTORY             0x00000010
#define FSYS_ATTR_ARCHIVE               0x00000020

#define FSYS_FAT_DENTRY_SIZE            0x20
#define MAX_DISK_LETTER_LEN             26
#define MAX_DIR_SCAN_LEVEL              32

/* names of supported file system */
#define FSYS_NTFS_NAME                  "NTFS"
#define FSYS_FAT_NAME                   "FAT"
#define FSYS_EXFAT_NAME                 "exFAT"
#define FSYS_CDFS_NAME                  "CDFS"
#define FSYS_UDF_NAME                   "UDF"
/* Global error codes */
#define FSYS_ERR_OK                     (int16_t)0x0000
#define FSYS_ERR_EOF                    (int16_t)0xfff0
#define FSYS_ERR_DISKFULL               (int16_t)0xffe0
#define FSYS_ERR_INVALIDPAR             (int16_t)0xffd0
#define FSYS_ERR_WRITEONLY              (int16_t)0xffc0
#define FSYS_ERR_READONLY               (int16_t)0xffb0
#define FSYS_ERR_READERROR              (int16_t)0xffa0
#define FSYS_ERR_WRITEERROR             (int16_t)0xff90
#define FSYS_ERR_DISKCHANGED            (int16_t)0xff80
#define FSYS_ERR_CLOSE                  (int16_t)0xff70

#define FSYS_MAX_FPARTS                 16
#ifdef CONFIG_SOC_SUN20IW1
#define FSYS_MAX_XPARTS                 9
#else
#define FSYS_MAX_XPARTS                 8
#endif
#define FSYS_MAX_UPARTS                 2
#define FSYS_MAX_PARTS                  (FSYS_MAX_FPARTS + FSYS_MAX_XPARTS + FSYS_MAX_UPARTS)
#define PART_LETTER_FREE                0xff
#ifdef CONFIG_SOC_SUN20IW1
#define PART_LETTER_FREESTART           'F'
#else
#define PART_LETTER_FREESTART           'E'
#endif
#define PART_LETTER_USER                0xfe

#define PART_LETTER_USERSTART           'V'
#define PART_LETTER_DMS                 'B'
#define PART_LETTER_RAMDISK             'C'
#define PART_LETTER_ROOTFS              'D'
#define PART_LETTER_UDISK               'E'
#define PART_LETTER_SYSDATA             'Z'
#define PART_LETTER_SYSBOOT             'Y'
#define PART_LETTER_BOOTFS              'X'
#define PART_LETTER_RESERVEV            'V'
#define PART_LETTER_RESERVEU            'U'
#define PART_LETTER_RESERVEA            'A'

/* I/O coSYmmands */
#define FSYS_CMD_FLUSH_CACHE            1000L
#define FSYS_CMD_CHK_DSKCHANGE          1010L
#define FSYS_CMD_READ_SECTOR            1100L
#define FSYS_CMD_WRITE_SECTOR           1110L
#define FSYS_CMD_FORMAT_MEDIA           2222L
#define FSYS_CMD_FORMAT_AUTO            2333L
#define FSYS_CMD_INC_BUSYCNT            3001L
#define FSYS_CMD_DEC_BUSYCNT            3002L
#define FSYS_CMD_GET_DISKFREE           4000L
#define FSYS_CMD_GET_DEVINFO            4011L
#define FSYS_CMD_FLASH_ERASE_CHIP       9001L
/* part I/O coSYmmands */
#define FSYS_PART_CMD_GET_PARTSIZE      1000L 
#define FSYS_PART_CMD_FLUSH_CACHE       1001L
#define FSYS_PART_CMD_INC_BUSYCNT       1100L
#define FSYS_PART_CMD_DEC_BUSYCNT       1101L
#define FSYS_PART_CMD_GET_STATUS        1102L
#define FSYS_PART_CMD_GET_INFO          1103L
#define FSYS_PART_CMD_FORMAT            1104L
#define FSYS_PART_MODE_CACHE_USED       1002L
#define FSYS_PART_MODE_CACHE_UNUSED     1003L

/* part I/O control command */
#define PART_IOC_USR_BASE               0x00000000
#define PART_IOC_SYS_BASE               0x80000000
#define IS_PARTIOCSYS(cmd)              (cmd>=PART_IOC_SYS_BASE)
/* for sys */
#define PART_IOC_SYS_GETNAME            (PART_IOC_SYS_BASE + 0)
#define PART_IOC_SYS_SETFSPRIV          (PART_IOC_SYS_BASE + 1)
#define PART_IOC_SYS_GETFSPRIV          (PART_IOC_SYS_BASE + 2)
#define PART_IOC_SYS_GETPDPRIV          (PART_IOC_SYS_BASE + 3)
#define PART_IOC_SYS_GETDEV             (PART_IOC_SYS_BASE + 4)
#define PART_IOC_SYS_GETLETTER          (PART_IOC_SYS_BASE + 5)
#define PART_IOC_SYS_GETLASTPART        (PART_IOC_SYS_BASE + 6)

/* for raw part */
#define PART_IOC_USR_GETPARTSIZE        (PART_IOC_USR_BASE+0)
#define PART_IOC_USR_FLUSHCACHE         (PART_IOC_USR_BASE+1)
#define PART_IOC_USR_CACHEUSED          (PART_IOC_USR_BASE+2)
#define PART_IOC_USR_CACHEUNUSED        (PART_IOC_USR_BASE+3)
#define PART_IOC_USR_GETSCTSIZE         (PART_IOC_USR_BASE+4)
/* for dms part */
#define PART_IOC_USR_GETITEM            (PART_IOC_USR_BASE+5)
#define PART_IOC_USR_GETDEVCLSROOT      (PART_IOC_USR_BASE+6)
/* for cd-rom part */
#define PART_IOC_CDROM_LAST_WRITTEN     (PART_IOC_USR_BASE+7)
#define PART_IOC_CDROM_MULTISESSION     (PART_IOC_USR_BASE+8)

/* fs I/O control command */
#define FS_IOC_USR_BASE                 0x00000000
#define FS_IOC_SYS_BASE                 0x80000000
#define IS_FSIOCSYS(cmd)                (cmd>=FS_IOC_SYS_BASE)
/* for sys */
#define FS_IOC_SYS_GETFLAGS             (FS_IOC_SYS_BASE+0x80)
#define FS_IOC_SYS_SETFLAGS             (FS_IOC_SYS_BASE+0x81)
#define FS_IOC_SYS_GETVERSION           (FS_IOC_SYS_BASE+0x82)
#define FS_IOC_SYS_SETVERSION           (FS_IOC_SYS_BASE+0x83)
#define FS_IOC_SYS_GETATTR              (FS_IOC_SYS_BASE+0x84)

/* for fat fs */
#define FS_IOC_USR_DEBUG_FATCHUNK       (FS_IOC_SYS_BASE+0x100)
#define FSYS_SECTOR_SIZE                512
#define FSYS_PART_MEDIACHANGED          0x0001

/* for minfs fs */
#define MINFS_IOC_GET_UNCOMPRESS_FILE_SIZE  (FS_IOC_SYS_BASE+0x200)
#define MINFS_IOC_GET_UNCOMPRESS_FILE_DATA  (FS_IOC_SYS_BASE+0x201)

#define DEVFS_NODE_TYPE                 1
#define DEVFS_CLASS_TYPE                2

/* flags for esFSYS_statfs (get file system status) operation */
#define FSYS_KSTATUS_TYPE               0x00000001  /* get fs magic type            */
#define FSYS_KSTATUS_BSIZE              0x00000002  /* get fs block size            */
#define FSYS_KSTATUS_BLOCKS             0x00000004  /* get fs total blocks number   */
#define FSYS_KSTATUS_BFREE              0x00000008  /* get fs free blocks number(for NTFS it is very slowly) */
#define FSYS_KSTATUS_FILES              0x00000010  /* get fs total files number    */
#define FSYS_KSTATUS_NAMELEN            0x00000100  /* get fs name length           */
#define FSYS_KSTATUS_ATTR               0x00000200  /* get fs attribute(eg:READONLY and so on) */
#define FSYS_KSTATUS_MEDIAATTR          0x00000400  /* get fs media attribute       */
#define FSYS_KSTATUS_VOLLETTER          0x00000800  /* get fs volume letter         */
#define FSYS_KSTATUS_VOLNAME            0x00001000  /* get fs volume name           */
#define FSYS_KSTATUS_FSNAME             0x00002000  /* get fs type name             */
#define FSYS_KSTATUS_ALL                0x0000FFFF  /* get fs all kernal status information*/

struct ktimestamp
{
    uint16_t        year;
    uint8_t         month;
    uint8_t         day;
    uint8_t         hour;
    uint8_t         minute;
    uint8_t         second;
    uint8_t         milliseconds;
};

struct kstat
{
    unsigned short      mode;
    unsigned int        nlink;
    long long           size;
    long long           pos;
    struct ktimestamp   atime;
    struct ktimestamp   mtime;
    struct ktimestamp   ctime;
    unsigned long       blksize;
    unsigned long long  blocks;
};

/*
 * Kernel pointers have redundant information, so we can use a
 * scheme where we can return either an error code or a dentry
 * pointer with the same return value.
 *
 * This should be a per-architecture thing, to allow different
 * error and pointer decisions.
 */
#define MAX_ERRNO   4095

#define IS_ERR_VALUE(x) ((x) >= (unsigned long)-MAX_ERRNO)

static inline void *ERR_PTR(long error)
{
    return (void *) error;
}

static inline long PTR_ERR(const void *ptr)
{
    return (long) ptr;
}

static inline long IS_ERR(const long ptr)
{
    return IS_ERR_VALUE((unsigned long)ptr);
}

static inline bool IS_ERR_OR_NULL(const void *ptr)
{
	return (!ptr) || IS_ERR_VALUE((unsigned long)ptr);
}

struct kstatpt
{
    char        *p_pdname;             /* part driver name, like "dmspart" or "dospart" */
    uint32_t    p_bsize;              /* part block size */
    uint32_t    p_blocks;             /* part blocks number */

    uint32_t    p_attr;               /* copy from part.attr, indecate part attribute like rw mode and formated */
    uint8_t     p_status;             /* copy of part.satus, fs used or raw used or dead */
    uint8_t     p_name[MAX_PART_NAME_LEN];    /* strcopy from part.dname, it is part name derived from dev */

    /* the device information */
    uint8_t     p_devname[MAX_DEV_NODE_NAME_LEN];       /* device node name */
    uint8_t     p_devclassname[MAX_DEV_CLASS_NAME_LEN]; /* device class name*/
    uint32_t    p_devattr;                              /* device attribute */
};

#endif
