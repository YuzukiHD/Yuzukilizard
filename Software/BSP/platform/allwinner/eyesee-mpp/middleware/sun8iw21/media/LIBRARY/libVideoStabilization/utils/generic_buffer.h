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

#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <linux/types.h>
#include <string.h>
#include <poll.h>
#include <stdbool.h>
#include <endian.h>
#include <getopt.h>
#include <inttypes.h>

#define SUPPORT_MULTI_GYRO_INS

/* DDDDDDDDDDDDDDo not change the enum sequence!!!!!!
*  DDDDDDDDDDDDDDo not change the enum sequence!!!!!!
*/
enum gyro_init_para {
    SAMPLE_FREQUENCY = 0,
    IN_ACCEL_X_EN,
    IN_ACCEL_Y_EN,
    IN_ACCEL_Z_EN,
    IN_ANGLVEL_X_EN,
    IN_ANGLVEL_Y_EN,
    IN_ANGLVEL_Z_EN,
    IN_TIMESTAMP_EN,
};

struct gyro_init_sequence {
    const char *dir_base_name;
    const char *file_name;
    union {
        int val;
        const char *string;
    } init_val;
    bool is_int;
};

enum timestamp_proc_mth {
    TS_AVERAGE_PROC = 0,
    TS_INCREASE_PROC,
};

struct gyro_device_attr {
    /*
    * Input gyro device attributes
    */
    const char *dev_name;
    const char *dev_dir_path;
    unsigned long rb_len;
    unsigned long kfifo_len;
    /* 3:means angle x,y,z 6:means accel x,y,z angle x,y,z */
    int axi_num;
    int sample_freq;

    /* You can force the opened device re-open use the new attributes, or
    * just open it if it was closed.
    * UUUUUUUUUUUUUUUUUUUUUUUUUse it carefully.
    */
    bool force_open;
    enum timestamp_proc_mth proc_mth;

    /*
    * Output gyro device attributes
    */
    struct gyro_init_sequence *init_seq;
    int gyro_fd;
    struct iio_channel_info *iio_chn;
    /* how many elems we can got from one packet(x,y,z,timestamp) */
    int arry_elems;
    /* How length bytes wo got from one packet */
    int pkt_size;
    /* User will read kernel fifo and cache it */
    char *usr_cache;

    /*
    * Private region, use what you want to use
    */
    void *private;
};

struct gyro_pkt {
    uint64_t time_stamp;
    float accel_x;
    float accel_y;
    float accel_z;
    float angle_x;
    float angle_y;
    float angle_z;
};

/* AAAAAAAAAAAAAAAAAttention, @open,@read,@close is not multi-thread safe. Be careful to use it.
* that means that the same gyro_ins can be used only for two threads(one read, one write.)
*@open: Open gyro device instance. NNNNNNNot mul-thread safe.
*/
struct gyro_instance {
    int (*open)(struct gyro_instance *gins, struct gyro_device_attr *gyro_attr);
    int (*read)(struct gyro_instance *gins, struct gyro_pkt *pkt, int num);
    void (*close)(struct gyro_instance *gins);

    /* Must be invoked after [open] was invoked. */
    struct ring_buffer* (*get_rb_hdl)(struct gyro_instance *gins);

    /* Output attribute. */
    int idx;
    bool bopen;
    struct gyro_device_attr *gyro_attr;

    /* Used by gyro util, user do not touch it. */
    void *priv;
};

void print2byte(float *output, uint16_t input, struct iio_channel_info *info);
void print4byte(float *output, uint32_t input, struct iio_channel_info *info);
void print8byte(uint64_t *pts, uint64_t input, struct iio_channel_info *info);

int open_mpu6500(struct gyro_device_attr *gyro_attr);
/* Return number we got from gyro device, unit: gyro_attr->pkt_size
* return negative value means read is failed.
*/
int read_mpu6500(struct gyro_device_attr *gyro_attr, int buf_num);
int close_mpu6500(struct gyro_device_attr *gyro_attr);

/* Multi-gyro device instance support. */
#define gyro_ins_open(pins, pattr) pins->open(pins, pattr)
#define gyro_ins_read(pins, pbuf, num) pins->read(pins, pbuf, num)
#define gyro_ins_close(pins) pins->close(pins)
/* Return [struct ring_buffer*] */
#define gyro_ins_get_rbhdl(pins) pins->get_rb_hdl(pins)

struct gyro_instance *create_gyro_inst(void);
void destory_gyro_inst(struct gyro_instance *gyro_ins);

