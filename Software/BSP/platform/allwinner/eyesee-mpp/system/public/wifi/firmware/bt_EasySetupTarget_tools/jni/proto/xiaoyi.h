#ifndef __CHANGHONG_H__
#define __CHANGHONG_H__

#include <easy_setup.h>

#define XIAOYI_MAX_LEN (128)

/* no param for xiaoyi */

/* xiaoyi result */
typedef struct {
    easy_setup_result_t es_result;
    uint8 len;
    uint8 buf[XIAOYI_MAX_LEN];
} xiaoyi_result_t;

void xiaoyi_set_result(const void* p);

int xiaoyi_get_buffer(uint8* len, uint8* buf);

#endif /* __CHANGHONG_H__ */
