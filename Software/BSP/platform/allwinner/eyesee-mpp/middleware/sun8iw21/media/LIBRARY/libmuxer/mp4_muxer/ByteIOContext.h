
#ifndef _BYTEIOCONTEXT_H_
#define _BYTEIOCONTEXT_H_

#include <FsWriter.h>

#define ByteIOContext FsWriter

void put_buffer_cache(void *mov, ByteIOContext *s, char *buf, int size);  //MOVContext* mov
void put_byte_cache(void *mov, ByteIOContext *s, int b);
void put_le32_cache(void *mov, ByteIOContext *s, unsigned int val);
void put_be32_cache(void *mov, ByteIOContext *s, unsigned int val);
void put_be16_cache(void *mov, ByteIOContext *s, unsigned int val);
void put_be24_cache(void *mov, ByteIOContext *s, unsigned int val);
void put_tag_cache(void *mov, ByteIOContext *s, const char *tag);

#endif  /* _BYTEIOCONTEXT_H_ */

