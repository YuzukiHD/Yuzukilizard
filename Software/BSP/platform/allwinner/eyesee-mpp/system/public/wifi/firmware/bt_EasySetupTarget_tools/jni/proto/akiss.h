#ifndef __AKISS_H__
#define __AKISS_H__

#include <easy_setup.h>

#define AKISS_KEY_STRING_LEN (16)

typedef struct {
    uint8 key_bytes[AKISS_KEY_STRING_LEN];  /* key string for decoding */
} akiss_param_t;

typedef struct {
    easy_setup_result_t es_result;
    uint8 random; /* akiss random byte */
} akiss_result_t;

int akiss_set_key(const char* key);
void akiss_get_param(void* p);
void akiss_set_result(const void* p);

int akiss_get_random(uint8* random);

#endif /* __AKISS_H__ */
