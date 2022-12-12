/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxTsParser.c
 * Description : TsParser
 * History :
 *
 */

//#define CONFIG_LOG_LEVEL    OPTION_LOG_LEVEL_DETAIL
#define LOG_TAG "tsParser"
#include <cdx_log.h>
#include <CdxBitReader.h>
#include <CdxMemory.h>
#include <CdxTime.h>
#include <CdxTsParser.h>
#include <stdint.h>
#define SAVE_FILE    (0)

#if SAVE_FILE
    FILE    *file = NULL;
#endif

#if !OPEN_CHECK
#undef CDX_CHECK
#define CDX_CHECK(e)
#undef CDX_FORCE_CHECK
#define CDX_FORCE_CHECK(e) \
    do { if(!e){CDX_LOGD("CDX_CHECK(%s) failed.", #e);} } while (0)
#endif

//#include <pthread.h>
/* I want the expression "y" evaluated even if verbose logging is off.*/
#define MY_LOGV(x, y) \
    do { unsigned tmp = y; /*CDX_LOGV(x, tmp);*/ (void)tmp; } while (0)

static cdx_uint16 U16_AT(const cdx_uint8 *ptr)
{
    return ptr[0] << 8 | ptr[1];
}

CDX_INTERFACE status_t feedTSPacket(TSParser *mTSParser,
    const cdx_uint8 *data, size_t size);
status_t parseTS(TSParser *mTSParser, CdxBitReaderT *br);
void parseAdaptationField(TSParser *mTSParser,
    CdxBitReaderT *br, unsigned PID);
status_t parsePID(TSParser *mTSParser, CdxBitReaderT *br, unsigned pid,
    unsigned continuityCounter, unsigned payloadUnitStartIndicator);

CDX_INTERFACE status_t PSISectionAppend(PSISection *section,
    const cdx_uint8 *data, cdx_uint32 size);
CDX_INTERFACE cdx_bool PSISectionIsComplete(PSISection *section);
CDX_INTERFACE cdx_bool PSISectionIsEmpty(PSISection *section);
CDX_INTERFACE const cdx_uint8 *PSISectionData(PSISection *section);
CDX_INTERFACE size_t PSISectionSize(PSISection *section);
CDX_INTERFACE void PSISectionClear(PSISection *section);
CDX_INTERFACE void DestroyPSISection(PSISection *section);
CDX_INTERFACE void DestroyPSISections(TSParser *mTSParser);
CDX_INTERFACE void DestroyStream(Stream *mStream);
CDX_INTERFACE void DestroyStreams(Program *mProgram);
CDX_INTERFACE void DestroyProgram(Program *mProgram);
CDX_INTERFACE void DestroyPrograms(TSParser *mTSParser);
void updatePCR(TSParser *TSParser, cdx_uint64 PCR, size_t byteOffsetFromStart);
PSISection *findPSISectionByPID(TSParser *mTSParser, unsigned PID);
Program *findProgramByPID(TSParser *mTSParser, cdx_int32 PID);
Stream *findStreamByPID(Program *mProgram, unsigned PID);
Stream *findStreamByMediaType(Program *mProgram, MediaType mMediaType);
cdx_bool ProgramParsePID(Program *mProgram,
    unsigned pid, unsigned continuityCounter,
    unsigned payloadUnitStartIndicator,
    CdxBitReaderT *br, status_t *err);
CDX_INTERFACE status_t StreamAppend(Stream *mStream,
        const cdx_uint8 *data, cdx_uint32 size);
status_t StreamParse(Stream *mStream, unsigned continuityCounter,
        unsigned payloadUnitStartIndicator, CdxBitReaderT *br);
status_t StreamFlush(Stream *mStream);
status_t parsePES(Stream *mStream, CdxBitReaderT *br);
cdx_int32 onPayloadData(Stream *mStream,
        unsigned PTS_DTS_flags, cdx_uint64 PTS, cdx_uint64 DTS,
        const uint8_t *data, size_t size);
int64_t convertPTSToTimestamp(Program *mProgram, cdx_uint64 PTS);
status_t parseProgramMap(Program *program, CdxBitReaderT *br);
void parseProgramAssociationTable(TSParser *mTSParser, CdxBitReaderT *br);
static cdx_err SetProgram(TSParser *mTSParser,
    cdx_int32 programNumber, cdx_int32 programMapPID);
static cdx_err SetStream(Program *program, unsigned elementaryPID,
    cdx_int32 streamType/*, cdx_int32 PCR_PID*/);
static cdx_err SetPSISection(TSParser *mTSParser, unsigned PID);
static cdx_int32 SeemToBeMvc(Program* mProgram);
cdx_int32 GetAACDuration(const cdx_uint8 *data, cdx_int32 datalen);
cdx_int32 ProbeStream(Stream *stream);
cdx_int32 GetEs(TSParser *mTSParser, CdxPacketT *cdx_pkt);
cdx_int32 TSControl(CdxParserT *parser, cdx_int32 cmd, void *param);
cdx_int32 TSForceStop(CdxParserT *parser);
cdx_uint32 probe_input_format(cdx_char *buf, cdx_int32 len);

static void PrintTsStatus(TSParser *mTSParser)
{
    Program *program = NULL;
    Stream *stream = NULL;
    CDX_LOGD("mProgramCount = %u, "
            "mStreamCount = %u",
            mTSParser->mProgramCount,
            mTSParser->mStreamCount);
    CdxListForEachEntry(program, &mTSParser->mPrograms, node)
    {
        CDX_LOGD("mProgramNumber = 0x%04x, "
                "mProgramMapPID = 0x%04x, "
                "mStreamCount = %u, "
                "audioCount = %u, "
                "videoCount = %u, "
                "subsCount = %u, "
                "metaCount = %u, "
                "program = %p ",
                program->mProgramNumber,
                program->mProgramMapPID,
                program->mStreamCount,
                program->audioCount,
                program->videoCount,
                program->subsCount,
                program->metaCount,
                program);
        CdxListForEachEntry(stream, &program->mStreams, node)
        {
            CDX_LOGD("mElementaryPID = 0x%04x, "
                    "mStreamType = 0x%04x, "
                    "mMediaType = %u, "
                    "codec_id = 0x%x ",
                    stream->mElementaryPID,
                    stream->mStreamType,
                    stream->mMediaType,
                    stream->codec_id );

        }
    }
}


CDX_INTERFACE status_t PSISectionAppend(PSISection *section,
    const cdx_uint8 *data,cdx_uint32 size)
{
    CDX_CHECK(section);
    CdxBufferT *mBuffer = section->mBuffer;
    CdxBufferAppend(mBuffer, data, size);
    return OK;
}

CDX_INTERFACE cdx_bool PSISectionIsComplete(PSISection *section)
{
    CDX_CHECK(section);
    CdxBufferT *mBuffer = section->mBuffer;
    cdx_uint32 size;
    if (mBuffer == NULL || (size = CdxBufferGetSize(mBuffer)) < 3)
    {
        return CDX_FALSE;
    }

    unsigned sectionLength = U16_AT(CdxBufferGetData(mBuffer) + 1) & 0xfff;
    CDX_CHECK((sectionLength & 0xc00) == 0);
    return size >= sectionLength + 3;
}

CDX_INTERFACE cdx_bool PSISectionIsEmpty(PSISection *section)
{
    CDX_CHECK(section);
    CdxBufferT *mBuffer = section->mBuffer;
    return mBuffer == NULL || CdxBufferGetSize(mBuffer) == 0;
}

CDX_INTERFACE const cdx_uint8 *PSISectionData(PSISection *section)
{
    CDX_CHECK(section);
    CdxBufferT *mBuffer = section->mBuffer;
    return mBuffer == NULL ? NULL : CdxBufferGetData(mBuffer);
}

CDX_INTERFACE size_t PSISectionSize(PSISection *section)
{
    CDX_CHECK(section);
    CdxBufferT *mBuffer = section->mBuffer;
    return mBuffer == NULL ? 0 : CdxBufferGetSize(mBuffer);
}


CDX_INTERFACE void PSISectionClear(PSISection *section)
{
    CDX_CHECK(section);
    CdxBufferT *mBuffer = section->mBuffer;
    if (mBuffer != NULL)
    {
        CdxBufferSetRange(mBuffer, 0, 0);
    }
    section->mPayloadStarted = CDX_FALSE;
}

CDX_INTERFACE void DestroyPSISection(PSISection *section)
{
    CDX_CHECK(section);
    CdxListDel(&section->node);
    CdxBufferDestroy(section->mBuffer);
    CdxFree(section);
}

CDX_INTERFACE void DestroyPSISections(TSParser *mTSParser)
{
    PSISection *posPSI, *nextPSI;

    CdxListForEachEntrySafe(posPSI, nextPSI, &mTSParser->mPSISections, node)
    {
        //mTSParser->enablePid[posPSI->PID] = 0;
        DestroyPSISection(posPSI);
    }

}
CDX_INTERFACE void DestroyStream(Stream *mStream)
{
    CDX_CHECK(mStream);
    CdxListDel(&mStream->node);
    CdxBufferDestroy(mStream->pes[0].mBuffer);
    CdxBufferDestroy(mStream->pes[1].mBuffer);
    if(mStream->tmpBuf)
    {
        free(mStream->tmpBuf);
    }
    if(mStream->metadata)
    {
        free(mStream->metadata);
    }
#if (PROBE_STREAM && !DVB_TEST)
    if(mStream->probeBuf)
    {
        free(mStream->probeBuf);
    }
#endif
    Program *program = mStream->mProgram;
    switch(mStream->mMediaType)
    {
        case TYPE_AUDIO:
            program->audioCount--;
            break;
        case TYPE_VIDEO:
            program->videoCount--;
            break;
        case TYPE_SUBS:
            program->subsCount--;
            break;
        case TYPE_META:
            program->metaCount--;
            break;
        default:
            break;
    }
    program->mStreamCount--;
    program->mTSParser->mStreamCount--;
    program->mTSParser->enablePid[mStream->mElementaryPID] = 0;
    CdxFree(mStream);
}

CDX_INTERFACE void DestroyStreams(Program *mProgram)
{
    Stream *posStream, *nextStream;

    CdxListForEachEntrySafe(posStream, nextStream, &mProgram->mStreams, node)
    {
        DestroyStream(posStream);
    }
}

CDX_INTERFACE void DestroyProgram(Program *mProgram)
{
    CDX_CHECK(mProgram);
    //CDX_LOGD("mProgram =%p", mProgram);
    //CDX_LOGD("node =%p", &mProgram->node);
    //CdxListNodeT *node = &mProgram->node;
    //CDX_LOGD("prev =%p", node->prev);
    //CDX_LOGD("next =%p", node->next);
    if(mProgram->mProgramMapPID >= 0)
    {
        mProgram->mTSParser->enablePid[mProgram->mProgramMapPID] = 0;
    }
    CdxListDel(&mProgram->node);
    //CDX_LOGD("CdxListDel");
    DestroyStreams(mProgram);
    mProgram->mTSParser->mProgramCount--;
    CdxFree(mProgram);
}

CDX_INTERFACE void DestroyPrograms(TSParser *mTSParser)
{
    Program *posProgram, *nextProgram;

    CdxListForEachEntrySafe(posProgram, nextProgram, &mTSParser->mPrograms, node)
    {
        DestroyProgram(posProgram);
    }
    mTSParser->curProgram = NULL;
}

CDX_INTERFACE status_t feedTSPacket(TSParser *mTSParser,
    const cdx_uint8 *data, size_t size)
{
    CDX_UNUSE(size);
    CDX_CHECK(size == TS_PACKET_SIZE);
    CdxBitReaderT *br = CdxBitReaderCreate(data, TS_PACKET_SIZE);
    status_t err = parseTS(mTSParser, br);
    ++(mTSParser->mNumTSPacketsParsed);
    CDX_LOGV("mNumTSPacketsParsed = %d", mTSParser->mNumTSPacketsParsed);
    CdxBitReaderDestroy(br);
    return err;
}

#if DVB_USED
static cdx_int32 CdxBitReaderSkipBitsSafe(CdxBitReaderT *br, cdx_uint32 n)
{
    cdx_int32 ret = 0;
    cdx_uint32 nLeftNum = 0;
    nLeftNum = CdxBitReaderNumBitsLeft(br);
    if(nLeftNum < n)
    {
        CDX_LOGE("DTMB SKIP BITS SAFE:Skip Bits unSafe");
        CdxBitReaderSkipBits(br, nLeftNum);
        ret = -1;
    }
    else
    {
        CdxBitReaderSkipBits(br, n);
    }
    return ret;
}

static cdx_int32 checkCounter(TSParser *mTSParser,
    cdx_uint32 PID,cdx_uint32 continuity_counter,
    const cdx_uint8 *data,cdx_uint32 adaptation_field_control)
{
    cdx_int32 i;
    cdx_int32 hasAdaptionField = 0;
    cdx_int32 discontinuity_indicator = 0;

    cdx_int32 ret = 0;

    if(adaptation_field_control == 2  || adaptation_field_control == 3)
        hasAdaptionField = 1;

    discontinuity_indicator = hasAdaptionField && data[4] != 0 && (data[5] & 0x80);

    if(hasAdaptionField)
    {
        if(data[4] == 0)
        {
            CDX_LOGV("ADP LENGTH 0!\n");
        }

        if(data[5] & 0x80)
        {
            CDX_LOGV("DTMB DISCONTINUE!\n");
        }
    }

    for(i=0;i<mTSParser->curCountPos;i++)
    {
        if(mTSParser->mapCounter[i].pid == PID)
        {
            if(adaptation_field_control == 1 || adaptation_field_control == 3)
            {
                if(discontinuity_indicator == 0)
                {
                    if(continuity_counter !=
                        ((mTSParser->mapCounter[i].counter + 1)&0x0f))
                    {
                        if(continuity_counter ==
                            mTSParser->mapCounter[i].counter)
                        {
                            if(memcmp(mTSParser->mapCounter[i].packet,data,188))
                            {
                                //CDX_BUF_DUMP(mTSParser->mapCounter[i].packet,188);
                                //CDX_BUF_DUMP(data,188);
                                CDX_LOGV("same counter:%d pid:%d\n",
                                    continuity_counter,PID);
                            }
                            ret = -2;
                        }
                        else
                        {
                            ret = -1;
                        }
                    }
                }
            }

            memcpy(mTSParser->mapCounter[i].packet,data,188);
            mTSParser->mapCounter[i].counter = continuity_counter;
            break;
        }
    }

    if(i == mTSParser->curCountPos)
    {
        mTSParser->mapCounter[mTSParser->curCountPos].counter = continuity_counter;
        mTSParser->mapCounter[mTSParser->curCountPos].pid = PID;
        memcpy(mTSParser->mapCounter[mTSParser->curCountPos].packet,data,188);
        mTSParser->curCountPos++;
    }

    return 0;

}
#endif

status_t parseTS(TSParser *mTSParser, CdxBitReaderT *br)
{
#if DVB_USED
    const cdx_uint8 *pHeader = CdxBitReaderData(br);
#endif
    unsigned sync_byte = CdxBitReaderGetBits(br, 8);
    CDX_CHECK(sync_byte == 0x47u);
    unsigned transportErrorIndicator = CdxBitReaderGetBits(br, 1);
    if(transportErrorIndicator)
    {
        CDX_LOGW("transportErrorIndicator = %u", transportErrorIndicator);
        return OK;
    }

    unsigned payloadUnitStartIndicator = CdxBitReaderGetBits(br, 1);
    CDX_LOGV("payloadUnitStartIndicator = %u", payloadUnitStartIndicator);

    MY_LOGV("transport_priority = %u", CdxBitReaderGetBits(br, 1));

    unsigned PID = CdxBitReaderGetBits(br, 13);
    CDX_LOGV("PID = 0x%04x", PID);

    if(mTSParser->enablePid[PID] == 0)
    {
        return OK;
    }

    MY_LOGV("transport_scrambling_control = %u", CdxBitReaderGetBits(br, 2));

    unsigned adaptationFieldControl = CdxBitReaderGetBits(br, 2);
    CDX_LOGV("adaptationFieldControl = %u", adaptationFieldControl);

    unsigned continuityCounter = CdxBitReaderGetBits(br, 4);
    CDX_LOGV("PID = 0x%04x, continuityCounter = %u", PID, continuityCounter);

#if DVB_USED
    cdx_int32 checkValue =
    checkCounter(mTSParser,PID,continuityCounter,pHeader,adaptationFieldControl);
    if(checkValue == -2)
    {
        //CDX_LOGW("return because same packet!");
        return OK;
    }
#endif
    if (adaptationFieldControl == 2 || adaptationFieldControl == 3)
    {
#if ParseAdaptationField
        parseAdaptationField(mTSParser, br, PID);
#else
        if(adaptationFieldControl == 2)
        {
            return OK;
        }
        unsigned tmp = CdxBitReaderGetBits(br, 8);
        if(tmp > 182)
        {
            CDX_LOGE("error!! adaptation_field_length(%u)", tmp);
            return OK;
        }
        CdxBitReaderSkipBits(br, tmp << 3);
#endif
    }

    status_t err = OK;

    if (adaptationFieldControl == 1 || adaptationFieldControl == 3)
    {
        err = parsePID(mTSParser,
                br, PID, continuityCounter, payloadUnitStartIndicator);
    }
    return err;
}
void parseAdaptationField(TSParser *mTSParser,
    CdxBitReaderT *br, unsigned PID)
{
    unsigned nAdaptation_field_length = CdxBitReaderGetBits(br, 8);
    (void)PID;

    if (nAdaptation_field_length > 0)
    {
        unsigned bDiscontinuity_indicator = CdxBitReaderGetBits(br, 1);

        if (bDiscontinuity_indicator)
        {
            CDX_LOGV("PID 0x%04x: discontinuity_indicator = 1 (!!!)", PID);
        }

        CdxBitReaderSkipBits(br, 2);
        unsigned PCR_flag = CdxBitReaderGetBits(br, 1);

        size_t numBitsRead = 4;

        if (PCR_flag)
        {
            CdxBitReaderSkipBits(br, 4);
            cdx_uint64 PCR_base = CdxBitReaderGetBits(br, 32);
            PCR_base = (PCR_base << 1) | CdxBitReaderGetBits(br, 1);

            CdxBitReaderSkipBits(br, 6);
            unsigned PCR_ext = CdxBitReaderGetBits(br, 9);

            // The number of bytes from the start of the current
            // MPEG2 transport stream packet up and including
            // the final byte of this PCR_ext field.
            size_t byteOffsetFromStartOfTSPacket =
                (188 - CdxBitReaderNumBitsLeft(br) / 8);

            cdx_uint64 PCR = PCR_base * 300 + PCR_ext;

            //CDX_LOGV("PCR = 0x%016llx (%.2f)",PCR, PCR / 27E6);

            // The number of bytes received by this parser up to and
            // including the final byte of this PCR_ext field.
            size_t byte_offset_from_start =
                mTSParser->mNumTSPacketsParsed * mTSParser->mRawPacketSize
                + byteOffsetFromStartOfTSPacket;

/*
            for (size_t i = 0; i < TSParser->programCount; ++i)
        {
                updatePCR(PID, PCR, byteOffsetFromStart);
            }
*/
            updatePCR(mTSParser, PCR, byte_offset_from_start);

            numBitsRead += 52;
        }

        CDX_CHECK(nAdaptation_field_length * 8 >= numBitsRead);
        if(nAdaptation_field_length * 8 >= numBitsRead)
        {
            CdxBitReaderSkipBits(br, nAdaptation_field_length * 8 - numBitsRead);
        }
        else
        {
            CDX_LOGE("adaptation_field_length * 8 < numBitsRead");
        }
    }
}


void updatePCR(TSParser *TSParser, cdx_uint64 PCR, size_t byteOffsetFromStart)
{
    CDX_LOGV("PCR 0x%016llx @ %d", PCR, byteOffsetFromStart);

    if (TSParser->mNumPCRs == 2)
    {
        TSParser->mPCR[0] = TSParser->mPCR[1];
        TSParser->mPCRBytes[0] = TSParser->mPCRBytes[1];
        TSParser->mSystemTimeUs[0] = TSParser->mSystemTimeUs[1];
        TSParser->mNumPCRs = 1;
    }

    TSParser->mPCR[TSParser->mNumPCRs] = PCR;
    TSParser->mPCRBytes[TSParser->mNumPCRs] = byteOffsetFromStart;
    TSParser->mSystemTimeUs[TSParser->mNumPCRs] = CdxGetNowUs();

    ++(TSParser->mNumPCRs);

    cdx_uint64 delta;

    if (TSParser->mNumPCRs == 2
        && (delta = TSParser->mPCR[1] - TSParser->mPCR[0]) > 0)
    {
        TSParser->dynamicRate =
            (TSParser->mPCRBytes[1] - TSParser->mPCRBytes[0])* 27E6 / delta;
        TSParser->accumulatedDeltaPCR += delta;
        TSParser->overallRate =
            TSParser->mPCRBytes[1]* 27E6 / TSParser->accumulatedDeltaPCR;

        CDX_LOGV("dynamicRate = %llu, overallRate = %llu bytes/sec",
            TSParser->dynamicRate, TSParser->overallRate);
    }
}

PSISection *findPSISectionByPID(TSParser *mTSParser, unsigned PID)
{
    PSISection *item = NULL;
    CdxListForEachEntry(item, &mTSParser->mPSISections, node)
    {
        if (item->PID == PID)
        {
            return item;
        }
    }
    return NULL;
}


Program *findProgramByPID(TSParser *mTSParser, cdx_int32 PID)
{
    Program *mProgram = NULL;
    CdxListForEachEntry(mProgram, &mTSParser->mPrograms, node)
    {
        if (mProgram->mProgramMapPID == PID)
        {
            return mProgram;
        }
    }
    return NULL;
}
Stream *findStreamByPID(Program *mProgram, unsigned PID)
{
    Stream *mStream = NULL;
    CdxListForEachEntry(mStream, &mProgram->mStreams, node)
    {
        if (mStream->mElementaryPID == PID)
        {
            return mStream;
        }
    }
    return NULL;
}
Stream *findStreamByMediaType(Program *mProgram, MediaType mMediaType)
{
    Stream *mStream = NULL;
    CdxListForEachEntry(mStream, &mProgram->mStreams, node)
    {
        if (mStream->mMediaType == mMediaType)
        {
            return mStream;
        }
    }
    return NULL;
}
#if DVB_USED
static cdx_uint32 CdxComputeCrc32(cdx_uint32 initVector, cdx_uint8* pData, cdx_uint32 dataLen)
{
    static const cdx_uint32 crc32Table[] =
    {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
    0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
    0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
    0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039, 0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
    0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
    0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
    0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
    0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
    0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
    0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
    0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
    0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
    0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
    0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
    0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
    0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
    0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
    0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
    0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
    0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
    0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
    0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
    };

    cdx_uint32   i = 0;
    cdx_uint32   j = 0;
    cdx_uint32   crc;

    crc = initVector;

    for (j = 0; j < dataLen;  j++)
    {
        i    = ((crc >> 24) ^ *pData++) & 0xff;
        crc    = (crc << 8) ^ crc32Table[i];
    }
    return crc;
}
#endif

status_t parsePSISection(TSParser *mTSParser, PSISection *section, unsigned pid)
{
    status_t err = OK;

    CdxBitReaderT *sectionBits = CdxBitReaderCreate(PSISectionData(section),
        PSISectionSize(section));
#if DVB_USED
    cdx_uint8 *secBufer = (cdx_uint8* )CdxBitReaderData(sectionBits);
    cdx_uint32 secLen = (cdx_uint32)((((secBufer[1] & 0x0F) << 8)
        | secBufer[2]) + 3);
    cdx_uint32 crcValid = !CdxComputeCrc32(0xffffffff, secBufer, secLen);

    if (crcValid)
    {
        mTSParser->crcValidity[pid] = 100;
    }
    else if (mTSParser->crcValidity[pid] < 10)
    {
        CDX_LOGV("check crc failed, section data may error!");
        mTSParser->crcValidity[pid]++;
    }
    else
    {
        crcValid = 2;
    }

    if (crcValid)
    {
        if(crcValid != 1)
        {
            if(mTSParser->isDvbStream == 1)
            {
                CDX_LOGE("for dtv this case is not allowed,section data may error!");
                PSISectionClear(section);
                err = ERROR_SECTION;
                goto _exit;
            }
            else
            {
                //Todo
            }
        }
        else
        {
            CDX_LOGV("section data may ok!");
        }
    }
#endif
    if (section->PID == 0)
    {
        parseProgramAssociationTable(mTSParser, sectionBits);
        PSISectionClear(section);/*section本身不释放*/
    }
    else
    {
        Program *mProgram = findProgramByPID(mTSParser, section->PID);/*ProgramMapPID*/
        if(mProgram)
        {
            err = parseProgramMap(mProgram, sectionBits);
            PSISectionClear(section);
        }
        else
        {
            DestroyPSISection(section);
        }
    }
#if DVB_USED
_exit:
    CdxBitReaderDestroy(sectionBits);
    return err;
#else
    CdxBitReaderDestroy(sectionBits);
    return err;
#endif
}


status_t parsePID(TSParser *mTSParser, CdxBitReaderT *br, unsigned pid,
    unsigned continuityCounter, unsigned payloadUnitStartIndicator)
{
    status_t mStatus = OK;
    PSISection *section = findPSISectionByPID(mTSParser, pid);
    if(!section)
    {
        if(mTSParser->autoGuess == 1)
        {
            if(pid >= 0x1fc8 && pid <= 0x1fcf)  //* 这个判断一定满足，getmediainfo已经过滤过了
            {
                SetPSISection(mTSParser, pid);
                SetProgram(mTSParser, -1, pid);
                section = findPSISectionByPID(mTSParser, pid);
            }
        }
    }

    if (section)
    {
        if (payloadUnitStartIndicator)
        {
            unsigned skip = CdxBitReaderGetBits(br, 8);/*pointer_field*/
            if(skip > 0)
            {
                if(PSISectionIsEmpty(section))
                {
#if DVB_USED
                    if(CdxBitReaderSkipBitsSafe(br, skip * 8) < 0)
                    {
                        return OK;
                    }
#else
                    CdxBitReaderSkipBits(br, skip * 8);
#endif
                }
                else
                {
                    PSISectionAppend(section, CdxBitReaderData(br), skip);
                    mStatus = parsePSISection(mTSParser, section, pid);
#if DVB_USED
                    if(mStatus == ERROR_SECTION)
                    {
                        CDX_LOGE("Section data error,mStatus = %d", mStatus);
                        return OK;
                    }
#endif
                    CDX_LOGV("err = %d", mStatus);
                    CdxBitReaderSkipBits(br, skip * 8);
                }
            }
            section->mPayloadStarted = CDX_TRUE;
        }

        if (!section->mPayloadStarted)
        {
            return OK;
        }

        CDX_CHECK((CdxBitReaderNumBitsLeft(br) % 8) == 0);
        PSISectionAppend(section, CdxBitReaderData(br),
            CdxBitReaderNumBitsLeft(br) / 8);

        if (!PSISectionIsComplete(section))
        {
            return OK;
        }
        mStatus = parsePSISection(mTSParser, section, pid);
#if DVB_USED
        if(mStatus == ERROR_SECTION)
        {
            CDX_LOGE("Section data error,err = %d", mStatus);
            return OK;
        }
#endif
        return mStatus;
    }

    cdx_bool handled = CDX_FALSE;
    Program *program = NULL;

    CdxListForEachEntry(program, &mTSParser->mPrograms, node)
    {
        if (ProgramParsePID(program, pid, continuityCounter,
            payloadUnitStartIndicator, br, &mStatus))
        {
            if (mStatus != OK)
            {
                return mStatus;
            }
            handled = CDX_TRUE;
            break;
        }
    }

    if (!handled)
    {
        CDX_LOGV("pid 0x%04x not handled.", pid);
    }
    return OK;
}


cdx_bool ProgramParsePID(Program *mProgram,
        unsigned pid, unsigned continuityCounter,
        unsigned payloadUnitStartIndicator,
        CdxBitReaderT *br, status_t *err)
{
    *err = OK;

    Stream *mStream = findStreamByPID(mProgram, pid);
    if(!mStream)
    {
        if(mProgram->mTSParser->autoGuess == 2)
        {
            SetStream(mProgram, pid, -1);
            mStream = findStreamByPID(mProgram, pid);
        }
        else
        {
            return CDX_FALSE;
        }
    }
    *err = StreamParse(mStream, continuityCounter, payloadUnitStartIndicator, br);
    return CDX_TRUE;
}

CDX_INTERFACE status_t StreamAppend(Stream *mStream,
    const cdx_uint8 *data, cdx_uint32 size)
{
    CDX_CHECK(mStream);
    CdxBufferT *mBuffer = mStream->pes[mStream->pesIndex].mBuffer;
    if(mStream->mMediaType == TYPE_VIDEO
        && (mStream->codec_id == VIDEO_CODEC_FORMAT_H264
        || mStream->codec_id == VIDEO_CODEC_FORMAT_H265
        || mStream->codec_id == VIDEO_CODEC_FORMAT_WMV3
        || mStream->codec_id == VIDEO_CODEC_FORMAT_MPEG2)
        && mStream->mProgram->mTSParser->currentES /*== mStream*/
        && mStream->mProgram->mTSParser->autoGuess == -1)
        /*对于H264，如果currentES被置起时，两个buff都被占用*/
    {
        memcpy(mStream->tmpBuf, data, size);
        mStream->tmpDataSize = size;
    }
    else
    {
        if(mStream->tmpDataSize)
        {
            CdxBufferAppend(mBuffer, mStream->tmpBuf, mStream->tmpDataSize);
            mStream->tmpDataSize = 0;
        }
        /*CDX_LOGV("StreamAppend before:%u", CdxBufferGetSize(mBuffer))*/
        CdxBufferAppend(mBuffer, data, size);
    }
    return OK;
}
status_t StreamParse(Stream *mStream, unsigned continuityCounter,
        unsigned payloadUnitStartIndicator, CdxBitReaderT *br)
{
/*
#if 0
    if (mExpectedContinuityCounter >= 0
            && (unsigned)mExpectedContinuityCounter != continuityCounter) {
        ALOGI("discontinuity on stream pid 0x%04x", mElementaryPID);

        mPayloadStarted = CDX_FALSE;
        mBuffer->setRange(0, 0);
        mExpectedContinuityCounter = -1;

#if 0
        // Uncomment this if you'd rather see no corruption whatsoever on
        // screen and suspend updates until we come across another IDR frame.

        if (mStreamType == STREAMTYPE_H264) {
            ALOGI("clearing video queue");
            mQueue->clear(CDX_TRUE);
        }
#endif

        return OK;
    }

#endif
*/
    mStream->mExpectedContinuityCounter = (continuityCounter + 1) & 0x0f; //

    if (payloadUnitStartIndicator)
    {
        if (mStream->mPayloadStarted)
        {
            // Otherwise we run the danger of receiving the trailing bytes
            // of a PES packet that we never saw the start of and assuming
            // we have a a complete PES packet.
  /*
            if(mStream->mProgram->mTSParser->autoGuess == -1)
            {
                mStream->mProgram->mTSParser->pesCount++;
            }
            */

            status_t err = StreamFlush(mStream);

            if (err != OK)
            {
                return err;
            }
        }
        mStream->mPayloadStarted = CDX_TRUE;
    }

    if (mStream->mPayloadStarted == 0)
    {
        return OK;
    }

    size_t nPayloadSizeBits = CdxBitReaderNumBitsLeft(br);
    //CDX_LOGV("nPayloadSizeBits = %u", nPayloadSizeBits);
    CDX_CHECK(nPayloadSizeBits % 8 == 0u);

    StreamAppend(mStream, CdxBitReaderData(br), nPayloadSizeBits / 8);
    return OK;
}


status_t StreamFlush(Stream *mStream)
{
//    CDX_LOGI("StreamFlush");
    CDX_CHECK(mStream);
    CdxBufferT *mBuffer = mStream->pes[mStream->pesIndex].mBuffer;
    cdx_uint32 size = CdxBufferGetSize(mBuffer);
    if (!size)
    {
        if(!mStream->tmpDataSize)
        {
            if(CdxBufferGetSize(mStream->pes[!mStream->pesIndex].mBuffer))
                //用于h264吐出最后一个au
            {
                mStream->accessUnit.pts = mStream->pes[!mStream->pesIndex].pts;
                mStream->accessUnit.data =
                    CdxBufferGetData(mStream->pes[!mStream->pesIndex].mBuffer);
                mStream->accessUnit.size =
                    CdxBufferGetSize(mStream->pes[!mStream->pesIndex].mBuffer);
                mStream->mProgram->mTSParser->currentES = mStream;
                CdxBufferSetRange(mStream->pes[!mStream->pesIndex].mBuffer, 0, 0);
            }
            //CDX_LOGW("flushing stream 0x%04x size = 0", mStream->mElementaryPID);
            return OK;
        }
        else
        {
            CdxBufferAppend(mBuffer, mStream->tmpBuf, mStream->tmpDataSize);
            size = mStream->tmpDataSize;
            mStream->tmpDataSize = 0;
        }
    }

    CDX_LOGV("flushing stream 0x%04x size = %d", mStream->mElementaryPID, size);
    //CdxBufferDump(mBuffer);
    CdxBitReaderT *br = CdxBitReaderCreate(
        (const cdx_uint8 *)CdxBufferGetData(mBuffer), size);

    status_t err = parsePES(mStream, br);
    CdxBitReaderDestroy(br);
    return err;
}

static cdx_int32 VerifyStream(Stream *mStream, unsigned stream_id)
    //类似于new_pes_av_stream
{
    if(mStream->codec_id != 0)
    {
        return 0;
    }
    cdx_int32   codec_type = -1;
    cdx_uint32   codec_id;
    cdx_uint32   codec_sub_id = 0;
    cdx_uint32    codec_meta_id = METADATA_STREAM_ID;
    codec_id   = 0xffffffff;
    //CDX_LOGD("mStream->mStreamType(%d)", mStream->mStreamType);
    switch(mStream->mStreamType)
    {
    case CDX_STREAM_TYPE_AUDIO_MPEG1:
    case CDX_STREAM_TYPE_AUDIO_MPEG2:
        codec_type = TYPE_AUDIO;
        codec_id   = AUDIO_CODEC_FORMAT_MP3;
        break;
    case CDX_STREAM_TYPE_VIDEO_MPEG1:
    case CDX_STREAM_TYPE_VIDEO_MPEG2:
        codec_type = TYPE_VIDEO;
        codec_id   = VIDEO_CODEC_FORMAT_MPEG2;
        break;
    case CDX_STREAM_TYPE_VIDEO_MPEG4:
        codec_type = TYPE_VIDEO;
        codec_id   = VIDEO_CODEC_FORMAT_DIVX4; // we should goto mpeg4Normal decoder
        break;
    case CDX_STREAM_TYPE_OMX_VIDEO_CodingAVC:
        codec_type = TYPE_VIDEO;
        codec_id   = VIDEO_CODEC_FORMAT_H264;
        break;
    case CDX_STREAM_TYPE_VIDEO_MVC:
        codec_type = TYPE_VIDEO;
        codec_id   = VIDEO_CODEC_FORMAT_H264;
        break;
    case CDX_STREAM_TYPE_VIDEO_H265:
        codec_type = TYPE_VIDEO;
        codec_id   = VIDEO_CODEC_FORMAT_H265;
        break;
    case CDX_STREAM_TYPE_VIDEO_SHVC:
        codec_type = TYPE_VIDEO;
        codec_id   = VIDEO_CODEC_FORMAT_H265;
        break;
    case CDX_STREAM_TYPE_VIDEO_VC1:
        codec_type = TYPE_VIDEO;
        codec_id   = VIDEO_CODEC_FORMAT_WMV3;
        break;
    case CDX_STREAM_TYPE_AUDIO_AAC:
    case CDX_STREAM_TYPE_AUDIO_MPEG4:
        codec_type = TYPE_AUDIO;
        codec_id   = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
        break;

    case CDX_STREAM_TYPE_AUDIO_AC3:
    case CDX_STREAM_TYPE_AUDIO_AC3_:
    case CDX_STREAM_TYPE_AUDIO_EAC3:
        codec_type = TYPE_AUDIO;
        codec_id   = AUDIO_CODEC_FORMAT_AC3;
        break;

    case CDX_STREAM_TYPE_UNKOWN:
        CDX_LOGW("========== careful, streamType is 0, maybe  error");
        codec_type = TYPE_AUDIO;
        codec_id   = AUDIO_CODEC_FORMAT_AC3;
        break;

    case CDX_STREAM_TYPE_AUDIO_AC3_TRUEHD:
        //必要时参考原ts
        codec_type = TYPE_AUDIO;
        codec_id   = AUDIO_CODEC_FORMAT_AC3;
        break;

    case CDX_STREAM_TYPE_AUDIO_DTS:
    case CDX_STREAM_TYPE_AUDIO_DTS_:
    case CDX_STREAM_TYPE_AUDIO_HDMV_DTS:
    case CDX_STREAM_TYPE_AUDIO_DTS_HRA:
    case CDX_STREAM_TYPE_AUDIO_DTS_MA:
        codec_type = TYPE_AUDIO;
        codec_id   = AUDIO_CODEC_FORMAT_DTS;
        break;

#if SUPPORT_SUBTITLE_DVB
    case CDX_STREAM_TYPE_SUBTITLE_DVB:
        codec_type = TYPE_SUBS;
        codec_id   = SUBTITLE_CODEC_DVBSUB;
        break;
#endif

    case CDX_STREAM_TYPE_PCM_BLURAY:
        codec_type = TYPE_AUDIO;
        codec_id   = AUDIO_CODEC_FORMAT_PCM;
        break;
    case CDX_STREAM_TYPE_HDMV_PGS_SUBTITLE:
        CDX_LOGV("PGS subtitle");
        codec_type = TYPE_SUBS;
        codec_id   = SUBTITLE_CODEC_PGS;//
        break;
    case CDX_STREAM_TYPE_VIDEO_AVS:
        codec_type = TYPE_VIDEO;
        codec_id   = VIDEO_CODEC_FORMAT_AVS;
        break;
    case CDX_STREAM_TYPE_PRIVATE_DATA:
        if(!mStream->mProgram->videoCount)
        {
            codec_type = TYPE_VIDEO;
            codec_id   = VIDEO_CODEC_FORMAT_H265;
        }
        break;
    case CDX_STREAM_TYPE_METADATA:
        codec_type = TYPE_META;
        codec_id   = METADATA_STREAM_ID;
        break;
    default:
        if(stream_id == 0)
        {
            DestroyStream(mStream);
            return -1;
        }
        else if (stream_id >= 0xc0 && stream_id <= 0xdf)
            //* according to iso13818-1 Table 2-18 "Stream_id assignments", CXC.
        {
            codec_type = TYPE_AUDIO;
            codec_id   = AUDIO_CODEC_FORMAT_MP2;
        }
        else if (stream_id == 0xbd)
        {
            if(SeemToBeMvc(mStream->mProgram))
            {
                codec_type = TYPE_AUDIO;
                codec_id   = AUDIO_CODEC_FORMAT_PCM;
            }
            else
            {
                codec_type = TYPE_AUDIO;
                codec_id   = AUDIO_CODEC_FORMAT_AC3;
            }
        }
        else if(stream_id == 0xfd)      //* stream id for BD
        {
            if(SeemToBeMvc(mStream->mProgram))
            {
                codec_type = TYPE_AUDIO;
                codec_id = AUDIO_CODEC_FORMAT_AC3;
            }
            else
            {
                codec_type = TYPE_VIDEO;
                codec_id   = VIDEO_CODEC_FORMAT_WMV3;
            }
        }
        else if(stream_id >= 0xe0
            && stream_id <= 0xef
            && !mStream->mProgram->videoCount)
            //* according to iso13818-1 Table 2-18 "Stream_id assignments", CXC.
        {
            codec_type = TYPE_VIDEO;
            if(mStream->mStreamType == CDX_STREAM_TYPE_PRIVATE_DATA)
            {
                codec_id   = VIDEO_CODEC_FORMAT_H265;
            }
            else
            {
                codec_id   = VIDEO_CODEC_FORMAT_MPEG2;
            }
        }
        else
        {
            DestroyStream(mStream);
            return -1;
        }
        break;
    }

    mStream->codec_id   = codec_id;
    mStream->codec_sub_id    = codec_sub_id;
    mStream->codec_meta_id   = codec_meta_id;
    mStream->mMediaType = codec_type;
    Program *program = mStream->mProgram;

    switch(mStream->mMediaType)
    {
        case TYPE_AUDIO:
            program->audioCount++;
            break;
        case TYPE_VIDEO:
            program->videoCount++;
            break;
        case TYPE_SUBS:
            program->subsCount++;
            break;
        case TYPE_META:
            program->metaCount++;
            break;
        default:
            break;
    }
    return 0;
}


status_t parsePES(Stream *mStream, CdxBitReaderT *br)
{
    //CDX_LOGD("parsePES");
#if DVB_USED
    cdx_uint32 leftBit = CdxBitReaderNumBitsLeft(br);
    if(leftBit < 8 * TS_PES_START_SIZE)
    {
        CDX_LOGW("payload may too short, len: %d", leftBit);
        goto _exit;
    }
#endif
    unsigned packet_startcode_prefix = CdxBitReaderGetBits(br, 24);
    if(packet_startcode_prefix != 0x000001u)
    {
        CDX_LOGW("PID = 0x%02x maybe not PES", mStream->mElementaryPID);

        //return OK;
        goto _exit;
    }

    unsigned streamId = CdxBitReaderGetBits(br, 8);
    CDX_LOGV("streamId = 0x%02x", streamId);

    unsigned PESPacketLength = CdxBitReaderGetBits(br, 16);
    CDX_LOGV("PESPacketLength = %u", PESPacketLength);

    if (streamId != 0xbc  // program_stream_map
            && streamId != 0xbe  // padding_stream
            && streamId != 0xbf  // private_stream_2
            && streamId != 0xf0  // ECM
            && streamId != 0xf1  // EMM
            && streamId != 0xff  // program_stream_directory
            && streamId != 0xf2  // DSMCC
            && streamId != 0xf8) {  // H.222.1 type E

        if(mStream->mProgram->mTSParser->autoGuess == 2
            && !((streamId >= 0xc0 && streamId <= 0xdf)
            || (streamId >= 0xe0 && streamId <= 0xef)
            || (streamId == 0xbd)
            || (streamId == 0xfd)))
        {
            //return OK;

            goto _exit;
        }
        if(mStream->mProgram->mTSParser->autoGuess != -1
            && VerifyStream(mStream, streamId) < 0)
        {
            //return OK;

            goto _exit;
        }

        unsigned temp = CdxBitReaderGetBits(br, 2);

        if(temp == 2u)
            CDX_FORCE_CHECK(1);
        else
            CDX_FORCE_CHECK(0);

        MY_LOGV("PES_scrambling_control = %u", CdxBitReaderGetBits(br, 2));
        MY_LOGV("PES_priority = %u", CdxBitReaderGetBits(br, 1));
        MY_LOGV("data_alignment_indicator = %u", CdxBitReaderGetBits(br, 1));
        MY_LOGV("copyright = %u", CdxBitReaderGetBits(br, 1));
        MY_LOGV("original_or_copy = %u", CdxBitReaderGetBits(br, 1));

        unsigned PTS_DTS_flags = CdxBitReaderGetBits(br, 2);
        //CDX_LOGV("PTS_DTS_flags = %u", PTS_DTS_flags);

        unsigned ESCR_flag = CdxBitReaderGetBits(br, 1);
        //CDX_LOGV("ESCR_flag = %u", ESCR_flag);

        unsigned ES_rate_flag = CdxBitReaderGetBits(br, 1);
        //CDX_LOGV("ES_rate_flag = %u", ES_rate_flag);

        unsigned DSM_trick_mode_flag = CdxBitReaderGetBits(br, 1);
        //CDX_LOGV("DSM_trick_mode_flag = %u", DSM_trick_mode_flag);

        unsigned additional_copy_info_flag = CdxBitReaderGetBits(br, 1);
        //CDX_LOGV("additional_copy_info_flag = %u", additional_copy_info_flag);

        unsigned PES_CRC_flag = CdxBitReaderGetBits(br, 1);
        //CDX_LOGV("PES_CRC_flag = %u", PES_CRC_flag);

        unsigned PES_extension_flag = CdxBitReaderGetBits(br, 1);
        //CDX_LOGV("PES_extension_flag = %u", PES_extension_flag);

        unsigned PES_header_data_length = CdxBitReaderGetBits(br, 8);
        //CDX_LOGV("PES_header_data_length = %u", PES_header_data_length);

        unsigned optionalBytesRemaining = PES_header_data_length;

        cdx_uint64 PTS = 0, DTS = 0;

        if (PTS_DTS_flags == 3 || PTS_DTS_flags == 2)
        {
            CDX_CHECK(optionalBytesRemaining >= 5u);
            if(optionalBytesRemaining < 5u)
            {
                CDX_LOGE("optional_bytes_remaining(%u) < 5u",
                    optionalBytesRemaining);
                goto _exit;
            }
#if PtsDebug
            const cdx_uint8 *tmp = CdxBitReaderData(br);
            CDX_BUF_DUMP(tmp, 5);
#endif

            //CDX_FORCE_CHECK(CdxBitReaderGetBits(br, 4) == PTS_DTS_flags);
            CdxBitReaderGetBits(br, 4);

            PTS = ((cdx_uint64)CdxBitReaderGetBits(br, 3)) << 30;

            temp = CdxBitReaderGetBits(br, 1);
            if(temp == 1u)
                CDX_FORCE_CHECK(1);
            else
                CDX_FORCE_CHECK(0);
            PTS |= ((cdx_uint64)CdxBitReaderGetBits(br, 15)) << 15;

            temp = CdxBitReaderGetBits(br, 1);
            if(temp == 1u)
                CDX_FORCE_CHECK(1);
            else
                CDX_FORCE_CHECK(0);
            PTS |= CdxBitReaderGetBits(br, 15);

            temp = CdxBitReaderGetBits(br, 1);
            if(temp == 1u)
                CDX_FORCE_CHECK(1);
            else
                CDX_FORCE_CHECK(0);
#if PtsDebug
            CDX_LOGD("PTS = 0x%016llx (%.2f)", PTS, PTS / 90000.0);
#endif
            optionalBytesRemaining -= 5;

            if (PTS_DTS_flags == 3)
            {
                CDX_CHECK(optionalBytesRemaining >= 5u);
                if(optionalBytesRemaining < 5u)
                {
                    CDX_LOGE("optional_bytes_remaining(%u) < 5u",
                        optionalBytesRemaining);
                    goto _exit;
                }
#if PtsDebug
                const cdx_uint8 *tmp = CdxBitReaderData(br);
                CDX_BUF_DUMP(tmp, 5);
#endif
                //CDX_FORCE_CHECK(CdxBitReaderGetBits(br, 4) == 1u);
                CdxBitReaderGetBits(br, 4);

                DTS = ((cdx_uint64)CdxBitReaderGetBits(br, 3)) << 30;

                temp = CdxBitReaderGetBits(br, 1);
                if(temp == 1u)
                    CDX_FORCE_CHECK(1);
                else
                    CDX_FORCE_CHECK(0);
                DTS |= ((cdx_uint64)CdxBitReaderGetBits(br, 15)) << 15;

                temp = CdxBitReaderGetBits(br, 1);
                if(temp == 1u)
                    CDX_FORCE_CHECK(1);
                else
                    CDX_FORCE_CHECK(0);
                DTS |= CdxBitReaderGetBits(br, 15);

                temp = CdxBitReaderGetBits(br, 1);
                if(temp == 1u)
                    CDX_FORCE_CHECK(1);
                else
                    CDX_FORCE_CHECK(0);
#if PtsDebug
                CDX_LOGD("DTS = 0x%016llx (%.2f)", DTS, DTS / 90000.0);
#endif
                optionalBytesRemaining -= 5;
            }
        }

        if (ESCR_flag)
        {
            CDX_CHECK(optionalBytesRemaining >= 6u);

            if(optionalBytesRemaining < 6u)
            {
                CDX_LOGE("optional_bytes_remaining(%u) < 6u",
                    optionalBytesRemaining);
                goto _exit;
            }

            CdxBitReaderGetBits(br, 2);

            cdx_uint64 ESCR = ((cdx_uint64)CdxBitReaderGetBits(br, 3)) << 30;

            temp = CdxBitReaderGetBits(br, 1);
            if(temp == 1u)
                CDX_FORCE_CHECK(1);
            else
                CDX_FORCE_CHECK(0);
            ESCR |= ((cdx_uint64)CdxBitReaderGetBits(br, 15)) << 15;

            temp = CdxBitReaderGetBits(br, 1);
            if(temp == 1u)
                CDX_FORCE_CHECK(1);
            else
                CDX_FORCE_CHECK(0);
            ESCR |= CdxBitReaderGetBits(br, 15);

            temp = CdxBitReaderGetBits(br, 1);
            if(temp == 1u)
                CDX_FORCE_CHECK(1);
            else
                CDX_FORCE_CHECK(0);

            CDX_LOGV("ESCR = %llu", ESCR);
            MY_LOGV("ESCR_extension = %u", CdxBitReaderGetBits(br, 9));

            temp = CdxBitReaderGetBits(br, 1);
            if(temp == 1u)
                CDX_FORCE_CHECK(1);
            else
                CDX_FORCE_CHECK(0);

            optionalBytesRemaining -= 6;
        }

        if (ES_rate_flag) {
            CDX_CHECK(optionalBytesRemaining >= 3u);

            if(optionalBytesRemaining < 3u)
            {
                CDX_LOGE("optional_bytes_remaining(%u) < 3u",
                    optionalBytesRemaining);
                goto _exit;
            }

            temp = CdxBitReaderGetBits(br, 1);
            if(temp == 1u)
                CDX_FORCE_CHECK(1);
            else
                CDX_FORCE_CHECK(0);
            MY_LOGV("ES_rate = %u", CdxBitReaderGetBits(br, 22));

            temp = CdxBitReaderGetBits(br, 1);
            if(temp == 1u)
                CDX_FORCE_CHECK(1);
            else
                CDX_FORCE_CHECK(0);

            optionalBytesRemaining -= 3;
        }

        if (DSM_trick_mode_flag) {
#if DVB_USED
            if(optionalBytesRemaining < 1u)
            {
                CDX_LOGE("optionalBytesRemaining(%u) < 1u",
                    optionalBytesRemaining);
                goto _exit;
            }
#endif
            CdxBitReaderGetBits(br, 8);
            optionalBytesRemaining--;
        }
        if (additional_copy_info_flag) {
#if DVB_USED
            if(optionalBytesRemaining < 1u)
            {
                CDX_LOGE("optionalBytesRemaining(%u) < 1u",
                    optionalBytesRemaining);
                goto _exit;
            }
#endif
            CdxBitReaderGetBits(br, 8);
            optionalBytesRemaining--;
        }
        if (PES_CRC_flag) {
#if DVB_USED
            if(optionalBytesRemaining < 2u)
            {
                CDX_LOGE("optionalBytesRemaining(%u) < 21u",
                    optionalBytesRemaining);
                goto _exit;
            }
#endif
            CdxBitReaderGetBits(br, 16);
            optionalBytesRemaining -= 2;
        }
        if (PES_extension_flag) {
#if DVB_USED
            if(optionalBytesRemaining < 1u)
            {
                CDX_LOGE("optionalBytesRemaining(%u) < 1u",
                    optionalBytesRemaining);
                goto _exit;
            }
#endif
            unsigned flags = CdxBitReaderGetBits(br, 8);
            optionalBytesRemaining -= 1;

            if(flags & 0x80)
            {
                //CDX_LOGD("************ encrypted");
                //for miracast, if pes packet contains private data, ie, this
                //packet is encrypted.
                if(mStream->mProgram->mTSParser->miracast)
                {
                    memcpy(mStream->privateData, CdxBitReaderData(br),
                        PES_PRIVATE_DATA_SIZE);
                    mStream->hdcpEncrypted = 1;
                }
            }
        }
#if DVB_USED
        if(CdxBitReaderSkipBitsSafe(br, optionalBytesRemaining * 8) < 0)
        {
            goto _exit;
        }
#else
        CdxBitReaderSkipBits(br, optionalBytesRemaining * 8);
#endif
        //CDX_LOGD("mStream = %p, mStream->counter =%d", mStream, mStream->counter);
        // ES data follows.
        if (PESPacketLength != 0)
        {
            CDX_CHECK(PESPacketLength >= PES_header_data_length + 3);

            if(PESPacketLength < PES_header_data_length + 3)
            {
                CDX_LOGE("PES_packet_length < PES_header_data_length + 3");
                goto _exit;
            }

            unsigned dataLength =
                PESPacketLength - 3 - PES_header_data_length;
            if (CdxBitReaderNumBitsLeft(br) < dataLength * 8)
            {
                CDX_LOGE("PES packet does not carry enough data.");
               dataLength = CdxBitReaderNumBitsLeft(br) / 8;
                //return ERROR_MALFORMED;
            }
            //CDX_LOG_CHECK(dataLength <= CdxBitReaderNumBitsLeft(br) / 8,
             //   "%u ,%u", dataLength, CdxBitReaderNumBitsLeft(br));

    /*没有把    onPayloadData放在if中是因为在autoGuess时期，需要获取pts以得到duration    */
            onPayloadData(mStream,
                    PTS_DTS_flags, PTS, DTS, CdxBitReaderData(br), dataLength);
            //CDX_BUF_DUMP(CdxBitReaderData(br), dataLength);
            if(mStream->mStreamType == 0)
            {
                 cdx_uint32 ret =
                     probe_input_format((cdx_char*)CdxBitReaderData(br), dataLength);
                switch(ret)
                {
                    case AUDIO_CODEC_FORMAT_DTS:
                        mStream->mStreamType = CDX_STREAM_TYPE_AUDIO_DTS;
                        mStream->mMediaType = TYPE_AUDIO;
                        mStream->codec_id   = AUDIO_CODEC_FORMAT_DTS;
                        break;
                    case AUDIO_CODEC_FORMAT_AC3:
                        mStream->mStreamType = CDX_STREAM_TYPE_AUDIO_AC3;
                        mStream->mMediaType = TYPE_AUDIO;
                        mStream->codec_id   = AUDIO_CODEC_FORMAT_AC3;
                        break;
                    default:
                        CDX_LOGW("unknown stream format");
                        break;
                }
            }
            //CDX_LOGI("onPayloadData end");
        }
        else
        {
            size_t payloadSizeBits = CdxBitReaderNumBitsLeft(br);
            CDX_CHECK(payloadSizeBits % 8 == 0u);
            onPayloadData(mStream,
                    PTS_DTS_flags, PTS, DTS,
                    CdxBitReaderData(br), payloadSizeBits >> 3);
            //CDX_BUF_DUMP(CdxBitReaderData(br), payloadSizeBits >> 3);
            if(mStream->mStreamType == 0)
            {
                 cdx_uint32 ret =
                     probe_input_format((cdx_char*)CdxBitReaderData(br), payloadSizeBits >> 3);
                switch(ret)
                {
                    case AUDIO_CODEC_FORMAT_DTS:
                        mStream->mStreamType = CDX_STREAM_TYPE_AUDIO_DTS;
                        mStream->mMediaType = TYPE_AUDIO;
                        mStream->codec_id   = AUDIO_CODEC_FORMAT_DTS;
                        break;
                    case AUDIO_CODEC_FORMAT_AC3:
                        mStream->mStreamType = CDX_STREAM_TYPE_AUDIO_AC3;
                        mStream->mMediaType = TYPE_AUDIO;
                        mStream->codec_id   = AUDIO_CODEC_FORMAT_AC3;
                        break;
                    default:
                        CDX_LOGW("unknown stream format");
                        break;
                }
            }
            //CDX_LOGI("onPayloadData end");
        }
        return OK;

    }
    /*
    else
    {
        CDX_CHECK(PES_packet_length != 0u);
        CdxBitReaderSkipBits(br, PES_packet_length * 8);
        goto _exit;
    }
    */
_exit:

    CdxBufferSetRange(mStream->pes[mStream->pesIndex].mBuffer, 0, 0);
    //mStream->pesIndex = !mStream->pesIndex;
    return OK;

}

const uint8_t *findStartCodePrefix(const uint8_t *data, size_t size)
{
    const uint8_t *dataEnd = data + size;
    register cdx_int32 count = 0;
    for(; data < dataEnd; data++)
    {
        if(*data == 0)
        {
            count++;
        }
        else if(*data == 1 && count >= 2)
        {
            data -= count > 2 ? 3 : 2;
            return data;
        }
        else
        {
            count = 0;
        }
    }
    return NULL;
}

cdx_int32 onPayloadData(Stream *mStream,
        unsigned PTS_DTS_flags, cdx_uint64 PTS, cdx_uint64 DTS,
        const uint8_t *data, size_t size)
{
    (void)DTS;
    mStream->pes[mStream->pesIndex].pts = -1;
    if (PTS_DTS_flags == 2 || PTS_DTS_flags == 3)
    {
        mStream->pes[mStream->pesIndex].pts = PTS;
        if(!mStream->hasFirstPTS)
        {
            mStream->hasFirstPTS = 1;
            mStream->firstPTS = PTS;
        }
        if(mStream->mProgram->mTSParser->autoGuess != 2
            && mStream->mProgram->mFirstPTS == -1)
        {
            mStream->mProgram->mFirstPTS = PTS;
        }
    }
    if(mStream->mProgram->mTSParser->autoGuess != -1)
    {
        mStream->mProgram->mTSParser->currentES = mStream;
        CdxBufferSetRange(mStream->pes[mStream->pesIndex].mBuffer, 0, 0);
        mStream->pesIndex = !mStream->pesIndex;
        return 0;
    }
    if(mStream->hdcpEncrypted)
    {
        TSParser *mTSParser = mStream->mProgram->mTSParser;

        if(!mTSParser->hdcpHandle)
        {
            if(mTSParser->hdcpOps)
            {
                mTSParser->hdcpOps->init(&mTSParser->hdcpHandle);
            }
            else
            {
                CDX_LOGW("mTSParser->hdcpOps == NULL");
                mTSParser->currentES = mStream;
                CdxBufferSetRange(mStream->pes[mStream->pesIndex].mBuffer, 0, 0);
                mStream->pesIndex = !mStream->pesIndex;
                return 0;
            }

        }
        cdx_int32 stream_type;
        if(mStream->mMediaType == TYPE_VIDEO)
        {
            stream_type = HDCP_STREAM_TYPE_VIDEO;
        }
        else if(mStream->mMediaType == TYPE_AUDIO)
        {
            stream_type = HDCP_STREAM_TYPE_AUDIO;
        }
        else
        {
            CDX_LOGE(" should not be here !");
            return -1;
        }
        cdx_uint32 ret = mTSParser->hdcpOps->decrypt(mTSParser->hdcpHandle,
            mStream->privateData, (uint8_t *)data, (uint8_t *)data, size, stream_type);
        if(ret)
        {
            CDX_LOGE("HDCP_Decrypt error(%u)", ret);
            return -1;
        }
    }
    mStream->counter++;
    CDX_LOGV("onPayloadData mMediaType=%d, counter =%d",
        mStream->mMediaType, mStream->counter);
    CdxBufferSetRange(mStream->pes[mStream->pesIndex].mBuffer,
        data - CdxBufferGetBase(mStream->pes[mStream->pesIndex].mBuffer), size);
    //mStream->pes[mStream->pesIndex].ESdata = data;
    //mStream->pes[mStream->pesIndex].size = size;
    if(mStream->mMediaType == TYPE_VIDEO
        && (mStream->codec_id == VIDEO_CODEC_FORMAT_H264
        || mStream->codec_id == VIDEO_CODEC_FORMAT_H265
        || mStream->codec_id == VIDEO_CODEC_FORMAT_WMV3
        || mStream->codec_id == VIDEO_CODEC_FORMAT_MPEG2))
    {
        //mStream->accessUnit[mStream->accessUnitIndex].tail = mStream->curPES;
        mStream->pes[mStream->pesIndex].AUdata = findStartCodePrefix(data, size);
        if(mStream->pes[mStream->pesIndex].AUdata)
        {

            cdx_int32 AUsize = mStream->pes[mStream->pesIndex].AUdata -
                CdxBufferGetData(mStream->pes[mStream->pesIndex].mBuffer);
            if(mStream->accessUnitStarted)
            {
                if(AUsize)
                {
                    CdxBufferAppend(mStream->pes[!mStream->pesIndex].mBuffer,
                        CdxBufferGetData(mStream->pes[mStream->pesIndex].mBuffer), AUsize);
                    //mStream->accessUnit.size += size;
                }

                mStream->accessUnit.pts = mStream->pes[!mStream->pesIndex].pts;
                mStream->accessUnit.data =
                    CdxBufferGetData(mStream->pes[!mStream->pesIndex].mBuffer);
                mStream->accessUnit.size =
                    CdxBufferGetSize(mStream->pes[!mStream->pesIndex].mBuffer);
                mStream->mProgram->mTSParser->currentES = mStream;
                CdxBufferSetRange(mStream->pes[!mStream->pesIndex].mBuffer, 0, 0);

            }
            else
            {
                mStream->accessUnitStarted = CDX_TRUE;
            }
            CdxBufferSetRange(mStream->pes[mStream->pesIndex].mBuffer,
            mStream->pes[mStream->pesIndex].AUdata -
            CdxBufferGetBase(mStream->pes[mStream->pesIndex].mBuffer),
            CdxBufferGetSize(mStream->pes[mStream->pesIndex].mBuffer) - AUsize);
            mStream->pesIndex = !mStream->pesIndex;

        }
        else
        {
            if(mStream->accessUnitStarted)
            {
                CdxBufferAppend(mStream->pes[!mStream->pesIndex].mBuffer, data, size);
                if(mStream->pes[!mStream->pesIndex].pts == -1)
                {
                    mStream->pes[!mStream->pesIndex].pts =
                        mStream->pes[mStream->pesIndex].pts;
                }

            }
            CdxBufferSetRange(mStream->pes[mStream->pesIndex].mBuffer, 0, 0);
        }
    }
    else
    {
        mStream->accessUnit.data = data;
        mStream->accessUnit.size = size;
        mStream->accessUnit.pts = mStream->pes[mStream->pesIndex].pts;
        mStream->mProgram->mTSParser->currentES = mStream;
        CdxBufferSetRange(mStream->pes[mStream->pesIndex].mBuffer, 0, 0);
        mStream->pesIndex = !mStream->pesIndex;
    }
    return 0;
}

#if 0
int64_t convertPTSToTimestamp(Program *mProgram, cdx_uint64 PTS)
{
    if(!mProgram->mFirstPTSValid)
    {
        mProgram->mFirstPTSValid = CDX_TRUE;
        mProgram->mFirstPTS = PTS;
        PTS = 0;
    }
    else if(PTS < mProgram->mFirstPTS)
    {
        ALOGW("PTS < mFirstPTS");
        PTS = 0;
    }
    else
    {
        PTS -= mProgram->mFirstPTS;
    }

    int64_t timeUs = (PTS * 100) / 9;

    return timeUs;
}

typedef struct StreamInfo
{
    unsigned mType;
    unsigned mPID;
}StreamInfo;
#endif

#define MKTAG(a,b,c,d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#define MKBETAG(a,b,c,d) ((d) | ((c) << 8) | ((b) << 16) | ((unsigned)(a) << 24))

cdx_int32 parseDescriptor(CdxBitReaderT *br, void *father/*, cdx_int32 fatherType*/)
{
    unsigned descriptor_tag = CdxBitReaderGetBits(br, 8);
    unsigned descriptor_length = CdxBitReaderGetBits(br, 8);
    //CDX_LOGD("tag: 0x%02x len=%u", descriptor_tag, descriptor_length);
    //CDX_BUF_DUMP(CdxBitReaderData(br), descriptor_length);
    Stream *stream = NULL;
    Program *program = NULL;
    unsigned format_identifier;
    unsigned metadata_application_format = 0;
    unsigned metadata_application_format_identifier= 0;
    unsigned metadata_format = 0;
    unsigned metadata_format_identifier = 0;
    unsigned metadata_service_id = 0;
    unsigned reserved = 0;
    cdx_int32 ret = 0;
    /*
    if(fatherType == 0)
    {
        stream = (Stream *)father;
    }
    else if(fatherType == 1)
    {
        program = (Program *)father;
    }
    */
    switch(descriptor_tag)
    {
        case 0x0a:/*ISO_639_language_descriptor*/
            stream = (Stream *)father;
            stream->lang[0] = CdxBitReaderGetBits(br, 8);
            stream->lang[1] = CdxBitReaderGetBits(br, 8);
            stream->lang[2] = CdxBitReaderGetBits(br, 8);
            stream->lang[3] = 0;
            descriptor_length -= 3;
            break;
        case 0x56: /* DVB teletext descriptor */
            stream = (Stream *)father;
            stream->mMediaType = TYPE_SUBS;
            stream->lang[0] = CdxBitReaderGetBits(br, 8);
            stream->lang[1] = CdxBitReaderGetBits(br, 8);
            stream->lang[2] = CdxBitReaderGetBits(br, 8);
            stream->lang[3] = 0;
            descriptor_length -= 3;
            break;

#if SUPPORT_SUBTITLE_DVB
        case 0x59:/*DVB subtitle*/
            stream = (Stream *)father;
            stream->mStreamType = CDX_STREAM_TYPE_SUBTITLE_DVB;
            stream->mMediaType = TYPE_SUBS;
            stream->lang[0] = CdxBitReaderGetBits(br, 8);
            stream->lang[1] = CdxBitReaderGetBits(br, 8);
            stream->lang[2] = CdxBitReaderGetBits(br, 8);
            stream->lang[3] = 0;

            CdxBitReaderGetBits(br, 8);
            //unsigned comp_page =
            CdxBitReaderGetBits(br, 16);
            //unsigned anc_page  =
            CdxBitReaderGetBits(br, 16);
            descriptor_length -= 8;

            break;
#endif

        case 0x81: /* AC-3 audio descriptor */
            stream = (Stream *)father;
            if(stream->mStreamType != CDX_STREAM_TYPE_AUDIO_AC3 &&
               stream->mStreamType != CDX_STREAM_TYPE_AUDIO_EAC3 &&
               stream->mStreamType != CDX_STREAM_TYPE_AUDIO_AC3_ &&
               stream->mStreamType != CDX_STREAM_TYPE_AUDIO_AC3_TRUEHD)
                break;

            stream->mStreamType = CDX_STREAM_TYPE_AUDIO_AC3;
            stream->mMediaType = TYPE_AUDIO;
            if(stream->metadata)
            {
                free(stream->metadata);
            }
            stream->metadata = (AudioMetaData *)CdxMalloc(sizeof(AudioMetaData));
            memset(stream->metadata, 0x00, sizeof(AudioMetaData));
            AudioMetaData *metadata = (AudioMetaData *)stream->metadata;
            unsigned sample_rate_code = CdxBitReaderGetBits(br, 3);
            if(sample_rate_code < 4)
            {
                metadata->samplingFrequency = AC3_SamplingRate[sample_rate_code];
            }
            else
            {
                CDX_LOGW("ac3 sample_rate_code >= 4");
            }
            CdxBitReaderGetBits(br, 5);
            unsigned bit_rate_code = CdxBitReaderGetBits(br, 6);
            if((bit_rate_code & 0x1f) <= 18)
            {
                if(bit_rate_code >> 5)
                {
                    unsigned index = bit_rate_code & 0x1f;
                    CDX_LOGW("ac3 bit_rate_code=%u index=%u", bit_rate_code, index);
                    metadata->maxBitRate = AC3_BitRate[index]*1000;
                }
                else
                {
                    metadata->bitRate = AC3_BitRate[bit_rate_code]*1000;
                }
            }
            else
            {
                CDX_LOGW("ac3 bit_rate_code=%u", bit_rate_code);
            }
            CdxBitReaderGetBits(br, 5);
            unsigned num_channels = CdxBitReaderGetBits(br, 4);
            if(num_channels < 8)
            {
                metadata->channelNum = AC3_Channels[num_channels];
            }
            else
            {
                CDX_LOGW("ac3 num_channels = %u", num_channels);
            }
            CdxBitReaderGetBits(br, 1);
            descriptor_length -= 3;
            break;

        case 0x05:
            format_identifier = CdxBitReaderGetBits(br, 32);
            descriptor_length -= 4;
            if(format_identifier == MKBETAG('H', 'D', 'M', 'V'))
            {
                if(!descriptor_length)
                {
                    program = (Program *)father;
                    program->format = 1;
                }
                else
                {
                    stream = (Stream *)father;
                    CdxBitReaderGetBits(br, 8);
                    unsigned stream_coding_type = CdxBitReaderGetBits(br, 8);
                    descriptor_length -= 2;
                    switch(stream_coding_type)
                    {
                        case 0x80:
                            stream->mStreamType = CDX_STREAM_TYPE_PCM_BLURAY;
                            stream->mMediaType = TYPE_AUDIO;
                            if(stream->metadata)
                            {
                                free(stream->metadata);
                            }
                            stream->metadata =
                                (AudioMetaData *)CdxMalloc(sizeof(AudioMetaData));
                            memset(stream->metadata, 0x00, sizeof(AudioMetaData));
                            AudioMetaData *metadata =
                                (AudioMetaData *)stream->metadata;
                            unsigned audio_presentation_type =
                                CdxBitReaderGetBits(br, 4);
                            switch(audio_presentation_type)
                            {
                                case 1:
                                    metadata->channelNum = 1;
                                    break;
                                case 3:
                                    metadata->channelNum = 2;
                                    break;
                                case 6:
                                    metadata->channelNum = 6;
                                    break;
                            }
                            unsigned sampling_frequency =
                                CdxBitReaderGetBits(br, 4);
                            if(sampling_frequency == 1)
                            {
                                metadata->samplingFrequency = 48000;
                            }
                            else if(sampling_frequency == 4)
                            {
                                metadata->samplingFrequency = 96000;
                            }
                            else if(sampling_frequency == 5)
                            {
                                metadata->samplingFrequency = 192000;
                            }
                            unsigned bit_per_sample =
                                CdxBitReaderGetBits(br, 2);
                            switch(bit_per_sample)
                            {
                                case 1:
                                    metadata->bitPerSample = 16;
                                    break;
                                case 2:
                                    metadata->bitPerSample = 20;
                                    break;
                                case 3:
                                    metadata->bitPerSample = 24;
                                    break;
                            }
                            CdxBitReaderGetBits(br, 6);
                            descriptor_length -= 2;
                            break;
                        case 0x02:
                        case 0x1B:
                        case 0x20:
                            stream->mStreamType = stream_coding_type;
                            stream->mMediaType = TYPE_VIDEO;
                            if(stream->metadata)
                            {
                                free(stream->metadata);
                            }
                            stream->metadata =
                                (VideoMetaData *)CdxMalloc(sizeof(VideoMetaData));
                            memset(stream->metadata, 0x00, sizeof(VideoMetaData));
                            VideoMetaData *metadata1 =
                                (VideoMetaData *)stream->metadata;
                            metadata1->videoFormat = CdxBitReaderGetBits(br, 4);
                            metadata1->frameRate = CdxBitReaderGetBits(br, 4);
                            metadata1->aspectRatio = CdxBitReaderGetBits(br, 4);
                            CdxBitReaderGetBits(br, 4);
                            descriptor_length -= 2;
                            break;
                        default:
                            CDX_LOGE("stream_coding_type =%u",
                                stream_coding_type);
                            break;
                    }

                }
            }
            else if(format_identifier == MKBETAG('A', 'C', '-', '3'))
            {
                stream = (Stream *)father;
                stream->mStreamType = CDX_STREAM_TYPE_AUDIO_AC3;
                stream->mMediaType = TYPE_AUDIO;
            }
            else if(format_identifier == MKBETAG('V', 'C', '-', '1'))
            {
                stream = (Stream *)father;
                stream->mStreamType = CDX_STREAM_TYPE_VIDEO_VC1;
                stream->mMediaType = TYPE_VIDEO;
            }
            else if((format_identifier>>8 == MKBETAG('D', 'T', 'S', '1')>>8)
                || (format_identifier>>8 == MKBETAG('d', 't', 's', '1')>>8))
            {
                stream = (Stream *)father;
                stream->mStreamType = CDX_STREAM_TYPE_AUDIO_DTS;
                stream->mMediaType = TYPE_AUDIO;
            }
            else if(format_identifier == MKBETAG('H', 'E', 'V', 'C')
                || format_identifier == MKBETAG('h', 'e', 'v', 'c'))
            {
                stream = (Stream *)father;
                stream->mStreamType = CDX_STREAM_TYPE_VIDEO_H265;
                stream->mMediaType = TYPE_VIDEO;
            }
            else
            {
                CDX_LOGE("tag: 0x05 format_identifier=%u", format_identifier);
            }
            break;

        case 0x25:
            metadata_application_format = CdxBitReaderGetBits(br, 16);
            descriptor_length -= 2;
            if(metadata_application_format == 0xFFFF)
            {
                metadata_application_format_identifier = CdxBitReaderGetBits(br, 32);
                descriptor_length -= 4;
            }
            metadata_format = CdxBitReaderGetBits(br, 8);
            descriptor_length -= 1;
            if (metadata_format== 0xFF)
            {
                metadata_format_identifier = CdxBitReaderGetBits(br, 32);
                descriptor_length -= 4;
            }
            metadata_service_id = CdxBitReaderGetBits(br, 8);
            unsigned metadata_locator_record_flag = CdxBitReaderGetBits(br, 1);
            unsigned MPEG_carriage_flags = CdxBitReaderGetBits(br, 2);
            reserved = CdxBitReaderGetBits(br, 5);
            descriptor_length -= 2;
            CDX_LOGV("metadata_locator_record_flag: %u", metadata_locator_record_flag);
            CDX_LOGV("MPEG_carriage_flags: %u", MPEG_carriage_flags);
            CDX_LOGV("reserved: %x", reserved);

            if (MPEG_carriage_flags == (0|1|2))
            {
                unsigned program_number = CdxBitReaderGetBits(br, 16);
                descriptor_length -= 2;
            }
             if(metadata_application_format_identifier == MKBETAG('I', 'D', '3', ' ')
                && (metadata_format_identifier == MKBETAG('I', 'D', '3', ' ')))
             {
                stream = (Stream *)father;
                if(stream->mStreamType == CDX_STREAM_TYPE_METADATA)
                {
                    stream->mStreamType = CDX_STREAM_TYPE_METADATA;
                    stream->mMediaType = TYPE_META;
                }
             }
            break;
            case 0x26:
                metadata_application_format = CdxBitReaderGetBits(br, 16);
                descriptor_length -= 2;
                if(metadata_application_format == 0xFFFF)
                {
                    metadata_application_format_identifier = CdxBitReaderGetBits(br, 32);
                    descriptor_length -= 4;
                }
                metadata_format = CdxBitReaderGetBits(br, 8);
                descriptor_length -= 1;
                if (metadata_format== 0xFF)
                {
                    metadata_format_identifier = CdxBitReaderGetBits(br, 32);
                    descriptor_length -= 4;
                }
                metadata_service_id = CdxBitReaderGetBits(br, 8);
                unsigned decoder_config_flags  = CdxBitReaderGetBits(br, 3);
                unsigned DSM_CC_flag  = CdxBitReaderGetBits(br, 1);
                reserved = CdxBitReaderGetBits(br, 4);
                descriptor_length -= 2;
                CDX_LOGV("decoder_config_flags: %u", decoder_config_flags);
                CDX_LOGV("DSM_CC_flag: %u", DSM_CC_flag);
                CDX_LOGV("reserved: %x", reserved);

                if(metadata_application_format_identifier == MKBETAG('I', 'D', '3', ' ')
                    && (metadata_format_identifier == MKBETAG('I', 'D', '3', ' ')))
                {
                    stream = (Stream *)father;
                    if(stream->mStreamType == CDX_STREAM_TYPE_METADATA)
                    {
                        stream->mStreamType = CDX_STREAM_TYPE_METADATA;
                        stream->mMediaType = TYPE_META;
                    }
                }
                break;
        case 0x7B:
            stream = (Stream *)father;
            if(stream->mStreamType == CDX_STREAM_TYPE_PRIVATE_DATA)
            {
                stream->mStreamType = CDX_STREAM_TYPE_AUDIO_DTS;
                stream->mMediaType = TYPE_AUDIO;
            }
            break;
        case 0x6A:
        case 0x7A:
            stream = (Stream *)father;
            if(stream->mStreamType == CDX_STREAM_TYPE_PRIVATE_DATA)
            {
                stream->mStreamType = CDX_STREAM_TYPE_AUDIO_AC3;
                stream->mMediaType = TYPE_AUDIO;
            }
            break;
        default:
            CDX_LOGV("tag: 0x%02x len=%u", descriptor_tag, descriptor_length);
            break;

    }

    CdxBitReaderSkipBits(br, descriptor_length * 8);

    return 0;
}

Stream *VideoInProgram(Program *program)
{
    Stream *stream;
    if(program->videoCount)
    {
        CdxListForEachEntry(stream, &program->mStreams, node)
        {
            if(stream->mMediaType == TYPE_VIDEO
                && stream->mStreamType != CDX_STREAM_TYPE_VIDEO_MVC)
            {
                return stream;
            }
        }
    }
    return NULL;
}
Stream *MinorVideoInProgram(Program *program)
{
    Stream *stream;
    if(program->videoCount)
    {
        CdxListForEachEntry(stream, &program->mStreams, node)
        {
            if(stream->mMediaType == TYPE_VIDEO
                && stream->mStreamType == CDX_STREAM_TYPE_VIDEO_MVC)
            {
                return stream;
            }
        }
    }
    return NULL;
}
Stream *AudioInProgram(Program *program)
{
    Stream *stream;
    if(program->audioCount)
    {
        CdxListForEachEntry(stream, &program->mStreams, node)
        {
            if(stream->mMediaType == TYPE_AUDIO)
            {
                return stream;
            }
        }
    }
    return NULL;
}
Stream *SubtitleInProgram(Program *program)
{
    Stream *stream;
    if(program->subsCount)
    {
        CdxListForEachEntry(stream, &program->mStreams, node)
        {
            if(stream->mMediaType == TYPE_SUBS)
            {
                return stream;
            }
        }
    }
    return NULL;
}

Stream *MetaDataInProgram(Program *program)
{
    Stream *stream;
    if(program->metaCount)
    {
        CdxListForEachEntry(stream, &program->mStreams, node)
        {
            if(stream->mMediaType == TYPE_META)
            {
                return stream;
            }
        }
    }
    return NULL;
}

static cdx_int32 SeemToBeMvc(Program* mProgram)
{
    if(VideoInProgram(mProgram) && MinorVideoInProgram(mProgram))
    {
        return 1;
    }
    return 0;
}

status_t parseProgramMap(Program *program, CdxBitReaderT *br)
{
    CDX_LOGV("parseProgramMap");
    unsigned table_id = CdxBitReaderGetBits(br, 8);
    //CDX_FORCE_CHECK(table_id == 0x02u);
    if(table_id != 0x02u)
    {
        CDX_LOGE("should not be here.");
        return OK;
    }

    unsigned section_syntax_indictor = CdxBitReaderGetBits(br, 1);
    //CDX_CHECK(section_syntax_indictor == 1u);

    unsigned temp = CdxBitReaderGetBits(br, 1);
    if(temp == 0u)
        CDX_FORCE_CHECK(1);
    else
        CDX_FORCE_CHECK(0);

    CdxBitReaderGetBits(br, 2);/*reserved*/

    unsigned section_length = CdxBitReaderGetBits(br, 12);
    CDX_LOGV("  section_length = %u", section_length);
    CDX_CHECK((section_length & 0xc00) == 0u && section_length <= 1021u);

    unsigned program_number = CdxBitReaderGetBits(br, 16);
    if(program->mProgramNumber == -1)
    {
        program->mProgramNumber = program_number;
    }
    CDX_LOGV("  program_number = %u", program_number);

    CdxBitReaderGetBits(br, 2);/*reserved*/
    //MY_LOGV("  version_number = %u", CdxBitReaderGetBits(br, 5));
    unsigned version_number = CdxBitReaderGetBits(br, 5);
    //CDX_FORCE_CHECK(CdxBitReaderGetBits(br, 1) == 1u);/*current_next_indicator*/
    /*current_next_indicator如果不为1，要么直接跳过返回，要么先parse了存下来*/
    //MY_LOGV("  current_next_indicator = %u", CdxBitReaderGetBits(br, 1));
    unsigned current_next_indicator = CdxBitReaderGetBits(br, 1);
    if(((version_number == program->version_number
        && (program->mTSParser->bdFlag >= 2
        || VideoInProgram(program)
        || AudioInProgram(program)))
        || !current_next_indicator) && !program->mTSParser->b_hls_discontinue)
    {
        return OK;
    }
    CDX_LOGV("program->version_number(%d), version_number(%d)",
        program->version_number, version_number);
    program->version_number = version_number;
    //CDX_LOGD("program->mTSParser->status(%d)", program->mTSParser->status);
   /* //由于从节目选定，到置起CDX_PSR_IDLE之前还有estimateduration的环节，其间也可能引起parseProgramMap
   //以下判断若存在，则可能导致needUpdateProgram 没有置起
    if(program->mTSParser->status >= CDX_PSR_IDLE)
    {
    }
   */
    program->mTSParser->needUpdateProgram = 1;

    MY_LOGV("  section_number = %u", CdxBitReaderGetBits(br, 8));
    MY_LOGV("  last_section_number = %u", CdxBitReaderGetBits(br, 8));
    CdxBitReaderGetBits(br, 3);/*reserved*/

    //unsigned PCR_PID =
        CdxBitReaderGetBits(br, 13);
    //CDX_LOGV("  PCR_PID = 0x%04x", PCR_PID);

    CdxBitReaderGetBits(br, 4);/*reserved*/

    unsigned program_info_length = CdxBitReaderGetBits(br, 12);
    CDX_LOGV("  program_info_length = %u", program_info_length);
    CDX_CHECK((program_info_length & 0xc00) == 0u);

    // infoBytesRemaining is the number of bytes that make up the
    // variable length section of ES_infos. It does not include the
    // final CRC.
    int nInfoBytesRemaining = section_length - 9 - program_info_length - 4;

    while(program_info_length >= 2)
    {
        const cdx_uint8 *data = CdxBitReaderData(br);
        cdx_uint8 len = data[1];
        if(len > program_info_length - 2)
        {
            //something else is broken, exit the program_descriptors_loop
            break;
        }
        parseDescriptor(br, program);
        program_info_length -= len + 2;
    }
    CdxBitReaderSkipBits(br, program_info_length * 8);

#if 0    /*版本号改变的情况下，节目成分可能并没有变化(<阿凡达>，此时0x89描述符的信息有改变)*/
    DestroyStreams(program);
    while (nInfoBytesRemaining > 0)
    {
        CDX_CHECK(nInfoBytesRemaining >= 5u);

        unsigned stream_type = CdxBitReaderGetBits(br, 8);
        CDX_LOGV("    stream_type = 0x%02x", stream_type);

        CdxBitReaderGetBits(br, 3);/*reserved*/

        unsigned elementaryPID = CdxBitReaderGetBits(br, 13);
        CDX_LOGV("    elementary_PID = 0x%04x", elementaryPID);

        SetStream(program, elementaryPID, stream_type);
        Stream *stream = findStreamByPID(program, elementaryPID);
#if 0
/*
        if(stream)
        {
            if(stream->mStreamType != stream_type)
            {
                //CDX_LOGW("stream != NULL ??");
                DestroyStream(stream);
            }
        }*/

        if(!stream)
        {
            SetStream(program, elementaryPID, stream_type);
            stream = findStreamByPID(program, elementaryPID);
        }
        else if(stream->mStreamType != stream_type)
        {
            DestroyStream(stream);
            SetStream(program, elementaryPID, stream_type);
            stream = findStreamByPID(program, elementaryPID);
        }
        else
        {
            stream->mPayloadStarted = CDX_FALSE;
            stream->accessUnitStarted = CDX_FALSE;
            stream->tmpDataSize = 0;
            stream->pesIndex = 0;
            CdxBufferSetRange(stream->pes[0].mBuffer, 0, 0);
            CdxBufferSetRange(stream->pes[1].mBuffer, 0, 0);
        }
#endif
        CdxBitReaderGetBits(br, 4);/*reserved*/
        unsigned nES_info_length = CdxBitReaderGetBits(br, 12);
        CDX_CHECK((nES_info_length & 0xc00) == 0u);
        CDX_LOGV("    nES_info_length = %u, nInfoBytesRemaining = %u",
            nES_info_length, nInfoBytesRemaining);
        CDX_CHECK(nInfoBytesRemaining - 5 >= nES_info_length);

        unsigned nInfo_bytes_remaining = nES_info_length;
        while (nInfo_bytes_remaining >= 2)
        {
            const cdx_uint8 *data = CdxBitReaderData(br);
            cdx_uint8 len = data[1];
            if(len > nInfo_bytes_remaining - 2)
            {
                //something else is broken, exit the program_descriptors_loop
                break;
            }
            parseDescriptor(br, stream);
            nInfo_bytes_remaining -= len + 2;

        }
        CdxBitReaderSkipBits(br, nInfo_bytes_remaining * 8);

        VerifyStream(stream, 0);
        nInfoBytesRemaining -= 5 + nES_info_length;

    }
#else

        while (nInfoBytesRemaining > 0)
        {
            CDX_CHECK(nInfoBytesRemaining >= 5);
            if(nInfoBytesRemaining < 5)
            {
                CDX_LOGE("nInfoBytesRemaining < 5");
                return OK;
            }

            unsigned stream_type = CdxBitReaderGetBits(br, 8);
            CDX_LOGV("    stream_type = 0x%02x", stream_type);

            CdxBitReaderGetBits(br, 3);/*reserved*/

            unsigned elementaryPID = CdxBitReaderGetBits(br, 13);
            CDX_LOGV("    elementary_PID = 0x%04x", elementaryPID);

            //SetStream(program, elementaryPID, stream_type);
            Stream *stream = findStreamByPID(program, elementaryPID);
            if(!stream)
            {
                SetStream(program, elementaryPID, stream_type);
                stream = findStreamByPID(program, elementaryPID);
            }
            else if(stream->mStreamType != (cdx_int32)stream_type)
            {
                DestroyStream(stream);
                SetStream(program, elementaryPID, stream_type);
                stream = findStreamByPID(program, elementaryPID);
            }
            else
            {
                /*
                stream->mPayloadStarted = CDX_FALSE;
                stream->accessUnitStarted = CDX_FALSE;
                stream->tmpDataSize = 0;
                stream->pesIndex = 0;
                CdxBufferSetRange(stream->pes[0].mBuffer, 0, 0);
                CdxBufferSetRange(stream->pes[1].mBuffer, 0, 0);
                */
            }
            stream->existInNewPmt = 1;
            CdxBitReaderGetBits(br, 4);/*reserved*/
            int nES_info_length = CdxBitReaderGetBits(br, 12);
            CDX_CHECK((nES_info_length & 0xc00) == 0u);
            CDX_LOGV("    nES_info_length = %u, nInfoBytesRemaining = %u",
                nES_info_length, nInfoBytesRemaining);
            int eninfo_valid = (nInfoBytesRemaining - 5 >= nES_info_length);
            CDX_CHECK(eninfo_valid);
            if(!eninfo_valid)
            {
                CDX_LOGE("nES_info_length invalid");
            }

            unsigned nInfo_bytes_remaining = nES_info_length;
            while (nInfo_bytes_remaining >= 2 && eninfo_valid)
            {
                const cdx_uint8 *data = CdxBitReaderData(br);
                cdx_uint8 len = data[1];
                if(len > nInfo_bytes_remaining - 2)
                {
                    //something else is broken, exit the program_descriptors_loop
                    break;
                }
                parseDescriptor(br, stream);
                nInfo_bytes_remaining -= len + 2;

            }
            if (eninfo_valid)
                CdxBitReaderSkipBits(br, nInfo_bytes_remaining * 8);

            VerifyStream(stream, 0);
            nInfoBytesRemaining -= 5 + nES_info_length;
        }
        Stream *posStream, *nextStream;

        CdxListForEachEntrySafe(posStream, nextStream, &program->mStreams, node)
        {
            if(!posStream->existInNewPmt)
            {
                DestroyStream(posStream);
            }
            else
            {
                posStream->existInNewPmt = 0;
            }

            if(posStream->mStreamType == CDX_STREAM_TYPE_PRIVATE_DATA
                && posStream->codec_id == VIDEO_CODEC_FORMAT_H265
                && program->videoCount >= 2)
            {
                DestroyStream(posStream);
            }
        }

#endif
    CDX_CHECK(nInfoBytesRemaining == 0u);
    MY_LOGV("  CRC = 0x%08x", CdxBitReaderGetBits(br, 32));
    return OK;
}

void parseProgramAssociationTable(TSParser *mTSParser, CdxBitReaderT *br)
{
    CDX_LOGV("parseProgramAssociationTable");
    unsigned table_id = CdxBitReaderGetBits(br, 8);
    //CDX_FORCE_CHECK(table_id == 0x00u);
    if(table_id != 0x00u)
    {
        CDX_LOGE("should not be here.");
        return ;
    }
    unsigned section_syntax_indictor = CdxBitReaderGetBits(br, 1);
    CDX_CHECK(section_syntax_indictor == 1u);

    unsigned temp = CdxBitReaderGetBits(br, 1);
    if(temp == 0u)
        CDX_FORCE_CHECK(1);
    else
        CDX_FORCE_CHECK(0);

    CdxBitReaderGetBits(br, 2);/*reserved*/

    unsigned section_length = CdxBitReaderGetBits(br, 12);
    CDX_LOGV("  section_length = %u", section_length);
    CDX_CHECK((section_length & 0xc00) == 0u && section_length <= 1021u);

    MY_LOGV("  transport_stream_id = %u", CdxBitReaderGetBits(br, 16));
    CdxBitReaderGetBits(br, 2);/*reserved*/

    //MY_LOGV("  version_number = %u", CdxBitReaderGetBits(br, 5));
    unsigned version_number = CdxBitReaderGetBits(br, 5);
    //CDX_FORCE_CHECK(CdxBitReaderGetBits(br, 1) == 1u);/*current_next_indicator*/
    /*current_next_indicator如果不为1，要么直接跳过返回，要么先parse了存下来*/
    //MY_LOGV("  current_next_indicator = %u", CdxBitReaderGetBits(br, 1));
    unsigned current_next_indicator = CdxBitReaderGetBits(br, 1);
    if((version_number == mTSParser->pat_version_number
        || !current_next_indicator) &&
        !mTSParser->b_hls_discontinue)
    {
        return;
    }
    /*
    if(mTSParser->pat_version_number != (unsigned)-1 && mTSParser->status < CDX_PSR_IDLE)
    {
        CDX_LOGW("pat change!");
        return;
    }*/
    CDX_LOGV("mTSParser->pat_version_number(%d), version_number(%d)",
    mTSParser->pat_version_number, version_number);
    mTSParser->pat_version_number = version_number;
    MY_LOGV("  section_number = %u", CdxBitReaderGetBits(br, 8));
    MY_LOGV("  last_section_number = %u", CdxBitReaderGetBits(br, 8));

    size_t numProgramBytes = (section_length - 5 /* header */ - 4 /* crc */);
    CDX_CHECK((numProgramBytes % 4) == 0u);
    cdx_int32 mPmtId = 0;
    //DestroyPrograms(mTSParser);
    size_t i;
    for (i = 0; i < numProgramBytes / 4; ++i)
    {
        unsigned program_number = CdxBitReaderGetBits(br, 16);
        CDX_LOGV("    program_number = %u", program_number);

        CdxBitReaderGetBits(br, 3);/*reserved*/

        if (program_number == 0)
        {
            MY_LOGV("    network_PID = 0x%04x", CdxBitReaderGetBits(br, 13));
        }
        else
        {
            unsigned programMapPID = CdxBitReaderGetBits(br, 13);
            CDX_LOGV("    program_map_PID = 0x%04x", programMapPID);

            cdx_bool found = CDX_FALSE;
            Program *program = NULL;

            CdxListForEachEntry(program, &mTSParser->mPrograms, node)
            {
                if(program->mProgramNumber == (cdx_int32)program_number)
                {
                    program->mProgramMapPID = programMapPID;
                    found = CDX_TRUE;
                    if(mTSParser->b_hls_discontinue)
                    {
                        mTSParser->needSelectProgram = 1;
                        SetProgram(mTSParser, program_number, programMapPID);
                    }
                    break;
                }
            }
            if (!found)
            {
                mTSParser->needSelectProgram = 1;
                if(mTSParser->autoGuess == -1)
                {
                    CDX_LOGW("pat change!");
                    mTSParser->autoGuess = 0;//从自动选择开始
                }
                SetProgram(mTSParser, program_number, programMapPID);
            }
            if (!findPSISectionByPID(mTSParser, programMapPID))
            {
                SetPSISection(mTSParser, programMapPID);
            }
            program = findProgramByPID(mTSParser, programMapPID);
            program->existInNewPat = 1;
            if(mTSParser->b_hls_discontinue)
                mPmtId = program->mProgramMapPID;
        }
    }

    MY_LOGV("  CRC = 0x%08x", CdxBitReaderGetBits(br, 32));

    Program *posProgram, *nextProgram;

    CdxListForEachEntrySafe(posProgram, nextProgram, &mTSParser->mPrograms, node)
    {
        if(!posProgram->existInNewPat)
        {
            DestroyProgram(posProgram);
        }
        else
        {
            posProgram->existInNewPat = 0;
        }
    }
    /*pat change, but pmt is the same.*/
    if(mTSParser->b_hls_discontinue)
        mTSParser->enablePid[mPmtId] = 1;

}

static cdx_err SetProgram(TSParser *mTSParser,
    cdx_int32 programNumber, cdx_int32 programMapPID)
{
    CDX_CHECK(mTSParser);
    Program *program = CdxMalloc(sizeof(Program));
    CDX_FORCE_CHECK(program);
    memset(program, 0x00, sizeof(Program));

    program->mProgramNumber = programNumber;
    program->mProgramMapPID = programMapPID;
    program->version_number = (unsigned)-1;

    program->mFirstPTS = -1;
    CdxListInit(&program->mStreams);
    CDX_LOGV("new program, programNumber =%d programMapPID =0x%x, program(%p)",
        programNumber, programMapPID, program);
    program->mTSParser = mTSParser;

    if(program->mProgramMapPID >= 0)
    {
        mTSParser->enablePid[program->mProgramMapPID] = 1;
    }
    CdxListAddTail(&program->node, &mTSParser->mPrograms);
    mTSParser->mProgramCount++;
    return CDX_SUCCESS;
}

static cdx_err SetStream(Program *program, unsigned elementaryPID,
    cdx_int32 streamType/*, cdx_int32 PCR_PID*/)
{
    CDX_CHECK(program);
    Stream *stream = CdxMalloc(sizeof(Stream));
    CDX_FORCE_CHECK(stream);
    memset(stream, 0, sizeof(Stream));
    stream->mElementaryPID = elementaryPID;
    stream->mStreamType = streamType;
    stream->mPayloadStarted = CDX_FALSE;
    stream->accessUnitStarted = CDX_FALSE;
    stream->pes[0].mBuffer = CdxBufferCreate(NULL, 64*1024, NULL, 0);
    stream->pes[1].mBuffer = CdxBufferCreate(NULL, 64*1024, NULL, 0);
    stream->tmpBuf = CdxMalloc(188);
    stream->mProgram = program;
    CdxListAddTail(&stream->node, &program->mStreams);
    program->mStreamCount++;
    program->mTSParser->mStreamCount++;
    program->mTSParser->enablePid[elementaryPID] = 1;
    stream->mMediaType = -1;
    return CDX_SUCCESS;
}
cdx_int32 AdjustBufferOfStreams(Program *program)
{
    Stream *stream;
    CdxListForEachEntry(stream, &program->mStreams, node)
    {
        if(stream == program->mTSParser->curVideo)
        {
            CdxBufferDestroy(stream->pes[0].mBuffer);
            CdxBufferDestroy(stream->pes[1].mBuffer);
            stream->pes[0].mBuffer =
                CdxBufferCreate(NULL, VideoStreamBufferSize, NULL, 0);
            stream->pes[1].mBuffer =
                CdxBufferCreate(NULL, VideoStreamBufferSize, NULL, 0);
#if (PROBE_STREAM && !DVB_TEST)
            stream->probeBuf = (cdx_uint8 *)CdxMalloc(SIZE_OF_VIDEO_PROVB_DATA);
            stream->probeBufSize = SIZE_OF_VIDEO_PROVB_DATA;
#endif
        }
        else if(stream == program->mTSParser->curMinorVideo)
        {
            CdxBufferDestroy(stream->pes[0].mBuffer);
            CdxBufferDestroy(stream->pes[1].mBuffer);
            stream->pes[0].mBuffer =
                CdxBufferCreate(NULL, VideoStreamBufferSize/2, NULL, 0);
            stream->pes[1].mBuffer =
                CdxBufferCreate(NULL, VideoStreamBufferSize/2, NULL, 0);
#if (PROBE_STREAM && !DVB_TEST)  //不分配会导致TSOpenThread中probe部分的快速退出，可能probe并不充分
            stream->probeBuf =
            (cdx_uint8 *)CdxMalloc(SIZE_OF_VIDEO_PROVB_DATA/2);
            stream->probeBufSize = SIZE_OF_VIDEO_PROVB_DATA/2;
#endif
        }
        else
        {
            CdxBufferSetRange(stream->pes[0].mBuffer, 0, 0);
            CdxBufferSetRange(stream->pes[1].mBuffer, 0, 0);
#if (PROBE_STREAM && !DVB_TEST)
            stream->probeBuf = (cdx_uint8 *)CdxMalloc(SIZE_OF_AUDIO_PROVB_DATA);
            stream->probeBufSize = SIZE_OF_AUDIO_PROVB_DATA;
#endif
        }
        stream->mPayloadStarted = CDX_FALSE;
        stream->accessUnitStarted = CDX_FALSE;
        stream->tmpDataSize = 0;
        stream->pesIndex = 0;
    }
    return 0;
}
cdx_int32 ResetStreamsInProgram(Program *program)
{
    Stream *stream;
    CdxListForEachEntry(stream, &program->mStreams, node)
    {
        stream->mPayloadStarted = CDX_FALSE;
        stream->accessUnitStarted = CDX_FALSE;
        stream->tmpDataSize = 0;
        stream->pesIndex = 0;
        CdxBufferSetRange(stream->pes[0].mBuffer, 0, 0);
        CdxBufferSetRange(stream->pes[1].mBuffer, 0, 0);
    }
    return 0;
}
cdx_int32 ResetPSISection(TSParser *mTSParser)
{
    PSISection *section;
    CdxListForEachEntry(section, &mTSParser->mPSISections, node)
    {
        PSISectionClear(section);
    }
    return 0;
}

static cdx_err SetPSISection(TSParser *mTSParser, unsigned PID)
{
    CDX_CHECK(mTSParser);
    PSISection *section = CdxMalloc(sizeof(PSISection));
    CDX_FORCE_CHECK(section);
    section->PID = PID;
    section->mBuffer = CdxBufferCreate(NULL, 1024, NULL, 0);
    section->mPayloadStarted = CDX_FALSE;
    CdxListAddTail(&section->node, &mTSParser->mPSISections);
    return CDX_SUCCESS;
}

static void HandlePTS(TSParser *mTSParser)
{
    CDX_CHECK(mTSParser->currentES);
    Stream *stream = mTSParser->currentES;
    AccessUnit *mES = &stream->accessUnit;
    if(mES->pts == -1)
    {
        mES->mTimestampUs = -1;
        return;
    }
#if HandlePtsInLocal
    if(stream->mMediaType == TYPE_VIDEO
        && stream->mStreamType != CDX_STREAM_TYPE_VIDEO_MVC)
    {
        cdx_int64 new_pts = mES->pts*100/9;
        if(!mTSParser->syncOn && !mTSParser->hasVideoSync)
        {
            cdx_uint64 cur_time = mTSParser->preOutputTimeUs;

            if(mTSParser->hasAudioSync == 0)
            {
                mTSParser->videoPtsBaseUs = new_pts - cur_time;//如果new_pts < cur_time,无妨
                mTSParser->commonPtsBaseUs = mTSParser->videoPtsBaseUs;
            }
            else
            {
                mTSParser->videoPtsBaseUs = mTSParser->commonPtsBaseUs;
            }

            if(new_pts >= mTSParser->videoPtsBaseUs)
            {
                mTSParser->hasVideoSync = 1;
                if(stream->mProgram->audioCount)
                {
                    if(mTSParser->hasAudioSync == 1)
                    {
                        mTSParser->syncOn = 1;
                    }
                }
                else
                {
                    mTSParser->syncOn = 1;
                }
            }
            else
            {
            //说明 mTSParser->videoPtsBaseUs = mTSParser->commonPtsBaseUs;
            //说明文件中该video单元在audio 单元之后，但其pts小于audio,且这个差值大于cur_time(跳播后的时间点)
            //一种最可能的情况跳播之后很快出现pts回头
                CDX_LOGW("new_pts < mTSParser->videoPtsBaseUs");
                mTSParser->videoPtsBaseUs = new_pts - cur_time;
                mTSParser->commonPtsBaseUs = mTSParser->videoPtsBaseUs;
                mTSParser->hasVideoSync = 1;
                mTSParser->hasAudioSync = 0;

            }

        }
        else
        {
            //When av is not synchronous, difference between two ptses may be very large.
            //Do not take this case as pts loop back, same for audio.
            if((new_pts + 2000*1000 < mTSParser->vd_pts)
                || (mTSParser->vd_pts + 2000*1000 < new_pts))
            {
                CDX_LOGV(" new_pts = %lld, last_pts = %lld.",
                    new_pts, mTSParser->vd_pts);

                if(mTSParser->commonPtsBaseUs == mTSParser->videoPtsBaseUs)
                {
                    if((mTSParser->ad_pts + 2000 >= new_pts)
                        && (new_pts + 2000 >= mTSParser->ad_pts))
                    {
                        CDX_LOGV(" last audio pts(%lld) donot jump.",
                        mTSParser->ad_pts);
                    }
                    else
                    {
                        //* adjust video pts base to process video stream clock loopback.
                        mTSParser->commonPtsBaseUs = new_pts +
                        mTSParser->commonPtsBaseUs - mTSParser->vd_pts - 40000;
                        mTSParser->videoPtsBaseUs = mTSParser->commonPtsBaseUs;
                        CDX_LOGV(" video: remap video_pts_base_in_us to %lld",
                            mTSParser->commonPtsBaseUs);
                    }
                }
                else
                {
                    //* pts base has been adjusted by audio stream clock loopback processing.
                    mTSParser->videoPtsBaseUs = mTSParser->commonPtsBaseUs;
                    CDX_LOGV(" set video_pts_base_in_us to %lld",
                        mTSParser->commonPtsBaseUs);
                }
            }
            else
            {
                if(stream->mProgram->audioCount)
                {
                    if(mTSParser->videoPtsBaseUs != mTSParser->commonPtsBaseUs
                        && mTSParser->audioPtsBaseUs == mTSParser->commonPtsBaseUs)
                    {
                        mTSParser->unsyncVideoFrame++;
                    }
                }
            }
        }

        mTSParser->vd_pts = new_pts;

        mES->mTimestampUs = new_pts - mTSParser->videoPtsBaseUs;

        CDX_LOGV(" new_pts %lld. output video pts = %lld, video base %lld",
            new_pts, mES->mTimestampUs, mTSParser->videoPtsBaseUs);

        if(mES->mTimestampUs < 0)
        {
            CDX_LOGW("new_pts < mTSParser->videoPtsBaseUs 1");
            mES->mTimestampUs = mTSParser->preOutputTimeUs;
        }

    }
    else if(stream->mMediaType == TYPE_VIDEO
        && stream->mStreamType == CDX_STREAM_TYPE_VIDEO_MVC)
    {
        cdx_int64 new_pts = mES->pts*100/9;
        mES->mTimestampUs = new_pts - mTSParser->videoPtsBaseUs;
        if(mES->mTimestampUs < 0)
        {
            CDX_LOGW("new_pts < mTSParser->videoPtsBaseUs 1");
            mES->mTimestampUs = mTSParser->preOutputTimeUs;
        }
    }
    else if(stream->mMediaType == TYPE_AUDIO)
    {
        cdx_int64 new_pts = mES->pts*100/9;
        if(!mTSParser->syncOn && !mTSParser->hasAudioSync)
        {
            cdx_int64 cur_time = mTSParser->preOutputTimeUs;

            if(mTSParser->hasVideoSync == 0)
            {
                mTSParser->audioPtsBaseUs = new_pts - cur_time;//如果new_pts < cur_time,无妨
                mTSParser->commonPtsBaseUs = mTSParser->audioPtsBaseUs;
            }
            else
            {
                mTSParser->audioPtsBaseUs = mTSParser->commonPtsBaseUs;
            }

            if(new_pts >= mTSParser->audioPtsBaseUs)
            {
                mTSParser->hasAudioSync = 1;
                if(stream->mProgram->videoCount)
                {
                    if(mTSParser->hasVideoSync == 1)
                    {
                        mTSParser->syncOn = 1;
                    }
                }
                else
                {
                    mTSParser->syncOn = 1;
                }
            }
            else
            {
                CDX_LOGW("new_pts < mTSParser->audioPtsBaseUs");
                mTSParser->audioPtsBaseUs = new_pts - cur_time;
                mTSParser->commonPtsBaseUs = mTSParser->audioPtsBaseUs;
                mTSParser->hasVideoSync = 0;
                mTSParser->hasAudioSync = 1;
            }

        }
        else
        {
            //When av is not synchronous, difference between two ptses may be very large.
            //Do not take this case as pts loop back, same for audio.
            if((new_pts + 2000*1000 < mTSParser->ad_pts)
                || (mTSParser->ad_pts + 2000*1000 < new_pts))
            {
                CDX_LOGV(" audio: audio pts remap, new_pts = %lld, last_pts = %lld.",
                    new_pts, mTSParser->ad_pts);

                if(mTSParser->commonPtsBaseUs == mTSParser->audioPtsBaseUs)
                {
                    if((mTSParser->vd_pts + 2000 >= new_pts)
                        && (new_pts + 2000 >= mTSParser->vd_pts))
                    {
                        CDX_LOGV(" last video pts(%lld) donot jump.",
                        mTSParser->vd_pts);
                    }
                    else
                    {
                        //* adjust video pts base to process video stream clock loopback.
                        mTSParser->commonPtsBaseUs =
                        new_pts + mTSParser->commonPtsBaseUs
                        - mTSParser->ad_pts - stream->preAudioFrameDuration;
                        mTSParser->audioPtsBaseUs = mTSParser->commonPtsBaseUs;
                        CDX_LOGV(" remap audioPtsBaseUs to %lld",
                            mTSParser->commonPtsBaseUs);
                    }
                }
                else
                {
                    //* pts base has been adjusted by video stream clock loopback processing.
                    mTSParser->audioPtsBaseUs = mTSParser->commonPtsBaseUs;
                    CDX_LOGV(" already remap by video, set audioPtsBaseUs to %lld",
                        mTSParser->commonPtsBaseUs);
                }
            }
            else
            {
                if(stream->mProgram->videoCount)
                {
                    if(mTSParser->audioPtsBaseUs != mTSParser->commonPtsBaseUs
                        && mTSParser->videoPtsBaseUs == mTSParser->commonPtsBaseUs)
                    {
                        mTSParser->unsyncAudioFrame++;
                    }
                }
            }

        }

        if(stream->codec_id == CODEC_ID_AAC)
        {
            //记录AudioFrameDuration给下一个音帧用
            stream->preAudioFrameDuration = 1000*GetAACDuration(mES->data, mES->size);
            if(stream->preAudioFrameDuration <= 0
                || stream->preAudioFrameDuration >= 5000000)
                stream->preAudioFrameDuration = 40000;
        }
        else
        {
            stream->preAudioFrameDuration = 40000;
        }

        mTSParser->ad_pts = new_pts;

        mES->mTimestampUs = new_pts - mTSParser->audioPtsBaseUs;

        CDX_LOGV(" new_pts %lld. output audio pts = %lld, audio base %lld",
            new_pts, mES->mTimestampUs, mTSParser->audioPtsBaseUs);

        if(mES->mTimestampUs < 0)
        {
            CDX_LOGW("new_pts < mTSParser->audioPtsBaseUs 1");
            mES->mTimestampUs = mTSParser->preOutputTimeUs;
        }
    }
    else
    {//* subtitle;
        cdx_int64 new_pts = mES->pts*100/9;
        mES->mTimestampUs = new_pts - mTSParser->videoPtsBaseUs;
        if(mES->mTimestampUs < 0)
        {
            CDX_LOGW("new_pts < mTSParser->videoPtsBaseUs 1");
            mES->mTimestampUs = mTSParser->preOutputTimeUs;
        }

        if(!mTSParser->syncOn && !mTSParser->hasVideoSync)
        {
            //是否需要丢弃
            CDX_LOGV("subtitle : !mTSParser->syncOn && !mTSParser->hasVideoSync");
        }
    }
#else
    mES->mTimestampUs = mES->pts*100/9;
#if PtsDebug
    CDX_LOGD("mES->pts(%llx)", mES->pts);
#endif
#endif

    if(stream->mMediaType == TYPE_AUDIO
        ||(stream->mMediaType == TYPE_VIDEO
        && stream->mStreamType != CDX_STREAM_TYPE_VIDEO_MVC ))
    {
        if(mES->mTimestampUs != -1)
        {
            mTSParser->preOutputTimeUs = mES->mTimestampUs;
        }
    }
    return;

}


static cdx_int32 Analyze(const cdx_uint8* buf, cdx_int32 size,
    cdx_int32 packet_size, cdx_int32* index)
{
    cdx_int32 stat[204];
    cdx_int32 i;
    cdx_int32 j=0;
    cdx_int32 result=0;

    memset(stat, 0, packet_size*sizeof(cdx_int32));

    for(j=i=0; i<size; i++)
    {
        if(buf[i] == 0x47)
        {
            stat[j]++;
            if(stat[j] > result)
            {
                result= stat[j];
                if(index)
                    *index = j;
            }
        }

        j++;
        if( packet_size == j )
            j= 0;
    }

    return result;
}

/* auto detect FEC presence. Must have at least 1024 bytes  */
static cdx_int32 GetPacketSize(const cdx_uint8 *buf, cdx_uint32 size)
{
    cdx_int32 score, fec_score, dvhs_score;

    if (size < (5 * TS_FEC_PACKET_SIZE + 1))
        return -1;

    score       = Analyze(buf, size, TS_PACKET_SIZE, NULL);
    dvhs_score  = Analyze(buf, size, TS_DVHS_PACKET_SIZE, NULL);
    fec_score   = Analyze(buf, size, TS_FEC_PACKET_SIZE, NULL);

    if(score > dvhs_score && score > fec_score)
        return TS_PACKET_SIZE;
    else if(dvhs_score > fec_score && dvhs_score > score)
        return TS_DVHS_PACKET_SIZE;
    else if(fec_score > score && fec_score > dvhs_score)
        return TS_FEC_PACKET_SIZE;
    else
        return -1;
}
cdx_int32 PrintCacheBuffer(CacheBuffer *cacheBuffer)
{
    if(!cacheBuffer || !cacheBuffer->bigBuf || !cacheBuffer->bufSize)
    {
        CDX_LOGE("PrintCacheBuffer fail");
        return -1;
    }
    CDX_LOGV("bigBuf = %p, validDataSize = %u, writePos = %u, readPos = %u, endPos = %d",
        cacheBuffer->bigBuf, cacheBuffer->validDataSize, cacheBuffer->writePos,
        cacheBuffer->readPos, cacheBuffer->endPos);
    return 0;
}


/*return -1 TSResync fail*/
/*return 1 TSResync succeed*/
/*return 0 TSResync succeed and cdxStream Eos ，最后数据不足188byte也属此例*/

static cdx_int32 TSResync(TSParser *mTSParser)
{
    CdxStreamT *cdxStream = mTSParser->cdxStream;
    CacheBuffer *mCacheBuffer = &mTSParser->mCacheBuffer;
    cdx_uint32 resyncSize = mTSParser->mRawPacketSize * 5;//
    cdx_int32 ret;
    cdx_uint8 *pkt;
    cdx_uint8 *pkt_192;//It's needed in mixed TS packets, like 188 and 192 bytes packets.
    _resync:
    if(mTSParser->forceStop)
    {
        CDX_LOGD("mTSParser->forceStop, TSResync fail");
        //PrintCacheBuffer(&mTSParser->mCacheBuffer);
        return -1;
    }
    while(!mTSParser->cdxStreamEos
        && mCacheBuffer->validDataSize < resyncSize)
    {
        if(mCacheBuffer->bufSize - mCacheBuffer->writePos <
            (cdx_uint32)mTSParser->sizeToReadEverytime)
        {
            mCacheBuffer->endPos = mCacheBuffer->writePos;
            mTSParser->endPosFlag = 1;
            mCacheBuffer->writePos = 0;
        }
        ret = CdxStreamRead(cdxStream, mCacheBuffer->bigBuf + mCacheBuffer->writePos,
            mTSParser->sizeToReadEverytime);
#if SAVE_FILE
        if(ret > 0)
        {
            if (file)
            {
                fwrite(mCacheBuffer->bigBuf + mCacheBuffer->writePos, 1, ret, file);
                sync();
            }
            else
            {
                CDX_LOGW("save file = NULL");
            }
        }
#endif
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamRead fail");
            mTSParser->mErrno = PSR_IO_ERR;
            return -1;
        }
        else if(ret == 0)
        {
            CDX_LOGD("CdxStream EOS");
            mTSParser->cdxStreamEos = 1;
            break;
        }
        mCacheBuffer->validDataSize += ret;
        mCacheBuffer->writePos += ret;
    }

    while(mCacheBuffer->validDataSize > mTSParser->mRawPacketSize)
    {
        if(mTSParser->forceStop)
        {
            CDX_LOGD("mTSParser->forceStop, TSResync fail");
            //PrintCacheBuffer(&mTSParser->mCacheBuffer);
            return -1;
        }
        if(*(mCacheBuffer->bigBuf + mCacheBuffer->readPos) == 0x47)
        {
            pkt = mCacheBuffer->bigBuf + mCacheBuffer->readPos;
            if(mCacheBuffer->endPos > 0 &&
                mCacheBuffer->readPos + mTSParser->mRawPacketSize >=
                (cdx_uint32)mCacheBuffer->endPos)
            {
                pkt = pkt + mTSParser->mRawPacketSize - mCacheBuffer->endPos;
                pkt_192 = pkt + 4;
            }
            else
            {
                pkt += mTSParser->mRawPacketSize;
                pkt_192 = pkt + 4;
            }

            if(*pkt == 0x47 || *pkt_192 == 0x47)
            {
                return 1 - mTSParser->cdxStreamEos;//resynced
            }
            else if(((*pkt == 0) && (*(pkt+1) == 0) && (*(pkt+2) == 0))
                || ((*(pkt_192) == 0) && (*(pkt_192+1) == 0) && (*(pkt_192+2) == 0)))
            //process redundant zero data, first three bytes are important to ts.
            {
                CDX_LOGV("sync byte maybe lost or redundant data");
                return 1 - mTSParser->cdxStreamEos;
            }
            else
            {
                CDX_LOGW("incomplete ts packet");
            }
        }
        mCacheBuffer->readPos++;
        if(mCacheBuffer->endPos > 0
            && mCacheBuffer->readPos >= (cdx_uint32)mCacheBuffer->endPos)
        {
            mCacheBuffer->readPos = 0;
            mCacheBuffer->endPos = -1;
        }
        mCacheBuffer->validDataSize--;
    }

    if(!mTSParser->cdxStreamEos)
    {
        goto _resync;
    }
    else
    {
        while(mCacheBuffer->validDataSize >= TS_PACKET_SIZE)//但<= mTSParser->mRawPacketSize
        {
            if(*(mCacheBuffer->bigBuf + mCacheBuffer->readPos) == 0x47)
            {
                return 0;//resynced
            }
            mCacheBuffer->readPos++;
            if(mCacheBuffer->endPos > 0
                && mCacheBuffer->readPos >= (cdx_uint32)mCacheBuffer->endPos)
            {
                mCacheBuffer->readPos = 0;
                mCacheBuffer->endPos = -1;
            }
            mCacheBuffer->validDataSize--;
        }

        //CDX_LOGD("mCacheBuffer->validDataSize < TS_PACKET_SIZE");
        //PrintCacheBuffer(&mTSParser->mCacheBuffer);
        return 0;//-1可能导致最后一笔数据吐不出来
    }

}

static cdx_int32 SelectProgram(TSParser *mTSParser)
{
    Program *program = NULL, *nextProgram = NULL, *tmpProgram = NULL;
    Stream *stream = NULL, *nextStream = NULL;
    if(mTSParser->mProgramCount > 1)
    {
        CdxListForEachEntry(program, &mTSParser->mPrograms, node)
        {
            stream = VideoInProgram(program);
            if(stream)
            {
                mTSParser->hasVideo = 1;
                mTSParser->curVideo = stream;
                stream = MinorVideoInProgram(program);
                if(stream)
                {
                    mTSParser->hasMinorVideo = 1;
                    mTSParser->curMinorVideo = stream;
                }

                stream = AudioInProgram(program);
                if(stream)
                {
                    mTSParser->hasAudio = 1;
                }

                stream = SubtitleInProgram(program);
                if(stream)
                {
                    mTSParser->hasSubtitle = 1;
                }

                stream = MetaDataInProgram(program);
                if(stream)
                {
                    mTSParser->hasMetadata = 1;
                }
                break;
            }
        }
        if(mTSParser->hasVideo == 0)
        {
            CdxListForEachEntry(program, &mTSParser->mPrograms, node)
            {
                stream = AudioInProgram(program);
                if(stream)
                {
                    mTSParser->hasAudio = 1;
                    break;
                }
            }
        }

        if(!mTSParser->hasVideo && !mTSParser->hasAudio)
        {
            CDX_LOGW("hasVideo == 0 && hasAudio == 0 ?!");
            return -1;
        }

        mTSParser->curProgram = program;
        CdxListForEachEntrySafe(program, nextProgram, &mTSParser->mPrograms, node)
        {
            if(program != mTSParser->curProgram)
            {
                DestroyProgram(program);
            }
        }
    }
    else//mTSParser->mProgramCount == 1
    {
        program = CdxListEntry((&mTSParser->mPrograms)->head, typeof(*program), node);
        cdx_int32 flag = 0;
        if(mTSParser->bdFlag)
        {
            CDX_LOGV("mTSParser->bdFlag = %d", mTSParser->bdFlag);
            if(mTSParser->bdFlag == 1)
            {
                CdxListForEachEntry(stream, &program->mStreams, node)
                {
                    if(stream->mMediaType == TYPE_VIDEO
                        && stream->mStreamType != CDX_STREAM_TYPE_VIDEO_MVC
                        && !flag)
                    {
                        mTSParser->hasVideo = 1;
                        mTSParser->curVideo = stream;
                        flag = 1;
                    }
                    else if(stream->mMediaType == TYPE_AUDIO)
                    {
                        mTSParser->hasAudio = 1;
                    }
                    else if(stream->mMediaType == TYPE_SUBS)
                    {
                        mTSParser->hasSubtitle = 1;
                    }
                    else if(stream->mMediaType == TYPE_META)
                    {
                        mTSParser->hasMetadata = 1;
                    }
                    else
                    {
                        CDX_LOGW("stream->mMediaType == TYPE_UNKNOWN");
                    }
                }
            }
            else if(mTSParser->bdFlag == 2)
            {
                CdxListForEachEntrySafe(stream, nextStream, &program->mStreams, node)
                {
                    if(stream->mMediaType == TYPE_VIDEO
                        && stream->mStreamType == CDX_STREAM_TYPE_VIDEO_MVC
                        && !flag)
                    {
                        mTSParser->hasMinorVideo = 1;
                        mTSParser->curMinorVideo = stream;
                        flag = 1;
                    }
                    else
                    {//只取一路从流，协议规定最多两路
                        DestroyStream(stream);
                    }
                }
            }
            else if(mTSParser->bdFlag == 3)
            {
            }
            mTSParser->curProgram = program;
            goto SelectStreams;
        }


        if(!program->videoCount && !program->audioCount)
        {
            CDX_LOGW("hasVideo == 0 && hasAudio == 0 ?!");
            return -1;
        }
        else if(program->videoCount > 0)//videoCount是含minorvideo的
        {
            stream = VideoInProgram(program);
            if(stream)
            {
                mTSParser->hasVideo = 1;
                mTSParser->curVideo = stream;
            }

        }

        if(mTSParser->hasVideo == 0)
        {
            CdxListForEachEntrySafe(stream, nextStream, &program->mStreams, node)
            {
                if(mTSParser->autoGuess == 2)
                {
                    if(stream->mMediaType == TYPE_AUDIO
                        && mTSParser->hasAudio == 0)//只保留一个audio
                    {
                        mTSParser->hasAudio = 1;
                    }
                    else
                    {
                        DestroyStream(stream);
                    }
                }
                else
                {
                    if(stream->mMediaType == TYPE_AUDIO)
                    {
                        mTSParser->hasAudio = 1;
                    }
                    else
                    {
                        DestroyStream(stream);
                    }
                }
            }
            if(mTSParser->hasAudio == 0)
            {
                CDX_LOGW("hasVideo == 0 && hasAudio == 0 ?!");
                return -1;
            }

        }
        else
        {
            if(mTSParser->autoGuess == 2)
            {
                cdx_int32 allStreamHasFirstPTS;
                while(1)//可能需要修改，以兼容恶劣情况
                {
                /*//TSPrefetch本身已经考虑了forcestop
                    if(mTSParser->forceStop)
                    {
                        CDX_LOGE("PSR_USER_CANCEL");
                        return -1;
                    }
                 */
                    allStreamHasFirstPTS = 1;
                    CdxListForEachEntry(stream, &program->mStreams, node)
                    {
                        if(!stream->hasFirstPTS)
                        {
                            allStreamHasFirstPTS = 0;
                            break;
                        }
                    }
                    if(allStreamHasFirstPTS)
                    {
                        break;
                    }
                    if(mTSParser->endPosFlag)
                    {
                        //说明CacheBuffer空间小了，probe不足
                        CDX_LOGW("mCacheBuffer->endPos != -1 && !allStreamHasFirstPTS");
                        CDX_LOGW("should not be here");
                        return -1;
                    }

                    if(GetEs(mTSParser, NULL) < 0)
                    {
                        CDX_LOGE("should not be here");
                        return -1;
                    }
                }
                //以下所有stream都有firstPts
                Stream *selectStream = NULL;
                cdx_uint64 minDiff = (cdx_uint64)-1;
                cdx_int64 diff = 0;
                CdxListForEachEntrySafe(stream, nextStream, &program->mStreams, node)
                {
                    if(stream->mMediaType == TYPE_VIDEO
                        && stream == mTSParser->curVideo)
                    {
                        continue;
                    }
                    else if(stream->mMediaType == TYPE_AUDIO)//只保留一个audio
                    {
                        diff = stream->firstPTS - mTSParser->curVideo->firstPTS;
                        if(diff < 0)
                        {
                            diff = -diff;
                        }
                        if((cdx_uint64)diff < minDiff)
                        {
                            minDiff = diff;
                            if(selectStream)
                            {
                                DestroyStream(selectStream);
                            }
                            selectStream = stream;
                            continue;
                        }
                    }
                    DestroyStream(stream);
                }
                if(selectStream)
                {
                    mTSParser->hasAudio = 1;
                }
            }
            else
            {
                CdxListForEachEntry(stream, &program->mStreams, node)
                {
                    if(stream->mMediaType == TYPE_VIDEO
                        && stream->mStreamType != CDX_STREAM_TYPE_VIDEO_MVC)
                    {
                    }
                    else if(stream->mMediaType == TYPE_VIDEO
                        && stream->mStreamType == CDX_STREAM_TYPE_VIDEO_MVC
                        && !mTSParser->hasMinorVideo)
                    {
                        mTSParser->hasMinorVideo = 1;
                        mTSParser->curMinorVideo = stream;
                    }
                    else if(stream->mMediaType == TYPE_AUDIO)
                    {
                        mTSParser->hasAudio = 1;
                    }
                    else if(stream->mMediaType == TYPE_SUBS)
                    {
                        mTSParser->hasSubtitle = 1;
                    }
                    else if(stream->mMediaType == TYPE_META)
                    {
                        mTSParser->hasMetadata = 1;
                    }
                    else
                    {
                        CDX_LOGW("stream->mMediaType == TYPE_UNKNOWN");
                    }
                }
            }

        }

        mTSParser->curProgram = program;

    }

SelectStreams:

    program = mTSParser->curProgram;

    CdxListForEachEntrySafe(stream, nextStream, &program->mStreams, node)
    {
        if((stream->mMediaType != TYPE_VIDEO
            && stream->mMediaType != TYPE_AUDIO
            && stream->mMediaType != TYPE_SUBS
            && stream->mMediaType != TYPE_META)
            || (stream->mMediaType == TYPE_VIDEO
            && mTSParser->attribute & DISABLE_VIDEO)
            || (stream->mMediaType == TYPE_AUDIO
            && mTSParser->attribute & DISABLE_AUDIO)
            || (stream->mMediaType == TYPE_SUBS
            && mTSParser->attribute & DISABLE_SUBTITLE)
            || (stream->mMediaType == TYPE_VIDEO
            && stream->mStreamType != CDX_STREAM_TYPE_VIDEO_MVC
            && stream != mTSParser->curVideo)
            || (stream->mMediaType == TYPE_VIDEO
            && stream->mStreamType == CDX_STREAM_TYPE_VIDEO_MVC
            && stream != mTSParser->curMinorVideo))
        {
            DestroyStream(stream);
        }
        /*//放在这里是不恰当的，因为在select之后的estimateDuration中还会prefetch
        else
        {

            stream->mPayloadStarted = CDX_FALSE;
            CdxBufferSetRange(stream->pes[0].mBuffer, 0, 0);
            CdxBufferSetRange(stream->pes[1].mBuffer, 0, 0);
        }
        */
    }
    /*
    DestroyPSISections(mTSParser);
    memset(mTSParser->enablePid, 0, sizeof(mTSParser->enablePid));
    CdxListForEachEntry(stream, &program->mStreams, node)
    {
        mTSParser->enablePid[stream->mElementaryPID] = 1;
    }*/
    CdxListForEachEntry(stream, &program->mStreams, node)
    {
        mTSParser->enablePid[stream->mElementaryPID] = 1;
    }

    //* disable pat and pmt tables after program selected.
    CdxListForEachEntry(tmpProgram, &mTSParser->mPrograms, node)
    {
        mTSParser->enablePid[tmpProgram->mProgramMapPID] = 0;
    }
    mTSParser->enablePid[TS_PAT_PID] = 0;

    mTSParser->needSelectProgram = 0;
    mTSParser->needUpdateProgram = 0;
    return 0;

}


#define AV_RB16(p)  ((*(p))<<8 | (*((p)+1)))
/*forceStop or notFound, return -1*/
/*Found, return pts*/
cdx_int64 ProbePTS(cdx_uint8* data, cdx_uint32 data_size, TSParser *mTSParser)
{
    cdx_uint32 pktSize = mTSParser->mRawPacketSize;
    cdx_uint8* pkt;
    cdx_uint8* p;
    cdx_uint8* dataEnd;
    cdx_uint8* p_end;
    cdx_int64 pts;
    cdx_uint32 code;
    cdx_uint32 flags;
    cdx_uint32 afc;
    cdx_uint32 ptsFound;

    dataEnd  = data + data_size;
    pts      = 0;
    ptsFound = 0;

    for(pkt = data; pkt <= dataEnd - pktSize; pkt += pktSize)
    {
        //* outside program force parser to quit.

        if(mTSParser->forceStop)
            return -1;

        if(*pkt != 0x47)
        {
            //* resync;
            for(; pkt <= (dataEnd - pktSize); pkt++)
            {
                if((*pkt == 0x47) && (*(pkt + pktSize) == 0x47))
                    break;
            }

            if(pkt >= (dataEnd - pktSize))
                break;
        }

        if((pkt[1] & 0x40) == 0)    //* not the first packet of pes.
            continue;
/*
        if(mTSParser->autoGuess == 2 && (AV_RB16(pkt + 1) & 0x1fff) != mTSParser->pidForProbePTS)
        {
            continue;
        }
*/

        cdx_uint32 pid = AV_RB16(pkt + 1) & 0x1fff;
        if(!mTSParser->enablePid[pid] ||
           ((mTSParser->pidForProbePTS != 0x1fff) && (pid != mTSParser->pidForProbePTS)))
        {
            continue;
        }
        afc = (pkt[3]>>4) & 0x3;
        p   = pkt + 4;
        if(afc == 0 || afc == 2)
            continue;

        if(afc == 3)
        {
            p += p[0] + 1;
        }

        /* if past the end of packet, ignore */
        p_end = pkt + 188;
        if (p >= p_end)
            continue;

        for(; p < p_end - 13; p++)
        {
            code = p[0]<<16 | p[1]<<8 | p[2];
            if (code != 0x01)
                continue;

            code = p[3] | 0x100;

            if (!((code >= 0x1c0 && code <= 0x1df)
                || (code >= 0x1e0 && code <= 0x1ef)
                || (code == 0x1bd)
                || (code == 0x1fd)))
                continue;

            //flags = p[6]<<8 | p[7];
            flags = p[7];
            if (flags & 0xc0)        //* check pts flag
            {
                pts = (((p[9] & 0xeLL)<<29)
                    | ((cdx_int64)p[10]<<22)
                    | ((p[11]&0xfeLL)<<14)
                    | ((cdx_int64)p[12]<<7)
                    | (p[13]>>1));
                if(pts == 0x1ffffffffLL)  //* empty packet.
                    continue;
                ptsFound = 1;
                break;
            }
        }

        if(ptsFound)
            break;
    }

    if(ptsFound==0)
    {
        CDX_LOGW("not found pts");
        return -1;
    }
    return pts;
}

cdx_int64 ProbeLastPTS(cdx_uint8* data,
    cdx_uint32 data_size, TSParser *mTSParser)
{
    cdx_uint32 pktSize = mTSParser->mRawPacketSize;
    cdx_uint8* pkt;
    cdx_uint8* p;
    cdx_uint8* dataEnd;
    cdx_uint8* p_end;
    cdx_int64 pts;
    cdx_uint32 code;
    cdx_uint32 flags;
    cdx_uint32 afc;
    cdx_uint32 ptsFound;

    dataEnd  = data + data_size;
    pts      = 0;
    ptsFound = 0;

    for(pkt = dataEnd - pktSize; pkt >= data; pkt -= pktSize)
    {
        //* outside program force parser to quit.
        if(mTSParser->forceStop)
        {
            CDX_LOGV("PSR_USER_CANCEL");
            return -1;
        }

        if(*pkt != 0x47)
        {
            //* resync;
            for(; pkt >= data + pktSize; pkt--)
            {
                if((*pkt == 0x47) && (*(pkt - pktSize) == 0x47))
                    break;
            }
            if(pkt <= data)
                break;
        }

        if((pkt[1] & 0x40) == 0)    //* not the first packet of pes.
            continue;
/*
        if(mTSParser->autoGuess == 2 && (AV_RB16(pkt + 1) & 0x1fff) != mTSParser->pidForProbePTS)
        {
            continue;
        }
*/
        cdx_uint32 pid = AV_RB16(pkt + 1) & 0x1fff;
        if(!mTSParser->enablePid[pid] ||
           ((mTSParser->pidForProbePTS != 0x1fff) && (pid != mTSParser->pidForProbePTS)))
        {
            continue;
        }
        afc = (pkt[3]>>4) & 0x3;
        p   = pkt + 4;
        if(afc == 0 || afc == 2)
            continue;

        if(afc == 3)
        {
            p += p[0] + 1;
        }

        /* if past the end of packet, ignore */
        p_end = pkt + 188;
        if (p >= p_end)
            continue;

        for(; p < p_end - 13; p++)
        {
            code = p[0]<<16 | p[1]<<8 | p[2];
            if (code != 0x01)
                continue;

            code = p[3] | 0x100;

            if (!((code >= 0x1c0 && code <= 0x1df)
                || (code >= 0x1e0 && code <= 0x1ef)
                || (code == 0x1bd)
                || (code == 0x1fd)))
                continue;

            //flags = p[6]<<8 | p[7];
            flags = p[7];
            if (flags & 0xc0)        //* check pts flag
            {
                pts = (p[9] & 0xeLL)<<29
                    | (cdx_int64)p[10]<<22
                    | (p[11]&0xfeLL)<<14
                    | (cdx_int64)p[12]<<7
                    | p[13]>>1;
                if(pts == 0x1ffffffffLL)  //* empty packet.
                    continue;
                ptsFound = 1;
                break;
            }
        }

        if(ptsFound)
            break;
    }
    if(ptsFound==0)
    {
        CDX_LOGW("not found last pts");
        return -1;
    }
    return pts;
}

/* return <0 fail*/
#define block_num 10
cdx_int32 ProbeByteRate(cdx_uint8* buf, cdx_uint32 buf_size, TSParser *mTSParser)
{
    cdx_uint32 i, j, k, pos, run_len, seekMethod;
    cdx_int32 data_size;
    cdx_int64 block_size;
    cdx_int64 first_pts, last_pts, byte_rate, final_pts;
    cdx_int64 pts[block_num];
    cdx_uint32 ptsIndex[block_num];
    cdx_int64 ptsValid[block_num+1];
    cdx_int32 ret;
    mTSParser->fileValidSize = mTSParser->fileSize - mTSParser->mCacheBuffer.readPos;
    block_size = mTSParser->fileValidSize / block_num;
    CdxStreamT *cdxStream = mTSParser->cdxStream;
    byte_rate   = 0;
    seekMethod = 0;
    final_pts   = 0;
    if(mTSParser->fileValidSize >= buf_size*block_num)
    {
        j = 0;
        for(i=0; i<block_num; i++)
        {
            pts[i] = 0;
            ret = CdxStreamSeek(cdxStream,
                mTSParser->mCacheBuffer.readPos + i*block_size, SEEK_SET);
            if(ret < 0)
            {
                CDX_LOGE("CdxStreamSeek fail");
                return -1;
            }
            data_size = CdxStreamRead(cdxStream, buf, buf_size);
            if(data_size < 0)
            {
                return -1;
            }
            if(data_size > 0)
                pts[i] = ProbePTS(buf, data_size, mTSParser);

            //CDX_LOGD("ProbeByteRate pts[%d]=%lld", i, pts[i]);

            if(mTSParser->forceStop)
            {
                return -1;
            }

            if(i == block_num - 1 && block_size > buf_size)
            {
                ret = CdxStreamSeek(cdxStream, mTSParser->fileSize - buf_size, SEEK_SET);
                if(ret < 0)
                {
                    CDX_LOGE("CdxStreamSeek fail");
                    return -1;
                }
                data_size = CdxStreamRead(cdxStream, buf, buf_size);
                if(data_size < 0)
                {
                    return -1;
                }
                final_pts = ProbeLastPTS(buf, data_size, mTSParser);
                CDX_LOGD("ProbeByteRate final pts=%lld", final_pts*100/9);
            }

            if(pts[i] > 0)
            {
                ptsValid[j] = pts[i];
                ptsIndex[j] = i;
                j++;
            }

            //* outside program force parser to quit.
            if(mTSParser->forceStop)
            {
                return -1;
            }
        }

        if(j>1)
        {
            for(i=0; i<j-1; i++)
            {
                if(ptsValid[i] > ptsValid[i+1])
                    break;
            }

            first_pts   = ptsValid[0];
            last_pts    = ptsValid[j-1];
            if(i == j-1 && (final_pts < 0 || final_pts >= last_pts))
            {
                //* pts normal mode.
                cdx_int64 max_pts_diff, m;
                max_pts_diff = ptsValid[1] - ptsValid[0];
                for(m=1; m<j-1; m++)
                {
                    if(max_pts_diff < ptsValid[m+1]-ptsValid[m])
                        max_pts_diff = ptsValid[m+1]-ptsValid[m];
                    else
                        continue;
                }
#if 0
                if(j == block_num && final_pts > last_pts)//
                    byte_rate =
                    mTSParser->fileValidSize * 1000 /(final_pts - first_pts) *90;
                else
                    byte_rate =
                    block_size*1000*(ptsIndex[j-1] - ptsIndex[0]) / (last_pts - first_pts)*90;
#else
                //mTSParser->curProgram->mFirstPTS已经获取到了
                if(ptsIndex[j-1] == block_num - 1 && final_pts > 0)
                    //ptsIndex[j-1] == block_num - 1 与j == block_num不等价
                {
                    cdx_int64 expect_final_pts;
                    if(mTSParser->curProgram->mFirstPTS < first_pts)
                    {
                        first_pts = mTSParser->curProgram->mFirstPTS;
                    }
                    expect_final_pts = first_pts + block_num*max_pts_diff;
                    // need check.
                    if(final_pts < expect_final_pts)
                    {
                        byte_rate =
                            mTSParser->fileValidSize * 1000 /(final_pts - first_pts) *90;
                    }
                    else
                    {
                        CDX_LOGW("final pts may jump.");
                        byte_rate = block_size*1000*
                                    (ptsIndex[j-1] - ptsIndex[0]) / (last_pts - first_pts)*90;
                    }
                }
                else
                {
                    byte_rate =
                        block_size*1000*(ptsIndex[j-1] - ptsIndex[0]) / (last_pts - first_pts)*90;
                }
#endif
                seekMethod = 0;
            }
            else
            {
                CDX_LOGW("pts loop back");
                //* pts loop back mode.
                k           = 0;
                pos         = 0;
                run_len     = 0;
                ptsValid[j] = 0;
                for(i=0; i<j; i++)
                {
                    if(ptsValid[i]>=ptsValid[i+1]
                        || (ptsValid[i]+3600000)<ptsValid[i+1])
                    {
                        if(i - pos > run_len)
                        {
                            k       = pos;
                            run_len = i - pos;
                        }

                        pos = i+1;
                    }
                }
                //run_len取了一个最长的有效间隔
                if(run_len > 0)
                {
                    first_pts   = ptsValid[k];
                    last_pts    = ptsValid[k+run_len];
                    byte_rate   = block_size*1000*run_len/(last_pts - first_pts)*90;
                    seekMethod = 1;
                }
            }
        }

        if(byte_rate == 0)
        {
            if(mTSParser->mRawPacketSize == 192)
                byte_rate = 3840*1024;    //* 30Mbps for blueray HD
            else
                byte_rate = 2560*1024;    //* 20Mbps

            seekMethod = 1;
        }

        mTSParser->byteRate = byte_rate;
        mTSParser->seekMethod = seekMethod;
        mTSParser->durationMs =
            mTSParser->fileValidSize*1000/mTSParser->byteRate;

        if(mTSParser->durationMs > 36000000)         //* no more than 10 hour
        {
            if(mTSParser->mRawPacketSize == 192)
            {
                mTSParser->byteRate = 3840*1024;    //* 30Mbps for blueray HD
            }
            else
            {
                mTSParser->byteRate = 2560*1024;    //* 20Mbps
            }
            mTSParser->durationMs =
                mTSParser->fileValidSize*1000/mTSParser->byteRate;
        }
    }
    else
    {//小文件，因为buf_size只是1M
        if(mTSParser->curProgram->mFirstPTS == -1)
        {
            CDX_LOGE("should not be here.");
        }
        first_pts = mTSParser->curProgram->mFirstPTS;
        if(buf_size >= mTSParser->fileSize)
        {
            ret = CdxStreamSeek(cdxStream, 0, SEEK_SET);
            if(ret < 0)
            {
                CDX_LOGE("CdxStreamSeek fail");
                return -1;
            }
            data_size = CdxStreamRead(cdxStream, buf, mTSParser->fileSize);
            if(data_size < 0)
            {
                return -1;
            }
        }
        else
        {
            ret = CdxStreamSeek(cdxStream, mTSParser->fileSize - buf_size, SEEK_SET);
            if(ret < 0)
            {
                CDX_LOGE("CdxStreamSeek fail");
                return -1;
            }
            data_size = CdxStreamRead(cdxStream, buf, buf_size);
            if(data_size < 0)
            {
                return -1;
            }
        }
        last_pts = ProbeLastPTS(buf, data_size, mTSParser);
        if((last_pts > first_pts) && (last_pts < (first_pts + 3600*1000*90)))
            //* small file size, no more than one hour
        {
            mTSParser->byteRate =
                (mTSParser->fileValidSize*1000)/(last_pts - first_pts)*90;
            mTSParser->durationMs = (last_pts - first_pts)/90;
            mTSParser->seekMethod  = 0;
        }
        else
        {
            if(mTSParser->mRawPacketSize == 192)
            {
                mTSParser->byteRate        = 3840*1024;    //* 30Mbps for blueray HD
                mTSParser->durationMs =
                    mTSParser->fileValidSize*1000/mTSParser->byteRate;
            }
            else
            {
                mTSParser->byteRate        = 2560*1024;    //* 20Mbps
                mTSParser->durationMs =
                    mTSParser->fileValidSize*1000/mTSParser->byteRate;
            }

            mTSParser->seekMethod = 1;
        }
    }

    CDX_LOGV("ts file total time = %lld", mTSParser->durationMs);

    return 0;
}

/* <0 fail*/
cdx_int32 EstimateDuration(TSParser *mTSParser)
{
    if(mTSParser->fileSize <= 0)
    {
        CDX_LOGW("mTSParser->fileSize <= 0, EstimateDuration fail");
        return 0;
    }

    Stream *stream;
    Program *program = mTSParser->curProgram;
    CdxListForEachEntry(stream, &program->mStreams, node)
    {
        if(stream->hasFirstPTS
            && (cdx_uint64)program->mFirstPTS > stream->firstPTS)
        {
            program->mFirstPTS = stream->firstPTS;
        }
    }
    while(mTSParser->curProgram->mFirstPTS == -1)//注意不要用program->mFirstPTS
    {
        /*
        if(mTSParser->forceStop)
        {
            CDX_LOGE("PSR_USER_CANCEL");
            return -1;
        }
        */
        if(mTSParser->endPosFlag)
        {
            CDX_LOGE("program->mFirstPTS == -1 && mTSParser->mCacheBuffer->endPos != -1");
            return -1;
        }

        if(GetEs(mTSParser, NULL) < 0)
        {
            CDX_LOGE("TSPrefetch fail");
            return -1;
        }
    }
#if 0
    Stream *stream;
    /*
    CdxListForEachEntry(stream, &program->mStreams, node)
    {
        if(stream->hasFirstPTS)
        {
            firstPTS = stream->firstPTS;
            break;
        }
        i++;
    }
    if(i == program->mStreamCount)
    {
        CDX_LOGW("all streams still not probe the first pts!");
    }
    CdxListForEachEntry(stream, &program->mStreams, node)
    {
        if(!stream->hasFirstPTS)
        {
            stream->firstPTS= firstPTS;
        }
    }
    */


    unsigned i = 0;
    if(mTSParser->curVideo && mTSParser->curVideo->hasFirstPTS)
    {
        stream = mTSParser->curVideo;
    }
    else if(mTSParser->curMinorVideo && mTSParser->curMinorVideo->hasFirstPTS)
    {
        stream = mTSParser->curMinorVideo;
    }
    else
    {
        CdxListForEachEntry(stream, &program->mStreams, node)
        {
            if(stream->hasFirstPTS)
            {
                break;
            }
            i++;
        }
    }
    if(i == program->mStreamCount)
    {
        CDX_LOGE("all streams still not probe the first pts!");
        return;
    }
    mTSParser->pidForProbePTS = stream->mElementaryPID;
#endif
    CdxStreamT *cdxStream = mTSParser->cdxStream;
    cdx_int64 curPos = CdxStreamTell(cdxStream);
    if(curPos < 0)
    {
        CDX_LOGE("CdxStreamTell is unsupported");
        return -1;
    }

    cdx_int32 err = 0;
    cdx_uint32 durationMs = 0;
    cdx_int64 firstPTS = mTSParser->curProgram->mFirstPTS;
    cdx_int64 lastPTS = 0;
    cdx_uint8 *tmpBuf;
    cdx_uint32 bufSize;
    if(mTSParser->isNetStream || mTSParser->isDvbStream)
    {
        if(CdxStreamSeekAble(cdxStream))
        {
//#define PROBE_NETWORK_PACKET_NUM 30
#define PROBE_NETWORK_PACKET_NUM 1000
#define NUM_OF_CHUNK 20

            bufSize = PROBE_NETWORK_PACKET_NUM * mTSParser->mRawPacketSize;
            tmpBuf = (cdx_uint8 *)malloc(bufSize / NUM_OF_CHUNK);
            CDX_FORCE_CHECK(tmpBuf);
            CDX_LOGV("fileSize = %llu, tmp_size =%u", mTSParser->fileSize, bufSize);
            cdx_int32 dataSize, i;
            CdxDataSourceT dupSource = {0};
            char tmpUrl[4096] = {0};
            if (TSControl(&mTSParser->base, CDX_PSR_CMD_GET_URL, tmpUrl))
            {
                CDX_LOGW("get uri failed");
                goto _exit;
            }

            dupSource.uri = tmpUrl;
            ExtraDataContainerT *extraData = NULL;
            if (TSControl(&mTSParser->base, CDX_PSR_CMD_GET_STREAM_EXTRADATA, &extraData))
                extraData = NULL;
            if (extraData != NULL) {
                dupSource.extraData = extraData->extraData;
                dupSource.extraDataType = extraData->extraDataType;
            }

            dupSource.offset = mTSParser->fileSize - bufSize;
            if (dupSource.offset < 0)
                dupSource.offset = 0;
            dupSource.probeSize = 128;

            CdxStreamT *dupStream = NULL;
            if (CdxStreamOpen(&dupSource,
                &mTSParser->statusLock, &mTSParser->forceStop, &dupStream, NULL))
            {
                CDX_LOGW("stream open failed");
                if (dupStream != NULL)
                    CdxStreamClose(dupStream);
                err = -1;
                goto _exit;
            }

            for (i = 0; i < NUM_OF_CHUNK; i++)
            {
                dataSize = CdxStreamRead(dupStream, tmpBuf, bufSize / NUM_OF_CHUNK);
                if(dataSize <= 0)
                {
                    CDX_LOGE("CdxStreamRead fail");
                    err = -1;
                    CdxStreamClose(dupStream);
                    goto _exit;
                }
                lastPTS = ProbeLastPTS(tmpBuf, dataSize, mTSParser);
                if(mTSParser->forceStop)
                {
                    err = -1;
                    CdxStreamClose(dupStream);
                    goto _exit;
                }
                if(lastPTS > firstPTS)
                {
                    durationMs = (lastPTS - firstPTS)/90;

                    if(durationMs < (3600*1000*10))
                        break;
                }
            }
            CdxStreamClose(dupStream);
            CDX_LOGV("firstPTS %lld, lastPTS %lld, durationMs %u, fileSize %llu",
                firstPTS, lastPTS, durationMs, mTSParser->fileSize);

            if(i == NUM_OF_CHUNK)
            {
                if(!mTSParser->overallRate ||
                    (durationMs = mTSParser->fileSize / mTSParser->overallRate * 1000)
                    >= (3600*1000*10))
                {
                    durationMs = 60*30*1000; //30min;
                }
            }
            mTSParser->byteRate = mTSParser->fileSize*1000 / durationMs;
            mTSParser->durationMs = durationMs;
            mTSParser->seekMethod = 1;

        }
        else if(mTSParser->overallRate)
        {
            mTSParser->seekMethod = -1;
            mTSParser->durationMs = mTSParser->fileSize / mTSParser->overallRate * 1000;
            CDX_LOGV("durationMs = %lld", mTSParser->durationMs);
            return 0;
        }
        else
        {
            mTSParser->seekMethod = -1;
            mTSParser->durationMs = 0;
            return 0;
        }

    }
    else
    {
        Stream *stream;
        CdxListForEachEntry(stream, &program->mStreams, node)
        {
            if(stream->hasFirstPTS && stream->mMediaType == TYPE_VIDEO)
            {
                mTSParser->pidForProbePTS = stream->mElementaryPID;
                CDX_LOGD("stream type %d hasFirstPTS %lld, pid 0x%x !", stream->mMediaType, stream->firstPTS, mTSParser->pidForProbePTS);
                break;
            }
        }
        bufSize = 1.5*1024*1024;
        tmpBuf     = (cdx_uint8*)malloc(bufSize);
        CDX_FORCE_CHECK(tmpBuf);
        err = ProbeByteRate(tmpBuf, bufSize, mTSParser);
        if(err < 0)
        {
            CDX_LOGE("ProbeByteRate fail");
            goto _exit;
        }
    }
    err = CdxStreamSeek(cdxStream, curPos, SEEK_SET);
_exit:
    if(tmpBuf)
    {
        free(tmpBuf);
    }
    return err;

}

////////////////////////////////////////////////////////////////////////////////


cdx_int32 TSClose(CdxParserT *parser)
{
    CDX_CHECK(parser);
    TSParser *mTSParser = (TSParser *)parser;
    TSForceStop(parser);
    if(mTSParser->cdxStream)
    {
        CdxStreamClose(mTSParser->cdxStream);/*可能导致openthread的风险*/
        mTSParser->cdxStream = NULL;
    }
    DestroyPrograms(mTSParser);
    DestroyPSISections(mTSParser);
    if(mTSParser->mCacheBuffer.bigBuf)
    {
        CdxFree(mTSParser->mCacheBuffer.bigBuf);
    }

    if(mTSParser->tmpBuf)
    {
        CdxFree(mTSParser->tmpBuf);
    }
    if(mTSParser->hdcpHandle)
    {
        mTSParser->hdcpOps->deinit(mTSParser->hdcpHandle);
    }
    if(mTSParser->tempVideoInfo.pCodecSpecificData)
    {
        free(mTSParser->tempVideoInfo.pCodecSpecificData);
    }
    pthread_mutex_destroy(&mTSParser->statusLock);
    pthread_cond_destroy(&mTSParser->cond);
    pthread_rwlock_destroy(&mTSParser->controlLock);
#if SAVE_FILE
    if (file)
    {
        fclose(file);
        file = NULL;
    }
#endif
    CdxFree(mTSParser);
    return OK;
}
cdx_int32 TSRead(CdxParserT *parser, CdxPacketT *cdx_pkt)
{
    TSParser *mTSParser = (TSParser *)parser;
    if(mTSParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGW("status != CDX_PSR_PREFETCHED, TSRead invaild");
        mTSParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    CDX_CHECK(mTSParser->currentES);
    const cdx_uint8 *data = mTSParser->currentES->accessUnit.data;
    if(cdx_pkt->length <= cdx_pkt->buflen)
    {
        memcpy(cdx_pkt->buf, data, cdx_pkt->length);
    }
    else
    {
        memcpy(cdx_pkt->buf, data, cdx_pkt->buflen);
        memcpy(cdx_pkt->ringBuf, data + cdx_pkt->buflen,
                cdx_pkt->length - cdx_pkt->buflen);
    }
   //CDX_LOGV("currentES =%p, data =%p, length=%d", mTSParser->currentES, data, cdx_pkt->length);
    //CDX_LOGV("cdx_pkt->type =%d, cdx_pkt->length=%d", cdx_pkt->type, cdx_pkt->length);
    /*
if(cdx_pkt->type == CDX_MEDIA_AUDIO)
{
    if(cdx_pkt->length <= cdx_pkt->buflen)
    {
        CDX_BUF_DUMP(cdx_pkt->buf, cdx_pkt->length);
    }
    else
    {
        CDX_BUF_DUMP(cdx_pkt->buf, cdx_pkt->buflen);
        CDX_BUF_DUMP(cdx_pkt->ringBuf, cdx_pkt->length - cdx_pkt->buflen);
    }
}
*/
    //cdx_pkt->flags |= FIRST_PART | LAST_PART;
    mTSParser->currentES = NULL;
    mTSParser->status = CDX_PSR_IDLE;
    return 0;
}


/*0，表明从stream的残留中取得了一个es包*/
/*-1,表明未从stream的残留中取得es包，即残留在stream中的数据已经完全吐干净了*/

cdx_int32 SubmitLastData(TSParser *mTSParser)
{
    Stream *stream;
    if(mTSParser->curProgram)
    {
        CdxListForEachEntry(stream, &mTSParser->curProgram->mStreams, node)
        {
             StreamFlush(stream);
             if(mTSParser->currentES)
             {
                return 0;
             }

        }
    }
    mTSParser->mErrno = PSR_EOS;
    return -1;
}

static cdx_int32 SetMediaInfo(TSParser *mTSParser, CdxMediaInfoT *pmediainfo)
{
    memset(pmediainfo, 0, sizeof(CdxMediaInfoT));
    Program *program = mTSParser->curProgram;
    pmediainfo->fileSize = mTSParser->fileSize;
    //pmediainfo->flags = 0;
    pmediainfo->bSeekable = mTSParser->seekMethod == -1 ? 0 : 1;
    pmediainfo->programNum = 1;
    pmediainfo->programIndex = 0;
    struct CdxProgramS *cdxProgram = &pmediainfo->program[0];
    cdxProgram->duration = mTSParser->durationMs;
    CDX_LOGV("cdxProgram->duration = %u", cdxProgram->duration);
    cdxProgram->audioNum = program->audioCount;
    cdxProgram->videoNum = program->videoCount;//
    cdxProgram->subtitleNum = program->subsCount;
    cdxProgram->metadataNum = program->metaCount;
    cdxProgram->audioIndex = 0;
    cdxProgram->videoIndex = 0;
    cdxProgram->subtitleIndex = 0;
    cdxProgram->firstPts = mTSParser->firstPts;
    VideoStreamInfo *video = &cdxProgram->video[0];

    if(mTSParser->bdFlag < 2 && mTSParser->curVideo)
    {
        Stream *streamVideo = mTSParser->curVideo;
#if ProbeSpecificData
        if(0)
#else
        if(streamVideo->metadata)
#endif
        {
            VideoMetaData *videoMetaData = (VideoMetaData *)streamVideo->metadata;
            video->nWidth = videoMetaData->width;
            video->nHeight = videoMetaData->height;
            video->nFrameRate = videoMetaData->frameRate;
            video->nAspectRatio = videoMetaData->aspectRatio;
            if(video->nFrameRate)
            {
                video->nFrameDuration = 1000*1000*1000LL / video->nFrameRate;
            }

            video->nCodecSpecificDataLen = mTSParser->tempVideoInfo.nCodecSpecificDataLen;
            video->pCodecSpecificData = mTSParser->tempVideoInfo.pCodecSpecificData;
        }
        else
        {
            memcpy(video, &mTSParser->tempVideoInfo, sizeof(VideoStreamInfo));

        }
        cdxProgram->id = streamVideo->mElementaryPID;
        video->bIs3DStream = SeemToBeMvc(program);
        video->eCodecFormat = mTSParser->curVideo->codec_id;
    }
    Stream *stream, *nextStream = NULL;
    cdx_int32 i = 0, j = 0;
    CdxListForEachEntrySafe(stream, nextStream, &program->mStreams, node)
    {
        if(stream->mMediaType == TYPE_AUDIO && i < AUDIO_STREAM_LIMIT)
        {
            cdxProgram->audio[i].eCodecFormat = stream->codec_id;
            cdxProgram->audio[i].eSubCodecFormat = stream->codec_sub_id;
            if(stream->codec_id == AUDIO_CODEC_FORMAT_MPEG_AAC_LC)
                cdxProgram->audio[i].nFlags |= 1;
            memcpy(cdxProgram->audio[i].strLang, stream->lang, 4);
            if(stream->metadata)
            {
                cdxProgram->audio[i].nChannelNum =
                    ((AudioMetaData *)stream->metadata)->channelNum;
                cdxProgram->audio[i].nSampleRate =
                    ((AudioMetaData *)stream->metadata)->samplingFrequency;
                cdxProgram->audio[i].nBitsPerSample =
                    ((AudioMetaData *)stream->metadata)->bitPerSample;
                cdxProgram->audio[i].nAvgBitrate =
                    ((AudioMetaData *)stream->metadata)->bitRate;
                cdxProgram->audio[i].nMaxBitRate =
                    ((AudioMetaData *)stream->metadata)->maxBitRate;
#if 1
                /*
                if(cdxProgram->audio[i].nChannelNum == 2)
                {
                    cdxProgram->audio[i].eSubCodecFormat = WAVE_FORMAT_EXTENSIBLE - 2;
                }
                else if(cdxProgram->audio[i].nChannelNum == 6)
                {
                    cdxProgram->audio[i].eSubCodecFormat = WAVE_FORMAT_EXTENSIBLE - 1;
                }
                */
#else
                cdxProgram->audio[i].eSubCodecFormat = 1;
#endif


                CDX_LOGV("*********metadata %p, %d, %d , %d, %d, %d",
                    stream->metadata, cdxProgram->audio[i].nChannelNum,
                    cdxProgram->audio[i].nSampleRate, cdxProgram->audio[i].nBitsPerSample,
                    cdxProgram->audio[i].nAvgBitrate, cdxProgram->audio[i].nMaxBitRate);
            }
            if(stream->codec_id == AUDIO_CODEC_FORMAT_PCM)
            {
                cdxProgram->audio[i].eSubCodecFormat = WAVE_FORMAT_EXTENSIBLE - 1;
                cdxProgram->audio[i].eSubCodecFormat |= ABS_EDIAN_FLAG_BIG;
            }
            stream->streamIndex = i;
            i++;
        }
        else if(stream->mMediaType == TYPE_SUBS && j < SUBTITLE_STREAM_LIMIT)
        {

            cdxProgram->subtitle[j].eCodecFormat = stream->codec_id;
            //cdxProgram->subtitle[j].sub_codec_type = stream->codec_sub_id;
            memcpy(cdxProgram->subtitle[j].strLang, stream->lang, 4);
            stream->streamIndex = j;
            j++;
        }
        else if(stream->mMediaType == TYPE_VIDEO)
        {
            CDX_LOGV("stream->mMediaType == TYPE_VIDEO");
        }
        else if(stream->mMediaType == TYPE_META)
        {
            CDX_LOGV("stream->mMediaType == TYPE_META");
        }
        else
        {
            DestroyStream(stream);
        }
    }
    return 0;
}


#if Debug
static cdx_int32 count = 0;
#endif
cdx_int32 TSPrefetch(CdxParserT *parser, CdxPacketT *cdx_pkt)
{
    CDX_CHECK(parser);
    TSParser *mTSParser = (TSParser *)parser;

    if(mTSParser->status != CDX_PSR_IDLE && mTSParser->status != CDX_PSR_PREFETCHED)
    {
        CDX_LOGW("status != CDX_PSR_IDLE && status!= CDX_PSR_PREFETCHED, TSPrefetch invaild");
        mTSParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    if(mTSParser->mErrno == PSR_EOS)
    {
        CDX_LOGI("PSR_EOS");
        return -1;
    }
    if(mTSParser->status == CDX_PSR_PREFETCHED)
    {
        memcpy(cdx_pkt, &mTSParser->pkt, sizeof(CdxPacketT));
        return 0;
    }

    pthread_mutex_lock(&mTSParser->statusLock);
    if(mTSParser->forceStop)
    {
        pthread_mutex_unlock(&mTSParser->statusLock);
        return -1;
    }
    mTSParser->status = CDX_PSR_PREFETCHING;
    pthread_mutex_unlock(&mTSParser->statusLock);
#if Debug
    CDX_LOGD("prefetch start");
#endif
    cdx_int32 ret = GetEs(mTSParser, cdx_pkt);
    pthread_mutex_lock(&mTSParser->statusLock);
    if(ret < 0)
    {
        mTSParser->status = CDX_PSR_IDLE;
    }
    else
    {
        memcpy(&mTSParser->pkt, cdx_pkt, sizeof(CdxPacketT));
        mTSParser->status = CDX_PSR_PREFETCHED;
#if Debug
        CDX_LOGD("prefetch end, pts=%lld, type=%d, streamIndex=%d, cdx_pkt->length=%d, count=%d",
        cdx_pkt->pts, cdx_pkt->type, cdx_pkt->streamIndex, cdx_pkt->length, count);
        count++;
#endif
    }
    pthread_mutex_unlock(&mTSParser->statusLock);
    pthread_cond_signal(&mTSParser->cond);
    return ret;
}
/*cdx_pkt可以是空*/
cdx_int32 GetEs(TSParser *mTSParser, CdxPacketT *cdx_pkt)
{
    CacheBuffer *mCacheBuffer = &mTSParser->mCacheBuffer;
    cdx_uint32 rawPacketSize = mTSParser->mRawPacketSize;
    cdx_uint8 *buf;
    cdx_uint32 size;
    cdx_int32 ret = 0;

_GetES:
    while(!mTSParser->currentES)
    {
        if(mTSParser->forceStop)
        {
            CDX_LOGV("PSR_USER_CANCEL");
            ret = -1;
            goto _exit;
        }
        do
        {
            ret = TSResync(mTSParser);
            if(ret < 0)
            {
                CDX_LOGE("TSResync fail");
                goto _exit;
            }
            else if(ret == 0)
            {
                break;
            }
        }
        while(mCacheBuffer->validDataSize < rawPacketSize);

        if(mCacheBuffer->validDataSize < rawPacketSize)
            //ret == 0和mCacheBuffer->validDataSize < rawPacketSize不等价
        {
            if(mCacheBuffer->validDataSize >= TS_PACKET_SIZE)
            {
                size = mCacheBuffer->endPos - mCacheBuffer->readPos;
                if(mCacheBuffer->endPos > 0 && size < TS_PACKET_SIZE)
                //endPos可以是bufSize，但不会是0
                {
                    buf = mTSParser->tmpBuf;

                    memcpy(buf, mCacheBuffer->bigBuf + mCacheBuffer->readPos, size);
                    memcpy(buf + size, mCacheBuffer->bigBuf, TS_PACKET_SIZE - size);
                    mCacheBuffer->readPos = TS_PACKET_SIZE - size;
                    /*此时eos了，readPos和endPos可以不关心*/
                    mCacheBuffer->endPos = -1;

                }
                else
                {
                    buf = mCacheBuffer->bigBuf + mCacheBuffer->readPos;
                    mCacheBuffer->readPos += TS_PACKET_SIZE;
                    if(mCacheBuffer->endPos > 0
                        && mCacheBuffer->readPos >= (cdx_uint32)mCacheBuffer->endPos)
                    {
                        mCacheBuffer->endPos = -1;
                        mCacheBuffer->readPos = 0;
                    }
                }
                mCacheBuffer->validDataSize -= TS_PACKET_SIZE;

                ret = feedTSPacket(mTSParser, (const cdx_uint8 *)buf, TS_PACKET_SIZE);
                if(ret != OK)
                {
                    CDX_LOGE("feedTSPacket fail");
                    mTSParser->mErrno = PSR_UNKNOWN_ERR;
                    goto _exit;
                }
            }
            if(!mTSParser->currentES)
            {
                if(mTSParser->mIslastSegment)
                {
                    ret = SubmitLastData(mTSParser);
                }
                else
                {
                    mTSParser->mErrno = PSR_EOS;
                }
            }
            break;

        }
        else
        {
            size = mCacheBuffer->endPos - mCacheBuffer->readPos;
            if(mCacheBuffer->endPos > 0 && size < TS_PACKET_SIZE)//endPos可以是bufSize，但不会是0
            {
                buf = mTSParser->tmpBuf;

                memcpy(buf, mCacheBuffer->bigBuf + mCacheBuffer->readPos, size);
                memcpy(buf + size, mCacheBuffer->bigBuf, TS_PACKET_SIZE - size);
                mCacheBuffer->readPos = rawPacketSize - size;
                mCacheBuffer->endPos = -1;

            }
            else
            {
                buf = mCacheBuffer->bigBuf + mCacheBuffer->readPos;
                if(mCacheBuffer->endPos > 0 && size <= rawPacketSize)
                {
                    mCacheBuffer->endPos = -1;
                    mCacheBuffer->readPos = rawPacketSize - size;
                }
                else
                {
                    mCacheBuffer->readPos += rawPacketSize;
                }

            }
            mCacheBuffer->validDataSize -= rawPacketSize;

            ret = feedTSPacket(mTSParser, (const cdx_uint8 *)buf, TS_PACKET_SIZE);
            if(ret != OK)
            {
                CDX_LOGE("feedTSPacket fail");
                mTSParser->mErrno = PSR_UNKNOWN_ERR;
                goto _exit;
            }
        }
        if(/*mTSParser->status >= CDX_PSR_IDLE &&*/ (mTSParser->needSelectProgram
            || mTSParser->needUpdateProgram))
        {
            CDX_LOGV("needSelectProgram(%d), needUpdateProgram(%d)",
                mTSParser->needSelectProgram, mTSParser->needUpdateProgram);
            if(mTSParser->needSelectProgram && mTSParser->needUpdateProgram)
            {
                cdx_int32 allProgramHasStream = 1;
                cdx_int32 programHasVideo = 0;
                Program *program;
                CdxListForEachEntry(program, &mTSParser->mPrograms, node)
                {
                    if(mTSParser->bdFlag < 2)
                    {
                        Stream *video = VideoInProgram(program);
                        if(video)
                        {
                            programHasVideo = 1;
                            break;
                        }
                        Stream *audio = AudioInProgram(program);
                        if(!audio)
                        {
                            allProgramHasStream = 0;
                        }
                    }
                    else
                    {
                        if(!program->mStreamCount)
                        {
                            allProgramHasStream = 0;
                            break;
                        }
                    }
                }
                if(programHasVideo || allProgramHasStream)
                {
                    if(SelectProgram(mTSParser) < 0 || !mTSParser->curProgram)
                    {
                        CDX_LOGE("SelectProgram fail");
                        ret = -1;
                        mTSParser->mErrno = PSR_UNKNOWN_ERR;
                        goto _exit;
                    }
                    CdxMediaInfoT *mediainfo = (CdxMediaInfoT *)CdxMalloc(sizeof(CdxMediaInfoT));
                    if(!mediainfo)
                    {
                        CDX_LOGE("malloc failed!");
                        ret = -1;
                        goto _exit;
                    }
                    SetMediaInfo(mTSParser, mediainfo);
                    struct CdxProgramS *cdxProgram = &mediainfo->program[0];
                    struct CdxProgramS *cdxProgram1 = &mTSParser->mediaInfo.program[0];
                    if(memcmp(cdxProgram, cdxProgram1, sizeof(*cdxProgram)))
                    {
                        CdxMediaInfoT *tmp = (CdxMediaInfoT *)CdxMalloc(sizeof(CdxMediaInfoT));
                        if(!tmp)
                        {
                            CDX_LOGE("malloc failed!");
                            ret = -1;
                            goto _exit;
                        }
                        memcpy(tmp, &mTSParser->mediaInfo, sizeof(CdxMediaInfoT));
                        memcpy(&mTSParser->mediaInfo, mediainfo, sizeof(CdxMediaInfoT));
                        cdxProgram1 = &tmp->program[0];
                        if(mTSParser->status >= CDX_PSR_IDLE && mTSParser->callback)
                        {
                            cdx_int32 v_change = memcmp(cdxProgram->video,
                                cdxProgram1->video, sizeof(cdxProgram->video));
                            cdx_int32 a_change = memcmp(cdxProgram->audio,
                                cdxProgram1->audio, sizeof(cdxProgram->audio));

                            if(v_change)
                            {
                                CDX_LOGD("video stream has changed.");
                                int param[1];

                                param[0] = a_change ? 1:0;
                                while(!mTSParser->b_steam_change)
                                {
                                    usleep(1000);
                                }
                                if(!mTSParser->b_steam_change)
                                    continue;
                                mTSParser->callback(mTSParser->pUserData,
                                   PARSER_NOTIFY_VIDEO_STREAM_CHANGE, (void*)param);
                            }
                            else if(a_change)
                            {
                                CDX_LOGD("audio streams has changed.");
                                while(!mTSParser->b_steam_change)
                                {
                                    usleep(1000);
                                }
                                mTSParser->callback(mTSParser->pUserData,
                                    PARSER_NOTIFY_AUDIO_STREAM_CHANGE, NULL);
                            }
                            else
                                CDX_LOGW("unkown stream change.");
                        }

                        CdxFree(tmp);
                        tmp = NULL;
                        mTSParser->enablePid[TS_PAT_PID] = 0;
                    }
                    else
                    {
                        if(mTSParser->b_hls_discontinue)
                        {
                            CDX_LOGD("stream pts has changed.");
                            mTSParser->enablePid[TS_PAT_PID] = 0;
                        }
                    }
                    mTSParser->b_hls_discontinue = 0;
                    mTSParser->needSelectProgram = 0;
                    mTSParser->needUpdateProgram = 0;
                    mTSParser->autoGuess = -1;
                    if(mediainfo != NULL)
                    {
                        CdxFree(mediainfo);
                        mediainfo = NULL;
                    }
                }
                else
                {
                    mTSParser->currentES = NULL;
                }
            }
            else if(mTSParser->needUpdateProgram)
            {
            //update stream
                Stream *stream = VideoInProgram(mTSParser->curProgram);
                mTSParser->curVideo = stream;
                if(stream)
                {
                    mTSParser->hasVideo = 1;
                }
                CdxMediaInfoT *mediainfo = (CdxMediaInfoT *)CdxMalloc(sizeof(CdxMediaInfoT));
                if(!mediainfo)
                {
                    CDX_LOGE("malloc failed!");
                    ret = -1;
                    goto _exit;
                }
                SetMediaInfo(mTSParser, mediainfo);
                struct CdxProgramS *cdxProgram = &mediainfo->program[0];
                struct CdxProgramS *cdxProgram1 = &mTSParser->mediaInfo.program[0];
                if(memcmp(cdxProgram, cdxProgram1, sizeof(*cdxProgram)))
                {
                    CdxMediaInfoT *tmp = (CdxMediaInfoT *)CdxMalloc(sizeof(CdxMediaInfoT));
                    if(!tmp)
                    {
                        CDX_LOGE("malloc failed!");
                        ret = -1;
                        goto _exit;
                    }
                    memcpy(tmp, &mTSParser->mediaInfo, sizeof(CdxMediaInfoT));
                    memcpy(&mTSParser->mediaInfo, mediainfo, sizeof(CdxMediaInfoT));
                    cdxProgram1 = &tmp->program[0];
                    if(mTSParser->status >= CDX_PSR_IDLE &&mTSParser->callback)
                    {
                        if(memcmp(cdxProgram->video,
                            cdxProgram1->video, sizeof(cdxProgram->video)))
                        {
                            CDX_LOGD("video stream has changed.");
                            mTSParser->callback(mTSParser->pUserData,
                                PARSER_NOTIFY_VIDEO_STREAM_CHANGE, NULL);
                        }
                        if(memcmp(cdxProgram->audio,
                            cdxProgram1->audio, sizeof(cdxProgram->audio)))
                        {
                            CDX_LOGD("audio streams has changed.");
                            mTSParser->callback(mTSParser->pUserData,
                                PARSER_NOTIFY_AUDIO_STREAM_CHANGE, NULL);
                        }
                    }
                    CdxFree(tmp);
                    tmp = NULL;
                }
                mTSParser->needUpdateProgram = 0;
                if(mediainfo != NULL)
                {
                    CdxFree(mediainfo);
                    mediainfo = NULL;
                }
            }
        }

    }

    if(mTSParser->currentES)
    {
        if(cdx_pkt)
        {
            memset(cdx_pkt, 0x00, sizeof(CdxPacketT));
            HandlePTS(mTSParser);
            Stream *mStream = mTSParser->currentES;
            if(mStream->mMediaType == TYPE_VIDEO)
            {
                if(mStream->mStreamType == CDX_STREAM_TYPE_VIDEO_MVC)
                {
                    cdx_pkt->flags |= MINOR_STREAM;
                }
                cdx_pkt->type = CDX_MEDIA_VIDEO;
            }
            else if(mStream->mMediaType == TYPE_AUDIO)
            {
                cdx_pkt->type = CDX_MEDIA_AUDIO;
            }
            else if(mStream->mMediaType == TYPE_SUBS)
            {
                cdx_pkt->type = CDX_MEDIA_SUBTITLE;
                cdx_pkt->duration = mStream->accessUnit.durationUs;
            }
            else if(mStream->mMediaType == TYPE_META)
            {
                cdx_pkt->type = CDX_MEDIA_METADATA;
            }
            else
            {
                CDX_LOGW("Unkown MediaType, should not be here");
                mTSParser->currentES = NULL;

                DestroyStream(mStream);

                goto _GetES;
            }
            //
            if(!mStream->accessUnit.size)
            {
                CDX_LOGV("mStream->accessUnit.size=%d", mStream->accessUnit.size);
                mTSParser->currentES = NULL;
                goto _GetES;
            }
            cdx_pkt->pts = mStream->accessUnit.mTimestampUs;
            cdx_pkt->length = mStream->accessUnit.size;
            cdx_pkt->streamIndex = mStream->streamIndex;
            cdx_pkt->flags |= (FIRST_PART|LAST_PART);
            //CDX_BUF_DUMP(mStream->mES.data, cdx_pkt->length);
        }
        else//参见函数SelectProgram
        {
            mTSParser->currentES = NULL;
        }
        ret = 0;
    }
    else
    {
        if(cdx_pkt)
        {
            cdx_pkt->length = 0;
        }
        CDX_LOGV("mTSParser->currentES == NULL");
        ret = -1;
    }
_exit:
    return ret;
}

cdx_int32 ResetCacheBuffer(CacheBuffer *cacheBuffer)
{
    if(!cacheBuffer || !cacheBuffer->bigBuf || !cacheBuffer->bufSize)
    {
        CDX_LOGE("ResetCacheBuffer fail");
        return -1;
    }
    cacheBuffer->validDataSize = 0;
    cacheBuffer->writePos = 0;
    cacheBuffer->readPos = 0;
    cacheBuffer->endPos = -1;
    return 0;
}

#define PROBE_PACKET_NUM (42*1024)
cdx_int32 TSInit(CdxParserT *parser)
{
    CDX_LOGI("TSOpenThread start");
    TSParser *mTSParser = (TSParser *)parser;

    CdxStreamT *cdxStream = mTSParser->cdxStream;
    CacheBuffer *mCacheBuffer = &mTSParser->mCacheBuffer;
    mTSParser->fileSize = CdxStreamSize(cdxStream);
    mTSParser->isNetStream = CdxStreamIsNetStream(cdxStream);
    mTSParser->isDvbStream = CdxStreamIsDtmbStream(cdxStream);
#if DVB_USED
    int count = 0;
    mTSParser->curCountPos = 0;
    memset((void*)mTSParser->mapCounter, 0, sizeof(PIDCounterMap)*MAX_PID_NUM);
    for(count = 0;count < MAX_PID_NUM;count++)
    {
       mTSParser->mapCounter[count].pid = 0x1fff;
    }
#endif
    if(mTSParser->miracast)
    {
        mTSParser->sizeToReadEverytime = SizeToReadEverytimeInMiracast;
    }
    else if(mTSParser->isNetStream || mTSParser->isDvbStream)
    {
        mTSParser->sizeToReadEverytime = SizeToReadEverytimeInNet;
    }
    else
    {
        mTSParser->sizeToReadEverytime = SizeToReadEverytimeInLocal;
    }

    mTSParser->byteRate = 0;
    mTSParser->durationMs = 0;
    mTSParser->seekMethod = -1;
    mTSParser->pidForProbePTS = 0x1FFF;

    cdx_uint8 *buf;
    cdx_int32 size;
    cdx_int32 ret;
    status_t err;

    cdx_uint32 readPos = 0;//记录要读取的位置
    cdx_int32 endPos = -1;//记录要读取数据的截止位置
    cdx_int32 nGetPcket = 0;
#if CTS
    cdx_uint32 readPosA = 0;
    cdx_int32 endPosA = -1;
    cdx_int32 flag = 0;
#endif
__READ_DATA:
    nGetPcket++;
    while(mCacheBuffer->validDataSize < 4*1024)
    {
        ret = CdxStreamRead(cdxStream,
            mCacheBuffer->bigBuf + mCacheBuffer->writePos, 4*1024);
#if SAVE_FILE
        if(ret > 0)
        {
            if (file)
            {
                fwrite(mCacheBuffer->bigBuf + mCacheBuffer->writePos, 1, ret, file);
                sync();
            }
            else
            {
                CDX_LOGW("save file = NULL");
            }
        }
#endif

        if(ret < 0)//"="小于4k的视频认为不能播
        {
            CDX_LOGE("CdxStreamRead fail");
            goto _exit;
        }
        else if(ret == 0)
        {
            CDX_LOGW("eos, fileSize(%u)", mCacheBuffer->validDataSize);
            break;
        }
        mCacheBuffer->validDataSize += ret;
        mCacheBuffer->writePos += ret;
    }

    cdx_int32 rawPacketSize = GetPacketSize(mCacheBuffer->bigBuf, mCacheBuffer->validDataSize);
    if(rawPacketSize < 0)
    {
        CDX_LOGW("data maybe invalid, please check");
        // set 10 times to avoid unexpected loop.
        if(nGetPcket < 10)
        {
            mCacheBuffer->validDataSize = 0;
            mCacheBuffer->writePos = 0;
            mCacheBuffer->readPos = 0;
            goto __READ_DATA;
        }
        else
        {
            CDX_LOGE("GetPacketSize fail");
            goto _exit;
        }
    }
    mTSParser->mRawPacketSize = rawPacketSize;
    if(mTSParser->bdFlag)
    {
        CDX_CHECK(mTSParser->mRawPacketSize == 192);
        mCacheBuffer->readPos += 4;
        mCacheBuffer->validDataSize -= 4;
    }
    mTSParser->autoGuess = 0;
    cdx_uint32 usedPacketCount;
    Program *program;
_ProbeProgram:
    for(usedPacketCount = 0; usedPacketCount < PROBE_PACKET_NUM; usedPacketCount++)
    {
        if(mTSParser->forceStop)
        {
            CDX_LOGE("PSR_USER_CANCEL");
            goto _exit;
        }
        if(!mTSParser->autoGuess)
        {
            if(mTSParser->mProgramCount)
            /*假设一个pat表不会用多个ts packet来传输，否则就有风险*/
            //实际上，由于有PSISectionIsComplete的判定，风险极小
            {
                cdx_int32 allProgramHasStream = 1;
                cdx_int32 programHasVideo = 0;
                CdxListForEachEntry(program, &mTSParser->mPrograms, node)
                {
                    if(mTSParser->bdFlag < 2)
                    {
                        Stream *video = VideoInProgram(program);
                        if(video)
                        {
                            programHasVideo = 1;
                            break;
                        }
                        Stream *audio = AudioInProgram(program);
                        #if !CTS

                        if(!audio)
                        {
                            allProgramHasStream = 0;
                        }
                        #else
                        if(audio && !flag)
                        {
                            readPosA = mCacheBuffer->readPos;//记录同步的成果
                            endPosA = mCacheBuffer->endPos;
                            flag = 1;
                        }
                        allProgramHasStream = 0;
                        break;
                        #endif
                    }
                    else
                    {
                        if(!program->mStreamCount)
                        {
                            allProgramHasStream = 0;
                            break;
                        }
                    }
                }
                if(programHasVideo || allProgramHasStream)
                {
                    break;//如果有pat,pmt,解析完这两个表可以退出了，不用将PROBE_PACKET_NUM个包探测完
                }
            }
            do
            {
                ret = TSResync(mTSParser);
                if(ret < 0)
                {
                    CDX_LOGE("TSResync fail");
                    goto _exit;
                }
                else if(ret == 0)
                {
                    break;
                }
            }
            while(mCacheBuffer->validDataSize < (cdx_uint32)rawPacketSize);
            if(usedPacketCount == 0)
            {
                readPos = mCacheBuffer->readPos;//记录同步的成果
                endPos = mCacheBuffer->endPos;
            }

        }
        else
        {
            if(usedPacketCount == 0)
            {
                mCacheBuffer->readPos = readPos;
                mCacheBuffer->endPos = endPos;
                mCacheBuffer->validDataSize = mCacheBuffer->writePos - mCacheBuffer->readPos;
                if(mTSParser->autoGuess == 1)
                {
                    memset(mTSParser->enablePid, 0, sizeof(mTSParser->enablePid));
                    unsigned i;
                    for(i = 0x1fc8; i <= 0x1fcf; i++)//* for ISDB One Segment.
                    {
                        mTSParser->enablePid[i] = 1;
                    }
                }
                else
                {
                    CDX_LOGW("maybe no pats and pmts, or pats and pmts are wrong.");
                    if(mTSParser->autoGuess == 2)
                    {
                        memset(mTSParser->enablePid, 0, sizeof(mTSParser->enablePid));
                        unsigned i;
                        //* except SI table, because in this case no need SI.
                        for(i = 0x20; i < MAX_PID_NUM; i++)
                        {
                            mTSParser->enablePid[i] = 1;
                        }
                        DestroyPrograms(mTSParser);
                        //* construct a program.
                        SetProgram(mTSParser, -1, -1);
                        DestroyPSISections(mTSParser);
                    }
                }
            }

            if(mTSParser->autoGuess == 1)//只要有一个节目中有video就可退出
            {
                if(mTSParser->mProgramCount)/*假设一个pat\pmt表不会用多个ts packet来传输，否则就有风险*/
                {
                    cdx_int32 programHasVideo = 0;
                    CdxListForEachEntry(program, &mTSParser->mPrograms, node)
                    {
                        Stream *video = VideoInProgram(program);
                        if(video)
                        {
                            programHasVideo = 1;
                            break;
                        }
                    }
                    if(programHasVideo)
                    {
                        break;
                    }
                }
            }
            do
            {
                ret = TSResync(mTSParser);
                if(ret < 0)
                {
                    CDX_LOGE("TSResync fail");
                    goto _exit;
                }
                else if(ret == 0)
                {
                    break;
                }
            }
            while(mCacheBuffer->validDataSize < (cdx_uint32)rawPacketSize);
        }
        //CDX_LOGV("validDataSize =%u", mCacheBuffer->validDataSize);
        if(mCacheBuffer->validDataSize < (cdx_uint32)rawPacketSize)//说明cdxStream eos了
        {
            CDX_LOGD("eos");
            break;
        }
        //以下mCacheBuffer->validDataSize >= rawPacketSize
        size = mCacheBuffer->endPos - mCacheBuffer->readPos;//可能负数
        if(mCacheBuffer->endPos > 0 && size < TS_PACKET_SIZE)//endPos可以是bufSize，但不会是0
        {
            buf = mTSParser->tmpBuf;

            memcpy(buf, mCacheBuffer->bigBuf + mCacheBuffer->readPos, size);
            memcpy(buf + size, mCacheBuffer->bigBuf, TS_PACKET_SIZE - size);
            mCacheBuffer->readPos = rawPacketSize - size;
            mCacheBuffer->endPos = -1;

        }
        else
        {
            buf = mCacheBuffer->bigBuf + mCacheBuffer->readPos;
            if(mCacheBuffer->endPos > 0 && size <= rawPacketSize)
            {
                mCacheBuffer->endPos = -1;
                mCacheBuffer->readPos = rawPacketSize - size;
            }
            else
            {
                mCacheBuffer->readPos += rawPacketSize;
            }

        }
        mCacheBuffer->validDataSize -= rawPacketSize;

        err = feedTSPacket(mTSParser, (const cdx_uint8 *)buf, TS_PACKET_SIZE);
        if(err != OK)
        {
            CDX_LOGE("feedTSPacket fail");
            goto _exit;
        }
        CDX_LOGV("usedPacketCount =%d", usedPacketCount);
    }

    //CDX_LOGD("_ProbeProgram");
    //CDX_LOGD("usedPacketCount =%d", usedPacketCount);
    /*
    if(mTSParser->forceStop)
    {
        CDX_LOGE("PSR_USER_CANCEL");
        goto _exit;
    }
    */

    if(!mTSParser->mStreamCount)
    {
        if(mTSParser->autoGuess != 2)
        {
            mTSParser->autoGuess++;
            CDX_LOGD("autoGuess = %d", mTSParser->autoGuess);
            goto _ProbeProgram;
        }
        else
        {
            CDX_LOGE("ProbeProgram fail");
            goto _exit;
        }
    }
    CDX_LOGV("readPos = %d, endPos = %d, validDataSize = %d",
        mCacheBuffer->readPos, mCacheBuffer->endPos, mCacheBuffer->validDataSize);
    CDX_LOGV("mTSParser->mStreamCount(%d), mTSParser->mProgramCount(%d)",
        mTSParser->mStreamCount, mTSParser->mProgramCount);
    //select_program and stream
    //PrintTsStatus(mTSParser);
    if(SelectProgram(mTSParser) < 0 || !mTSParser->curProgram)
    {
        CDX_LOGE("SelectProgram fail");
        goto _exit;
    }

#if !CTS
    if(mTSParser->autoGuess == 2)
    {
        mCacheBuffer->readPos = readPos;
        mCacheBuffer->endPos = endPos;
        mCacheBuffer->validDataSize = mCacheBuffer->writePos - mCacheBuffer->readPos;
    }
    else
    {
        readPos = mCacheBuffer->readPos;
        endPos = mCacheBuffer->endPos;
    }

#else

    if(!mTSParser->autoGuess
        && !VideoInProgram(mTSParser->curProgram)
        && AudioInProgram(mTSParser->curProgram))
    {
        readPos = mCacheBuffer->readPos = readPosA;
        endPos = mCacheBuffer->endPos = endPosA;
    }
#endif

    mTSParser->autoGuess = -1;
    if(!(mTSParser->attribute & NO_NEED_DURATION))
    {
        if(EstimateDuration(mTSParser) < 0)//EstimateDuration中会决定seekMethod
        {
            CDX_LOGE("EstimateDuration fail");
            goto _exit;
        }
    }
    else
    {
        if(!mTSParser->isNetStream && CdxStreamSeekAble(cdxStream))
        {
            mTSParser->seekMethod = 0;
        }
        else if(mTSParser->isNetStream && CdxStreamSeekAble(cdxStream))
        {
            mTSParser->seekMethod = 1;
        }
    }
    if(CdxStreamAttribute(cdxStream) & CDX_STREAM_FLAG_STT)
    {
        mTSParser->seekMethod = 2;
    }

    AdjustBufferOfStreams(mTSParser->curProgram);
#if (PROBE_STREAM && !DVB_TEST)
    CDX_CHECK(mTSParser->endPosFlag == 0);
    if(mTSParser->endPosFlag != 0)
    {
        CDX_LOGE("mTSParser->endPosFlag != 0");
    }
    mCacheBuffer->readPos = readPos;
    mCacheBuffer->endPos = endPos;
    mCacheBuffer->validDataSize = mCacheBuffer->writePos - mCacheBuffer->readPos;
    mTSParser->mNumTSPacketsParsed = 0;
    mTSParser->currentES = NULL;

    CdxPacketT pkt;
    cdx_int32 flag = 0;
    if(mTSParser->bdFlag < 2 && mTSParser->curVideo)
    {
        while(1)
        {
            if(mTSParser->forceStop)
            {
                CDX_LOGE("PSR_USER_CANCEL");
                goto _exit;
            }
            ret = GetEs(mTSParser, &pkt);
            if(ret < 0)
            {
                CDX_LOGW("GetEs fail, may be EOS");
                break;
            }
            //CDX_LOGD("GetEs, ret(%d)", ret);
            Stream *stream = mTSParser->currentES;
            if(!flag && (stream->mMediaType == TYPE_VIDEO || stream->mMediaType == TYPE_AUDIO
                || stream->mMediaType == TYPE_SUBS))
            {
                mTSParser->firstPts = pkt.pts;
                flag = 1;
            }

            if(stream->mMediaType == TYPE_VIDEO && stream == mTSParser->curVideo)
            {
                if(pkt.length + stream->probeDataSize > stream->probeBufSize)
                {
                    CDX_LOGE("ProbeStream size too big");
                    mTSParser->currentES = NULL;
                    break;
                }
                else
                {
                    cdx_uint8 *data = stream->probeBuf + stream->probeDataSize;
                    memcpy(data, stream->accessUnit.data, pkt.length);
                    stream->probeDataSize += pkt.length;
                    mTSParser->currentES = NULL;
                }
#if !ProbeSpecificData

                if(ProbeStream(stream) < 0)
                {
                    CDX_LOGW("ProbeStream fail.");
                }
                if(stream->metadata)
                {
                    break;
                }

#else
                cdx_int32 ret = ProbeVideoSpecificData(&mTSParser->tempVideoInfo,
                    stream->probeBuf, stream->probeDataSize, stream->codec_id,
                    CDX_PARSER_TS);
                if(ret == PROBE_SPECIFIC_DATA_ERROR)
                {
                    CDX_LOGE("probeVideoSpecificData error");
                    break;
                }
                else if(ret == PROBE_SPECIFIC_DATA_SUCCESS)
                {
                    break;
                }
                else if(ret == PROBE_SPECIFIC_DATA_NONE)
                {

                    //readPos = mCacheBuffer->readPos;
                    //endPos = mCacheBuffer->endPos;
                    stream->probeDataSize = 0;
                }
                else if(ret == PROBE_SPECIFIC_DATA_UNCOMPELETE)
                {
                }
                else
                {
                    CDX_LOGE("probeVideoSpecificData (%d), it is unknown.", ret);
                }
#endif

            }
            else
            {
                mTSParser->currentES = NULL;
            }
        }
    }
#if 0
    program = mTSParser->curProgram;
    Stream *stream;
    CdxListForEachEntry(stream, &program->mStreams, node)
    {
        if((stream->mMediaType == TYPE_VIDEO && stream == mTSParser->curVideo)
            || stream->mMediaType == TYPE_AUDIO)
        {
            if(ProbeStream(stream) < 0)
            {
                CDX_LOGW("ProbeStream fail.");
            }
        }
        /*
        if(stream->probeBuf)
        {
            free(stream->probeBuf);
            stream->probeBuf = NULL;
        }*/
    }
#endif


#endif


    //CDX_CHECK(mCacheBuffer->endPos == -1);
    if(mTSParser->endPosFlag)
    {
        CDX_LOGW("mCacheBuffer->endPos != -1");
#if (PROBE_STREAM && !DVB_TEST)
        program = mTSParser->curProgram;
        Stream *stream;
        CdxListForEachEntry(stream, &program->mStreams, node)
        {
            if(stream->probeBuf)
            {
                CDX_LOGD("stream->probeBufSize(%u), stream->probeDataSize(%u)",
                    stream->probeBufSize, stream->probeDataSize);
            }
        }
#endif
        struct StreamSeekPos streamSeekPos;
        streamSeekPos.pos = readPos;
        ret = TSControl((CdxParserT *)mTSParser, CDX_PSR_CMD_STREAM_SEEK, &streamSeekPos);
        if(ret < 0)
        {
            CDX_LOGW("CDX_PSR_CMD_STREAM_SEEK fail.");
        }
    }
    else
    {
        /* play from the start of the file instead of the read pos.*/
        //mCacheBuffer->readPos = readPos;
        mCacheBuffer->readPos = 0;
        mCacheBuffer->endPos = endPos;
        mCacheBuffer->validDataSize = mCacheBuffer->writePos - mCacheBuffer->readPos;
        ResetStreamsInProgram(mTSParser->curProgram);
        ResetPSISection(mTSParser);
    }
    mTSParser->mNumTSPacketsParsed = 0;
    SetMediaInfo(mTSParser, &mTSParser->mediaInfo);
    PrintTsStatus(mTSParser);
    mTSParser->currentES = NULL;
    mTSParser->mErrno = PSR_OK;
    pthread_mutex_lock(&mTSParser->statusLock);
    mTSParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&mTSParser->statusLock);
    pthread_cond_signal(&mTSParser->cond);
    //CdxAtomicDec(&mTSParser->ref);
    CDX_LOGI("TSOpenThread success");
    return 0;
_exit:
    mTSParser->mErrno = PSR_OPEN_FAIL;
    pthread_mutex_lock(&mTSParser->statusLock);
    mTSParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&mTSParser->statusLock);
    pthread_cond_signal(&mTSParser->cond);
    //CdxAtomicDec(&mTSParser->ref);
    return -1;
}

cdx_int32 TSGetMediaInfo(CdxParserT *parser, CdxMediaInfoT *pmediainfo)
{
    TSParser *mTSParser = (TSParser *)parser;
    /*
    if(mTSParser->status < CDX_PSR_IDLE)
    {
        CDX_LOGE("status < CDX_PSR_IDLE, TSGetMediaInfo invaild");
        mTSParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }*/
    memcpy(pmediainfo, &mTSParser->mediaInfo, sizeof(CdxMediaInfoT));
    return 0;
}


cdx_uint32 TSAttribute(CdxParserT *parser)
{
    TSParser *mTSParser = (TSParser *)parser;
    return mTSParser->attribute;
}

cdx_int32 TSForceStop(CdxParserT *parser)
{
    CDX_LOGV("TSForceStop start");
    TSParser *mTSParser = (TSParser *)parser;

    pthread_mutex_lock(&mTSParser->statusLock);
    mTSParser->forceStop = 1;
    mTSParser->mErrno = PSR_USER_CANCEL;
    cdx_int32 ret = CdxStreamForceStop(mTSParser->cdxStream);
    while(mTSParser->status != CDX_PSR_IDLE && mTSParser->status != CDX_PSR_PREFETCHED)
    {
        pthread_cond_wait(&mTSParser->cond, &mTSParser->statusLock);
    }
    pthread_mutex_unlock(&mTSParser->statusLock);
    mTSParser->mErrno = PSR_USER_CANCEL;
    mTSParser->status = CDX_PSR_IDLE;
    CDX_LOGV("TSForceStop end");
    return ret;
}

cdx_int32 TSClrForceStop(CdxParserT *parser)
{
    CDX_LOGV("TSClrForceStop start");
    TSParser *mTSParser = (TSParser *)parser;
    if(mTSParser->status != CDX_PSR_IDLE)
    {
        CDX_LOGW("mTSParser->status != CDX_PSR_IDLE");
        mTSParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    mTSParser->forceStop = 0;
    if(mTSParser->cdxStream)
    {
        cdx_int32 ret = CdxStreamClrForceStop(mTSParser->cdxStream);
        if(ret < 0)
        {
            CDX_LOGW("CdxStreamClrForceStop fail");
        }
    }
    CDX_LOGI("TSClrForceStop end");
    return 0;
}
static cdx_int32 GetCacheState(TSParser *mTSParser, struct ParserCacheStateS *cacheState)
{
    struct StreamCacheStateS streamCS;
    if(CdxStreamControl(mTSParser->cdxStream, STREAM_CMD_GET_CACHESTATE, &streamCS) < 0)
    {
        CDX_LOGE("STREAM_CMD_GET_CACHESTATE fail");
        return -1;
    }
    cacheState->nBandwidthKbps = streamCS.nBandwidthKbps;
    cacheState->nCacheCapacity = streamCS.nCacheCapacity + mTSParser->mCacheBuffer.bufSize;
    cacheState->nCacheSize = streamCS.nCacheSize + mTSParser->mCacheBuffer.validDataSize;

    /* cacheState->nPercentage = current_caching_positioin / duration
     * so current_file_position / file_size is not accurate, but it's good
     * enough for debugging purpose.
     */
    if (mTSParser->fileSize > 0)
    {
        int64_t curPos = CdxStreamTell(mTSParser->cdxStream);
        cacheState->nPercentage = (curPos * 100 / mTSParser->fileSize);
    }
    return 0;
}

cdx_int32 TSControl(CdxParserT *parser, cdx_int32 cmd, void *param)
{
    cdx_int32 ret = 0;
    cdx_int64 streamSeekPos;
    TSParser *mTSParser = (TSParser *)parser;

    /* These control cmds are called by multiple threads for HLS.
     * The bad guy is REPLACE_STREAM. Without a lock, it will destroy everything.
     */
    switch(cmd)
    {
        case CDX_PSR_CMD_SWITCH_AUDIO:
        case CDX_PSR_CMD_SWITCH_SUBTITLE:
            CDX_LOGI(" Ts parser is not support switch stream yet!!!");
            break;
        case CDX_PSR_CMD_SET_DURATION://目前没有用
            mTSParser->durationMs = *(cdx_uint64 *)param;
            break;
        case CDX_PSR_CMD_REPLACE_STREAM:
            if (mTSParser->cdxStream)
                CdxStreamForceStop(mTSParser->cdxStream);

            pthread_rwlock_wrlock(&mTSParser->controlLock);
            CDX_LOGD("replace stream!!!mTSParser->cdxStream(%p), param(%p)",
                mTSParser->cdxStream, param);
            if(mTSParser->cdxStream)
            {
                CdxStreamClose(mTSParser->cdxStream);
            }
            mTSParser->cdxStream = (CdxStreamT*)param;
            mTSParser->cdxStreamEos = 0;
            mTSParser->mIslastSegment = 0;
            if(mTSParser->mClrInfo == 1)
            {
                ResetCacheBuffer(&mTSParser->mCacheBuffer);
                ResetStreamsInProgram(mTSParser->curProgram);
                ResetPSISection(mTSParser);
                mTSParser->mClrInfo = 0;
            }

            mTSParser->mErrno = PSR_OK;
            pthread_rwlock_unlock(&mTSParser->controlLock);
            break;
        case CDX_PSR_CMD_CLR_INFO:
            pthread_rwlock_rdlock(&mTSParser->controlLock);
            logd("CDX_PSR_CMD_CLR_INFO");
            mTSParser->mClrInfo = 1;
            pthread_rwlock_unlock(&mTSParser->controlLock);
            break;
        case CDX_PSR_CMD_SET_LASTSEGMENT_FLAG:
            pthread_rwlock_rdlock(&mTSParser->controlLock);
            CDX_LOGD("CDX_PSR_CMD_SET_LASTSEGMENT_FLAG");
            mTSParser->mIslastSegment = 1;
            pthread_rwlock_unlock(&mTSParser->controlLock);
            break;
        case CDX_PSR_CMD_STREAM_SEEK:
            pthread_rwlock_rdlock(&mTSParser->controlLock);
            if(!mTSParser->cdxStream)
            {
                pthread_rwlock_unlock(&mTSParser->controlLock);
                CDX_LOGE("mTSParser->cdxStream == NULL, can not stream control");
                ret = -1;
                break;
            }
            if(!CdxStreamSeekAble(mTSParser->cdxStream))
            {
                pthread_rwlock_unlock(&mTSParser->controlLock);
                CDX_LOGV("CdxStreamSeekAble == 0");
                break;
            }
            if(ResetCacheBuffer(&mTSParser->mCacheBuffer) < 0)
            {
                CDX_LOGE("ResetCacheBuffer fail");
                mTSParser->mErrno = PSR_UNKNOWN_ERR;
                ret = -1;
                pthread_rwlock_unlock(&mTSParser->controlLock);
                break;
            }
            streamSeekPos = ((struct StreamSeekPos *)param)->pos;
            if(mTSParser->bdFlag)
            {
                streamSeekPos += 4;
            }
            ret = CdxStreamSeek(mTSParser->cdxStream, streamSeekPos, SEEK_SET);
            if(ret < 0)
            {
                CDX_LOGE("CdxStreamSeek fail");
            }
            mTSParser->cdxStreamEos = 0;
            mTSParser->currentES = NULL;
            ResetStreamsInProgram(mTSParser->curProgram);
            ResetPSISection(mTSParser);
            mTSParser->mNumTSPacketsParsed = streamSeekPos/192;
            pthread_rwlock_unlock(&mTSParser->controlLock);
            break;

        case CDX_PSR_CMD_GET_CACHESTATE:
            pthread_rwlock_rdlock(&mTSParser->controlLock);
            ret = GetCacheState(mTSParser, param);
            pthread_rwlock_unlock(&mTSParser->controlLock);
            break;

        case CDX_PSR_CMD_SET_FORCESTOP:
            pthread_rwlock_rdlock(&mTSParser->controlLock);
            ret = TSForceStop(parser);
            pthread_rwlock_unlock(&mTSParser->controlLock);
            break;

        case CDX_PSR_CMD_CLR_FORCESTOP:
            pthread_rwlock_rdlock(&mTSParser->controlLock);
            ret = TSClrForceStop(parser);
            pthread_rwlock_unlock(&mTSParser->controlLock);
            break;

        case CDX_PSR_CMD_SET_HDCP:
            mTSParser->hdcpOps = (struct HdcpOpsS *)param;
            break;

        case CDX_PSR_CMD_SET_CALLBACK:
        {
            struct CallBack *cb = (struct CallBack *)param;
            mTSParser->callback = cb->callback;
            mTSParser->pUserData = cb->pUserData;
            break;
        }
        case CDX_PSR_CMD_GET_URL:
        {
            char* tmpUrl = NULL;
            CDX_LOGD("*********** CDX_PSR_CMD_GET_URL");
            pthread_rwlock_rdlock(&mTSParser->controlLock);
            ret = CdxStreamGetMetaData(mTSParser->cdxStream, "uri", (void**)&tmpUrl);
            pthread_rwlock_unlock(&mTSParser->controlLock);
            if (ret == -1 ||tmpUrl == NULL)
            {
                CDX_LOGE("this stream not support CDX_PSR_CMD_GET_URL");
                ret = -1;
                break;
            }
            cdx_int32 urlLen = strlen(tmpUrl) + 1;
            if(urlLen > 4096)
            {
                CDX_LOGE("url length is too long");
                ret = -1;
                break;
            }
            CDX_LOGD("--- ts parser get url= %s", tmpUrl);
            memcpy(param, tmpUrl, urlLen);
            break;
        }
        case CDX_PSR_CMD_GET_STREAM_EXTRADATA:
        {
            pthread_rwlock_rdlock(&mTSParser->controlLock);
            if(mTSParser->cdxStream)
            {
                ret = CdxStreamGetMetaData(mTSParser->cdxStream, "extra-data", (void **)param);
            }
            pthread_rwlock_unlock(&mTSParser->controlLock);
            break;
        }
        case CDX_PSR_CMD_SET_HLS_DISCONTINUITY:
        {
            pthread_rwlock_rdlock(&mTSParser->controlLock);
            Program *program = NULL;

            mTSParser->b_hls_discontinue = 1;
            mTSParser->enablePid[TS_PAT_PID] = 1;
            pthread_rwlock_unlock(&mTSParser->controlLock);
            break;
        }
        case CDX_PSR_CMD_SET_HLS_STREAM_FORMAT_CHANGE:
            pthread_rwlock_rdlock(&mTSParser->controlLock);
            mTSParser->b_steam_change = 1;
            pthread_rwlock_unlock(&mTSParser->controlLock);
            break;
        default:
            break;
    }

    return ret;
}
cdx_int32 TSGetStatus(CdxParserT *parser)
{
    TSParser *mTSParser = (TSParser *)parser;
    return mTSParser->mErrno;
}

/*pts_pos返回的是有有效pts的包的位置*/
cdx_int64 FindNextPTS(TSParser *mTSParser, cdx_uint8* data, cdx_uint32 data_size,
    cdx_int64 *pts_pos)
{
    cdx_uint32 pktSize = mTSParser->mRawPacketSize;

    cdx_uint8* pkt;
    cdx_uint8* p;
    cdx_uint8* dataEnd;
    cdx_uint8* p_end;
    cdx_int64      pts;
    cdx_uint32   code;
    cdx_uint32   flags;
    cdx_uint32   afc;
    cdx_uint32   ptsFound;
    cdx_uint32   pid = 0;

    dataEnd  = data + data_size;
    pts      = 0;
    ptsFound = 0;
    *pts_pos = 0;
    for(pkt = data; pkt <= dataEnd - pktSize; pkt += pktSize)
    {
        if(mTSParser->forceStop)
        {
            CDX_LOGW("mTSParser->forceStop");
            return (cdx_int64)-1;
        }
        if(*pkt != 0x47)
        {
            //* resync;
            for(; pkt <= (dataEnd - pktSize); pkt++)
            {
                if((*pkt == 0x47) && (*(pkt + pktSize) == 0x47))
                    break;
            }

            if(pkt >= (dataEnd - pktSize))
                break;
        }
        if((pkt[1] & 0x40) == 0)    //* not the first packet of pes.
            continue;

        pid = AV_RB16(pkt + 1) & 0x1fff;
        if(!mTSParser->enablePid[pid] || !pid) // table pat should be discarded.
        {
            continue;
        }
        afc = (pkt[3]>>4) & 0x3;
        p   = pkt + 4;
        if(afc == 0 || afc == 2)
            continue;

        if(afc == 3)
        {
            p += p[0] + 1;
        }

        /* if past the end of packet, ignore */
        p_end = pkt + 188;
        if (p >= p_end)
            continue;

        for(; p < p_end - 13; p++)
        {
            code = p[0]<<16 | p[1]<<8 | p[2];
            if (code != 0x01)
                continue;

            code = p[3] | 0x100;

            if (!((code >= 0x1c0 && code <= 0x1df)
                || (code >= 0x1e0 && code <= 0x1ef)
                || (code == 0x1bd)
                || (code == 0x1fd)))
                continue;

            flags = p[6]<<8 | p[7];
            if (flags & 0xc0)        //* check pts flag
            {
                pts = (cdx_int64)(p[9] & 0xe)<<29
                    | p[10]<<22
                    | (p[11]&0xfe)<<14
                    | p[12]<<7
                    | p[13]>>1;
                if(pts == 0x1ffffffffLL)  //* empty packet.
                    continue;
                ptsFound = 1;
                break;
            }
        }

        if(ptsFound)
        {
            *pts_pos = pkt - data;
            break;
        }
    }

    if(ptsFound)
        return pts;
    else
        return (cdx_int64)-1;
}
/*return <0 表明CdxStreamSeek没有成功，stream应该仍在seek前的位置*/
/*return >=0 表明CdxStreamSeek成功，stream不在seek前的位置，当然找到的pts可能并不准确*/

cdx_int32 SeekToTime(TSParser *mTSParser, cdx_int64 timeUs, cdx_uint32 mode)
{
    CdxStreamT *cdxStream = mTSParser->cdxStream;
    cdx_int64        file_pos;
    cdx_uint32    cnt;
    cdx_int64        dst_pts;
    cdx_int64        pts;
    cdx_int64        pts_diff;
    cdx_int32          pts_low_thredhold;
    cdx_int32          pts_high_thredhold;
    cdx_int64    pts_pos;
    cdx_int64    tmp;
    cdx_int32 ret = 0;

    if(mode == 0)       //* play mode
    {
        pts_low_thredhold  = 90*1000;
        pts_high_thredhold = 90*1000;
    }
    else if(mode == 1)  //* forward mode
    {
        pts_low_thredhold  = 0;
        pts_high_thredhold = 90*1000;
    }
    else                //* backward mode
    {
        pts_low_thredhold  = 90*1000;
        pts_high_thredhold = 0;
    }

    pts_pos = 0;
    if(mTSParser->seekMethod == 0)
    {
        cdx_int64 curPos = CdxStreamTell(cdxStream);
        file_pos = mTSParser->byteRate * timeUs / (1000 * 1000);
        file_pos -= (file_pos % mTSParser->mRawPacketSize);

        tmp = 0;
        cnt = 0;
        pts_diff = 0;
        dst_pts = timeUs*9/100 + mTSParser->curProgram->mFirstPTS;

        cdx_uint32 tmp_size = 204*60;//
        cdx_uint8* tmp_buf = (cdx_uint8*)malloc(tmp_size);
        if(!tmp_buf)
        {
            CDX_LOGE("malloc fail.");
            return -1;
        }
        ret = CdxStreamSeek(cdxStream, file_pos, SEEK_SET);
        if(ret < 0)
        {
            CDX_LOGE("CdxStreamSeek fail");
            free(tmp_buf);
            goto _exit;
        }
        do
        {
            cdx_int32 data_size = CdxStreamRead(cdxStream, tmp_buf, tmp_size);
            if(data_size <= 0)
            {
                CDX_LOGW("CdxStreamRead fail");
                //ret = -1;
                break;
            }

            pts = FindNextPTS(mTSParser, tmp_buf, data_size, &tmp);
            file_pos = file_pos + tmp;//

            if (pts == (cdx_int64)-1)//包含forcestop的情况
            {
                CDX_LOGW("FindNextPTS fail");
                //ret = -1;
                /*
                if(mTSParser->forceStop)
                {
                    ret = -1;
                }
                */
                break;
            }

            if (pts<dst_pts)
                pts_diff = dst_pts - pts;
            else
                pts_diff = pts - dst_pts;

            CDX_LOGV("pts = %lld, pts_diff = %lld, cnt = %u", pts, pts_diff, cnt);
            if (pts > dst_pts + pts_high_thredhold)
            {
                file_pos -= mTSParser->byteRate*(pts_diff/90000);
                if (file_pos <= 0)
                    file_pos = 0;
            }
            else if (pts + pts_low_thredhold < dst_pts)
            {
                file_pos += mTSParser->byteRate*(pts_diff/90000);
                if (file_pos > mTSParser->fileSize)
                {
                    file_pos -= mTSParser->byteRate*(pts_diff/90000);
                    break;
                }
            }
            else
                break;

            CDX_LOGV("tmp = %lld, dst_file_pos = %lld", tmp, file_pos);
            cnt++;
            if(CdxStreamSeek(cdxStream, file_pos, SEEK_SET) < 0)
            {
                CDX_LOGE("CdxStreamSeek fail");
                break;
            }
            curPos = CdxStreamTell(cdxStream);
            CDX_LOGV("curPos = %lld, file_pos = %lld", curPos, file_pos);
        }
        while (cnt < 10);
        free(tmp_buf);
    }
    else if(mTSParser->seekMethod == 1)
    {
        file_pos = mTSParser->byteRate * timeUs / (1000 * 1000);
        file_pos -= (file_pos % mTSParser->mRawPacketSize);
        ret = CdxStreamSeek(cdxStream, file_pos, SEEK_SET);
    }
#if 0
    else if(mTSParser->seekMethod == 3)
    {
        int64     first_pts;
        int64     last_pts;
        CDX_U32    tmp_size;
        CDX_S32    read_size;
        CDX_U8    *tmp_buf;

        if(timeUs >= ts->total_time_in_ms || timeUs < 0)
            return -1;

        tmp_buf = (CDX_U8 *)malloc(MAX_NETWORK_SEEK_SIZE);
        if(tmp_buf == NULL)
            return -1;
        tmp_size = MAX_NETWORK_SEEK_SIZE;

        file_pos = ts->byte_rate * timeUs / 1000;
        file_pos -= tmp_size / 2;//预期file_pos前后的一个范围
        if(file_pos < 0)
            file_pos = 0;
        file_pos -= (file_pos % ts->raw_packet_size);
        cdx_seek(ts->pb, file_pos, SEEK_SET);
        read_size = cdx_read(tmp_buf, 1, tmp_size, ts->pb);
        if(read_size <= 0)
            return -1;
        first_pts = probe_pts(tmp_buf, read_size, ts->raw_packet_size, ts->parent);
        last_pts  = probe_last_pts(tmp_buf, read_size, ts->raw_packet_size, ts->parent);
        if(first_pts > 0 && last_pts > 0)
        {
            if(timeUs < first_pts)
            {
                file_pos -= (first_pts - timeUs) * ts->byte_rate / 1000;
            }
            else if(timeUs > last_pts)
            {
                file_pos += (timeUs - last_pts) * ts->byte_rate / 1000 + read_size;
            }
            else
            {
                file_pos = ts->byte_rate * timeUs / 1000;
            }

            file_pos -= file_pos % ts->raw_packet_size;
        }
        if(file_pos < 0)
            file_pos = 0;
        LOGV("timeUs %lld, first_pts %lld, last pts %lld", timeUs, first_pts, last_pts);
        if(tmp_buf)
            free(tmp_buf);
        cdx_seek(ts->pb, file_pos, SEEK_SET);
        return 0;
    }
#endif
_exit:

    return ret;
}

cdx_int32 TSSeekTo(CdxParserT *parser, cdx_int64 timeUs, SeekModeType seekModeType)
{
    CDX_UNUSE(seekModeType);
    CDX_LOGI("TSSeekTo start, timeUs = %lld", timeUs);
    TSParser *mTSParser = (TSParser *)parser;
    if(mTSParser->status != CDX_PSR_IDLE || mTSParser->seekMethod == -1)
    {
        CDX_LOGE("PSR_UNKNOWN_ERR, bug !!!");
        mTSParser->mErrno = PSR_UNKNOWN_ERR;
        return -1;
    }

    if(timeUs < 0)
    {
        CDX_LOGE("invalid value");
        mTSParser->mErrno = PSR_INVALID_OPERATION;
        return -1;
    }
    if(!(mTSParser->attribute & NO_NEED_DURATION)
        && (cdx_uint64)timeUs >= mTSParser->durationMs*1000)
    {
        CDX_LOGI("PSR_EOS");
        mTSParser->mErrno = PSR_EOS;
        return 0;
    }

    pthread_mutex_lock(&mTSParser->statusLock);
    if(mTSParser->forceStop)//此时一定是idle态
    {
        pthread_mutex_unlock(&mTSParser->statusLock);
        return -1;
    }
    mTSParser->status = CDX_PSR_SEEKING;
    pthread_mutex_unlock(&mTSParser->statusLock);

    mTSParser->mErrno = PSR_OK;
    CdxStreamT *cdxStream = mTSParser->cdxStream;
    cdx_int32 ret = 0;
    if(ResetCacheBuffer(&mTSParser->mCacheBuffer) < 0)
    {
        CDX_LOGE("ResetCacheBuffer fail");
        ret = -1;
        mTSParser->mErrno = PSR_UNKNOWN_ERR;
        goto _exit;
    }
    if(mTSParser->seekMethod == 2)
    {
        ret = CdxStreamSeekToTime(cdxStream, timeUs);
        if(ret >= 0)
        {
#if HandlePtsInLocal
            mTSParser->preOutputTimeUs = timeUs;
#else
            while(mTSParser->curProgram->mFirstPTS == -1)
            {
                if(mTSParser->endPosFlag)
                {
                    CDX_LOGE("program->mFirstPTS == -1 && mTSParser->mCacheBuffer->endPos != -1");
                    ret = -1;
                    mTSParser->mErrno = PSR_UNKNOWN_ERR;
                    goto _exit;
                }
                if(GetEs(mTSParser, NULL) < 0)
                {
                    CDX_LOGE("TSPrefetch fail");
                    ret = -1;
                    goto _exit;
                }
                /*
                if(mTSParser->forceStop)
                {
                    ret = -1;
                    mTSParser->mErrno = PSR_USER_CANCEL;
                    goto _exit;
                }
                */
            }

            mTSParser->preOutputTimeUs = timeUs + mTSParser->curProgram->mFirstPTS*100/9;
            /*mTSParser->hasAudioSync = 0;
            mTSParser->hasVideoSync = 0;
            mTSParser->syncOn = 0;*/ //不需要，因为HandlePtsInLocal = 0
#endif
        }
        goto _exit;
    }

    while(mTSParser->curProgram->mFirstPTS == -1)
    {
        if(mTSParser->endPosFlag)
        {
            CDX_LOGE("program->mFirstPTS == -1 && mTSParser->endPosFlag == 1");
            ret = -1;
            mTSParser->mErrno = PSR_UNKNOWN_ERR;
            goto _exit;
        }
        if(GetEs(mTSParser, NULL) < 0)
        {
            CDX_LOGE("TSPrefetch fail");
            ret = -1;
            goto _exit;
        }
        /*
        if(mTSParser->forceStop)
        {
            ret = -1;
            mTSParser->mErrno = PSR_USER_CANCEL;
            goto _exit;
        }*/
    }
#if HandlePtsInLocal
    cdx_int64 curTime = mTSParser->preOutputTimeUs;//已经是显示时间
#else
    cdx_int64 curTime = mTSParser->preOutputTimeUs - mTSParser->curProgram->mFirstPTS*100/9;
#endif

    cdx_int64 diffTime = timeUs - curTime;
    CDX_LOGV("TSSeekTo diffTime = %lld, curTime = %lld, preOutputTimeUs = %lld, mFirstPTS = %lld",
        diffTime, curTime, mTSParser->preOutputTimeUs, mTSParser->curProgram->mFirstPTS);
    if(diffTime == 0)
    {
        ret = 0;
        goto _exit;
    }
    else if(diffTime > 0)
    {
        ret = SeekToTime(mTSParser, timeUs, 1); //* seek forward.
    }
    else
    {
        ret = SeekToTime(mTSParser, timeUs, 2); //* seek backward.
    }

    if(ret < 0)
    {
        mTSParser->mErrno = PSR_UNKNOWN_ERR;
        goto _exit;
    }
    mTSParser->hasAudioSync = 0;
    mTSParser->hasVideoSync = 0;
    mTSParser->syncOn = 0;
#if HandlePtsInLocal
    mTSParser->preOutputTimeUs = timeUs;
#else
    mTSParser->preOutputTimeUs = timeUs + mTSParser->curProgram->mFirstPTS*100/9;
#endif

_exit:
    mTSParser->cdxStreamEos = 0;
    mTSParser->currentES = NULL;
    ResetStreamsInProgram(mTSParser->curProgram);
    ResetPSISection(mTSParser);
    pthread_mutex_lock(&mTSParser->statusLock);
    mTSParser->status = CDX_PSR_IDLE;
    pthread_mutex_unlock(&mTSParser->statusLock);
    pthread_cond_signal(&mTSParser->cond);
    CDX_LOGD("TSSeekTo end, ret = %d, OutputTimeUs = %lld", ret, mTSParser->preOutputTimeUs);
    return ret;
}

static struct CdxParserOpsS gTSParserOps =
{
    .control = TSControl,
    .prefetch = TSPrefetch,
    .read = TSRead,
    .getMediaInfo = TSGetMediaInfo,
    .getStatus = TSGetStatus,
    .attribute = TSAttribute,
    .seekTo = TSSeekTo,
    .close = TSClose,
    .init = TSInit
};
////////////////////////////////////////////////////////////////////////////////
cdx_uint32 TSParserProbe(CdxStreamProbeDataT *probeData)
{
    cdx_int32 score, fec_score, dvhs_score;
    cdx_int32 i = 0;

    // attemp to try limitted times.
    if(probeData->len >= ProbeDataLen && i < 10)
    {
        cdx_int32 len = (cdx_int32)probeData->len;
        while(len >= ProbeDataLen)
        {
            score = Analyze((const cdx_uint8*)probeData->buf+i*ProbeDataLen,
                ProbeDataLen, TS_PACKET_SIZE, NULL);
            if(score >= 5)
            {
                return 100;
            }
            dvhs_score = Analyze((const cdx_uint8*)probeData->buf+i*ProbeDataLen,
                ProbeDataLen, TS_DVHS_PACKET_SIZE, NULL);

            if(dvhs_score >= 5)
            {
                return 100;
            }
            fec_score = Analyze((const cdx_uint8*)probeData->buf+i*ProbeDataLen,
                ProbeDataLen, TS_FEC_PACKET_SIZE, NULL);

            if(fec_score >= 5)
            {
                return 100;
            }
            len -= ProbeDataLen;
            i++;
        }
        return 0;
    }
    else
    {
        CDX_LOGW("Probe data is not enough. probeData->len(%u)", probeData->len);
        cdx_uint32 len = probeData->len;
        cdx_uint8 *p = (cdx_uint8 *)probeData->buf;
        cdx_uint8 *p1;
        while(len > TS_PACKET_SIZE)
        {
            if(*p == 0x47)
            {
                p1 = p + TS_PACKET_SIZE;
                if(*p1 == 0x47)
                {
                    return len*100/probeData->len;
                }
            }
            p++;
            len--;
        }
        if(len == TS_PACKET_SIZE)
        {
            return (*p == 0x47)*len*100/probeData->len;
        }
        return 0;
    }
}

CdxParserT *TSParserOpen(CdxStreamT *cdxStream, cdx_uint32 flags)
{
    TSParser *mTSParser = CdxMalloc(sizeof(TSParser));
    CDX_FORCE_CHECK(mTSParser);
    memset(mTSParser, 0x00, sizeof(TSParser));

    CdxListInit(&mTSParser->mPSISections);
    SetPSISection(mTSParser, TS_PAT_PID);
    mTSParser->enablePid[TS_PAT_PID] = 1;
    memset(mTSParser->crcValidity, 0, sizeof(mTSParser->crcValidity));

    mTSParser->pat_version_number = (unsigned)-1;
    CdxListInit(&mTSParser->mPrograms);

    cdx_uint32 bufSize = PROBE_PACKET_NUM * 204 * 2 / 8;
    CDX_LOGD("ts buffer size %d bytes", bufSize);
    mTSParser->mCacheBuffer.bigBuf = (cdx_uint8 *)CdxMalloc(bufSize);
    CDX_FORCE_CHECK(mTSParser->mCacheBuffer.bigBuf);
    mTSParser->mCacheBuffer.bufSize = bufSize;
    mTSParser->mCacheBuffer.endPos = -1;

    mTSParser->tmpBuf = (cdx_uint8 *)CdxMalloc(TS_PACKET_SIZE);
    CDX_FORCE_CHECK(mTSParser->tmpBuf);

    mTSParser->cdxStream = cdxStream;
    mTSParser->attribute = flags & (NO_NEED_DURATION | DISABLE_VIDEO |
        DISABLE_AUDIO | DISABLE_SUBTITLE | MUTIL_AUDIO | BD_TXET | MIRACST);
    //CDX_LOGD("mTSParser->attribute = %u, flags = %u", mTSParser->attribute, flags);
    mTSParser->bdFlag = (mTSParser->attribute & BD_TXET)>>7;
    if(mTSParser->bdFlag)
    {
       mTSParser->attribute |= NO_NEED_DURATION;
    }
    if(mTSParser->attribute & MIRACST)
    {
        mTSParser->miracast = 1;
        //HDCP_Init(&mTSParser->hdcpHandle);
    }
#if SAVE_FILE
    file = fopen("/data/camera/save.ts", "wb+");
    if (!file)
    {
        CDX_LOGE("open file failure errno(%d)", errno);
    }
#endif
    if(!(flags&NOT_LASTSEGMENT))
    {
        mTSParser->mIslastSegment = 1;
    }

    pthread_mutex_init(&mTSParser->statusLock, NULL);
    pthread_cond_init(&mTSParser->cond, NULL);

    mTSParser->base.ops = &gTSParserOps;
    mTSParser->base.type = CDX_PARSER_TS;
    mTSParser->mErrno = PSR_INVALID;
    mTSParser->status = CDX_PSR_INITIALIZED;

    pthread_rwlock_init(&mTSParser->controlLock, NULL);
    return &mTSParser->base;
}

CdxParserCreatorT tsParserCtor =
{
    .create = TSParserOpen,
    .probe = TSParserProbe
};

