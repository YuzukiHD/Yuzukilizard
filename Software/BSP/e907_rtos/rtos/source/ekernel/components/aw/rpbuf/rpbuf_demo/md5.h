#ifndef __MD5_H_
#define __MD5_H_

#include <stdint.h>

typedef struct _md5_ctx {
	uint32_t count[2];
	uint32_t state[4];
	uint8_t buffer[64];
} md5_ctx;

void md5_digest(uint8_t *input, int len, uint8_t digest[16]);

void md5_init(md5_ctx *context);
void md5_update(md5_ctx *context, unsigned char *input, uint32_t len);
void md5_final(md5_ctx *context, uint8_t digest[16]);

#endif
