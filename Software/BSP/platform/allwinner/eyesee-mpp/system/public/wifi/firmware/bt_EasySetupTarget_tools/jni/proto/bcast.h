#ifndef __BCAST_H__
#define __BCAST_H__

#include <easy_setup.h>

#define BCAST_KEY_STRING_LEN (16)
#define BCAST_NONCE_PAD_LEN (13)

typedef struct {
    uint8 key_bytes[BCAST_KEY_STRING_LEN];  /* key string for decoding */
    uint8 random_bytes[BCAST_NONCE_PAD_LEN]; /* random bytes */
    uint8 key_bytes_qqcon[BCAST_KEY_STRING_LEN];  /* key string for decoding for qqcon */
    uint8 random_bytes_qqcon[BCAST_NONCE_PAD_LEN]; /* random bytes for qqcon */
} bcast_param_t;

typedef struct {
    easy_setup_result_t es_result;
    ip_address_t   host_ip_address;      /* setup client's ip address */
    uint16         host_port;            /* setup client's port */
} bcast_result_t;

void bcast_get_param(void* p);
void bcast_set_result(const void* p);

int bcast_set_key(const char* key);
int bcast_set_key_qqcon(const char* key);
int bcast_get_sender_ip(char buff[], int buff_len);
int bcast_get_sender_port(uint16* port);

#endif /* __BCAST_H__ */
