#ifndef __AISERVICE_HW_SCALE_H__
#define __AISERVICE_HW_SCALE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

int g2d_mem_open(void);
int g2d_mem_close(void);
void scale_create_buffer(void);
void scale_destory_buffer(void);
int scale_picture_nv12_to_nv12_byg2d(const unsigned char *srcpic, int src_width, int src_height, unsigned char *dstpic, int dst_width, int dst_height, int g2d_fd);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
