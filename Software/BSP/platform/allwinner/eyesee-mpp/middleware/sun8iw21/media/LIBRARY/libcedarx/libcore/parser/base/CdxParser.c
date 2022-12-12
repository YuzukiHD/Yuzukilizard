/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxParser.c
 * Description : Parser base
 * History :
 *
 */

#include <CdxParser.h>

#include <cdx_log.h>
#include <CdxMemory.h>
#include <CdxList.h>

#include <CdxStream.h>
#include <dlfcn.h>

/*************** for debug start ***********************/
#define ENABLE_RAW_STREAM_PARSER 0
#define ENABLE_SPECIAL_PARSER 0
/*************** for debug end *************************/

struct CdxParserNodeS
{
    CdxListNodeT node;
    CdxParserCreatorT *creator;
    CdxParserTypeT type;
    struct ParserUriKeyInfoS *keyInfo;
};

struct CdxParserListS
{
    CdxListT list;
    int size;
};

struct CdxParserListS parserList;
static int parserInit = 0;

extern CdxParserCreatorT asfParserCtor;
static struct ParserUriKeyInfoS asfKeyInfo =
{
    "asf",
    {NULL}, /*scheme*/
    {".asf", ".wmv", ".wma"}, /*suffix*/
    {NULL} /*attribute*/
};

extern CdxParserCreatorT remuxParserCtor;
static struct ParserUriKeyInfoS remuxKeyInfo =
{
    "rtsp",
    {"rtsp"}, /*scheme*/
    {NULL}, /*suffix*/
    {NULL} /*attribute*/
};

extern CdxParserCreatorT aviParserCtor;
static struct ParserUriKeyInfoS aviKeyInfo =
{
    "avi",
    {NULL},  /*scheme*/
    {".avi"}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT flvParserCtor;
static struct ParserUriKeyInfoS flvKeyInfo =
{
    "flv",
    {"rtmp"},  /*scheme*/
    {".flv"}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT tsParserCtor;
static struct ParserUriKeyInfoS tsKeyInfo =
{
    "ts",
    {NULL},  /*scheme*/
    {".ts", ".tp", ".tr", ".m2ts", ".mts"}, /*suffix*/
    {NULL}   /*attribute*/
};

#ifdef __ANDROID__
extern CdxParserCreatorT dashParserCtor;
static struct ParserUriKeyInfoS dashKeyInfo =
{
    "dash",
    {NULL},  /*scheme*/
    {".mpd"}, /*suffix*/
    {NULL}   /*attribute*/
};
#endif

extern CdxParserCreatorT movParserCtor;
static struct ParserUriKeyInfoS movKeyInfo =
{
    "mov",
    {NULL},  /*scheme*/
    {".mp4", ".mov", ".3gp"}, /*suffix*/
    {NULL}   /*attribute*/
};

#ifdef __ANDROID__
extern CdxParserCreatorT mmsParserCtor;
static struct ParserUriKeyInfoS mmsKeyInfo =
{
    "mms",
    {"mms", "mmsh", "mmst", "mmshttp"},  /*scheme*/
    {NULL}, /*suffix*/
    {NULL}   /*attribute*/
};
#endif

extern CdxParserCreatorT hlsParserCtor;
static struct ParserUriKeyInfoS hlsKeyInfo =
{
    "m3u8",
    {NULL},  /*scheme*/
    {".m3u", ".m3u8"}, /*suffix*/
    {"ext=m3u8", "type=ipad"}   /*attribute*/
};

extern CdxParserCreatorT mkvParserCtor;
static struct ParserUriKeyInfoS mkvKeyInfo =
{
    "mkv",
    {NULL},  /*scheme*/
    {".mkv", ".webm"}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT mpgParserCtor;
static struct ParserUriKeyInfoS mpgKeyInfo =
{
    "mpg",
    {NULL},  /*scheme*/
    {NULL}, /*suffix*/
    {NULL}   /*attribute*/
};

#ifdef __ANDROID__
extern CdxParserCreatorT bdParserCtor;
static struct ParserUriKeyInfoS bdKeyInfo =
{
    "bdmv",
    {"bdmv"},  /*scheme*/
    {NULL}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT pmpParserCtor;
static struct ParserUriKeyInfoS pmpKeyInfo =
{
    "pmp",
    {NULL},  /*scheme*/
    {".pmp"}, /*suffix*/
    {NULL}   /*attribute*/
};
#endif

extern CdxParserCreatorT oggParserCtor;
static struct ParserUriKeyInfoS oggKeyInfo =
{
    "ogg",
    {NULL},  /*scheme*/
    {".ogm", ".ogg"}, /*suffix*/
    {NULL}   /*attribute*/
};

#ifdef __ANDROID__
extern CdxParserCreatorT m3u9ParserCtor;
static struct ParserUriKeyInfoS m3u9KeyInfo =
{
    "m3u9",
    {NULL},  /*scheme*/
    {NULL}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT playlistParserCtor;
static struct ParserUriKeyInfoS playlistKeyInfo =
{
    "playlist",
    {NULL},  /*scheme*/
    {".list"}, /*suffix*/
    {NULL}   /*attribute*/
};
#endif

extern CdxParserCreatorT apeParserCtor;
static struct ParserUriKeyInfoS apeKeyInfo =
{
    "ape",
    {NULL},  /*scheme*/
    {".ape"}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT flacParserCtor;
static struct ParserUriKeyInfoS flacKeyInfo =
{
    "flac",
    {NULL},  /*scheme*/
    {".flac"}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT amrParserCtor;
static struct ParserUriKeyInfoS amrKeyInfo =
{
    "amr",
    {NULL},  /*scheme*/
    {".amr", ".awb"}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT atracParserCtor;
static struct ParserUriKeyInfoS atracKeyInfo =
{
    "atrac",
    {NULL},  /*scheme*/
    {".omg", ".oma", ".aa3"}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT mp3ParserCtor;
static struct ParserUriKeyInfoS mp3KeyInfo =
{
    "mp3",
    {NULL},  /*scheme*/
    {".mp1", ".mp2", ".mp3"}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT aacParserCtor;
static struct ParserUriKeyInfoS aacKeyInfo =
{
    "aac",
    {NULL},  /*scheme*/
    {".aac"}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT wavParserCtor;
static struct ParserUriKeyInfoS wavKeyInfo =
{
    "wav",
    {NULL},  /*scheme*/
    {".wav"}, /*suffix*/
    {NULL}   /*attribute*/
};

#ifdef __ANDROID__
extern CdxParserCreatorT mmshttpParserCtor;
static struct ParserUriKeyInfoS mmshttpKeyInfo =
{
    "mmshttp",
    {NULL},  /*scheme*/
    {NULL}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT awtsParserCtor;
static struct ParserUriKeyInfoS awtsKeyInfo =
{
    "awts",
    {NULL},  /*scheme*/
    {".awts"}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT sstrParserCtor;
static struct ParserUriKeyInfoS sstrKeyInfo =
{
    "sstr",
    {NULL},  /*scheme*/
    {".ism", ".isml", "(format=m3u8-aapl)"}, /*suffix*/
    {NULL}   /*attribute*/
};
#endif

extern CdxParserCreatorT cafParserCtor;
static struct ParserUriKeyInfoS cafKeyInfo =
{
    "caf",
    {NULL},  /*scheme*/
    {".caf", ".CAF"}, /*suffix*/
    {NULL}   /*attribute*/
};
extern CdxParserCreatorT g729ParserCtor;
static struct ParserUriKeyInfoS g729KeyInfo =
{
    "g729a",
    {NULL},  /*scheme*/
    {".g729"}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT id3v2ParserCtor;
static struct ParserUriKeyInfoS id3v2KeyInfo =
{
    "id3v2",
    {NULL},  /*scheme*/
    {NULL}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT plsParserCtor;
static struct ParserUriKeyInfoS plsKeyInfo =
{
    "pls",
    {"http"},  /*scheme*/
    {".pls"}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT dsdParserCtor;
static struct ParserUriKeyInfoS dsdKeyInfo =
{
    "dsd",
    {NULL},  /*scheme*/
    {".dff"}, /*suffix*/
    {NULL}   /*attribute*/
};

extern CdxParserCreatorT aiffParserCtor;
static struct ParserUriKeyInfoS aiffKeyInfo =
{
    "aiff",
    {NULL},  /*scheme*/
    {".aiff","aif"}, /*suffix*/
    {NULL}   /*attribute*/
};

#ifdef __ANDROID__
extern CdxParserCreatorT envParserCtor;
static struct ParserUriKeyInfoS envKeyInfo =
{
    "env",
    {"envelope"},  /*scheme*/
    {".env"}, /*suffix*/
    {NULL}   /*attribute*/
};

#endif

#if ENABLE_RAW_STREAM_PARSER
extern CdxParserCreatorT rawStreamParserCtor;
static struct ParserUriKeyInfoS rawStreamKeyInfo =
{
    "raw_stream",
    {NULL},  /*scheme*/
    {".bin", ".h265", ".265", ".hevc", ".h264", ".avc"}, /*suffix*/
    {NULL}   /*attribute*/
};
#endif

#if ENABLE_SPECIAL_PARSER
extern CdxParserCreatorT specialStreamParserCtor;
static struct ParserUriKeyInfoS specialStreamKeyInfo =
{
    "special_stream",
    {NULL},  /*scheme*/
    {".awsp"}, /*suffix*/
    {NULL}   /*attribute*/
};
#endif

int AwParserRegister(CdxParserCreatorT *creator, CdxParserTypeT type,
                    struct ParserUriKeyInfoS *keyInfo)
{
    struct CdxParserNodeS *parserNode;

    parserNode = malloc(sizeof(*parserNode));
    parserNode->creator = creator;
    parserNode->type = type;
    parserNode->keyInfo = keyInfo;

    if(parserInit == 0)
    {
        CdxListInit(&parserList.list);
        parserList.size = 0;
        parserInit = 1;
    }
    CdxListAddTail(&parserNode->node, &parserList.list);
    parserList.size++;
    return 0;
}

static void AwParserInit(void) __attribute__((constructor));
void AwParserInit(void)
{
    CDX_LOGI("aw parser init...");

    /* Make HLS be the first one since:
     * 1. HLS is widely used
     * 2. HLS parser's probe funtion is simple and reliable
     * 3. Other parsers' probe function are not reliable, so m3u8 can be
     * identified as other formats like TS.
     *
     * zhaozhili, 2016-11-21
     */
    //AwParserRegister(&hlsParserCtor, CDX_PARSER_HLS, &hlsKeyInfo);

    //AwParserRegister(&asfParserCtor, CDX_PARSER_ASF, &asfKeyInfo);
    AwParserRegister(&movParserCtor, CDX_PARSER_MOV, &movKeyInfo);
    //AwParserRegister(&remuxParserCtor, CDX_PARSER_REMUX, &remuxKeyInfo);

    //AwParserRegister(&flvParserCtor, CDX_PARSER_FLV, &flvKeyInfo);
    //AwParserRegister(&aviParserCtor, CDX_PARSER_AVI, &aviKeyInfo);
    AwParserRegister(&tsParserCtor, CDX_PARSER_TS, &tsKeyInfo);
    //AwParserRegister(&plsParserCtor, CDX_PARSER_PLS, &plsKeyInfo);

#ifdef __ANDROID__
    AwParserRegister(&dashParserCtor, CDX_PARSER_DASH, &dashKeyInfo);
    AwParserRegister(&mmsParserCtor, CDX_PARSER_MMS, &mmsKeyInfo);
    AwParserRegister(&mmshttpParserCtor, CDX_PARSER_MMSHTTP, &mmshttpKeyInfo);
#endif

    //AwParserRegister(&mkvParserCtor, CDX_PARSER_MKV, &mkvKeyInfo);

#ifdef __ANDROID__
    AwParserRegister(&bdParserCtor, CDX_PARSER_BD, &bdKeyInfo);
    AwParserRegister(&pmpParserCtor, CDX_PARSER_PMP, &pmpKeyInfo);
#endif

    //AwParserRegister(&oggParserCtor, CDX_PARSER_OGG, &oggKeyInfo);

#ifdef __ANDROID__
    AwParserRegister(&m3u9ParserCtor, CDX_PARSER_M3U9, &m3u9KeyInfo);
    AwParserRegister(&playlistParserCtor, CDX_PARSER_PLAYLIST, &playlistKeyInfo);

    AwParserRegister(&envParserCtor, CDX_PARSER_ENV, &envKeyInfo);
#endif

    AwParserRegister(&mpgParserCtor, CDX_PARSER_MPG, &mpgKeyInfo);
    //AwParserRegister(&apeParserCtor, CDX_PARSER_APE, &apeKeyInfo);
    //AwParserRegister(&flacParserCtor, CDX_PARSER_FLAC, &flacKeyInfo);
    //AwParserRegister(&amrParserCtor, CDX_PARSER_AMR, &amrKeyInfo);
#ifdef __ANDROID__
    AwParserRegister(&atracParserCtor, CDX_PARSER_ATRAC, &atracKeyInfo);
#endif
    AwParserRegister(&mp3ParserCtor, CDX_PARSER_MP3, &mp3KeyInfo);
    AwParserRegister(&aacParserCtor, CDX_PARSER_AAC, &aacKeyInfo);
    AwParserRegister(&wavParserCtor, CDX_PARSER_WAV, &wavKeyInfo);

#ifdef __ANDROID__
    AwParserRegister(&awtsParserCtor, CDX_PARSER_AWTS, &awtsKeyInfo);
    AwParserRegister(&sstrParserCtor, CDX_PARSER_SSTR, &sstrKeyInfo);
    AwParserRegister(&cafParserCtor, CDX_PARSER_CAF, &cafKeyInfo);
    AwParserRegister(&g729ParserCtor, CDX_PARSER_G729, &g729KeyInfo);
    AwParserRegister(&dsdParserCtor, CDX_PARSER_DSD, &dsdKeyInfo);
    AwParserRegister(&aiffParserCtor, CDX_PARSER_AIFF, &aiffKeyInfo);
#endif

    AwParserRegister(&id3v2ParserCtor, CDX_PARSER_ID3V2, &id3v2KeyInfo);
#if ENABLE_RAW_STREAM_PARSER
    AwParserRegister(&rawStreamParserCtor, CDX_PARSER_AWRAWSTREAM, &rawStreamKeyInfo);
#endif
#if ENABLE_SPECIAL_PARSER
    AwParserRegister(&specialStreamParserCtor, CDX_PARSER_AWSPECIALSTREAM, &specialStreamKeyInfo);
#endif //ENABLE_SPECIAL_PARSER
    CDX_LOGD("aw parser size:%d",parserList.size);
    return ;
}

static struct CdxParserNodeS *ParserTypeGuess(cdx_char *uri)
{
    if(!uri)
    {
        return NULL;
    }
    char *myUri, *pos;
    char *scheme = NULL, *suffix = NULL, *attr = NULL;
    char *colon = NULL, *dot = NULL, *question = NULL;

    struct CdxParserNodeS *psrNode = NULL, *retPsrNode = NULL;

    myUri = strdup(uri);
    pos = myUri;

    colon = strstr(myUri, "://");
    if (colon)
    {
        *colon = 0;
        scheme = pos;
        pos = colon + 1;
    }

    question = strchr(pos, '?');
    if (question)
    {
        *question = 0;
        attr = question + 1;
    }

    dot = strrchr(pos, '.');
    if (dot)
    {
        suffix = dot;
    }

    CdxListForEachEntry(psrNode, &parserList.list, node)
    {
        int i = 0;
        struct ParserUriKeyInfoS *keyInfo = psrNode->keyInfo;
        if (!keyInfo)
        {
            continue;
        }

        if (suffix)
        {
            for (i = 0; i < 10; i++)
            {
                if (!keyInfo->suffix[i])
                {
                    break;
                }

                if (strcasecmp(keyInfo->suffix[i], suffix) == 0)
                {
                    retPsrNode = psrNode;
                    goto out;
                }
            }
        }

        if (scheme)
        {
            for (i = 0; i < 10; i++)
            {
                if (!keyInfo->scheme[i])
                {
                    break;
                }

                if (strcasecmp(keyInfo->scheme[i], scheme) == 0)
                {
                    retPsrNode = psrNode;
                    goto out;
                }
            }
        }

        if (attr)
        {
            for (i = 0; i < 10; i++)
            {
                if (!keyInfo->attr[i])
                {
                    break;
                }

                if (strstr(attr, keyInfo->attr[i]))
                {
                    retPsrNode = psrNode;
                    goto out;
                }
            }
        }
    }

out:
    if (myUri)
    {
        free(myUri);
        myUri = NULL;
    }

    if (retPsrNode)
    {
        CDX_LOGI("I think it's something about '%s', type id(%d)",
            retPsrNode->keyInfo->comment ? retPsrNode->keyInfo->comment : "unknow",
            retPsrNode->type);
    }
    else
    {
        CDX_LOGW("Sorry, I don't know what it is!");
    }
    return retPsrNode;
}

CdxParserT *CdxParserCreate(CdxStreamT *stream, cdx_uint32 flags)
{
    cdx_char *sampleUri = NULL;
    struct CdxParserNodeS *psrNode;
    struct CdxParserNodeS *maxScoreNode = NULL;
    cdx_uint32 score = 0, maxScore = 0;
    CdxParserT *parser = NULL;
    cdx_int32 retryTimes = 0;
    CdxStreamProbeDataT *probeDataOld = NULL;
    union {
        CdxStreamProbeDataT probeData;
        char buf[sizeof(*probeDataOld) + sizeof(probeDataOld->uri[0])];
    } probeDataUnion;
    CdxStreamProbeDataT *probeData = &probeDataUnion.probeData;

    /*fast guess*/
    CdxStreamGetMetaData(stream, STREAM_METADATA_REDIRECT_URI, (void **)&sampleUri);

    if (!sampleUri)
    {
        CdxStreamGetMetaData(stream, STREAM_METADATA_ACCESSIBLE_URI, (void **)&sampleUri);
    }
    if (!sampleUri)
    {
        CdxStreamGetMetaData(stream, "uri", (void **)&sampleUri);
    }

probe:
    probeDataOld = CdxStreamGetProbeData(stream);
    memset(probeData, 0x00, sizeof(CdxStreamProbeDataT));
    probeData->buf = probeDataOld->buf;
    probeData->len = probeDataOld->len;
    probeData->uri[0] = sampleUri;
    psrNode = ParserTypeGuess(sampleUri);

    if (psrNode)
    {
        if (psrNode->creator->probe(probeData) == 100)
        {
            maxScoreNode = psrNode;
            goto found;
        }
    }

    /*fast guess not work, should ask all parser*/
    CdxListForEachEntry(psrNode, &parserList.list, node)
    {
        CDX_CHECK(psrNode->creator);
        CDX_CHECK(psrNode->creator->probe);
        score = psrNode->creator->probe(probeData);

        if (score == 100)
        {
            maxScoreNode = psrNode;
            goto found;
        }
        else if(score > maxScore)
        {
            maxScore = score;
            maxScoreNode = psrNode;
        }

    }

    if(!maxScoreNode || maxScore < 100)
    {
        cdx_uint32 size = 128*1024;//no more than 128*1024
        cdx_int32 ret;
        if(probeDataOld->len < size && retryTimes < 1)
        {
            CDX_LOGD("increase probe size, and try again.");
            ret = CdxStreamControl(stream, STREAM_CMD_SET_PROBE_SIZE, &size);
            if(ret == 0)
            {
                retryTimes++;
                goto probe;
            }
        }
        if(maxScoreNode)
            goto found;
        CDX_LOGW("Sorry, I don't know what it is!");
        return NULL;
    }

found:
    CDX_LOGD("Good, it's '%s'",
            maxScoreNode->keyInfo->comment ? maxScoreNode->keyInfo->comment : "unknow");
    probeDataOld->sync_pos = probeData->sync_pos;
    parser = maxScoreNode->creator->create(stream, flags);
    if (!parser)
        return NULL;
    parser->type = maxScoreNode->type;
    CDX_LOGD("parser type(%d)", parser->type);
    return parser;
}
int CdxParserOpen(CdxStreamT *stream, cdx_uint32 flags, pthread_mutex_t *mutex, cdx_bool *exit,
    CdxParserT **parser, ContorlTask *parserTasks)
{
    if(mutex)
        pthread_mutex_lock(mutex);

    if(exit && *exit)
    {
        CDX_LOGW("open parser user cancel.");
        if(mutex)
            pthread_mutex_unlock(mutex);
        return -1;
    }
    *parser = CdxParserCreate(stream, flags);

    if(mutex)
        pthread_mutex_unlock(mutex);

    if(!*parser)
    {
        CDX_LOGW("should not be here.");
        return -1;
    }
    int ret;
    while(parserTasks)
    {
        ret = CdxParserControl(*parser, parserTasks->cmd, parserTasks->param);
        if(ret < 0)
        {
            CDX_LOGE("CdxParserControl fail, cmd=%d", parserTasks->cmd);
            return ret;
        }
        parserTasks = parserTasks->next;
    }
    return CdxParserInit(*parser);

}
int CdxParserPrepare(CdxDataSourceT *source, cdx_uint32 flags, pthread_mutex_t *mutex,
        cdx_bool *exit, CdxParserT **parser, CdxStreamT **stream, ContorlTask *parserTasks,
        ContorlTask *streamTasks)
{

    CDX_LOGD("source uri '%s'", source->uri);
    int ret = CdxStreamOpen(source, mutex, exit, stream, streamTasks);
    if (ret < 0)
    {
        CDX_LOGE("open stream fail, uri(%s)", source->uri);
        goto out;
    }

    ret = CdxParserOpen(*stream, flags, mutex, exit, parser, parserTasks);
    if (ret < 0)
    {
        CDX_LOGE("open parser fail, uri(%s)", source->uri);
        goto out;
    }

out:
    return ret;
}

