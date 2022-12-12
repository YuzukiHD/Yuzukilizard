#ifndef __JD_H__
#define __JD_H__

#include <easy_setup.h>

#define JD_KEY_STRING_LEN (16)

typedef struct {
    uint8 key_bytes[JD_KEY_STRING_LEN];  /* key string for decoding */
} jd_param_t;

typedef struct {
    easy_setup_result_t es_result;
    uint8 crc;
    uint32 ip;
    uint16 port;
} jd_result_t;

int jd_set_key(const char* key);
void jd_get_param(void* p);
void jd_set_result(const void* p);

int jd_get_crc(uint8* crc);
int jd_get_ip(uint32* ip);
int jd_get_port(uint16* port);

#endif /* __JD_H__ */
