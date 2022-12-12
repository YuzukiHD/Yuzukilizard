/****************************************************************************
 *
 *    Copyright 2012 - 2016 Vivante Corporation, Santa Clara, California.
 *    All Rights Reserved.
 *
 *    Permission is hereby granted, free of charge, to any person obtaining
 *    a copy of this software and associated documentation files (the
 *    'Software'), to deal in the Software without restriction, including
 *    without limitation the rights to use, copy, modify, merge, publish,
 *    distribute, sub license, and/or sell copies of the Software, and to
 *    permit persons to whom the Software is furnished to do so, subject
 *    to the following conditions:
 *
 *    The above copyright notice and this permission notice (including the
 *    next paragraph) shall be included in all copies or substantial
 *    portions of the Software.
 *
 *    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 *    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 *    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
 *    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************/

#ifndef _VX_UTILITY_H
#define _VX_UTILITY_H

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#include <malloc.h>
#ifdef LINUX
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#endif

#if defined(__linux__) || defined(__ANDROID__) || defined(__QNX__) || defined(__APPLE__) || defined(__CYGWIN__)
#include <dlfcn.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#elif defined(_WIN32) || defined(UNDER_CE)
#include <windows.h>
#endif

typedef struct _file_header
{
    unsigned short fileType;
    unsigned short fileSize1;
    unsigned short fileSize2;
    unsigned short fileReserved1;
    unsigned short fileReserved2;
    unsigned short fileOffset1;
    unsigned short fileOffset2;
} FileHeader;

typedef struct _info_header
{
    unsigned int infoSize;
    unsigned int imageWidth;
    int imageHeight;
    unsigned short imagePlane;
    unsigned short imageCount;
    unsigned int imageCompression;
    unsigned int imageSize;
    unsigned int hResolution;
    unsigned int vResolution;
    unsigned int clrUsed;
    unsigned int clrImportant;
} InfoHeader;

#define FALSE 0

#define OBJCHECK(objVX)                                                 \
    if (!(objVX))                                                       \
    {                                                                   \
        printf("%s:%d, %s\n", __FILE__, __LINE__, "obj create error."); \
        status = -1;                                                    \
        goto Exit;                                                      \
    }
#define MEMCHECK(memPtr)                                                   \
    if (!(memPtr))                                                         \
    {                                                                      \
        printf("%s:%d, %s\n", __FILE__, __LINE__, "memory create error."); \
        status = -1;                                                       \
        goto Exit;                                                         \
    }
#define FILECHECK(filePtr)                                             \
    if (!(filePtr))                                                    \
    {                                                                  \
        printf("%s:%d, %s\n", __FILE__, __LINE__, "file open error."); \
        status = -1;                                                   \
        goto Exit;                                                     \
    }

#define gcmIS_ERROR(status) (status < VX_SUCCESS)
#define gvxFUNC_CHECK(func)                                                        \
    do                                                                             \
    {                                                                              \
        status = func;                                                             \
        if (gcmIS_ERROR(status))                                                   \
        {                                                                          \
            printf("ERROR: status=%d @ %s(%d)\n", status, __FUNCTION__, __LINE__); \
            goto Exit;                                                             \
        }                                                                          \
    } while (FALSE)

int writeBMP(FILE *desFile, unsigned char *inputBuf, unsigned int width, unsigned int height, int cn);

#endif // VX_UTILITY_H
