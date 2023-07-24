#include <stdlib.h>

#include <stdio.h>
#include <rtthread.h>
#include <dfs_posix.h>

#include <typedef.h>
#include <kapi.h>
#include <log.h>

typedef struct __DEV_RAM
{
    __u32           card_no;
    __u32           boot;
    __u32           offset;
    __u32           used;
    char            name[32];
    char            major_name[16];
    char            minor_name[16];
    __hdle          hReg;
    __dev_blkinfo_t info;
} __dev_ram_t;

static char *data = NULL;

static __s32 ram_dev_ioctrl(__hdle hDev, __u32 Cmd, long Aux, void *pBuffer)
{
    __dev_ram_t *pDev = (__dev_ram_t *)hDev;

    switch (Cmd)
    {
        case DEV_CMD_GET_INFO:
        {
            if (!pBuffer)
            {
                return EPDK_FAIL;
            }

            *((__dev_blkinfo_t *)pBuffer) = pDev->info;
            return EPDK_OK;
        }

        default:
            break;
    }

    return EPDK_FAIL;
}

static __hdle ram_dev_open(void *open_arg, __u32 Mode)
{
    __dev_ram_t *pDev = (__dev_ram_t *)open_arg;

    return pDev;
}

static __s32 ram_dev_close(__hdle hDev)
{
    return 0;
}

#define SDXC_MAX_TRANS_LEN              (4 * 1024 * 1024)       /* max len is 4M */
static __u32 ram_dev_read(void *pBuffer, __u32 blk, __u32 n, __hdle hDev)
{
    __dev_ram_t *pDev = (__dev_ram_t *)hDev;
    __u32 nblk = n;
    __u32 offset = 0;

    memcpy(pBuffer, data + blk * 512, n * 512);
    return n;
}

static __u32 ram_dev_write(const void *pBuffer, __u32 blk, __u32 n, __hdle hDev)
{
    __dev_ram_t *pDev = (__dev_ram_t *)hDev;
    __u32 nblk = n;
    __u32 offset = 0;

    memcpy(data + blk * 512, pBuffer, n * 512);
    return n;
}

static __dev_devop_t ram_dev_op =
{
    ram_dev_open,
    ram_dev_close,
    ram_dev_read,
    ram_dev_write,
    ram_dev_ioctrl
};

static __dev_ram_t *pDev = NULL;

int ramfs_fat_init(int argc, char **argv)
{
    int aux = 0;

    pDev = (__dev_ram_t *)malloc(sizeof(__dev_ram_t));
    if (NULL == pDev)
    {
        __err("allocate memory failed");
        return -1;
    }

    memset(pDev, 0, sizeof(__dev_ram_t));
    snprintf(pDev->name, 32, "%s:%d", "SDMMC-DISK", aux);
    __log("sdcard device name: %s", pDev->name);

    pDev->card_no          = aux;
    pDev->boot             = 0;
    pDev->offset           = 0;
    pDev->used             = 0;
    pDev->info.hiddennum   = 0;
    pDev->info.headnum     = 2;
    pDev->info.secnum      = 4;
    pDev->info.partsize    = CONFIG_RAMFS_FAT_TEST_SIZE / 512;
    pDev->info.secsize     = 512;

    if (!data)
    {
        data = malloc(CONFIG_RAMFS_FAT_TEST_SIZE);
    }

    if (!data)
    {
        free(pDev);
        pDev = NULL;
        return -1;
    }
    memset(data, 0, CONFIG_RAMFS_FAT_TEST_SIZE);

    pDev->hReg = esDEV_DevReg("DISK", pDev->name, &ram_dev_op, (void *)pDev);
    if (NULL == pDev->hReg)
    {
        __err("sdcard register partition failed");
        return -1;
    }
    esFSYS_format("e:\\", "fat", 0);
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(ramfs_fat_init, __cmd_ramfs_fat, mount ramfs as fat filesystem);

static int ramfs_fat_deinit(void)
{
    if (!pDev)
    {
        return 0;
    }
    if (data)
    {
        free(data);
        data = NULL;
    }
    esDEV_DevUnreg(pDev->hReg);
    free(pDev);
    pDev = NULL;
    return 0;
}
