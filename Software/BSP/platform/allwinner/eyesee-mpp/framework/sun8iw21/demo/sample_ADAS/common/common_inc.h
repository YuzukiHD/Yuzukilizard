/*************************************************
Copyright (C), 2015, AllwinnerTech. Co., Ltd.
File name: common_ref.h
Author: yinzh@allwinnertech.com
Version: 0.1
Date: 2015-11-9
Description:
History:
*************************************************/
#pragma once

#include "common/app_def.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <sys/vfs.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
// #include <asm/sizes.h>

#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

#define ARRYSIZE(table)    (sizeof(table)/sizeof(table[0]))

