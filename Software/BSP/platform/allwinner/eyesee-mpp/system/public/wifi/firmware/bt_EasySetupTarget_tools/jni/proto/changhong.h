#ifndef __CHANGHONG_H__
#define __CHANGHONG_H__

#include <easy_setup.h>

/* no param for changhong */

/* changhong result */
typedef struct {
    easy_setup_result_t es_result;
    uint8 sec_mode; /* sec mode */
} changhong_result_t;

void changhong_set_result(const void* p);

int changhong_get_sec_mode(uint8* sec);

#endif /* __CHANGHONG_H__ */
