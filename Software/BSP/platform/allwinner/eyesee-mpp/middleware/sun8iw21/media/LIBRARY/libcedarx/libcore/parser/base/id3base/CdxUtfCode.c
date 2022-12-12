/*
 * Copyright (c) 2008-2017 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxUtfCode.c
 * Description : Parse Utf Code
 * History :
 *
 */

#include "CdxUtfCode.h"

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif

#define LOG_TAG "CdxUtfCode"

#define kUnicodeSurrogateHighStart 0x0000D800
#define kUnicodeSurrogateLowEnd    0x0000DFFF

static const cdx_uint32 kByteMask = 0x000000BF;
static const cdx_uint32 kByteMark = 0x00000080;
static const cdx_uint32 kUnicodeSurrogateHighEnd    = 0x0000DBFF;
static const cdx_uint32 kUnicodeSurrogateLowStart   = 0x0000DC00;

static const cdx_uint32 kUnicodeSurrogateStart      = kUnicodeSurrogateHighStart;
static const cdx_uint32 kUnicodeSurrogateEnd        = kUnicodeSurrogateLowEnd;
static const cdx_uint32 kUnicodeMaxCodepoint        = 0x0010FFFF;

static const cdx_uint32 kFirstByteMark[] = {
    0x00000000, 0x00000000, 0x000000C0, 0x000000E0, 0x000000F0
};

static size_t CdxUtf32CodepointUtf8Length(cdx_uint32 srcChar)
{
    // Figure out how many bytes the result will require.
    if (srcChar < 0x00000080) {
        return 1;
    } else if (srcChar < 0x00000800) {
        return 2;
    } else if (srcChar < 0x00010000) {
        if ((srcChar < kUnicodeSurrogateStart) || (srcChar > kUnicodeSurrogateEnd)) {
            return 3;
        } else {
            // Surrogates are invalid UTF-32 characters.
            return 0;
        }
    }
    // Max code point for Unicode is 0x0010FFFF.
    else if (srcChar <= kUnicodeMaxCodepoint) {
        return 4;
    } else {
        // Invalid UTF-32 character.
        return 0;
    }
}

static void CdxUtf32Codepoint2Utf8(cdx_uint8* dstP, cdx_uint32 srcChar, size_t bytes)
{
    dstP += bytes;
    switch (bytes)
    {   /* note: everything falls through. */
        case 4: *--dstP = (cdx_uint8)((srcChar | kByteMark) & kByteMask); srcChar >>= 6;
        case 3: *--dstP = (cdx_uint8)((srcChar | kByteMark) & kByteMask); srcChar >>= 6;
        case 2: *--dstP = (cdx_uint8)((srcChar | kByteMark) & kByteMask); srcChar >>= 6;
        case 1: *--dstP = (cdx_uint8)(srcChar | kFirstByteMark[bytes]);
    }
}

int32_t CdxUtf16toUtf8Length(const cdx_uint16 *src, size_t src_len)
{
    size_t ret = 0;
    const cdx_uint16* const end = src + src_len;
    if (src == NULL || src_len == 0) {
        return -1;
    }

    ret = 0;
    while (src < end) {
        if ((*src & 0xFC00) == 0xD800 && (src + 1) < end
                && (*++src & 0xFC00) == 0xDC00) {
            // surrogate pairs are always 4 bytes.
            ret += 4;
            src++;
        } else {
            ret += CdxUtf32CodepointUtf8Length((cdx_uint32) *src++);
        }
    }
    return ret;
}

void CdxUtf16toUtf8(const cdx_uint16* src, size_t src_len, char* dst)
{
    const cdx_uint16* cur_utf16;
    const cdx_uint16* const end_utf16 = src + src_len;
    char *cur;
    if (src == NULL || src_len == 0 || dst == NULL) {
        return;
    }

    cur_utf16 = src;
    cur = dst;
    while (cur_utf16 < end_utf16) {
        cdx_uint32 utf32;
        size_t len;
        // surrogate pairs
        if((*cur_utf16 & 0xFC00) == 0xD800 && (cur_utf16 + 1) < end_utf16
                && (*(cur_utf16 + 1) & 0xFC00) == 0xDC00) {
            utf32 = (*cur_utf16++ - 0xD800) << 10;
            utf32 |= *cur_utf16++ - 0xDC00;
            utf32 += 0x10000;
        } else {
            utf32 = (cdx_uint32) *cur_utf16++;
        }
        len = CdxUtf32CodepointUtf8Length(utf32);
        CdxUtf32Codepoint2Utf8((cdx_uint8*)cur, utf32, len);
        cur += len;
    }
    *cur = '\0';
}

void CdxUtf32toUtf8(const cdx_uint32* src, size_t src_len, char* dst)
{
    const cdx_uint32 *cur_utf32 = src;
    const cdx_uint32 *end_utf32 = src + src_len;
    char *cur = dst;

    if (src == NULL || src_len == 0 || dst == NULL) {
        return;
    }

    while (cur_utf32 < end_utf32) {
        size_t len = CdxUtf32CodepointUtf8Length(*cur_utf32);
        CdxUtf32Codepoint2Utf8((cdx_uint8 *)cur, *cur_utf32++, len);
        cur += len;
    }
    *cur = '\0';
}

int32_t CdxUtf32toUtf8Length(const cdx_uint32 *src, size_t src_len)
{
    size_t ret = 0;
    const cdx_uint32 *end = src + src_len;

    if (src == NULL || src_len == 0) {
        return -1;
    }

    while (src < end) {
        ret += CdxUtf32CodepointUtf8Length(*src++);
    }
    return ret;
}

char* CdxAllocFromUTF8(const char* in, size_t len)
{
    if (len > 0) {
        char *buf = (char *)malloc(len+1);
        if (buf) {
            memset(buf, 0x00, len + 1);
            memcpy(buf, in, len);
            return buf;
        }
        return NULL;
    }

    return NULL;
}

char* CdxAllocFromUTF16(const cdx_uint16* in, size_t len)
{
    int32_t bytes;
    char *buf;
    if (len == 0) return NULL;

    bytes = CdxUtf16toUtf8Length(in, len);
    if (bytes < 0) {
        return NULL;
    }

    buf = (char *)malloc(bytes+1);
    if (!buf) {
        return NULL;
    }
    memset(buf, 0x00, bytes + 1);
    CdxUtf16toUtf8(in, len, buf);
    return buf;
}

char* CdxAllocFromUTF32(const cdx_uint32* in, size_t len)
{
    int32_t bytes;
    char* buf = NULL;
    if (len == 0) {
        return NULL;
    }

    bytes = CdxUtf32toUtf8Length(in, len);
    if (bytes < 0) {
        return NULL;
    }

    buf = malloc(bytes+1);
    if (!buf) {
        return NULL;
    }
    memset(buf, 0x00, bytes + 1);
    CdxUtf32toUtf8(in, len, buf);

    return buf;
}
