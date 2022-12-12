#ifndef __NEEZE_H__
#define __NEEZE_H__

#include <easy_setup.h>

#define NEEZE_KEY_STRING_LEN (16)
#define NEEZE_NONCE_PAD_LEN (13)

typedef struct {
    uint8 key_bytes[NEEZE_KEY_STRING_LEN];  /* key string for decoding */
    uint8 random_bytes[NEEZE_NONCE_PAD_LEN]; /* random bytes */
    uint8 key_bytes_qqcon[NEEZE_KEY_STRING_LEN];  /* key string for decoding for qqcon */
    uint8 random_bytes_qqcon[NEEZE_NONCE_PAD_LEN]; /* random bytes for qqcon */
} neeze_param_t;

typedef struct {
    easy_setup_result_t es_result;
    ip_address_t host_ip_address;      /* setup client's ip address */
    uint16 host_port;            /* setup client's port */
} neeze_result_t;

void neeze_get_param(void* p);
void neeze_set_result(const void* p);

int neeze_set_key(const char* key);
int neeze_set_key_qqcon(const char* key);
int neeze_get_sender_ip(char buff[], int buff_len);
int neeze_get_sender_port(uint16* port);

#endif /* __NEEZE_H__ */
