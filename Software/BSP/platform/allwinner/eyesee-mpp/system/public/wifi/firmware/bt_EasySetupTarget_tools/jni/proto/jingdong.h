#ifndef __JINGDONG_H__
#define __JINGDONG_H__

#include <easy_setup.h>

#define JINGDONG_KEY_STRING_LEN (16)
#define JINGDONG_NONCE_PAD_LEN (16)

/* param for jingdong */
typedef struct {
    uint8 key_bytes[JINGDONG_KEY_STRING_LEN];  /* key string for decoding */
    uint8 random_bytes[JINGDONG_NONCE_PAD_LEN]; /* random bytes */
} jingdong_param_t;

/* jingdong result */
typedef struct {
    easy_setup_result_t es_result;
    uint8 sec_mode; /* sec mode */
} jingdong_result_t;

void jingdong_get_param(void* p);
void jingdong_set_result(const void* p);

int jingdong_set_key(const char* key);

int jingdong_get_sec_mode(uint8* sec);

#endif /* __JINGDONG_H__ */
