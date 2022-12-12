/*
 * Copyright (c) 2008-2017 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : Id3Base.c
 * Description : Base of Id3_v1 and Id3_v2 parser
 * History :
 *
 */

#include "Id3Base.h"

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif

#define LOG_TAG "Id3Base"

static const size_t kMaxMetadataSize = 256 * 1024 * 1024;

kbool CdxParseSyncsafeInteger(const cdx_uint8 encoded[4], size_t *x);
void CdxWriteSyncsafeInteger(cdx_uint8 *dst, size_t x);
size_t CdxStringSize(const cdx_uint8 *start, cdx_uint8 encoding);

kbool CdxParseSyncsafeInteger(const cdx_uint8 encoded[4], size_t *x) {
    cdx_int32 i;
    *x = 0;
    for (i = 0; i < 3; ++i) {
        if ((encoded[i] & 0x01) && (encoded[i + 1] & 0x80)) {
            return kfalse;
        }

        *x = ((*x) << 7) | encoded[i];
    }

    *x = ((*x) << 7) | encoded[3];
    return ktrue;
}

void CdxWriteSyncsafeInteger(cdx_uint8 *dst, size_t x) {
    size_t i;
    for (i = 0; i < 4; ++i) {
        dst[3 - i] = (x & 0x7f);
        x >>= 7;
    }
}

size_t CdxStringSize(const cdx_uint8 *start, cdx_uint8 encoding)
{
    // UCS-2
    size_t n = 0;
    if (encoding == 0x00 || encoding == 0x03) {
        // ISO 8859-1 or UTF-8
        return strlen((const char *)start) + 1;
    }

    while (start[n] != '\0' || start[n + 1] != '\0') {
        n += 2;
    }

    // Add size of null termination.
    return n + 2;
}

//-----iterator
static size_t __IteratorGetHeaderLength(void* hself)
{
    Iterator* it   = NULL;
    if(!hself) return 0;
    it = (Iterator*)hself;

    if (it->mParent->mVersion == ID3_V2_2) {
        return 6;
    } else if (it->mParent->mVersion == ID3_V2_3 || it->mParent->mVersion == ID3_V2_4) {
        return 10;
    } else {
        //CHECK(mParent.mVersion == ID3_V1 || mParent.mVersion == ID3_V1_1);
        return 0;
    }
}

static kerrno __IteratorGetString(void* hself, StringCtn *id, StringCtn *comment)
{
    Iterator* it   = NULL;
    kerrno err = KERR_NONE;
    if(!hself)
        return KERR_NULL_PTR;

    it  = (Iterator*)hself;
    err = it->getStringInternal(it, id, kfalse);
    if (comment != NULL) {
        err = it->getStringInternal(it, comment, ktrue);
    }
    return err;
}

static kerrno __IteratorGetStringInternal(void* hself, StringCtn *id, kbool otherdata)
{
    const cdx_uint8 *frameData = NULL;
    cdx_uint8 encoding;
    size_t n;
    Iterator* it   = NULL;
    if(!hself)
        return KERR_NULL_PTR;

    it = (Iterator*)hself;
    id->setTo8(id, "", strlen(""));

    frameData = it->mFrameData;
    if (frameData == NULL) {
        return KERR_INVAILD_DATA;
    }

    encoding = *frameData;
    it->encoding = encoding;
    if (it->mParent->mVersion == ID3_V1 || it->mParent->mVersion == ID3_V1_1) {
        if (it->mOffset == 126 || it->mOffset == 127) {
            // Special treatment for the track number and genre.
            char tmp[16] = {0};
            sprintf(tmp, "%d", (int)*frameData);
            id->setTo8(id, tmp, strlen(tmp));
            return KERR_NONE;
        }

        // this is supposed to be ISO-8859-1, but pass it up as-is to the caller, who will figure
        // out the real encoding
        id->setTo8(id, (const char*)frameData, it->mFrameSize);
        return KERR_NONE;
    }

    n = it->mFrameSize - it->getHeaderLength(it) - 1;
    if (otherdata) {
        // skip past the encoding, language, and the 0 separator
        cdx_int32 i;
        int skipped;
        frameData += 4;
        i = n - 4;
        while(--i >= 0 && *++frameData != 0)
            ;//do nothing...
        skipped = (frameData - it->mFrameData);
        if (skipped >= (int)n) {
            return KERR_OUT_OF_RANGE;
        }
        n -= skipped;
    }

    if (encoding == ENCODE_ISO_8859_1) {
        // supposedly ISO 8859-1
        id->setTo8(id, (const char*)frameData + 1, n);
    } else if (encoding == ENCODE_UTF_8) {
        // supposedly UTF-8
        id->setTo8(id, (const char *)(frameData + 1), n);
    } else if (encoding == ENCODE_UTF_16_BE) {
        // supposedly UTF-16 BE, no byte order mark.
        // API wants number of characters, not number of bytes...
        int len = n / 2;
        int i;
        const cdx_uint16 *framedata = (const cdx_uint16*) (frameData + 1);
        cdx_uint16 *framedatacopy = NULL;
#if 1
        framedatacopy = (cdx_uint16*)malloc(len);
        for (i = 0; i < len; i++) {
            framedatacopy[i] = bswap16(framedata[i]);
        }
        framedata = framedatacopy;
#endif
        id->setTo16(id, framedata, len);
        if (framedatacopy != NULL) {
            free(framedatacopy);
            framedatacopy = NULL;
        }
    } else if (encoding == ENCODE_USC_2) {
        // UCS-2
        // API wants number of characters, not number of bytes...
        int len = n / 2;
        int i;
        kbool eightBit;
        cdx_uint16 *framedatastorge = NULL;
        cdx_uint16 *framedata = NULL;
        framedatastorge = framedata = (cdx_uint16*)malloc(len*sizeof(cdx_uint16));
        memcpy(framedata, frameData + 1, len*sizeof(cdx_uint16));

        if (*framedata == 0xfffe) {
            // endianness marker doesn't match host endianness, convert
            //framedatacopy = (cdx_uint16*)malloc(len*sizeof(cdx_uint16));
            for (i = 0; i < len; i++) {
                framedata[i] = bswap16(framedata[i]);
            }
            //framedata = framedatacopy;
        }
        // If the string starts with an endianness marker, skip it
        if (*framedata == 0xfeff) {
            framedata++;
            len--;
        }

        // check if the resulting data consists entirely of 8-bit values
        eightBit = ktrue;
        for (i = 0; i < len; i++) {
            if (framedata[i] > 0xff) {
                eightBit = kfalse;
                break;
            }
        }
        if (eightBit) {
            // collapse to 8 bit, then let the media scanner client figure out the real encoding
            char *frame8 = (char*)malloc(len);
            for (i = 0; i < len; i++) {
                frame8[i] = framedata[i];
            }
            id->setTo8(id, frame8, len);
            free(frame8);
            frame8 = NULL;
        } else {
            id->setTo16(id, framedata, len);
        }

        if (framedatastorge != NULL) {
            free(framedatastorge);
            framedatastorge = NULL;
        }
    }
    return KERR_NONE;
}

static kbool __IteratorDone(void* hself)
{
    Iterator* it   = NULL;
    if(!hself)
        return ktrue;
    it = (Iterator*)hself;
    if(it->mFrameData == NULL)
        return ktrue;
    else
        return kfalse;
}

static kerrno __IteratorNext(void* hself)
{
    Iterator* it   = NULL;
    if(!hself)
        return KERR_NULL_PTR;
    it = (Iterator*)hself;
    if (it->mFrameData == NULL) {
        return KERR_NO_DATA;
    }

    it->mOffset += it->mFrameSize;

    return it->findFrame(it);
}

static kerrno __IteratorGetID(void* hself, StringCtn *id)
{
    Iterator* it   = NULL;
    if(!hself)
        return KERR_NULL_PTR;
    it = (Iterator*)hself;


    id->setTo8(id, "", strlen(""));

    if (it->mFrameData == NULL) {
        return KERR_NO_DATA;
    }

    if (it->mParent->mVersion == ID3_V2_2) {
        id->setTo8(id, (const char *)&it->mParent->mData[it->mOffset], 3);
    } else if (it->mParent->mVersion == ID3_V2_3 || it->mParent->mVersion == ID3_V2_4) {
        id->setTo8(id, (const char *)&it->mParent->mData[it->mOffset], 4);
    } else {
        //CHECK(mParent.mVersion == ID3_V1 || mParent.mVersion == ID3_V1_1);

        switch (it->mOffset) {
            case 3:
                id->setTo8(id, "TT2", strlen("TT2"));
                break;
            case 33:
                id->setTo8(id, "TP1", strlen("TP1"));
                break;
            case 63:
                id->setTo8(id, "TAL", strlen("TAL"));
                break;
            case 93:
                id->setTo8(id, "TYE", strlen("TYE"));
                break;
            case 97:
                id->setTo8(id, "COM", strlen("COM"));
                break;
            case 126:
                id->setTo8(id, "TRK", strlen("TRK"));
                break;
            case 127:
                id->setTo8(id, "TCO", strlen("TCO"));
                break;
            default:
                CDX_LOGE("should not be here.");
                break;
        }
    }
    return KERR_NONE;
}

static kerrno __IteratorFindFrame(void* hself)
{
    Iterator* it   = NULL;
    if(!hself)
        return KERR_NULL_PTR;
    it = (Iterator*)hself;

    for (;;) {
        it->mFrameData = NULL;
        it->mFrameSize = 0;

        if (it->mParent->mVersion == ID3_V2_2) {
            char id[4];
            if (it->mOffset + 6 > it->mParent->mSize) {
                return KERR_OUT_OF_RANGE;
            }

            if (!memcmp(&it->mParent->mData[it->mOffset], "\0\0\0", 3)) {
                return KERR_RESERVED;//what the hell?
            }

            it->mFrameSize =
                (it->mParent->mData[it->mOffset + 3] << 16)
                | (it->mParent->mData[it->mOffset + 4] << 8)
                | it->mParent->mData[it->mOffset + 5];

            it->mFrameSize += 6;

            if (it->mOffset + it->mFrameSize > it->mParent->mSize) {
                CDX_LOGV("partial frame at offset %zu (size = %zu, bytes-remaining = %zu)",
                        it->mOffset, it->mFrameSize, it->mParent->mSize - it->mOffset - (size_t)6);
                return KERR_OUT_OF_RANGE;
            }

            it->mFrameData = &it->mParent->mData[it->mOffset + 6];

            if (!it->mID) {
                break;
            }


            memcpy(id, &it->mParent->mData[it->mOffset], 3);
            id[3] = '\0';

            if (!strcmp(id, it->mID)) {
                break;
            }
        } else if (it->mParent->mVersion == ID3_V2_3
                || it->mParent->mVersion == ID3_V2_4) {
            size_t baseSize;
            cdx_uint16 flags;
            char id[5];
            if (it->mOffset + 10 > it->mParent->mSize) {
                return KERR_OUT_OF_RANGE;
            }

            if (!memcmp(&it->mParent->mData[it->mOffset], "\0\0\0\0", 4)) {
                return KERR_RESERVED;//what the hell?
            }

            if (it->mParent->mVersion == ID3_V2_4) {
                if (!CdxParseSyncsafeInteger(
                            &it->mParent->mData[it->mOffset + 4], &baseSize)) {
                    return KERR_INVAILD_DATA;
                }
            } else {
                baseSize = U32_AT(&it->mParent->mData[it->mOffset + 4]);
            }

            it->mFrameSize = 10 + baseSize;

            if (it->mOffset + it->mFrameSize > it->mParent->mSize) {
                CDX_LOGV("partial frame at offset %zu (size = %zu, bytes-remaining = %zu)",
                        it->mOffset, it->mFrameSize, it->mParent->mSize - it->mOffset - (size_t)10);
                return KERR_OUT_OF_RANGE;
            }

            flags = U16_AT(&it->mParent->mData[it->mOffset + 8]);

            if ((it->mParent->mVersion == ID3_V2_4 && (flags & 0x000c))
                || (it->mParent->mVersion == ID3_V2_3 && (flags & 0x00c0))) {
                // Compression or encryption are not supported at this time.
                // Per-frame unsynchronization and data-length indicator
                // have already been taken care of.

                CDX_LOGV("Skipping unsupported frame \
                            (compression, encryption or per-frame unsynchronization flagged");

                it->mOffset += it->mFrameSize;
                continue;
            }

            it->mFrameData = &it->mParent->mData[it->mOffset + 10];

            if (!it->mID) {
                break;
            }

            memcpy(id, &it->mParent->mData[it->mOffset], 4);
            id[4] = '\0';

            if (!strcmp(id, it->mID)) {
                break;
            }
        } else {
            //CHECK(mParent.mVersion == ID3_V1 || mParent.mVersion == ID3_V1_1);
            StringCtn *id = NULL;
            id = GenerateStringContainer();
            if(!id)
            {
                CDX_LOGE("No mem for StringContainer while Iterator finding frame!");
                return KERR_NO_MEM;
            }
            if (it->mOffset >= it->mParent->mSize) {
                return KERR_OUT_OF_RANGE;
            }

            it->mFrameData = &it->mParent->mData[it->mOffset];

            switch (it->mOffset) {
                case 3:
                case 33:
                case 63:
                    it->mFrameSize = 30;
                    break;
                case 93:
                    it->mFrameSize = 4;
                    break;
                case 97:
                    if (it->mParent->mVersion == ID3_V1) {
                        it->mFrameSize = 30;
                    } else {
                        it->mFrameSize = 29;
                    }
                    break;
                case 126:
                    it->mFrameSize = 1;
                    break;
                case 127:
                    it->mFrameSize = 1;
                    break;
                default:
                    //CHECK(!"Should not be here, invalid offset.");
                    break;
            }

            if (!it->mID) {
                break;
            }

            it->getID(it,id);
            if (strcmp(id->mString, it->mID) == 0)
            {
                break;
            }
            EraseStringContainer(&id);
        }

        it->mOffset += it->mFrameSize;
    }
    return KERR_NONE;
}

static const cdx_uint8* __IteratorGetData(void* hself, size_t *length)
{
    Iterator* it   = NULL;
    if(!hself) return NULL;
    it = (Iterator*)hself;

    *length = 0;

    if (it->mFrameData == NULL) {
        return NULL;
    }

    *length = it->mFrameSize - it->getHeaderLength(it);

    return it->mFrameData;
}

Iterator* GenerateIterator(ID3* parent, const char *id)
{
    Iterator* it   = NULL;
    if(!parent)
    {
        CDX_LOGE("Iterator's constructor need id3 parer!");
        return NULL;
    }
    it = (Iterator*)malloc(sizeof(Iterator));
    if(!it)
    {
        CDX_LOGE("No mem for Iterator!");
        return NULL;
    }
    memset(it, 0x00, sizeof(Iterator));

    it->getHeaderLength   = __IteratorGetHeaderLength;
    it->getString         = __IteratorGetString;
    it->getStringInternal = __IteratorGetStringInternal;
    it->done              = __IteratorDone;
    it->next              = __IteratorNext;
    it->getID             = __IteratorGetID;
    it->findFrame         = __IteratorFindFrame;
    it->getData           = __IteratorGetData;

    it->mParent = parent;
    it->mID     = NULL;
    it->mOffset = it->mParent->mFirstFrameOffset,
    it->mFrameData = NULL;
    it->mFrameSize = 0;

    if (id) {
        it->mID = (char *)malloc(strlen(id)+1);
        if(!it->mID)
        {
            CDX_LOGE("No mem for Iterator->mID");
            free(it);
            it = NULL;
            return NULL;
        }
        memset(it->mID, 0x00, strlen(id)+1);
        memcpy(it->mID, id, strlen(id));
    }

    it->findFrame(it);
    CDX_LOGV("Generating Iterator finish...");
    return it;
}

kbool EraseIterator(void* arg)
{
    Iterator* it   = NULL;
    void* null_ptr = NULL;

    memcpy(&it, arg, sizeof(it));
    if(!it)
    {
        CDX_LOGW("Iterator has already been free");
        return kfalse;
    }

    if(it->mID)
    {
        free(it->mID);
        it->mID = NULL;
    }

    memset(it, 0x00, sizeof(Iterator));
    free(it);
    memcpy(arg, &null_ptr, sizeof(arg));
    CDX_LOGV("Free Iterator finish...");
    return ktrue;
}
//
//-----id3
static size_t __id3Read(void* hself, void* buf, size_t sz)
{
    ID3* id3   = NULL;
    cdx_uint8* data = (cdx_uint8*)buf;
    size_t     readlen = 0;
    size_t     totalread = 0;
    if(!hself)
        return 0;
    id3 = (ID3*)hself;
    while(id3->extraBuf && id3->extraBufValidSz && sz)
    {
        readlen = id3->extraBufValidSz > sz?sz:id3->extraBufValidSz;
        memcpy(data, id3->extraBuf + id3->extraBufOffset, readlen);
        id3->extraBufValidSz -= readlen;
        id3->extraBufOffset  += readlen;
        data += readlen;
        sz   -= readlen;
        totalread += readlen;
    }
    if(sz != 0)//calling the jesus
    {
        readlen =  CdxStreamRead(id3->stream, data, sz);
        totalread += readlen;
    }
    return totalread;
}

static size_t __id3ReadAt(void* hself, int64_t offset, void* buf, size_t sz)
{
    ID3* id3   = NULL;
    if(!hself)
        return 0;
    id3 = (ID3*)hself;

    CdxStreamSeek(id3->stream, offset, SEEK_SET);
    return CdxStreamRead(id3->stream, buf, sz);
}

static kbool __id3GetSize(void* hself, int64_t* sz)
{
    ID3* id3   = NULL;
    int64_t curoff = 0;

    *sz = 0;
    if(!hself)
        return kfalse;
    id3 = (ID3*)hself;

    curoff = CdxStreamTell(id3->stream);
    CdxStreamSeek(id3->stream, 0, SEEK_END);
    *sz = CdxStreamTell(id3->stream);
    CdxStreamSeek(id3->stream, curoff, SEEK_SET);
    return ktrue;
}

static int64_t __id3Tell(void* hself)
{
    ID3* id3   = NULL;
    if(!hself)
        return 0;
    id3 = (ID3*)hself;

    return CdxStreamTell(id3->stream);
}

static kbool __id3RemoveUnsynchronizationV2_4(void* hself, kbool iTunesHack) {
    size_t oldSize;
    size_t offset = 0;
    ID3* id3   = NULL;
    if(!hself)
        return kfalse;
    id3 = (ID3*)hself;

    oldSize = id3->mSize;
    while (offset + 10 <= id3->mSize) {
        size_t dataSize;
        cdx_uint16 flags, prevFlags;
        if (!memcmp(&id3->mData[offset], "\0\0\0\0", 4)) {
            break;
        }

        if (iTunesHack) {
            dataSize = U32_AT(&id3->mData[offset + 4]);
        } else if (!CdxParseSyncsafeInteger(&id3->mData[offset + 4], &dataSize)) {
            return kfalse;
        }

        if (offset + dataSize + 10 > id3->mSize) {
            return kfalse;
        }

        flags = U16_AT(&id3->mData[offset + 8]);
        prevFlags = flags;

        if (flags & 1) {
            // Strip data length indicator

            memmove(&id3->mData[offset + 10], &id3->mData[offset + 14], id3->mSize - offset - 14);
            id3->mSize -= 4;
            dataSize -= 4;

            flags &= ~1;
        }

        if (flags & 2) {
            // This file has "unsynchronization", so we have to replace occurrences
            // of 0xff 0x00 with just 0xff in order to get the real data.

            size_t readOffset = offset + 11;
            size_t writeOffset = offset + 11;
            size_t i;
            for (i = 0; i + 1 < dataSize; ++i) {
                if (id3->mData[readOffset - 1] == 0xff
                        && id3->mData[readOffset] == 0x00) {
                    ++readOffset;
                    --id3->mSize;
                    --dataSize;
                }
                id3->mData[writeOffset++] = id3->mData[readOffset++];
            }
            // move the remaining data following this frame
            memmove(&id3->mData[writeOffset], &id3->mData[readOffset], oldSize - readOffset);

            flags &= ~2;
        }

        if (flags != prevFlags || iTunesHack) {
            CdxWriteSyncsafeInteger(&id3->mData[offset + 4], dataSize);
            id3->mData[offset + 8] = flags >> 8;
            id3->mData[offset + 9] = flags & 0xff;
        }

        offset += 10 + dataSize;
    }

    memset(&id3->mData[id3->mSize], 0, oldSize - id3->mSize);

    return ktrue;
}

static kerrno __id3RemoveUnsynchronization(void* hself) {
    size_t i;
    ID3* id3   = NULL;
    if(!hself)
        return KERR_NULL_PTR;
    id3 = (ID3*)hself;

    for (i = 0; i + 1 < id3->mSize; ++i) {
        if (id3->mData[i] == 0xff && id3->mData[i + 1] == 0x00) {
            memmove(&id3->mData[i + 1], &id3->mData[i + 2], id3->mSize - i - 2);
            --id3->mSize;
        }
    }
    return KERR_NONE;
}

static kbool __id3ParseV2(void *hself) {
    id3_header header;
    size_t size;
    ID3* id3   = NULL;

    if(!hself)
    {
        return kfalse;
    }
    id3 = (ID3*)hself;
    if (id3->read(id3, &header, sizeof(header)) != (size_t)sizeof(header)) {
        return kfalse;
    }

    if (memcmp(header.id, "ID3", 3)) {
        return kfalse;
    }

    if (header.version_major == 0xff || header.version_minor == 0xff) {
        return kfalse;
    }

    if (header.version_major == 2) {
        if (header.flags & 0x3f) {
            // We only support the 2 high bits, if any of the lower bits are
            // set, we cannot guarantee to understand the tag format.
            return kfalse;
        }

        if (header.flags & 0x40) {
            // No compression scheme has been decided yet, ignore the
            // tag if compression is indicated.
            return kfalse;
        }
    } else if (header.version_major == 3) {
        if (header.flags & 0x1f) {
            // We only support the 3 high bits, if any of the lower bits are
            // set, we cannot guarantee to understand the tag format.
            return kfalse;
        }
    } else if (header.version_major == 4) {
        if (header.flags & 0x0f) {
            // The lower 4 bits are undefined in this spec.
            return kfalse;
        }
    } else {
        return kfalse;
    }

    if (!CdxParseSyncsafeInteger(header.enc_size, &size)) {
        return kfalse;
    }

    if (size > kMaxMetadataSize) {
        CDX_LOGE("skipping huge ID3 metadata of size %zu", size);
        return kfalse;
    }

    id3->mData = (cdx_uint8 *)malloc(size);

    if (id3->mData == NULL) {
        return kfalse;
    }

    id3->mSize = size;
    id3->mRawSize = id3->mSize + sizeof(header);

    if (id3->read(id3, id3->mData, id3->mSize) != (size_t)id3->mSize) {
        free(id3->mData);
        id3->mData = NULL;
        return kfalse;
    }

    if (header.version_major == 4) {
        kbool success;
        void *copy = malloc(size);
        memcpy(copy, id3->mData, size);

        success = id3->doRemoveUnsynchronizationV2_4(id3, kfalse /* iTunesHack */);
        if (!success) {
            memcpy(id3->mData, copy, size);
            id3->mSize = size;

            success = id3->doRemoveUnsynchronizationV2_4(id3, ktrue /* iTunesHack */);

            if (success) {
                CDX_LOGV("Had to apply the iTunes hack to parse this ID3 tag");
            }
        }

        free(copy);
        copy = NULL;

        if (!success) {
            free(id3->mData);
            id3->mData = NULL;
            return kfalse;
        }
    } else if (header.flags & 0x80) {
        CDX_LOGV("removing unsynchronization");

        id3->doRemoveUnsynchronization(id3);
    }

    id3->mFirstFrameOffset = 0;
    if (header.version_major == 3 && (header.flags & 0x40)) {
        // Version 2.3 has an optional extended header.
        size_t extendedHeaderSize;
        cdx_uint16 extendedFlags;

        if (id3->mSize < 4) {
            free(id3->mData);
            id3->mData = NULL;
            return kfalse;
        }

        extendedHeaderSize = U32_AT(&id3->mData[0]) + 4;

        if (extendedHeaderSize > id3->mSize) {
            free(id3->mData);
            id3->mData = NULL;
            return kfalse;
        }

        id3->mFirstFrameOffset = extendedHeaderSize;

        extendedFlags = 0;
        if (extendedHeaderSize >= 6) {
            extendedFlags = U16_AT(&id3->mData[4]);

            if (extendedHeaderSize >= 10) {
                size_t paddingSize = U32_AT(&id3->mData[6]);

                if (id3->mFirstFrameOffset + paddingSize > id3->mSize) {
                    free(id3->mData);
                    id3->mData = NULL;
                    return kfalse;
                }

                id3->mSize -= paddingSize;
            }

            if (extendedFlags & 0x8000) {
                CDX_LOGV("have crc");
            }
        }
    } else if (header.version_major == 4 && (header.flags & 0x40)) {
        // Version 2.4 has an optional extended header, that's different
        // from Version 2.3's...
        size_t ext_size;

        if (id3->mSize < 4) {
            free(id3->mData);
            id3->mData = NULL;
            return kfalse;
        }

        if (!CdxParseSyncsafeInteger(id3->mData, &ext_size)) {
            free(id3->mData);
            id3->mData = NULL;
            return kfalse;
        }

        if (ext_size < 6 || ext_size > id3->mSize) {
            free(id3->mData);
            id3->mData = NULL;
            return kfalse;
        }

        id3->mFirstFrameOffset = ext_size;
    }

    if (header.version_major == 2) {
        id3->mVersion = ID3_V2_2;
    } else if (header.version_major == 3) {
        id3->mVersion = ID3_V2_3;
    } else {
        //CHECK_EQ(header.version_major, 4);
        id3->mVersion = ID3_V2_4;
    }

    return ktrue;
}

static kbool __id3ParseV1(void *hself)
{
    int64_t size;
    ID3* id3   = NULL;
    if(!hself)
        return kfalse;
    id3 = (ID3*)hself;

    if (!id3->getsize(id3, &size) || size < (int64_t)V1_TAG_SIZE) {
        return kfalse;
    }

    id3->mData = (cdx_uint8 *)malloc(V1_TAG_SIZE);
    if (id3->readAt(id3, size - V1_TAG_SIZE, id3->mData, V1_TAG_SIZE)
            != (size_t)V1_TAG_SIZE) {
        free(id3->mData);
        id3->mData = NULL;
        return kfalse;
    }

    if (memcmp("TAG", id3->mData, 3)) {
        free(id3->mData);
        id3->mData = NULL;
        return kfalse;
    }

    id3->mSize = V1_TAG_SIZE;
    id3->mFirstFrameOffset = 3;

    if (id3->mData[V1_TAG_SIZE - 3] != 0) {
        id3->mVersion = ID3_V1;
    } else {
        id3->mVersion = ID3_V1_1;
    }
    return ktrue;
}

static void* __id3GetAlbumArt(void *hself, size_t *length, StringCtn *mime)
{
    Iterator* it = NULL;
    ID3* id3   = NULL;
    if(!hself)
        return NULL;
    id3 = (ID3*)hself;

    *length = 0;
    mime->setTo8(mime, "", sizeof(""));

    it = GenerateIterator(id3,
            (id3->mVersion == ID3_V2_3 || id3->mVersion == ID3_V2_4) ? "APIC" : "PIC");

    while (!it->done(it)) {
        size_t size;
        cdx_uint8 *data = (cdx_uint8 *)it->getData(it, &size);

        if (id3->mVersion == ID3_V2_3 || id3->mVersion == ID3_V2_4) {
            cdx_uint8 encoding = data[0], picType = 0;
            size_t descLen = 0, mimeLen = 0;
            mime->setTo8(mime, (const char *)&data[1], strlen((const char *)&data[1]));
            mimeLen = strlen((const char *)&data[1]) + 1;

            picType = data[1 + mimeLen];
#if 0
            if (picType != 0x03) {
                // Front Cover Art
                it.next();
                continue;
            }
#endif

            descLen = CdxStringSize(&data[2 + mimeLen], encoding);

            *length = size - 2 - mimeLen - descLen;

            return &data[2 + mimeLen + descLen];
        } else {
            cdx_uint8 encoding = data[0];
            size_t  descLen = 0;
            if (!memcmp(&data[1], "PNG", 3)) {
                mime->setTo8(mime, "image/png", sizeof("image/png"));
            } else if (!memcmp(&data[1], "JPG", 3)) {
                mime->setTo8(mime, "image/jpeg", sizeof("image/jpeg"));
            } else if (!memcmp(&data[1], "-->", 3)) {
                mime->setTo8(mime, "text/plain", sizeof("text/plain"));
            } else {
                return NULL;
            }

#if 0
            cdx_uint8 picType = data[4];
            if (picType != 0x03) {
                // Front Cover Art
                it.next();
                continue;
            }
#endif

            descLen = CdxStringSize(&data[5], encoding);
            *length = size - 5 - descLen;

            return &data[5 + descLen];
        }
    }

    EraseIterator(&it);
    return NULL;
}

/*
*  Make insure that if current stream has extraly pre-read buffer data
*  stream : |-----------------x (current file point) -----------|
*                ^ --- extra buf -----^
*
*/
ID3* GenerateId3(CdxStreamT* in, cdx_uint8* init_buf, size_t buf_sz,
                    kbool ignoreV1)
{
    ID3* id3 = NULL;
    id3 = (ID3*)malloc(sizeof(ID3));
    if(!id3)
    {
        CDX_LOGE("No mem for ID3!");
        return NULL;
    }
    memset(id3, 0x00, sizeof(ID3));
    id3->stream = in;
    if(init_buf && (buf_sz != 0))
    {
        id3->extraBuf = (cdx_uint8*)malloc(buf_sz);
        id3->extraBufRange  = buf_sz;
        id3->extraBufOffset = 0;
        id3->extraBufValidSz = id3->extraBufRange - id3->extraBufOffset;
        memset(id3->extraBuf, 0x00, id3->extraBufRange);
        memcpy(id3->extraBuf, init_buf, buf_sz);
    }

    id3->doParseV1 = __id3ParseV1;
    id3->doParseV2 = __id3ParseV2;
    id3->doRemoveUnsynchronizationV2_4 = __id3RemoveUnsynchronizationV2_4;
    id3->doRemoveUnsynchronization     = __id3RemoveUnsynchronization;
    id3->read = __id3Read;
    id3->readAt = __id3ReadAt;
    id3->tell = __id3Tell;
    id3->getsize = __id3GetSize;
    id3->getAlbumArt = __id3GetAlbumArt;

    id3->mIsValid = id3->doParseV2(id3);
    if(!id3->mIsValid && !ignoreV1)
    {
        id3->mIsValid = id3->doParseV1(id3);
    }
    CDX_LOGD("Generating id3 base finish...");
    return id3;
}

kbool EraseId3(void* arg)
{
    ID3* id3 = NULL;
    void* null_ptr = NULL;

    memcpy(&id3, arg, sizeof(id3));
    if(!id3)
    {
        CDX_LOGW("id3 has already been free");
        return kfalse;
    }

    if(id3->mData)
    {
        free(id3->mData);
        id3->mData = NULL;
    }
    if(id3->extraBuf)
    {
        free(id3->extraBuf);
        id3->extraBuf = NULL;
    }
    memset(id3, 0x00, sizeof(ID3));

    free(id3);
    memcpy(arg, &null_ptr, sizeof(arg));
    CDX_LOGD("Free id3 base finish...");
    return ktrue;
}

void Id3BaseGetMetaData(CdxMediaInfoT *mediaInfo, ID3* id3Base)
{
    static const Map CDXMap[] = {
       { ARTIST, CdxMetaKeyArtist, "TPE1", "TP1" },
       { ALBUM, CdxMetaKeyAlbum, "TALB", "TAL" },
       { ALBUM_ARTIST, CdxMetaKeyAlbumArtist, "TPE2", "TP2" },
       { TITLE, CdxMetaKeyTitle, "TIT2", "TT2" },
       { COMPOSER, CdxMetaKeyComposer, "TCOM", "TCM" },
       { GENRE, CdxMetaKeyGenre, "TCON", "TCO" },
       { YEAR, CdxMetaKeyYear, "TYE", "TYER" },
       { AUTHOR, CdxMetaKeyAuthor, "TXT", "TEXT" },
       { CDTRACKNUMBER, CdxMetaKeyCDTrackNumber, "TRK", "TRCK" },
       { DISCNUMBER, CdxMetaKeyDiscNumber, "TPA", "TPOS" },
       { COMPILATION, CdxMetaKeyCompilation, "TCP", "TCMP" },
    };
    Iterator* it = NULL;
    static const size_t kNumMapEntries = sizeof(CDXMap) / sizeof(CDXMap[0]);
    size_t i;

    for (i = 0; i < kNumMapEntries; ++i) {
        it = GenerateIterator(id3Base, CDXMap[i].tag1);
        if (it->done(it)) {
            EraseIterator(&it);
            it = GenerateIterator(id3Base, CDXMap[i].tag2);
        }

        if (it->done(it)) {
            EraseIterator(&it);
            continue;
        }

        StringCtn* text = GenerateStringContainer();
        it->getString(it, text, NULL);
        SetMetaData(mediaInfo, CDXMap[i].idx, text->string(text), it->encoding);
        EraseStringContainer(&text);
        EraseIterator(&it);
    }
}

void Id3BaseExtraAlbumPic(CdxMediaInfoT *mediaInfo, ID3* id3Base)
{
    ID3*   id3 = id3Base;
    void  *AlbumPic = NULL;
    size_t AlbumPicSz = 0;
    StringCtn* text = GenerateStringContainer();
    AlbumPic = id3->getAlbumArt(id3, &AlbumPicSz, text);
    if(AlbumPic && AlbumPicSz != 0)
    {
        mediaInfo->nAlbumArtBufSize = AlbumPicSz;
        mediaInfo->pAlbumArtBuf     = (cdx_uint8 *)AlbumPic;
    }
}