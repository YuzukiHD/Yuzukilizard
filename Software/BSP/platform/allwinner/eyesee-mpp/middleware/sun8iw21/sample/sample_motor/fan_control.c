/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <getopt.h>
#include <paths.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "fan_control.h"
#include "sys/sysmacros.h"

#define PWM_IOCTL_BASE      'P'
#define PWM_CONFIG          _IOW(PWM_IOCTL_BASE, 1 , struct pwm_config)
#define PWM_ENABLE          _IOW(PWM_IOCTL_BASE, 2 , int)
#define PWM_DISABLE         _IOW(PWM_IOCTL_BASE, 3 , int)
#define GROUP_PWM_CONFIG	_IOW(PWM_IOCTL_BASE, 4, struct pwm_config_group)
#define GROUP_PWM_DISABLE   _IOW(PWM_IOCTL_BASE, 5, struct pwm_config_group)

struct pwm_config {
    int pwm_id;        //id，0对应pwm0，类推
    int duty_ns;       //占空比时间，单位ns
    int period_ns;     //周期时间，单位ns
    int pwm_polarity;  // 0表示 PWM_POLARITY_NORMAL, 1表示PWM_POLARITY_INVERSED
};

struct pwm_config_group {	
	int group_channel;      //组使用 0表示不使用，1表示使用第零组，2表示使用第一组
	int group_run_count;    //组脉冲数
	int pwm_polarity;  // 0表示 PWM_POLARITY_NORMAL, 1表示PWM_POLARITY_INVERSED
};

static int pwmFd = 0;
const int pwmID = 0;
const int periodNs=1000*1000*10;   //5ms

int AW_MPI_InitMotor()
{
    FILE* fp = fopen("/proc/devices","r");
    char *line = NULL;
    size_t len = 0;
    size_t read = 0;
    int major = 0;

    if (fp == NULL) return;
    while ( (read = getline(&line,&len, fp)) != -1) {  //line是字符指针，read是读取的字节数
        char *x = strstr(line, "sunxi-pwm-dev");    //返回"sunxi-pwm-dev"在line的位置指针
        if (x!=NULL) {
            char *numStr = calloc(sizeof(char), 64);
            strncpy(numStr,line,x-line-1);   //拷贝主设备号
            major = atoi(numStr);
            free(numStr);
            break;
        }
    }
    fclose(fp);
    if (line)
        free(line);
    if (major <= 0) {
        aloge("error find pwm node\n");
        return;
    }
    dev_t devT = makedev(major,0);
    if (mknod("/dev/fan", S_IFCHR | 0660, makedev(major, 0))) {
        if (errno != EEXIST) {
            aloge("error mknod %d\n",errno);
        }
    }
    pwmFd = open("/dev/fan",O_RDWR | O_CLOEXEC);
    if (pwmFd <= 0) {
       aloge("open /dev/fan fail! %s\n",strerror(errno));
       return -1;
    }
}

/*
 * 参数
 * Group:通道号[通道1、2]
 * angle:角度
 * polarity:方向[顺时针或逆时针]
 *
 */
int AW_MPI_EnableMotor(int Group, int angle, int polarity)
{
    int ret = 0;
    struct pwm_config_group config = {1, 1, 0};
    int run_count = (angle * 512)/360;  //24BYJ48-550电机转一圈需要512步
    aloge("set motor angle %d run_count %d",angle,run_count);
    config.group_channel = Group;
    config.group_run_count = run_count;
    config.pwm_polarity = polarity;

    ret = ioctl(pwmFd, GROUP_PWM_CONFIG, &config);
    if (ret < 0) {
        aloge("ioctl motor fail! %s\n",strerror(errno));
        return ret;
    }

    return ret;
}

/*
 * 参数
 * Group:通道号[通道1、2]
 *
 */
int AW_MPI_DisableMotor(int Group)
{
    int ret = 0;
    struct pwm_config_group config = { 1, 1, 0};

    config.group_channel = Group;
    aloge("disable motor!!");
    ret = ioctl(pwmFd, GROUP_PWM_DISABLE, &config);
    if (ret < 0) {
       aloge("ioctl motor fail! %s\n",strerror(errno));
       return ret;
    }

    return ret;
}
