/* Industrialio buffer test code.
 *
 * Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is primarily intended as an example application.
 * Reads the current buffer setup from sysfs and starts a short capture
 * from the specified device, pretty printing the result after appropriate
 * conversion.
 *
 * Command line parameters
 * generic_buffer -n <device_name> -t <trigger_name>
 * If trigger name is not specified the program assumes you want a dataready
 * trigger associated with the device and goes looking for it.
 *
 */

#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <sched.h>
#include <sys/syscall.h>
#include <linux/types.h>
#include <string.h>
#include <poll.h>
#include <endian.h>
#include <getopt.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cdx_list.h>
#include <utils/ring_buffer.h>

#include "iio_utils.h"
#include "generic_buffer.h"

struct gyro_chn_mgr {
    int elem_cnt;
     /* open_cnt maybe not equal to elem_cnt,
     * only you open one elem, then this value will be increase.*/
    int open_cnt;
    pthread_mutex_t lock;

    /* This thread will prodce gyro datas.
    * it will be run when the first gyro elem was opened, and
    * will exit when the final gyro elem was closed.
    */
    pthread_t gyro_pid;

    /* Only need to read it in product thread, must not change it. */
    bool bhalt; // Sometimes, we should re-open device, so just halt it in a while.

    /* Only need to write it in product thread, must not change it in other place. */
    bool bhas_stop;
    struct gyro_device_attr gyro_attr;
    /* In all mode, it just means the recently last non-zero timestamp */
    uint64_t last_pkt_timestamp;
    /* OOOOOOOOOnly used by [TS_AVERAGE_PROC] mode, means witch timestamp we are accessing,
    * attention, it may be very eraly, always time:[cur_pkt_timestamp <= last_pkt_timestamp]. */
    uint64_t cur_pkt_timestamp;

    /* Use this value to do average timestamp produce. */
    struct gyro_pkt *gpkt_cache;
    int gpkt_cnt;

    struct list_head mlist;
};

struct gyro_elem {
    int idx;
    bool bopen;
    struct gyro_instance gyro_ins;
    struct ring_buffer *rb_hd;
    struct list_head mlist;
};

static struct gyro_chn_mgr g_gyro_chn_mgr = {
    .elem_cnt = 0,
    .open_cnt = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .last_pkt_timestamp = 0,
    .cur_pkt_timestamp  = 0,
    .bhalt = 0,
    .bhas_stop = 0,
    .gpkt_cache = NULL,
    .gpkt_cnt = 0,
};

/**
 * size_from_channelarray() - calculate the storage size of a scan
 * @channels:       the channel info array
 * @num_channels:   number of channels
 *
 * Has the side effect of filling the channels[i].location values used
 * in processing the buffer output.
 **/
int size_from_channelarray(struct iio_channel_info *channels, int num_channels)
{
    int bytes = 0;
    int i = 0;

    while (i < num_channels) {
        if (bytes % channels[i].bytes == 0)
            channels[i].location = bytes;
        else
            channels[i].location = bytes - bytes % channels[i].bytes + channels[i].bytes;

        bytes = channels[i].location + channels[i].bytes;
        i++;
    }

    return bytes;
}

void print2byte(float *output, uint16_t input, struct iio_channel_info *info)
{
    /* First swap if incorrect endian */
    if (info->be)
        input = be16toh(input);
    else
        input = le16toh(input);

    /*
     * Shift before conversion to avoid sign extension
     * of left aligned data
     */
    input >>= info->shift;
    input &= info->mask;
    if (info->is_signed) {
        int16_t val = (int16_t)(input << (16 - info->bits_used)) >>
                  (16 - info->bits_used);
        // printf("%05f ", ((float)val + info->offset) * info->scale);
        *output = ((float)val + info->offset) * info->scale;
    } else {
        // printf("%05f ", ((float)input + info->offset) * info->scale);
        *output = ((float)input + info->offset) * info->scale;
    }
}

void print4byte(float *output, uint32_t input, struct iio_channel_info *info)
{
    /* First swap if incorrect endian */
    if (info->be)
        input = be32toh(input);
    else
        input = le32toh(input);

    /*
     * Shift before conversion to avoid sign extension
     * of left aligned data
     */
    input >>= info->shift;
    input &= info->mask;
    if (info->is_signed) {
        int32_t val = (int32_t)(input << (32 - info->bits_used)) >>
                  (32 - info->bits_used);
        printf("%05f ", ((float)val + info->offset) * info->scale);
        *output = ((float)val + info->offset) * info->scale;
    } else {
        printf("%05f ", ((float)input + info->offset) * info->scale);
        *output = ((float)input + info->offset) * info->scale;
    }
}

void print8byte(uint64_t *pts, uint64_t input, struct iio_channel_info *info)
{
    /* First swap if incorrect endian */
    if (info->be)
        input = be64toh(input);
    else
        input = le64toh(input);

    /*
     * Shift before conversion to avoid sign extension
     * of left aligned data
     */
    input >>= info->shift;
    input &= info->mask;
    if (info->is_signed) {
        int64_t val = (int64_t)(input << (64 - info->bits_used)) >>
                  (64 - info->bits_used);
        /* special case for timestamp */
        if (info->scale == 1.0f && info->offset == 0.0f) {
            // printf("%" PRId64 " ", val);
            *pts = val;
        } else {
            printf(" %05f ",((float)val + info->offset) * info->scale);
            *pts = (uint64_t)(((float)val + info->offset) * info->scale);
        }
    } else {
        printf(" %05f ", ((float)input + info->offset) * info->scale);
        *pts = (uint64_t)(((float)input + info->offset) * info->scale);
    }
    // printf("-> pts=%lld. ", *pts);
}

void _DyroDebugDumpAllChannelInfos(struct gyro_device_attr *gyro_attr)
{
    int i = 0;

    for (i = 0; i < gyro_attr->arry_elems; i++) {
        printf("Channel [%d]:\r\n", i);
        printf("\tName\t%s\r\n", gyro_attr->iio_chn[i].name);
        printf("\tGName\t%s\r\n", gyro_attr->iio_chn[i].generic_name);
        printf("\tScale\t%f\r\n", gyro_attr->iio_chn[i].scale);
        printf("\toffset\t%f\r\n", gyro_attr->iio_chn[i].offset);
        printf("\tindex\t%u\r\n", gyro_attr->iio_chn[i].index);
        printf("\tbytes\t%u\r\n", gyro_attr->iio_chn[i].bytes);
        printf("\tbitused\t%u\r\n", gyro_attr->iio_chn[i].bits_used);
        printf("\tshift\t%u\r\n", gyro_attr->iio_chn[i].shift);
        printf("\tbe\t%u\r\n", gyro_attr->iio_chn[i].be);
        printf("\tis_sign\t%u\r\n", gyro_attr->iio_chn[i].is_signed);
        printf("\tlocation\t%u\r\n\n", gyro_attr->iio_chn[i].location);
    }
}

int open_mpu6500(struct gyro_device_attr *gyro_attr)
{
    int ret = 0;
    int i = 0;
    char node_tmp[1024];

    struct gyro_init_sequence *init_seq = gyro_attr->init_seq;

    /* write initialize values */
    for (i = 0; init_seq[i].dir_base_name != NULL; i++) {
        if (init_seq[i].is_int) {
            ret = write_sysfs_int(init_seq[i].file_name, init_seq[i].dir_base_name, init_seq[i].init_val.val);
            if (ret < 0) {
                printf("Write %s/%s %d failed, do it again.\r\n",
                    init_seq[i].dir_base_name, init_seq[i].file_name, init_seq[i].init_val.val);
                ret = write_sysfs_int(init_seq[i].file_name, init_seq[i].dir_base_name, init_seq[i].init_val.val);
                if (ret < 0) {
                    printf("Write %s/%s %d failed again, give it up.\r\n",
                        init_seq[i].dir_base_name, init_seq[i].file_name, init_seq[i].init_val.val);
                    goto e_write;
                }
            }
//            printf("Write %d to %s/%s.\r\n", init_seq[i].init_val.val, init_seq[i].dir_base_name, init_seq[i].file_name);
        } else {
            write_sysfs_string(init_seq[i].file_name, init_seq[i].dir_base_name, init_seq[i].init_val.string);
            if (ret < 0) {
                printf("Write %s/%s %s failed, do it again.\r\n",
                    init_seq[i].dir_base_name, init_seq[i].file_name, init_seq[i].init_val.string);
                ret = write_sysfs_string(init_seq[i].file_name, init_seq[i].dir_base_name, init_seq[i].init_val.string);
                if (ret < 0) {
                    printf("Write %s/%s %s failed again, give it up.\r\n",
                        init_seq[i].dir_base_name, init_seq[i].file_name, init_seq[i].init_val.string);
                    goto e_write;
                }
            }
        }
    }

    ret = build_channel_array(gyro_attr->dev_dir_path, &gyro_attr->iio_chn, &gyro_attr->arry_elems);
    if (ret || !gyro_attr->arry_elems) {
        printf("Problem reading scan element information %s, ret %d\n", gyro_attr->dev_dir_path, ret);
        goto e_build;
    }

    ret = snprintf(node_tmp, sizeof(node_tmp), "%s/buffer/", gyro_attr->dev_dir_path);
    node_tmp[ret] = '\0';

    printf("DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDebug %s, kfifo %lu.\r\n", node_tmp, gyro_attr->kfifo_len);

    ret = write_sysfs_int("length", node_tmp, gyro_attr->kfifo_len);
    if (ret < 0) {
        printf("Write %s/%s %d failed, do it again.\r\n",
            init_seq[i].dir_base_name, init_seq[i].file_name, init_seq[i].init_val.val);
        ret = write_sysfs_int(init_seq[i].file_name, init_seq[i].dir_base_name, init_seq[i].init_val.val);
        if (ret < 0) {
            printf("Write %s/%s %d failed again, give it up.\r\n",
                init_seq[i].dir_base_name, init_seq[i].file_name, init_seq[i].init_val.val);
            goto e_writelen;
        }
    }
//    usleep(1000); /* wait some time */

    ret = snprintf(node_tmp, sizeof(node_tmp), "%s/buffer/", gyro_attr->dev_dir_path);
    node_tmp[ret] = '\0';
    ret = write_sysfs_int("enable", node_tmp, 1); // Enable gyro device
    if (ret < 0) {
        printf("Write %s 1 failed, do it again.\r\n", node_tmp);
        ret = write_sysfs_int("enable", node_tmp, 1); // Enable gyro device
        if (ret < 0) {
            printf("Write %s 1 failed again, give it up.\r\n", node_tmp);
            goto e_enable;
        }
    }
    usleep(20*1000); /* wait some time */

    gyro_attr->pkt_size = size_from_channelarray(gyro_attr->iio_chn, gyro_attr->arry_elems);
    // printf("scan_zise = %d.\r\n", scan_size);
    gyro_attr->usr_cache = malloc(gyro_attr->pkt_size * gyro_attr->kfifo_len);
    if (!gyro_attr->usr_cache) {
        printf("Alloc memorys for user cache gyro buffer failed. errno %d\r\n", errno);
        ret = -errno;
        goto e_alloc;
    }

    printf("DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDebug gyro pkt size is %d.\r\n", gyro_attr->pkt_size);

//    _DyroDebugDumpAllChannelInfos(gyro_attr);

    /* Attempt to open non blocking the access dev */
    gyro_attr->gyro_fd = open(gyro_attr->dev_name, O_RDONLY | O_NONBLOCK);
    if (gyro_attr->gyro_fd < 0) {
        printf("Open %s failed. errno %d.\r\n", gyro_attr->dev_name, errno);
        ret = -errno;
        goto e_opendev;
    }
#if 0
    /* wait mode, just for test */
    int flags = fcntl(fp_gyro, F_GETFL, 0);
    flags &= ~O_NONBLOCK;
    fcntl(fp_gyro, F_SETFL, flags);
#endif

    return 0;

e_opendev:
e_enable:
    free(gyro_attr->usr_cache);
e_alloc:
e_writelen:
    for (i = 0; i < gyro_attr->arry_elems; i++) {
        free(gyro_attr->iio_chn[i].name);
        free(gyro_attr->iio_chn[i].generic_name);
    }
    free(gyro_attr->iio_chn);
e_build:
e_write:
    return ret;
}

int read_mpu6500(struct gyro_device_attr *gyro_attr, int buf_num)
{
    int ret = 0;
    int read_total = buf_num * gyro_attr->pkt_size;

    ret = read(gyro_attr->gyro_fd, gyro_attr->usr_cache, read_total);
    if (ret < 0) {
        printf("Read %s failed, ret %d.\r\n", gyro_attr->dev_name, ret);
    } else {
        ret = ret / gyro_attr->pkt_size;
    }

    return ret;
}

int close_mpu6500(struct gyro_device_attr *gyro_attr)
{
    int i = 0;
    int ret = 0;
    char node_tmp[1024];
    struct gyro_init_sequence *init_seq = gyro_attr->init_seq;

    /* if gyro_fd == 0, means it is empty, not [stdin] fd. */
    if (!gyro_attr->gyro_fd || !gyro_attr->iio_chn) {
        printf("There has no device was opened.\r\n");
        ret = -1;
        goto e_nodev;
    }

    printf("=================================================== echo 0 > enable.\r\n");
    ret = snprintf(node_tmp, sizeof(node_tmp), "%s/buffer/", gyro_attr->dev_dir_path);
    node_tmp[ret] = '\0';
    ret = write_sysfs_int("enable", node_tmp, 0); // Disable gyro device
    if (ret < 0) {
        printf("Write %s 0 failed, do it again.\r\n", node_tmp);
        ret = write_sysfs_int("enable", node_tmp, 0); // Disable gyro device
        if (ret < 0) {
            printf("Write %s 0 failed again, give it up.\r\n", node_tmp);
            goto e_disable;
        }
    }

//    printf("=================================================== echo 0 > scans.\r\n");
    /* write deinitialize values */
    for (i = 0; init_seq[i].dir_base_name != NULL; i++) {
        if (init_seq[i].is_int) {
            ret = write_sysfs_int(init_seq[i].file_name, init_seq[i].dir_base_name, init_seq[i].init_val.val);
            if (ret < 0) {
                printf("Write %s/%s %d failed, do it again.\r\n",
                    init_seq[i].dir_base_name, init_seq[i].file_name, init_seq[i].init_val.val);
                ret = write_sysfs_int(init_seq[i].file_name, init_seq[i].dir_base_name, init_seq[i].init_val.val);
                if (ret < 0) {
                    printf("Write %s/%s %d failed again, give it up.\r\n",
                        init_seq[i].dir_base_name, init_seq[i].file_name, init_seq[i].init_val.val);
                    goto e_write;
                }
            }
//            printf("Write %d to %s/%s.\r\n", init_seq[i].init_val.val, init_seq[i].dir_base_name, init_seq[i].file_name);
        } else {
            write_sysfs_string(init_seq[i].file_name, init_seq[i].dir_base_name, init_seq[i].init_val.string);
            if (ret < 0) {
                printf("Write %s/%s %s failed, do it again.\r\n",
                    init_seq[i].dir_base_name, init_seq[i].file_name, init_seq[i].init_val.string);
                ret = write_sysfs_string(init_seq[i].file_name, init_seq[i].dir_base_name, init_seq[i].init_val.string);
                if (ret < 0) {
                    printf("Write %s/%s %s failed again, give it up.\r\n",
                        init_seq[i].dir_base_name, init_seq[i].file_name, init_seq[i].init_val.string);
                    goto e_write;
                }
            }
        }
    }

//    printf("=================================================== close devices.\r\n");
    /* No matter we close it success or not, just free every resource. */
    ret = close(gyro_attr->gyro_fd);
    if (ret < 0) {
        printf("Failed to close %s %d. errno %d.\r\n", gyro_attr->dev_name, gyro_attr->gyro_fd, errno);
        ret = -errno;
    }

    gyro_attr->gyro_fd = 0;
    free(gyro_attr->usr_cache);
    for (i = 0; i < gyro_attr->arry_elems; i++) {
        free(gyro_attr->iio_chn[i].name);
        free(gyro_attr->iio_chn[i].generic_name);
    }
    free(gyro_attr->iio_chn);

    return 0;

e_write:
e_disable:
e_nodev:
    return ret;
}

#define GYRO_SCAN_BASE "/sys/bus/iio/devices/iio:device0/scan_elements"
#define GYRO_DEV_BASE "/sys/bus/iio/devices/iio:device0"
#define GYRO_TRIGGER_BASE "/sys/bus/iio/devices/iio:device0/trigger"
#define GYRO_BUFFER_BASE "/sys/bus/iio/devices/iio:device0/buffer"

static struct gyro_init_sequence gyro_init_seq[] =
{
    {GYRO_DEV_BASE,  "sampling_frequency", {.val = 1000}, 1},
    {GYRO_SCAN_BASE, "in_accel_x_en",   {.val = 0}, 1},
    {GYRO_SCAN_BASE, "in_accel_y_en",   {.val = 0}, 1},
    {GYRO_SCAN_BASE, "in_accel_z_en",   {.val = 0}, 1},
    {GYRO_SCAN_BASE, "in_anglvel_x_en", {.val = 1}, 1},
    {GYRO_SCAN_BASE, "in_anglvel_y_en", {.val = 1}, 1},
    {GYRO_SCAN_BASE, "in_anglvel_z_en", {.val = 1}, 1},
    {GYRO_SCAN_BASE, "in_timestamp_en", {.val = 1}, 1},
    {NULL, NULL, {0}, 0},
};

#ifdef SUPPORT_MULTI_GYRO_INS
static int __device_open_mpu6500(struct gyro_device_attr * gyro_attr)
{
    return open_mpu6500(gyro_attr);
}

static int __device_close_mpu6500(struct gyro_device_attr * gyro_attr)
{
    return close_mpu6500(gyro_attr);
}

static int __gyro_read_parse_mpu6500(struct gyro_device_attr* gyro_attr, int pkt_num)
{
    int i = 0, k = 0, j = 0;
    int axi = 0;
    int ret = 0;
    char *read_tmp = gyro_attr->usr_cache;
    uint64_t time_avr_delta = 0;

    /* we got <ret> packets */
    ret = read_mpu6500(gyro_attr, pkt_num);
    if (ret <= 0) {
        printf("Read mpu6500 failed. return(%d).", ret);
        return 0;
    }

    struct gyro_pkt gyro_pkt;
    struct gyro_elem *gelem_cur = NULL;
    /* parse mpu6500 datas */
    for (i = 0; i < ret; i++) {
        float *float_tmp = &gyro_pkt.angle_x;
        if (gyro_attr->axi_num == 6)
            float_tmp = &gyro_pkt.accel_x;

        for (k = 0; k < gyro_attr->arry_elems; k++) {
            // printf("channels[%d].bytes=%d.\r\n", k, channels[k].bytes);
            switch (gyro_attr->iio_chn[k].bytes) {
                case 2: /*mpu6500 : x,y,z,time*/
                    print2byte(float_tmp++,
                        *(uint16_t *)(read_tmp + gyro_attr->iio_chn[k].location),
                        &gyro_attr->iio_chn[k]);
                    break;
                case 8: /*mpu6500 : x,y,z,time*/
                    print8byte(&gyro_pkt.time_stamp, *(uint64_t *)(read_tmp + gyro_attr->iio_chn[k].location),
                           &gyro_attr->iio_chn[k]);
                    gyro_pkt.time_stamp = gyro_pkt.time_stamp / 1000; // ns -> us
                    if (TS_INCREASE_PROC == g_gyro_chn_mgr.gyro_attr.proc_mth) {
                        /* Simple correct the zero timestamp value */
                        if (0 == gyro_pkt.time_stamp && g_gyro_chn_mgr.last_pkt_timestamp != 0)
                            gyro_pkt.time_stamp = (g_gyro_chn_mgr.last_pkt_timestamp + 1000000 / gyro_attr->sample_freq);
                        g_gyro_chn_mgr.last_pkt_timestamp = gyro_pkt.time_stamp;
                    }
                    break;
                default:
                    break;
            }
        }

        /* Read the next packet */
        read_tmp += gyro_attr->pkt_size;

        switch (g_gyro_chn_mgr.gyro_attr.proc_mth) {
            case TS_INCREASE_PROC: {
                list_for_each_entry(gelem_cur, &g_gyro_chn_mgr.mlist, mlist)
                {
                    if (!gelem_cur->bopen)
                        continue;

                    if (ring_buffer_full(gelem_cur->rb_hd)) {
                        static unsigned int full_cnt = 0;
                        if (0 == (full_cnt % g_gyro_chn_mgr.gyro_attr.sample_freq))
                            printf("Ring buffer[%d] full, drop the oldest one gyro data.\r\n", gelem_cur->idx);
                        full_cnt++;
                        ring_buffer_out(gelem_cur->rb_hd, NULL, 1);
                    }
                    ring_buffer_in(gelem_cur->rb_hd, &gyro_pkt, 1);
                }
            } break;

            case TS_AVERAGE_PROC:
            default: {
                g_gyro_chn_mgr.gpkt_cache[g_gyro_chn_mgr.gpkt_cnt] = gyro_pkt;

                if (g_gyro_chn_mgr.gpkt_cache[g_gyro_chn_mgr.gpkt_cnt].time_stamp == 0) {
                    g_gyro_chn_mgr.gpkt_cnt++;
                    continue;
                } else {
                    g_gyro_chn_mgr.cur_pkt_timestamp = g_gyro_chn_mgr.last_pkt_timestamp;
                    time_avr_delta = (g_gyro_chn_mgr.gpkt_cache[g_gyro_chn_mgr.gpkt_cnt].time_stamp
                        - g_gyro_chn_mgr.cur_pkt_timestamp)/(g_gyro_chn_mgr.gpkt_cnt+1);
                    g_gyro_chn_mgr.last_pkt_timestamp = g_gyro_chn_mgr.gpkt_cache[g_gyro_chn_mgr.gpkt_cnt].time_stamp;
                    g_gyro_chn_mgr.gpkt_cnt++;
                }

                for (j = 0; j < g_gyro_chn_mgr.gpkt_cnt; j++) {
                    if (g_gyro_chn_mgr.gpkt_cache[j].time_stamp == 0) {
                        g_gyro_chn_mgr.gpkt_cache[j].time_stamp = g_gyro_chn_mgr.cur_pkt_timestamp + time_avr_delta;
                        g_gyro_chn_mgr.cur_pkt_timestamp = g_gyro_chn_mgr.gpkt_cache[j].time_stamp;
                    }

                    list_for_each_entry(gelem_cur, &g_gyro_chn_mgr.mlist, mlist)
                    {
                        if (!gelem_cur->bopen)
                            continue;

                        if (ring_buffer_full(gelem_cur->rb_hd)) {
                            static unsigned int full_cnt = 0;
                            if (0 == (full_cnt % g_gyro_chn_mgr.gyro_attr.sample_freq))
                                printf("Ring buffer[%d] full, drop the oldest one gyro data.\r\n", gelem_cur->idx);
                            full_cnt++;
                            ring_buffer_out(gelem_cur->rb_hd, NULL, 1);
                        }
                        ring_buffer_in(gelem_cur->rb_hd, &g_gyro_chn_mgr.gpkt_cache[j], 1);
                    }
                }
                g_gyro_chn_mgr.gpkt_cnt = 0;
            } break;
        }
    }

    return i;
}

static void *__gyro_product_buffers(void *args)
{
    int ret = 0;
    fd_set stRdFdSet;
    struct timeval stTimeLimit;

#if 1
    struct sched_param stSchedPara;
    stSchedPara.sched_priority = 1;
    pthread_setschedparam(pthread_self(), SCHED_RR, &stSchedPara);
#endif
    prctl(PR_SET_NAME, "__gyro_product_buffers", 0, 0, 0);

    printf("Run gyro data product thread.\r\n");

    /* If we has no one user, then exit this thread. */
    printf("Open gyro fd[%d].\r\n", g_gyro_chn_mgr.gyro_attr.gyro_fd);
    while (g_gyro_chn_mgr.open_cnt) {
        if (g_gyro_chn_mgr.bhalt) {
            usleep(5*1000);
            g_gyro_chn_mgr.bhas_stop = 1;
            continue;
        }
        g_gyro_chn_mgr.bhas_stop = 0;

        /* @select function will change @stTimeLimit's value, so reset it. */
        stTimeLimit.tv_sec = 1;
        stTimeLimit.tv_usec = 0;

        /* Because the @gyro_fd may be changed, so reset @stRdFdSet in every loop time. */
        FD_ZERO(&stRdFdSet);
        FD_SET(g_gyro_chn_mgr.gyro_attr.gyro_fd, &stRdFdSet);

        ret = select(g_gyro_chn_mgr.gyro_attr.gyro_fd + 1, &stRdFdSet, NULL, NULL, &stTimeLimit);
        if (ret < 0) {
            printf("Select wait for gyro datas failed, return %d.\r\n", -errno);
            usleep(10000);
            continue;
        } else if (0 == ret) {
            printf("Select wait for gyro datas timeout.\r\n");
            continue;
        }

        pthread_mutex_lock(&g_gyro_chn_mgr.lock);
        __gyro_read_parse_mpu6500(&g_gyro_chn_mgr.gyro_attr, g_gyro_chn_mgr.gyro_attr.kfifo_len);
        pthread_mutex_unlock(&g_gyro_chn_mgr.lock);
    }

    printf("Gyro device data product thread exit.\r\n");
    return NULL;
}

/* NNNNNNNNNNNNNNNNNNNNNot multi-thread safe for every single gyro_ins,
* but for multi-gyro_ins, it is thread safety. */
static int __internal_open_mpu6500(struct gyro_instance *gins, struct gyro_device_attr *gyro_attr)
{
    int ret = 0;
    struct gyro_elem *gelem = (struct gyro_elem *)gins->priv;

    /* If we open it many times, just return,
    * you can not open it more than once. */
    if (gelem->bopen)
        return 0;

    pthread_mutex_lock(&g_gyro_chn_mgr.lock);
    /* First of all, create the ring buffer instance to store gyro datas. */
    gelem->rb_hd = ring_buffer_create(sizeof(struct gyro_pkt), gyro_attr->rb_len, RB_FL_NONE);
    if (NULL == gelem->rb_hd) {
        printf("Create ring buffer for gyro data cache failed.\r\n");
        ret = -1;
        goto ECrtRB;
    }

    /* If we have open the real gyro device, and needn't re-open it,
    * then just increase one open_cnt count. */
    if (g_gyro_chn_mgr.open_cnt && !gyro_attr->force_open) {
        g_gyro_chn_mgr.open_cnt++;
        goto exit;
    }

    /* If we has open the real gyro device more than one time, but
    * we want open it with new attribute, then close and open it again. */
    g_gyro_chn_mgr.gyro_attr.init_seq = &gyro_init_seq[0];
    if (g_gyro_chn_mgr.open_cnt && gyro_attr->force_open) {
        /* Close gyro device and open it with new attributes again.
        * 1. halt the running thread and close the running gyro device.
        * 2. use new attributes to re-open the gyro device.
        */
        g_gyro_chn_mgr.bhalt = 1;

        gyro_init_seq[IN_ACCEL_X_EN].init_val.val = 0;
        gyro_init_seq[IN_ACCEL_Y_EN].init_val.val = 0;
        gyro_init_seq[IN_ACCEL_Z_EN].init_val.val = 0;
        gyro_init_seq[IN_ANGLVEL_X_EN].init_val.val = 0;
        gyro_init_seq[IN_ANGLVEL_Y_EN].init_val.val = 0;
        gyro_init_seq[IN_ANGLVEL_Z_EN].init_val.val = 0;
        gyro_init_seq[IN_TIMESTAMP_EN].init_val.val = 0;

        /* Wait the thread get into stop status */
        pthread_mutex_unlock(&g_gyro_chn_mgr.lock);
        while (!g_gyro_chn_mgr.bhas_stop) { usleep(1*1000); }
        pthread_mutex_lock(&g_gyro_chn_mgr.lock);
        ret = __device_close_mpu6500(&g_gyro_chn_mgr.gyro_attr);
        if (0 != ret) {
            printf("Close mpu6500 failed, just use the last configuration. errno(%d).\r\n", -errno);
            g_gyro_chn_mgr.bhalt = 0;
            goto ECloseDev;
        }
        g_gyro_chn_mgr.last_pkt_timestamp = 0;
        g_gyro_chn_mgr.cur_pkt_timestamp = 0;
    }

    /* Copy the new gyro attribute. */
    g_gyro_chn_mgr.gyro_attr.dev_name = gyro_attr->dev_name;
    g_gyro_chn_mgr.gyro_attr.dev_dir_path = gyro_attr->dev_dir_path;
    g_gyro_chn_mgr.gyro_attr.rb_len = gyro_attr->rb_len;
    g_gyro_chn_mgr.gyro_attr.kfifo_len = gyro_attr->kfifo_len;
    g_gyro_chn_mgr.gyro_attr.axi_num = gyro_attr->axi_num;
    g_gyro_chn_mgr.gyro_attr.sample_freq = gyro_attr->sample_freq;
    g_gyro_chn_mgr.gyro_attr.force_open = gyro_attr->force_open;
    g_gyro_chn_mgr.gyro_attr.proc_mth = gyro_attr->proc_mth;

    /* Then open it. */
    gyro_init_seq[SAMPLE_FREQUENCY].init_val.val = g_gyro_chn_mgr.gyro_attr.sample_freq;
    gyro_init_seq[IN_ANGLVEL_X_EN].init_val.val = 1;
    gyro_init_seq[IN_ANGLVEL_Y_EN].init_val.val = 1;
    gyro_init_seq[IN_ANGLVEL_Z_EN].init_val.val = 1;
    gyro_init_seq[IN_TIMESTAMP_EN].init_val.val = 1;

    if (g_gyro_chn_mgr.gyro_attr.axi_num == 6) {
        gyro_init_seq[IN_ACCEL_X_EN].init_val.val = 1;
        gyro_init_seq[IN_ACCEL_Y_EN].init_val.val = 1;
        gyro_init_seq[IN_ACCEL_Z_EN].init_val.val = 1;
    }

    ret = __device_open_mpu6500(&g_gyro_chn_mgr.gyro_attr);
    if (0 != ret) {
        printf("Create ring buffer for gyro data cache failed.\r\n");
        ret = -1;
        goto EOpenDev;
    }
    g_gyro_chn_mgr.open_cnt++;

    if (1 >= g_gyro_chn_mgr.open_cnt) {
        if (TS_AVERAGE_PROC == g_gyro_chn_mgr.gyro_attr.proc_mth) {
            g_gyro_chn_mgr.gpkt_cache = malloc(sizeof(struct gyro_pkt)*g_gyro_chn_mgr.gyro_attr.kfifo_len);
            if (NULL == g_gyro_chn_mgr.gpkt_cache)
                goto EAllocPkt;
            g_gyro_chn_mgr.gpkt_cnt = 0;
        }
        ret = pthread_create(&g_gyro_chn_mgr.gyro_pid, NULL, __gyro_product_buffers, &g_gyro_chn_mgr);
        if (ret < 0) {
            printf("Create gyro data product thread failed. return (%d).\r\n", ret);
            goto ECrtTrd;
        }
    }

ECloseDev:
exit:
    g_gyro_chn_mgr.bhalt = 0;
    gelem->bopen = gins->bopen = 1;
    gins->gyro_attr = &g_gyro_chn_mgr.gyro_attr;
    pthread_mutex_unlock(&g_gyro_chn_mgr.lock);
    return ret;

ECrtTrd:
    if (TS_AVERAGE_PROC == g_gyro_chn_mgr.gyro_attr.proc_mth) {
        free(g_gyro_chn_mgr.gpkt_cache);
        g_gyro_chn_mgr.gpkt_cache = NULL;
        g_gyro_chn_mgr.gpkt_cnt = 0;
    }
EAllocPkt:
    g_gyro_chn_mgr.open_cnt--;
    gyro_attr->init_seq = &gyro_init_seq[0];
    gyro_init_seq[IN_ACCEL_X_EN].init_val.val = 0;
    gyro_init_seq[IN_ACCEL_Y_EN].init_val.val = 0;
    gyro_init_seq[IN_ACCEL_Z_EN].init_val.val = 0;
    gyro_init_seq[IN_ANGLVEL_X_EN].init_val.val = 0;
    gyro_init_seq[IN_ANGLVEL_Y_EN].init_val.val = 0;
    gyro_init_seq[IN_ANGLVEL_Z_EN].init_val.val = 0;
    gyro_init_seq[IN_TIMESTAMP_EN].init_val.val = 0;

    /* Doesn't metter it will return success or not. */
    __device_close_mpu6500(gyro_attr);
EOpenDev:
    ring_buffer_destroy(gelem->rb_hd);
ECrtRB:
    pthread_mutex_unlock(&g_gyro_chn_mgr.lock);
    return ret;
}

/* NNNNNNNNNNNNNNNNNNNNNot multi-thread safe for every single gyro_ins,
* but for multi-gyro_ins, it is thread safety. */
static int __internal_read_mpu6500(struct gyro_instance *gins, struct gyro_pkt *pkt, int num)
{
    struct gyro_elem *gelem = (struct gyro_elem *)gins->priv;

    if (!gelem->bopen)
        return -1;

//    if (ring_buffer_empty(gelem->rb_hd))
//        return 0;
    return ring_buffer_out(gelem->rb_hd, pkt, num);
}

/* NNNNNNNNNNNNNNNNNNNNNot multi-thread safe for every single gyro_ins,
* but for multi-gyro_ins, it is thread safety. */
static void __internal_close_mpu6500(struct gyro_instance *gins)
{
    struct gyro_elem *gelem = (struct gyro_elem *)gins->priv;
    struct gyro_device_attr *gyro_attr = &g_gyro_chn_mgr.gyro_attr;

    /* If it has been closed or just not do the open opr, then return.  */
    if (!gelem->bopen)
        return;

    pthread_mutex_lock(&g_gyro_chn_mgr.lock);
    /* We have lock protect, just destory it boldly. */
    ring_buffer_destroy(gelem->rb_hd);

    g_gyro_chn_mgr.open_cnt--;
    /* In general, this [g_gyro_chn_mgr.open_cnt < 0] will never happen. */
    if (g_gyro_chn_mgr.open_cnt < 0)
        g_gyro_chn_mgr.open_cnt = 0;

    gelem->bopen = gins->bopen = 0;

    /* There has no one user, just close the real gyro device. */
    if (!g_gyro_chn_mgr.open_cnt) {
        gyro_attr->init_seq = &gyro_init_seq[0];

        gyro_init_seq[IN_ACCEL_X_EN].init_val.val = 0;
        gyro_init_seq[IN_ACCEL_Y_EN].init_val.val = 0;
        gyro_init_seq[IN_ACCEL_Z_EN].init_val.val = 0;
        gyro_init_seq[IN_ANGLVEL_X_EN].init_val.val = 0;
        gyro_init_seq[IN_ANGLVEL_Y_EN].init_val.val = 0;
        gyro_init_seq[IN_ANGLVEL_Z_EN].init_val.val = 0;
        gyro_init_seq[IN_TIMESTAMP_EN].init_val.val = 0;

        /* Unlock it and wait the product thread exit. */
        pthread_mutex_unlock(&g_gyro_chn_mgr.lock);
        pthread_join(g_gyro_chn_mgr.gyro_pid, NULL);
        pthread_mutex_lock(&g_gyro_chn_mgr.lock);
        if (TS_AVERAGE_PROC == gyro_attr->proc_mth) {
            free(g_gyro_chn_mgr.gpkt_cache);
            g_gyro_chn_mgr.gpkt_cache = NULL;
            g_gyro_chn_mgr.gpkt_cnt = 0;
        }
        __device_close_mpu6500(gyro_attr);
        g_gyro_chn_mgr.last_pkt_timestamp = 0;
        g_gyro_chn_mgr.cur_pkt_timestamp = 0;
    }

exit:
    pthread_mutex_unlock(&g_gyro_chn_mgr.lock);
}

static struct ring_buffer* __internal_get_rb_hdl(struct gyro_instance *gins)
{
    struct gyro_elem *gelem = (struct gyro_elem *)gins->priv;

    if (!gelem->bopen)
        return NULL;
    return gelem->rb_hd;
}

/* Create one new gyro instance, add it into gManager list and increase one reference.
* Also initilize the new gyro instance.
*/
struct gyro_instance *create_gyro_inst(void)
{
    struct gyro_elem *gelem = malloc(sizeof(struct gyro_elem));
    if (NULL == gelem) {
        printf("No memory for struct gyro_elem. errno(%d)\r\n", -errno);
        return NULL;
    }

    memset(gelem, 0, sizeof(struct gyro_elem));

    /* This lock has been initialized with static global value. */
    pthread_mutex_lock(&g_gyro_chn_mgr.lock);
    if (0 == g_gyro_chn_mgr.elem_cnt)
        INIT_LIST_HEAD(&g_gyro_chn_mgr.mlist);
    /* Add to tail and increase one gyro elem count. */
    list_add_tail(&gelem->mlist, &g_gyro_chn_mgr.mlist);
    gelem->idx = g_gyro_chn_mgr.elem_cnt;
    g_gyro_chn_mgr.elem_cnt++;
    pthread_mutex_unlock(&g_gyro_chn_mgr.lock);

    /* Initialize the new gryo elem members.
    * Because this elem is independent, so no need lock it.
    */
    gelem->gyro_ins.idx = gelem->idx;
    gelem->gyro_ins.priv = gelem;

    gelem->gyro_ins.open  = __internal_open_mpu6500;
    gelem->gyro_ins.read  = __internal_read_mpu6500;
    gelem->gyro_ins.close = __internal_close_mpu6500;
    /* Sometimes, you may want to manage the ring buffer with ring_buffer module,
    * then use this interface. */
    gelem->gyro_ins.get_rb_hdl = __internal_get_rb_hdl;
    return &gelem->gyro_ins;
}

void destory_gyro_inst(struct gyro_instance *gyro_ins)
{
    struct gyro_elem *gelem_tmp = (struct gyro_elem *)gyro_ins->priv;
    struct gyro_elem *gelem_cur, *gelem_next;

    /* If the list is empty, then you must use a invalid gyro_ins */
    pthread_mutex_lock(&g_gyro_chn_mgr.lock);
    if (list_empty(&g_gyro_chn_mgr.mlist)) {
        printf("The gyro_ins list is empty, check your parameter.\r\n");
        goto exit;
    }

    /* If we find the right gyro_ins, then delete it and
    * decrease mgr count if this gyro_ins is closed. */
    list_for_each_entry_safe(gelem_cur, gelem_next, &g_gyro_chn_mgr.mlist, mlist)
    {
        if (gelem_tmp->idx == gelem_cur->idx) {
            if (gelem_cur->bopen) {
                printf("The gryo instrance you want to destroy is opened, please close it first.\r\n");
                goto exit;
            }
            list_del(&gelem_cur->mlist);
            free(gelem_cur);
            g_gyro_chn_mgr.elem_cnt--;
        }
    }

exit:
    pthread_mutex_unlock(&g_gyro_chn_mgr.lock);
}

#if 0
#define GYRO_DEV_NAME "/dev/iio:device0"
#define GYRO_DEV_DIR_PATH "/sys/bus/iio/devices/iio:device0"

int main(int argc, char *argv[])
{
    struct gyro_instance *gyro_ins = NULL;
    struct gyro_instance *gyro_ins1 = NULL;
    struct gyro_instance *gyro_ins2 = NULL;

    gyro_ins = create_gyro_inst();
    gyro_ins1 = create_gyro_inst();
    gyro_ins2 = create_gyro_inst();

    struct gyro_device_attr gyro_attr;
    memset(&gyro_attr, 0, sizeof(struct gyro_device_attr));
    gyro_attr.dev_name = GYRO_DEV_NAME;
    gyro_attr.dev_dir_path = GYRO_DEV_DIR_PATH;
    gyro_attr.rb_len = 1000;
    gyro_attr.kfifo_len = 200;
    gyro_attr.axi_num = 3;
    gyro_attr.sample_freq = 1000;

    gyro_ins->open(gyro_ins, &gyro_attr);
    gyro_ins1->open(gyro_ins1, &gyro_attr);
    gyro_ins2->open(gyro_ins2, &gyro_attr);

    int read_cnt = 0;
    static uint64_t time_cache = 0;
    while (1) {
        struct gyro_pkt gyro_pkt;
        if (gyro_ins->read(gyro_ins, &gyro_pkt, 1) > 0) {
            if (0 != time_cache && gyro_pkt.time_stamp - time_cache > 3000)
                printf("[0]%llu[%llu][%llu] %f\r\n", gyro_pkt.time_stamp, time_cache,
                    gyro_pkt.time_stamp - time_cache, gyro_pkt.angle_x);

            time_cache = gyro_pkt.time_stamp;
        }
        if (gyro_ins1->read(gyro_ins1, &gyro_pkt, 1) > 0)
            printf("[1]%llu %f\r\n", gyro_pkt.time_stamp, gyro_pkt.angle_x);
        if (gyro_ins2->read(gyro_ins2, &gyro_pkt, 1) > 0)
            printf("[2]%llu %f\r\n", gyro_pkt.time_stamp, gyro_pkt.angle_x);

        read_cnt++;
        usleep(500);
//        printf("---%d\r\n", read_cnt);

        if (read_cnt >= atoi(argv[1]))
            break;
    }

    gyro_ins2->close(gyro_ins2);
    gyro_ins1->close(gyro_ins1);
    gyro_ins->close(gyro_ins);

    destory_gyro_inst(gyro_ins2);
    destory_gyro_inst(gyro_ins1);
    destory_gyro_inst(gyro_ins);
    return 0;
}
#endif
#endif
