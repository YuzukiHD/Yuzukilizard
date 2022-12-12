/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxMmshStream.c
 * Description : MmshStream
 * History :
 *
 */

#include "CdxMmsStream.h"
#include <inttypes.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

extern void CdxUrlUnescapeString(char *outbuf, const char *inbuf);
extern CdxUrlT* CdxUrlNew(char* url);
extern void CdxUrlFree(CdxUrlT* curUrl);

extern int Connect2Server(aw_mms_inf_t* mmsStreamInf, char *host, int  port, int verb);
extern int MmsGetNetworkData(aw_mms_inf_t* mmsStreamInf, int fs, char* data, int dataLen);

int MmshStreamingReadFunc(aw_mms_inf_t* mmsStreamInf, int wantSize);
int MmshNopStreamingReadFunc(aw_mms_inf_t* mmsStreamInf, int wantSize);

int CdxNopStreamingSeek(aw_mms_inf_t* mmsStreamInf);
int CdxMmshStreamingSeek( aw_mms_inf_t* mmsStreamInf);

static char *CdxMmshGetNextField(CdxHttpHeaderT* httpHdr)
{
    char *ptr;
    CdxHttpFieldT *field;

    if(httpHdr==NULL )
    {
        return NULL;
    }

    field = httpHdr->fieldSearchPos;
    while(field!=NULL)
    {
        ptr = strstr( field->fieldName, ":" );
        if(ptr==NULL)
        {
            return NULL;
        }
        if(!strncasecmp( field->fieldName, httpHdr->fieldSearch, ptr-(field->fieldName)))
        {
            ptr++;      // Skip the column
            while(ptr[0]==' ')
            {
                ptr++; // Skip the spaces if there is some
            }
            httpHdr->fieldSearchPos = field->next;
            return ptr;    // return the value without the field name
        }
        field = field->next;
    }
    return NULL;
}

static char *CdxMmshGetField(CdxHttpHeaderT *httpHdr, const char *fieldName)
{
    if(httpHdr==NULL || fieldName==NULL)
    {
        return NULL;
    }
    httpHdr->fieldSearchPos = httpHdr->firstField;
    httpHdr->fieldSearch = realloc(httpHdr->fieldSearch, strlen(fieldName)+1 );
    if(httpHdr->fieldSearch==NULL)
    {
        return NULL;
    }
    strcpy(httpHdr->fieldSearch, fieldName);
    return CdxMmshGetNextField(httpHdr);
}

static void CdxMmshFree(CdxHttpHeaderT *httpHdr)
{
    CdxHttpFieldT *field = NULL;
    CdxHttpFieldT *field2free = NULL;

    if(httpHdr==NULL)
    {
        return;
    }
    if(httpHdr->protocol!=NULL)
    {
        free(httpHdr->protocol);
        httpHdr->protocol = NULL;
    }
    if(httpHdr->uri!=NULL)
    {
        free(httpHdr->uri);
        httpHdr->uri = NULL;
    }
    if(httpHdr->reasonPhrase!=NULL)
    {
        free(httpHdr->reasonPhrase);
        httpHdr->reasonPhrase = NULL;
    }
    if(httpHdr->fieldSearch!=NULL)
    {
        free(httpHdr->fieldSearch);
        httpHdr->fieldSearch = NULL;
    }
    if(httpHdr->method!=NULL)
    {
        free(httpHdr->method);
        httpHdr->method = NULL;
    }
    if(httpHdr->buffer!=NULL)
    {
        free(httpHdr->buffer);
        httpHdr->buffer = NULL;
    }
    field = httpHdr->firstField;
    while(field!=NULL)
    {
        field2free = field;
        if(field->fieldName)
        {
            free(field->fieldName);
            field->fieldName = NULL;
        }
        field = field->next;
        {
            free(field2free);
            field2free = NULL;
        }
    }
    free(httpHdr);
//    httpHdr = NULL;
}

static CdxHttpHeaderT *CdxMmshNewHeader(void)
{
    CdxHttpHeaderT *httpHdr = NULL;

    httpHdr = malloc(sizeof(CdxHttpHeaderT));
    if(httpHdr==NULL )
    {
        return NULL;
    }
    memset(httpHdr, 0, sizeof(CdxHttpHeaderT));

    return httpHdr;
}

static int CdxMmshResponseAppend(CdxHttpHeaderT *httpHdr, char *response, int length)
{
    if(httpHdr==NULL || response==NULL || length<0 )
    {
        return -1;
    }
    if((unsigned)length > SIZE_MAX - httpHdr->bufferSize - 1)
    {
        CDX_LOGE("********************Bad size in memory (re)allocation\n");
        return -1;
    }
    httpHdr->buffer = realloc(httpHdr->buffer, httpHdr->bufferSize+length+1);
   // buffer: response(2K). why realloc:in while, may append one more times..
    if(httpHdr->buffer==NULL)
    {
        CDX_LOGE("********************Memory (re)allocation failed\n");
        return -1;
    }
    memcpy(httpHdr->buffer + httpHdr->bufferSize, response, length);
    httpHdr->bufferSize += length;

    httpHdr->buffer[httpHdr->bufferSize] = 0; // close the string!
    return httpHdr->bufferSize;
}

static int CdxMmshIsHeaderEntire(CdxHttpHeaderT *httpHdr)
{
    if(httpHdr==NULL)
    {
        return -1;
    }
    if(httpHdr->buffer==NULL)
    {
        return 0; // empty
    }
    if(strstr(httpHdr->buffer, "\r\n\r\n")==NULL && strstr(httpHdr->buffer, "\n\n")==NULL )
    {
        return 0;
    }
    return 1;
}

void CdxMmshSetField(CdxHttpHeaderT *httpHdr, const char *fieldName)
{
    CdxHttpFieldT *newField = NULL;
    if(httpHdr==NULL || fieldName==NULL)
    {
        return;
    }
    newField = malloc(sizeof(CdxHttpFieldT));
    if(newField==NULL)
    {
        CDX_LOGE("**********************************Memory allocation failed\n");
        return;
    }
    newField->next = NULL;
    newField->fieldName = malloc(strlen(fieldName)+1);
    if(newField->fieldName==NULL)
    {
        CDX_LOGE("*************************Memory allocation failed\n");
        free(newField);
        newField = NULL;
        return;
    }
    strcpy(newField->fieldName, fieldName);

    if(httpHdr->lastField==NULL)
    {
        httpHdr->firstField = newField;
    }
    else
    {
        httpHdr->lastField->next = newField;
    }
    httpHdr->lastField = newField;
    httpHdr->fieldNb++;
}

int CdxMmshResponseParse(CdxHttpHeaderT *httpHdr)
{
    char *hdrPtr = NULL;
    char *ptr     = NULL;
    char *field=NULL;
    int posHdrSep = 0;
    int hdrSepLen = 0;
    size_t len;

    if(httpHdr==NULL)
    {
        return -1;
    }
    if(httpHdr->isParsed)
    {
        return 0;
    }
    // Get the protocol
    hdrPtr = strstr(httpHdr->buffer, " " );//"HTTP/1.1 "
    if(hdrPtr==NULL )
    {
        CDX_LOGE("*********************Malformed answer. No space separator found.\n");
        return -1;
    }
    len = hdrPtr-httpHdr->buffer;
    httpHdr->protocol = malloc(len+1);
    if(httpHdr->protocol==NULL )
    {
        return -1;
    }
    strncpy(httpHdr->protocol, httpHdr->buffer, len );
    httpHdr->protocol[len]='\0';
    if( !strncasecmp( httpHdr->protocol, "HTTP", 4) ) {
        if( sscanf( httpHdr->protocol+5,"1.%d", &(httpHdr->httpMinorVersion) )!=1 )
         //--write to httpHdr->httpMinorVersion
        {
            CDX_LOGE("******Malformed answer. Unable to get HTTP minor version.\n");
            return -1;
        }
    }

    // Get the status code
    if(sscanf(++hdrPtr, "%d", &(httpHdr->statusCode) )!=1 )
    {
        CDX_LOGE("*******Malformed answer. Unable to get status code.\n");
        return -1;
    }
    hdrPtr += 4;

    // Get the reason phrase
    ptr = strstr(hdrPtr, "\n" );
    if(ptr==NULL)
    {
        CDX_LOGE("******Malformed answer. Unable to get the reason phrase.\n");
        return -1;
    }
    len = ptr-hdrPtr;
    httpHdr->reasonPhrase = malloc(len+1);
    if(httpHdr->reasonPhrase==NULL)
    {
        CDX_LOGE("*************************Memory allocation failed\n");
        return -1;
    }
    strncpy(httpHdr->reasonPhrase, hdrPtr, len );
    if(httpHdr->reasonPhrase[len-1]=='\r' )
    {
        len--;
    }
    httpHdr->reasonPhrase[len]='\0';

    // Set the position of the header separator: \r\n\r\n
    hdrSepLen = 4;
    ptr = strstr(httpHdr->buffer, "\r\n\r\n" );
    if(ptr==NULL)
    {
        ptr = strstr(httpHdr->buffer, "\n\n" );
        if(ptr==NULL)
        {
            CDX_LOGE("******Header may be incomplete. No CRLF CRLF found.\n");
            return -1;
        }
        hdrSepLen = 2;
    }
    posHdrSep = ptr-httpHdr->buffer;

    // Point to the first line after the method line.
    hdrPtr = strstr( httpHdr->buffer, "\n" )+1;
    //--set all the response fields
    do
    {
        ptr = hdrPtr;
        while( *ptr!='\r' && *ptr!='\n' )
        {
            ptr++;
        }
        len = ptr-hdrPtr;
        if(len==0)
        {
            break;
        }

        char* tmp;
        tmp = realloc(field, len+1);
        if(tmp==NULL)
        {
            return -1;
        }
        field = tmp;

        strncpy( field, hdrPtr, len );
        field[len]='\0';
        CdxMmshSetField( httpHdr, field );
        hdrPtr = ptr+((*ptr=='\r')?2:1);
    } while( hdrPtr<(httpHdr->buffer+posHdrSep));

    if(field!=NULL)
    {
        free(field);
    }

    if(posHdrSep + hdrSepLen < (int)httpHdr->bufferSize)
    {
        // Response has data!
        httpHdr->body = httpHdr->buffer + posHdrSep + hdrSepLen;
        httpHdr->bodySize = httpHdr->bufferSize - (posHdrSep + hdrSepLen);
    }

    httpHdr->isParsed = 1;
    return 0;
}

void CdxMmshSetUri(CdxHttpHeaderT *httpHdr, const char *uri)
{
    if(httpHdr == NULL || uri == NULL)
    {
        return;
    }
    httpHdr->uri = malloc(strlen(uri)+1);
    if(httpHdr->uri == NULL )
    {
        CDX_LOGE("******************Memory allocation failed\n");
        return;
    }
    strcpy(httpHdr->uri, uri);
}

static int CdxMmshPase64Encode(const void *enc, int encLen, char *out, int outMax)
{
    const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char*encBuf = NULL;
    int            outLen = 0;
    unsigned int bits = 0;
    unsigned int shift = 0;

    encBuf = (unsigned char*)enc;
    outLen = 0;
    bits = 0;
    shift = 0;
    outMax &= ~3;

    while(1)
    {
        if(encLen>0)
        {
            // Shift in byte
            bits <<= 8;
            bits |= *encBuf;
            shift += 8;
            // Next byte
            encBuf++;
            encLen--;
        }
        else if(shift>0)
        {
            // Pad last bits to 6 bits - will end next loop
            bits <<= 6 - shift;
            shift = 6;
        }
        else
        {
            // As per RFC 2045, section 6.8,pad output as necessary: 0 to 2 '=' chars.
            while(outLen &3)
            {
                *out++ = '=';
                outLen++;
            }
            return outLen;
        }

        // Encode 6 bit segments
        while(shift>=6)
        {
            if(outLen >= outMax)
            {
                return -1;
            }
            shift -= 6;
            *out = b64[(bits >> shift)&0x3F];
            out++;
            outLen++;
        }
    }
}

int CdxMmshAddBasicAuthentication(CdxHttpHeaderT *httpHdr, const char *username,
                        const char *password)
{
    char *auth = NULL;
    char *usrPass = NULL;
    char *b64UsrPass = NULL;
    int encodedLen = 0;
    int passLen = 0;
    int outLen = 0;
    int res = -1;

    if(httpHdr==NULL || username==NULL)
    {
        return -1;
    }
    if(password!=NULL)
    {
        passLen = strlen(password);
    }

    usrPass = malloc(strlen(username)+passLen+2);
    if(usrPass==NULL)
    {
        CDX_LOGE("***************************Memory allocation failed\n");
        goto out;
    }

    sprintf(usrPass, "%s:%s", username, (password==NULL)?"":password );

    // Base 64 encode with at least 33% more data than the original size
    encodedLen = strlen(usrPass)*2;
    b64UsrPass = malloc(encodedLen);
    if(b64UsrPass==NULL)
    {
        CDX_LOGE("*********************Memory allocation failed\n");
        goto out;
    }

    outLen = CdxMmshPase64Encode(usrPass, strlen(usrPass), b64UsrPass, encodedLen);
    if(outLen < 0)
    {
        CDX_LOGE("************************Base64 out overflow\n");
        goto out;
    }

    b64UsrPass[outLen]='\0';

    auth = malloc(encodedLen+22);
    if(auth == NULL)
    {
        CDX_LOGE("*************************Memory allocation failed\n");
        goto out;
    }

    sprintf(auth, "Authorization: Basic %s", b64UsrPass);
    CdxMmshSetField(httpHdr, auth);
    res = 0;

out:
    if(usrPass)
    {
        free(usrPass);
        usrPass = NULL;
    }
    if(b64UsrPass)
    {
        free(b64UsrPass);
        b64UsrPass = NULL;
    }
    if(auth)
    {
        free(auth);
        auth = NULL;
    }
    return res;
}

static void CdxMmshSetMethod(CdxHttpHeaderT *httpHdr, const char *method)
{
    if(httpHdr == NULL || method == NULL)
    {
        return;
    }
    httpHdr->method = malloc(strlen(method)+1);
    if(httpHdr->method == NULL )
    {
        return;
    }
    strcpy(httpHdr->method, method);
}

char* CdxMmshBuildRequest(CdxHttpHeaderT *httpHdr)
{
    char *ptr = NULL;
    char *uri = NULL;
    int len = 0;
    CdxHttpFieldT *field = NULL;

    if(httpHdr == NULL )
    {
        return NULL;
    }

    if(httpHdr->method == NULL)
    {
        CdxMmshSetMethod(httpHdr, "GET");
    }
    if(httpHdr->uri == NULL)
    {
        CdxMmshSetUri(httpHdr, "/");
    }
    else
    {
        uri = malloc(strlen(httpHdr->uri) + 1);
        if(uri == NULL)
        {
            return NULL;
        }
        strcpy(uri,httpHdr->uri);
    }

    //-- Compute the request length.
    // Add the Method line
    len = strlen(httpHdr->method) + strlen(uri) + 12;//GET uri HTTP/1.1\r\n
    // Add the fields
    field = httpHdr->firstField;
    while(field != NULL)
    {
        len += strlen(field->fieldName) + 2;
        field = field->next;
    }
    // Add the CRLF
    len += 2;
    // Add the body
    if(httpHdr->body != NULL)
    {
        len += httpHdr->bodySize;
    }
    // Free the buffer if it was previously used
    if(httpHdr->buffer != NULL)
    {
        free(httpHdr->buffer);
        httpHdr->buffer = NULL;
    }
    httpHdr->buffer = malloc(len+1);
    if(httpHdr->buffer == NULL)
    {
        CDX_LOGE("**********************Memory allocation failed\n");
        free(uri);
        return NULL;
    }

    httpHdr->bufferSize = len;

    //-- Building the request
    ptr = httpHdr->buffer;
    // Add the method line
    ptr += sprintf( ptr, "%s %s HTTP/1.%u\r\n", httpHdr->method, uri, httpHdr->httpMinorVersion);
   //---minorVersion

    field = httpHdr->firstField;
    // Add the field
    while(field!=NULL)
    {
        ptr += sprintf( ptr, "%s\r\n", field->fieldName);
        field = field->next;
    }

    ptr += sprintf( ptr, "\r\n" );
    // Add the body
    if(httpHdr->body != NULL)
    {
        memcpy( ptr, httpHdr->body, httpHdr->bodySize);
    }

    if(uri)
    {
        free(uri);
        uri = NULL;
    }
    return httpHdr->buffer;
}

int CdxMmshAuthenticate(CdxHttpHeaderT *httpHdr, CdxUrlT *url, int *authRetry)
{
    char *aut;
    char *aut_space;
    if(*authRetry == 1)
    {
        return -1;
    }
    if(*authRetry > 0)
    {
        if(url->username)
        {
            free(url->username);
            url->username = NULL;
        }
        if(url->password)
        {
            free(url->password);
            url->password = NULL;
        }
    }

    aut = CdxMmshGetField(httpHdr, "WWW-Authenticate");
    if(aut!=NULL)
    {
        aut_space = strstr(aut, "realm=");
        if(aut_space!=NULL )
        {
            aut_space += 6;
        }
    }

    (*authRetry)++;
    return 0;
}

//*****************************************************************//
//*****************************************************************//

#define MMST_BUF_SIZE 102400
#define MMST_HDR_BUF_SIZE 8192
#define MMST_MAX_STREAMS 20
#define HTTP_BUFFER_SIZE 2048
#define READ_DATA_BUF_SIZE  128*1024
#define MMS_WAIT_DATA_TIME  30*1000

const char asfStreamHeaderGuid[16] = {0x91, 0x07, 0xdc, 0xb7,      //ASF_Stream_Properties_Object
  0xb7, 0xa9, 0xcf, 0x11, 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65};
const char asfFileHeaderGuid[16] = {0xa1, 0xdc, 0xab, 0x8c,     //ASF_File_Properties_Object
  0x47, 0xa9, 0xcf, 0x11, 0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65};
const char asfStreamGroupGuid[16] = {0xce, 0x75, 0xf8, 0x7b,
//ASF_Stream_Bitrate_Properties_Object
  0x8d, 0x46, 0xd1, 0x11, 0x8d, 0x82, 0x00, 0x60, 0x97, 0xc9, 0xa2, 0xb2};

//*****************************************************************//
//*****************************************************************//

CdxHttpHeaderT* MmshRequest(aw_mms_inf_t* mmsStreamInf,    unsigned int offsetHi,
                      unsigned int offsetLo, int64_t pos)
{
    char str[250];
    char *ptr = NULL;
    int i = 0;
    int enable = 0;
    int length = 0;
    int asfNbStream=0;
    int streamId = 0;
    CdxHttpHeaderT *httpHdr = NULL;
    CdxUrlT *url = NULL;
    CdxUrlT *serverUrl = NULL;
    aw_mmsh_ctrl_t *asfHttpCtrl = NULL;

    // Sanity check

    if(mmsStreamInf == NULL)
    {
        return NULL;
    }
    url = mmsStreamInf->awUrl;
    asfHttpCtrl = (aw_mmsh_ctrl_t*)mmsStreamInf->data;
    if(url==NULL || asfHttpCtrl==NULL)
    {
        return NULL;
    }

    // Common header for all requests.
    httpHdr = CdxMmshNewHeader();

    CdxMmshSetField(httpHdr, "Accept: */*" );
    CdxMmshSetField(httpHdr, "User-Agent: NSPlayer/4.1.0.3856" );
    CdxMmshAddBasicAuthentication(httpHdr, url->username, url->password );

    // Check if we are using a proxy
    if( !strcasecmp( url->protocol, "http_proxy" ))
    {
        serverUrl = CdxUrlNew((url->file)+1);
        if(serverUrl==NULL)
        {
            CdxMmshFree(httpHdr);
            return NULL;
        }
        CdxMmshSetUri(httpHdr, serverUrl->url);
        sprintf(str, "Host: %.220s:%d", serverUrl->hostname, serverUrl->port);
        CdxUrlFree(serverUrl);
    }
    else
    {
        CdxMmshSetUri(httpHdr, url->file);
        sprintf(str, "Host: %.220s:%d", url->hostname, url->port);
    }

    CdxMmshSetField(httpHdr, str );
    CdxMmshSetField(httpHdr, "Pragma: xClientGUID={c77e7400-738a-11d2-9add-0020af0a3278}" );
    sprintf(str,"Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=%u:%u,\
         request-context=%d,max-duration=%d",
            offsetHi, offsetLo, asfHttpCtrl->request, length );
    CdxMmshSetField(httpHdr, str);

    //***********************start for jump ****************************************//
    //****************************************************************************//
    if(pos > 0)
    {
        // Extend http_send_request with possibility to do partial content retrieval
        snprintf(str, sizeof(str), "Range: bytes=%"PRId64"-", (int64_t)pos);
        CdxMmshSetField(httpHdr, str);
    }
    //************************************************************************//
    //***********************end for jump***************************************//

    switch(asfHttpCtrl->streamingType)
    {
        case ASF_TYPE_LIVE:
        case ASF_TYPE_PLAYLIST:
        case ASF_TYPE_PRERECORDED:
            CdxMmshSetField(httpHdr, "Pragma: xPlayStrm=1" );
            ptr = str;
            ptr += sprintf(ptr, "Pragma: stream-switch-entry=");
            if(asfHttpCtrl->nAudio > 0)
            {
                for(i=0; i<asfHttpCtrl->nAudio ; i++)
                 {
                    streamId = asfHttpCtrl->audioStreams[i];
                    if(streamId == asfHttpCtrl->audioId)
                    {
                        enable = 0;
                    }
                    else
                    {
                        enable = 2;
                        continue;
                    }
                    asfNbStream++;
                    ptr += sprintf(ptr, "ffff:%x:%d ", streamId, enable);
                }
            }
            if(asfHttpCtrl->nVideo > 0)
            {
                for( i=0; i<asfHttpCtrl->nVideo ; i++ )
                 {
                    streamId = asfHttpCtrl->videoStreams[i];
                    if(streamId == asfHttpCtrl->videoId)
                    {
                        enable = 0;
                    }
                    else
                    {
                        enable = 2;
                        continue;
                    }
                    asfNbStream++;
                    ptr += sprintf(ptr, "ffff:%x:%d ", streamId, enable);
                }
            }
            CdxMmshSetField(httpHdr, str );
            sprintf( str, "Pragma: stream-switch-count=%d", asfNbStream);
            CdxMmshSetField(httpHdr, str);
            break;
        case ASF_TYPE_REDIRECTOR:
            break;
        case ASF_TYPE_UNKNOWN:
            // First request goes here.
            break;
        default:
            break;
    }

    CdxMmshSetField(httpHdr, "Connection: Close" );
    CdxMmshBuildRequest(httpHdr);
    return httpHdr;
}

static int AsfHeaderCheck(CdxHttpHeaderT *httpHdr)
{
    aw_asf_obj_header_t *objh = NULL;
    if(httpHdr==NULL )
    {
        return -1;
    }
    if(httpHdr->body==NULL || httpHdr->bodySize<sizeof(aw_asf_obj_header_t))
    {
        return -1;
    }
    objh = (aw_asf_obj_header_t*)httpHdr->body;
    if(ASF_LOAD_GUID_PREFIX(objh->guid)==0x75B22630)
    {
        return 0;
    }
    return -1;
}

/*
 * according to content type and feature get the stream type
 * (but what is the mmsh streaming type)
 */
static int MmshStreamingType(char *contentType, char *features, CdxHttpHeaderT *httpHdr)
{
    if(contentType==NULL )
    {
        return ASF_TYPE_UNKNOWN;
    }
    if( !strcasecmp(contentType, "application/octet-stream") ||
        !strcasecmp(contentType, "application/vnd.ms.wms-hdr.asfv1") ||
        // New in Corona, first request
        !strcasecmp(contentType, "application/x-mms-framed") ||  // New in Corana, second request
        !strcasecmp(contentType, "video/x-ms-wmv") ||
        !strcasecmp(contentType, "video/x-ms-asf"))
    {
        if((strstr(features, "broadcast")) && !(strstr(features, "playlist")))
        {
            CDX_LOGD("=====> ASF Live stream\n");
            return ASF_TYPE_LIVE;
        }
        else if ((strstr(features, "broadcast")) && (strstr(features, "playlist")))
        {
            CDX_LOGD("=====> ASF Playlist stream\n");
            return ASF_TYPE_PLAYLIST;
        }
        else
        {
            CDX_LOGD("=====> ASF Prerecorded\n");
            return ASF_TYPE_PRERECORDED;
        }
    }
    else
    {
        // Ok in a perfect world, web servers should be well configured
        // so we could used mime type to know the stream type,
        // but guess what? All of them are not well configured.
        // So we have to check for an asf header :(, but it works :p

        if(httpHdr->bodySize > sizeof(aw_asf_obj_header_t))
        {
            if(AsfHeaderCheck(httpHdr)==0)  // if it is the ASF_Header_Object
            {
                CDX_LOGV("=====> ASF Plain text\n");
                return ASF_TYPE_PLAINTEXT;
            }
            else if( (!strcasecmp(contentType, "text/html")))
            {
                CDX_LOGV("=====> HTML, Player is not a browser...yet!\n");
                return ASF_TYPE_UNKNOWN;
            }
            else
            {
                CDX_LOGV("=====> ASF Redirector\n");
                return ASF_TYPE_REDIRECTOR;
            }
        }
        else
        {
            if(    (!strcasecmp(contentType, "audio/x-ms-wax")) ||
                (!strcasecmp(contentType, "audio/x-ms-wma")) ||
                (!strcasecmp(contentType, "video/x-ms-asf")) ||
                (!strcasecmp(contentType, "video/x-ms-afs")) ||
                (!strcasecmp(contentType, "video/x-ms-wmv")) ||
                (!strcasecmp(contentType, "video/x-ms-wma")) )
            {
                return ASF_TYPE_REDIRECTOR;
            }
            else if( !strcasecmp(contentType, "text/plain") )
            {
                CDX_LOGV("=====> ASF Plain text\n");
                return ASF_TYPE_PLAINTEXT;
            }
            else
            {
                CDX_LOGV("=====> ASF unknown content-type: %s\n", contentType);
                return ASF_TYPE_UNKNOWN;
            }
        }
    }
    return ASF_TYPE_UNKNOWN;
}

/*
 **************************************************************************
 *the response of the first request:
 *  HTTP/1.0 200
    Content-type: application/octet-stream
    Server: Rex 7.1.0.3055
    Cache-Control: no-cache
    Pragma: no-cache
    Pragma: client-id=1616
    Pragma: features="broadcast"

 *  parse the response of http request,
 *  get the Content-Type, Pragma, features and streamingType
 **************************************************************************
 */
static int MmshParseResponse(aw_mmsh_ctrl_t *asfHttpCtrl, CdxHttpHeaderT *httpHdr)
{
    char *contentType = 0;
    char *pragma = 0;
    char features[128] = "\0";
    size_t len = 0;
    char *commaPtr=NULL;
    char *str = NULL;
//    char* end = NULL;

    if(CdxMmshResponseParse(httpHdr)<0)
    {
        return -1;
    }
    CDX_LOGD("status Code = %d", httpHdr->statusCode);
    switch(httpHdr->statusCode)
    {
        case 200:
            break;
        case 401: // Authentication required
            return ASF_TYPE_AUTHENTICATE;
        default:
        {
            return -1;
        }
    }
    contentType = CdxMmshGetField(httpHdr, "Content-Type");
    pragma = CdxMmshGetField(httpHdr, "Pragma");
    while(pragma!=NULL)
    {
         commaPtr=NULL;
        // The pragma line can get severals attributes separeted with a comma ','.
        do
        {
            if(!strncasecmp( pragma, "features=", 9))
            {
                pragma += 9;
                str = pragma;
                if (*str == '"')
                {
                    len++;
                    do
                    {
                        str++;
                        len++;
                        if(len > sizeof(features) - 1)
                        {
                            len = sizeof(features) - 1;
                            break;
                        }
                    }
                    while(*str != '"');
                }
                else
                {
                    len = strlen(pragma);
                    str = pragma + len -1;
                }
//                end = str;

                if(len > sizeof(features) - 1)
                {
                    //mp_msg(MSGT_NETWORK,MSGL_WARN,MSGTR_MPDEMUX_ASF_ASFHTTPParseWarnCuttedPragma,
                    //pragma,len,sizeof(features) - 1);
                    len = sizeof(features) - 1;
                }
                strncpy( features, pragma, len );
                features[len]='\0';
                break;
            }
            commaPtr = strstr(pragma, "," );
            if(commaPtr!=NULL)
            {
                pragma = commaPtr+1;
                if(pragma[0]==' ' )
                {
                     pragma++;
                }
            }
        }
        while(commaPtr!=NULL );
        pragma = CdxMmshGetNextField(httpHdr);
    }
    asfHttpCtrl->streamingType = MmshStreamingType(contentType, features, httpHdr);

    return 0;
}

//**************************************************************************************//
//**************************************************************************************//
int NopStreamingRead(int fd, char *buffer, int size, aw_mms_inf_t* mmsStreamInf)
{
    int len = 0;
    int ret = 0;
    int bufferLen = 0;

    //* asfHttpDataBuffer中的数据时在解析asf header object的时候 recv的，
    //需要先确认其中的数据是否已经copy到mmsStreamInf->buffer中
    //* 如果asfHttpDataBuffer中还有数据将它拷贝到buffer中，然后释放其内存
    if(mmsStreamInf->asfHttpDataBuffer != NULL) /*mmsh*/
    {
        bufferLen = mmsStreamInf->asfHttpDataSize - mmsStreamInf->asfHttpBufPos;
        len = (size<bufferLen)? size : bufferLen;
        memcpy(buffer, mmsStreamInf->asfHttpDataBuffer+mmsStreamInf->asfHttpBufPos, len);
        mmsStreamInf->asfHttpBufPos += len;
        if(mmsStreamInf->asfHttpBufPos >= mmsStreamInf->asfHttpDataSize)
        {
            free(mmsStreamInf->asfHttpDataBuffer);
            mmsStreamInf->asfHttpDataBuffer = NULL;
            mmsStreamInf->asfHttpDataSize = 0;
            mmsStreamInf->asfHttpBufPos = 0;
        }
    }

    if(len<size)
    {
        while(len < size)
        {

            ret = MmsGetNetworkData(mmsStreamInf, fd, buffer+len, size-len);
            if(ret <= 0)
            {
                CDX_LOGW("******************ret=%d, want=%d\n", ret, size-len);
                return -1;
            }
            len += ret;
        }
    }
    return len;
}

static int AsfReadWrapper(int fd, void *buffer, int len, aw_mms_inf_t* mmsStreamInf)
{
    char *buf = buffer;
    while(len > 0)
    {
        int got = NopStreamingRead(fd, buf, len, mmsStreamInf);
        if(got <= 0)
        {
            return got;
        }
        buf += got;
        len -= got;
    }
    return 1;
}

static int AsfStreaming(aw_asf_stream_chunck_t *streamChunck, int *dropPacket)
{
    if(dropPacket!=NULL )
    {
        *dropPacket = 0;
    }
    if(streamChunck->size<8)
    {
        printf("****************streamChunck->size=%d\n", streamChunck->size);
        return -1;
    }
    if(streamChunck->size != streamChunck->sizeConfirm)
    {
        printf("****************streamChunck->size=%d, streamChunck->sizeConfirm=%d\n",
            streamChunck->size,streamChunck->sizeConfirm);
        return -1;
    }

    switch(streamChunck->type)
    {
        case ASF_STREAMING_CLEAR:    // $C    Clear ASF configuration
            printf("=====> Clearing ASF stream configuration!\n");
            if(dropPacket!=NULL )
            {
                *dropPacket = 1;
            }
            return streamChunck->size;
            break;
        case ASF_STREAMING_DATA:    // $D    Data follows
            printf("=====> Data follows\n");
            break;
        case ASF_STREAMING_END_TRANS:    // $E    Transfer complete
            printf("=====> Transfer complete\n");
            if(dropPacket!=NULL)
            {
                *dropPacket = 1;
            }
            return streamChunck->size;
            break;
        case ASF_STREAMING_HEADER:    // $H    ASF header chunk follows
            printf("=====> ASF header chunk follows\n");
            break;
        default:
            printf("=====> Unknown stream type 0x%x\n", streamChunck->type);
    }
    return streamChunck->size+4;
}

/*
*******************************************************************************
*      chunk type  :  ASF_STREAMING_DATA                 return 0;
*                                ASF_STREAMING_CLEAR               return 1;
*                                ASF_STREAMING_HEADER            return 2;
*                                ASF_STREAMING_END_TRANS     return 3;
*                                 UNKNOWN                                           return -1
********************************************************************************
*/
int AsfStreamingType(aw_asf_stream_chunck_t *streamChunck, int *dropPacket)
{
    int ret = 0;
    if(dropPacket!=NULL )
    {
        *dropPacket = 0;
    }
    switch(streamChunck->type)
    {
        case ASF_STREAMING_CLEAR:       // $C    Clear ASF configuration
            CDX_LOGD("=====> Clearing ASF stream configuration!\n");
            if(dropPacket!=NULL )
            {
                *dropPacket = 1;
            }
            ret = 1;
            break;
        case ASF_STREAMING_DATA:        // $D    Data follows
            CDX_LOGV("=====> Data follows\n");
            ret = 0;
            break;
        case ASF_STREAMING_END_TRANS:    // $E    Transfer complete
            CDX_LOGD("=====> Transfer complete, End of stream\n");
            if(dropPacket!=NULL)
            {
                *dropPacket = 1;
            }
            ret = 3;
            break;
        case ASF_STREAMING_HEADER:       // $H    ASF header chunk follows
            CDX_LOGD("=====> ASF header chunk follows\n");
            ret = 2;
            break;
        default:
            CDX_LOGW("=====> Unknown chunk type 0x%x\n", streamChunck->type);
            ret = -1;
    }
    return ret;
}

/*
 *  find GUID from the position curPos in buf,
 *  bufLen is the size of buf
 */
int MmshFindAsfGuid(char *buf, const char *guid, int curPos, int bufLen)
{
    int i;
    for(i=curPos; i<bufLen - 19; i++)
    {
        if(memcmp(&buf[i], guid, 16) == 0)
        {
            return i + 16 + 8; // point after guid + length
        }
  }
  return -1;
}

int MmshGetMaxIdx(int sCount, int *sRates, int bound)
{
    int i, best = -1, rate = -1;
    for (i = 0; i < sCount; i++)
    {
        if(sRates[i] > rate && sRates[i] <= bound)
        {
            rate = sRates[i];
            best = i;
        }
   }
   return best;
}

static int AsfStreamingParseHeader(int fd, aw_mms_inf_t* mmsStreamInf)
{
    char* buffer=NULL;
    char* chunkBuffer=NULL;
    int i,r,size,pos = 0;
    int start;
    int bufferSize = 0;
    int chunkSize2read = 0;
    int bw = 0;
    int *vRates = NULL;
    int *aRates = NULL;
    int vRate = 0;
    int aRate = 0;
    int aIdx = -1;
    int vIdx = -1;
    aw_asf_stream_chunck_t chunk;
    aw_mmsh_ctrl_t* asfCtrl = NULL;
    #define INT_MAX_X 2147483647

    asfCtrl = mmsStreamInf->data;
    if(asfCtrl == NULL)
    {
        return -1;
    }
    bw = INT_MAX_X;

    // The ASF header can be in several network chunks. For example if the content description
    // is big, the ASF header will be split in 2 network chunk.
    // So we need to retrieve all the chunk before starting to parse the header.
    do
    {
        /* read header object data */
        if(AsfReadWrapper(fd, &chunk, sizeof(aw_asf_stream_chunck_t), mmsStreamInf) < 0)
        {
            return -1;
        }
        // Endian handling of the stream chunk

        le2me_ASF_stream_chunck_t(&chunk);
        size = AsfStreaming(&chunk, &r) - sizeof(aw_asf_stream_chunck_t);  // chunk body size

        if(size < 0)
        {
            CDX_LOGW("****************here1:MSGTR_MPDEMUX_ASF_ErrorParsingChunkHeader");
            goto len_err_out;
        }
        if(chunk.type != ASF_STREAMING_HEADER)  // the first rquest is always the Header Object
        {
            CDX_LOGW("***************MSGTR_MPDEMUX_ASF_NoHeaderAtFirstChunk");
            goto len_err_out;
        }

        // audit: do not overflow buffer_size
        #if 0
        if(size > (int)(SIZE_MAX - bufferSize))
        {
            goto len_err_out;
        }
        #endif

        buffer = malloc(size+bufferSize);
        if(buffer == NULL)
        {
           goto len_err_out;
        }

        if(chunkBuffer!=NULL)
        {
             memcpy(buffer, chunkBuffer, bufferSize);
           free(chunkBuffer);
        }
        chunkBuffer = buffer;
        buffer += bufferSize;
        bufferSize += size;

        //* read Header Object data
        if(AsfReadWrapper(fd, buffer, size, mmsStreamInf) <= 0)
        {
            goto len_err_out;
        }
        if(chunkSize2read==0)
        {
            aw_asf_header_t *asfh = (aw_asf_header_t *)buffer;
            if(size < (int)sizeof(aw_asf_header_t))
            {
                goto len_err_out;
            }
            chunkSize2read = GetLE64(&asfh->objh.size);  /* header object size*/
       }
    } while( bufferSize<chunkSize2read);

    buffer = chunkBuffer;
    size = bufferSize;

    start = sizeof(aw_asf_header_t);

    pos = MmshFindAsfGuid(buffer, asfFileHeaderGuid, start, size);   /*ASF_File_Properties_Object*/
    if(pos >= 0)
    {
        uint8_t *fileh = (uint8_t *)buffer + pos;
        pos += sizeof(aw_asf_file_header_t);
        if (pos > size)
        {
            goto len_err_out;
        }
        asfCtrl->packetSize = GetLE32(fileh + OFFSET_ASF_HEADER(maxPacketSize));
        mmsStreamInf->fileSize  = GetLE64(fileh + OFFSET_ASF_HEADER(fileSize));
        mmsStreamInf->fileDuration =
                    GetLE64(fileh + OFFSET_ASF_HEADER(playDuration))/10000;
        // before playing.
        // preroll: time in ms to bufferize before playing
       /* mmsStreamInf->prebufferSize =
                  (unsigned int)(((unsigned int)fileh->
                  preroll/1000)*((unsigned int)fileh->maxBitrate/8));
                  mmsStreamInf->prebufferSize =
                   (unsigned int)(((double)fileh->
                   preroll/1000.0)*((double)fileh->maxBitrate/8.0));*/
    }
    pos = start;
    while((pos = MmshFindAsfGuid(buffer, asfStreamHeaderGuid, pos, size)) >= 0)
   /*ASF_Stream_Properties_Object GUID and object size*/
    {
         /* data after object size in ASF_Stream_Properties_Object */
        aw_asf_stream_header_t *streamh = (aw_asf_stream_header_t *)&buffer[pos];
        pos += sizeof(aw_asf_stream_header_t);
        if (pos > size)
        {
            goto len_err_out;
        }
        switch(ASF_LOAD_GUID_PREFIX(streamh->type))
        {

            case 0xF8699E40 :     // ASF_Audio_Media  audio stream
                if(asfCtrl->audioStreams == NULL)
                {
                    asfCtrl->audioStreams = malloc(sizeof(int));
                    asfCtrl->nAudio = 1;
                }
                else
                {
                    asfCtrl->nAudio++;
                    asfCtrl->audioStreams = realloc(asfCtrl->audioStreams,
                                       asfCtrl->nAudio*sizeof(int));
                }
                asfCtrl->audioStreams[asfCtrl->nAudio-1] = AV_RL16(&streamh->streamNo);
                break;
           case 0xBC19EFC0 : // ASF_Vedio_Media   video stream
                if(asfCtrl->videoStreams == NULL)
                {
                    asfCtrl->videoStreams = malloc(sizeof(int));
                    asfCtrl->nVideo = 1;
                }
                else
                {
                    asfCtrl->nVideo++;
                    asfCtrl->videoStreams = realloc(asfCtrl->videoStreams,
                                       asfCtrl->nVideo*sizeof(int));
                }
                asfCtrl->videoStreams[asfCtrl->nVideo-1] = AV_RL16(&streamh->streamNo);
                break;
         }
     }

     // always allocate to avoid lots of ifs later
     vRates = calloc(asfCtrl->nVideo, sizeof(int));
     aRates = calloc(asfCtrl->nAudio, sizeof(int));

     pos = MmshFindAsfGuid(buffer, asfStreamGroupGuid, start, size);
    /*ASF_Stream_Bitrate_Properties_Object*/
     if (pos >= 0)
     {
         // stream bitrate properties object
         int streamCount;
         char *ptr = &buffer[pos];
         char *end = &buffer[size];
         if (ptr + 2 > end)
         {
             goto len_err_out;
         }
         streamCount = AV_RL16(ptr);    //* bitrate records count
         ptr += 2;
         for( i=0 ; i<streamCount ; i++ )
         {
             unsigned int  rate;
             int id;
             int j;
             if (ptr + 6 > end)
             {
                 goto len_err_out;
             }
             id = AV_RL16(ptr);        /*flags in Bitrate Records*/
             ptr += 2;
             rate = GetLE32(ptr);
             ptr += 4;
             for (j = 0; j < asfCtrl->nVideo; j++)
             {
                 if(id == asfCtrl->videoStreams[j])   //* find the proper stream number
                 {
                     vRates[j] = rate;
                     break;
                 }
             }
             for (j = 0; j < asfCtrl->nAudio; j++)
             {
                 if (id == asfCtrl->audioStreams[j])
                 {
                     aRates[j] = rate;
                     break;
                 }
             }
         }
     }

     free(buffer);
     buffer = NULL;

     // automatic stream selection based on bandwidth
     if (bw == 0)
     {
         bw = INT_MAX_X;
     }
     if (asfCtrl->nAudio)
     {
         // find lowest-bitrate audio stream
         aRate = aRates[0];
         aIdx = 0;
         for (i = 0; i<asfCtrl->nAudio; i++)
         {
             if (aRates[i] < aRate)
             {
                 aRate = aRates[i];
                 aIdx = i;
             }
         }
         if(MmshGetMaxIdx(asfCtrl->nVideo, vRates, bw - aRate) < 0)
         {
             // both audio and video are not possible, try video only next
             aIdx = -1;
             aRate = 0;
         }
     }
     // find best video stream
     vIdx = MmshGetMaxIdx(asfCtrl->nVideo, vRates, bw - aRate);
     if(vIdx >= 0)
     {
         vRate = vRates[vIdx];
     }

     // find best audio stream
     aIdx = MmshGetMaxIdx(asfCtrl->nAudio, aRates, bw - vRate);
     free(vRates);
     free(aRates);
     vRates = NULL;
     aRates = NULL;
     if (aIdx < 0 && vIdx < 0)
     {
         goto len_err_out;
     }
     if(buffer)
     {
         free(buffer);
         buffer = NULL;
     }

#if 0
     if(audioId > 0)
     {
         asfCtrl->audioId = audioId;// a audio stream was forced
     }
     else
#endif
     if(aIdx >= 0)
     {
         asfCtrl->audioId = asfCtrl->audioStreams[aIdx];
     }
#if 0
     else if(asfCtrl->nAudio)
     {
         audio_id = -2;
     }
  #endif
#if 0
     if(video_id > 0)
     {
         asfCtrl->videoId = video_id;    // a video stream was forced
     }
     else
#endif
     if (vIdx >= 0)
     {
         asfCtrl->videoId = asfCtrl->videoStreams[vIdx];
     }
 #if 0
     else if(asfCtrl->nVideo)
     {
         video_id = -2;
     }
#endif

     return 1;

len_err_out:
    if(buffer)
    {
        free(buffer);
        buffer = NULL;
    }
    if (vRates)
    {
        free(vRates);
        vRates = NULL;
    }
    if(aRates)
    {
        free(aRates);
        aRates = NULL;
    }
    return -1;
}

//***************************************************************************//
//**************************************************************************//
static int StreamingBufferize(aw_mms_inf_t* mmsStreamInf, char *buffer, int size)
{
    mmsStreamInf->asfHttpBufSize = 512*1024;
    mmsStreamInf->asfHttpDataBuffer = (char*)malloc(sizeof(char)* mmsStreamInf->asfHttpBufSize);
    if(mmsStreamInf->asfHttpDataBuffer == NULL)
    {
        mmsStreamInf->asfHttpBufSize = size;
        mmsStreamInf->asfHttpDataBuffer = (char*)malloc(sizeof(char)* mmsStreamInf->asfHttpBufSize);
        if(mmsStreamInf->asfHttpDataBuffer == NULL)
        {
            return -1;
        }
    }
    memcpy(mmsStreamInf->asfHttpDataBuffer, buffer, size);
    mmsStreamInf->asfHttpDataSize = size;
    mmsStreamInf->asfHttpBufPos   = 0;
    return size;
}

//**************************************************************//
//**************************************************************//

int MmshStreamingStart(char *uri, aw_mms_inf_t* mmsStreamInf)
{
    int i = 0;
    int r = 0;
    int ret = 0;
    int fd = 0;
    int done = 0;;
    int authRetry = 0;
    CdxUrlT*  url = NULL;
    CdxHttpHeaderT *httpHdr = NULL;
    aw_mmsh_ctrl_t *asfHttpCtrl = NULL;
    char buffer[HTTP_BUFFER_SIZE];
    #define DEMUXER_TYPE_PLAYLIST (2<<16)

    CDX_UNUSE(uri);

    fd = mmsStreamInf->fd;
    url = mmsStreamInf->awUrl;

    asfHttpCtrl = malloc(sizeof(aw_mmsh_ctrl_t));
    if(asfHttpCtrl==NULL)
    {
        CDX_LOGE("*******************memAlloc for asfHttpCtrl failed.\n");
        return -1;
    }
    asfHttpCtrl->packetSize = 0;
    asfHttpCtrl->streamingType = ASF_TYPE_UNKNOWN;
    asfHttpCtrl->request = 1;
    asfHttpCtrl->audioStreams = NULL;
    asfHttpCtrl->videoStreams = NULL;
    asfHttpCtrl->nAudio = 0;
    asfHttpCtrl->nVideo = 0;
    mmsStreamInf->data = (void*)asfHttpCtrl;

    do
    {
        done = 1;
        if(mmsStreamInf->sockFd > 0)
        {
            closesocket(mmsStreamInf->sockFd);
        }

        if(!strcasecmp(url->protocol, "http_proxy"))
        {
            if(url->port == 0)
            {
                url->port = 8080;
            }
        }
        else
        {
            if(url->port == 0)
            {
                url->port = 80;
            }
        }

        fd = Connect2Server(mmsStreamInf, url->hostname, url->port, 1);
        if (fd<0)
        {
            CDX_LOGI("[%s:%u]connect to server failed.\n", url->hostname, url->port);
            return fd;
        }

        //* http GET
        httpHdr = MmshRequest(mmsStreamInf, 0,0,0);   // send data httpHdr
        for(i=0; i < (int)httpHdr->bufferSize; )
        {
            r = send(fd, httpHdr->buffer+i, httpHdr->bufferSize-i, DEFAULT_SEND_FLAGS);
            if(r <0)
            {
                CDX_LOGV("**************MSGTR_MPDEMUX_ASF_SocketWriteError\n");
                goto err_out;
            }
            i += r;
            usleep(MMS_WAIT_DATA_TIME);
        }
        CdxMmshFree(httpHdr);
        httpHdr = CdxMmshNewHeader();  //* recv data httpHdr

        do
        {
            i = MmsGetNetworkData(mmsStreamInf, mmsStreamInf->sockFd, buffer,HTTP_BUFFER_SIZE);
         //recv 2K data every time
            if(i<=0)
            {
                CDX_LOGD("*****MmsGetNetworkData error!");
                goto err_out;
            }
            CdxMmshResponseAppend(httpHdr, buffer, i);
         // copy the data from buffer to httpHdr->buffer
        } while(!CdxMmshIsHeaderEntire(httpHdr));    // until http request header is ended

        httpHdr->buffer[httpHdr->bufferSize] = '\0';

        ret = MmshParseResponse(asfHttpCtrl, httpHdr); //* parse the response of the request
        if(ret < 0)
        {
            CDX_LOGD("***** mmsh parse response error!");
            goto err_out;
        }
        switch(asfHttpCtrl->streamingType)
        {
            case ASF_TYPE_LIVE:
            case ASF_TYPE_PLAYLIST:
            case ASF_TYPE_PRERECORDED:
            case ASF_TYPE_PLAINTEXT:
                if(httpHdr->bodySize > 0)
                {
                    if(StreamingBufferize(mmsStreamInf, httpHdr->body, (int)httpHdr->bodySize) < 0)
                    {
                        CDX_LOGD("***** streaming bufferize error!");
                        goto err_out;
                    }
                }
                if (asfHttpCtrl->streamingType == ASF_TYPE_PLAYLIST)
                {
                    mmsStreamInf->playListFlag = 1;
                }
                else
                {
                    mmsStreamInf->playListFlag = 0;
                }
                if(asfHttpCtrl->request == 1)
                {
                    if(asfHttpCtrl->streamingType != ASF_TYPE_PLAINTEXT)
                    {
                        // First request, we only got the ASF header.
                        ret = AsfStreamingParseHeader(fd,mmsStreamInf);
                        if(ret < 0)
                        {
                            CDX_LOGD("****** streaming parse header error!");
                            goto err_out;
                        }
                        if((asfHttpCtrl->nAudio==0) && (asfHttpCtrl->nVideo==0))
                        {
                            goto err_out;
                        }
                        asfHttpCtrl->request++;
                        done = 0;
                    }
                    else
                    {
                        done = 1;
                    }
                }
                break;
            case ASF_TYPE_REDIRECTOR:
                 CDX_LOGD("-- ASF_TYPE_REDIRECTOR");
                 goto err_out;
                 break;
            case ASF_TYPE_AUTHENTICATE:
                if(CdxMmshAuthenticate(httpHdr, url, &authRetry)<0)
                {
                    goto err_out;
                }
                asfHttpCtrl->streamingType = ASF_TYPE_UNKNOWN;
                done = 0;
                break;
            case ASF_TYPE_UNKNOWN:
            default:
            {
                goto err_out;
            }
        }
        //Check if we got a redirect.
    } while(!done);

    mmsStreamInf->fd = fd;

    CdxMmshFree(httpHdr);
    return 0;

err_out:
    CDX_LOGD("MmshStreamingStart  error");
    if(mmsStreamInf->sockFd > 0)
    {
        closesocket(mmsStreamInf->sockFd);
    }
    mmsStreamInf->fd = -1;
    CdxMmshFree(httpHdr);
    if(asfHttpCtrl != NULL)
    {
        free(asfHttpCtrl);
        asfHttpCtrl = NULL;
        mmsStreamInf->data = NULL;
    }
    return -1;
}

//*****************************************************************************//
//*******************************************************************************//

static int MmshStreamingRead( int fd, char *buffer, int size, aw_mms_inf_t* mmsStreamInf)
{
    aw_asf_stream_chunck_t chunk;
    int chunkSize = 0;
    int dropChunk = 0;
    int wantSize = 0;
    int pos = 0;
    int resetSize = 0;
    int remainDataSize = 0;
    unsigned char buf[8] = {0};
    char* headerBuffptr = NULL;
    aw_mmsh_ctrl_t *asfHttpCtrl = (aw_mmsh_ctrl_t*)mmsStreamInf->data;
    int ret;
    while(1)
    {
        if(mmsStreamInf->exitFlag == 1)
        {
            return -1;
        }

        if(mmsStreamInf->asfHttpDataBuffer == NULL)
        {
            CDX_LOGV("mmsStreamInf->asfHttpDataBuffer == NULL\n");
        }

        memset(&chunk, 0, sizeof(aw_asf_stream_chunck_t));
        if(AsfReadWrapper(fd, buf, 4, mmsStreamInf) <= 0)
        {
            return -1;
        }
        // Endian handling of the stream chunk
        chunk.type = ((buf[1])<<8) | buf[0];
        chunk.size = ((buf[3])<<8) | buf[2];
        resetSize = (chunk.size>8)?  8 : chunk.size;

        if(AsfReadWrapper(fd, buf,resetSize, mmsStreamInf) <= 0)
        {
            return -1;
        }
        chunk.sequenceNumber = (buf[3]<<24)|(buf[2]<<16)|(buf[1]<<8)|buf[0];
        chunk.unknown = (buf[5]<<8)|buf[4];
        if(resetSize < 8)
        {
            chunk.sizeConfirm = 8;
        }
        else
        {
            chunk.sizeConfirm = (buf[7]<<8) | buf[6];
            //chunk.sizeConfirm = buf[7]*16 + buf[6];
        }

        ret = AsfStreamingType(&chunk,&dropChunk);
        if ((ret == 1) && (mmsStreamInf->iostate == CDX_IO_STATE_OK))
        {
            CDX_LOGD("--- CDX_IO_STATE_CLEAR, buf_pos = %llx",
               (long long int)mmsStreamInf->buf_pos);
            mmsStreamInf->iostate = CDX_IO_STATE_CLEAR;

            pthread_mutex_lock(&mmsStreamInf->bufferMutex);
            mmsStreamInf->tmpReadPtr       = mmsStreamInf->bufWritePtr;
            mmsStreamInf->tmpReleasePtr    = mmsStreamInf->tmpReadPtr;
            mmsStreamInf->tmp_dmx_read_pos = mmsStreamInf->buf_pos;
            pthread_mutex_unlock(&mmsStreamInf->bufferMutex);
        }
        else if(ret == 2)  // restart
        {
            CDX_LOGD("--- Header Chunk, buf_pos = %llx", (long long int)mmsStreamInf->buf_pos);
            //mmsStreamInf->iostate = CDX_IO_STATE_INVALID;
        }

        chunkSize = chunk.sizeConfirm-8;

        wantSize = chunkSize;    /* no padding*/
        if(chunk.type != ASF_STREAMING_HEADER && (!dropChunk))
        {
            if(asfHttpCtrl->packetSize < chunkSize)
            {
                CDX_LOGD("**streamTYpe:%d, asfHttpCtrl->packetSize<%d>\
                     is smaller than the chunkSize<%d>, error.",
                     ret, asfHttpCtrl->packetSize, chunkSize);
                return -1;
            }
            wantSize = asfHttpCtrl->packetSize;
        }

        if(chunk.type == ASF_STREAMING_END_TRANS)
        {
            CDX_LOGV("--- stream End");
            return STREAM_READ_END;
        }

        while(1)
        {
            if(mmsStreamInf->exitFlag == 1)
            {
                return STREAM_READ_END;
            }
            if((mmsStreamInf->stream_buf_size-mmsStreamInf->validDataSize) > wantSize)
            {
                break;
            }
            //printf("**************remainSize=%d, wantSize=%d\n",
            //(mmsStreamInf->stream_buf_size-mmsStreamInf->bufferDataSize), wantSize);
            usleep(40*1000);
        }
        if(size < wantSize)
        {

            if(asfHttpCtrl->packetSize>=32*1024 || chunkSize>= 32*1024)
            {
                CDX_LOGD("******asfHttpCtrl->packetSize:%d chunkSize:%d > 32*1024.",
                    asfHttpCtrl->packetSize, chunkSize);
                return -1;
            }
            if(AsfReadWrapper(fd,  mmsStreamInf->asfChunkDataBuffer, chunkSize, mmsStreamInf) <= 0)
            {
                CDX_LOGD("--- AsfReadWrapper error");
                return -1;
            }
            if(dropChunk == 1)
            {
                continue;
            }

            memset(mmsStreamInf->asfChunkDataBuffer+chunkSize,0,wantSize-chunkSize);
            if(mmsStreamInf->bufWritePtr < mmsStreamInf->bufReleasePtr)
            {
                remainDataSize = mmsStreamInf->bufReleasePtr - mmsStreamInf->bufWritePtr;
                if(remainDataSize < wantSize)
                {
                    return -1;
                }
            }
            else
            {
                remainDataSize = mmsStreamInf->bufEndPtr+1-mmsStreamInf->bufWritePtr;
            }
            if(remainDataSize >= wantSize)
            {
                memcpy(buffer,mmsStreamInf->asfChunkDataBuffer, wantSize);
            }
            else
            {
                memcpy(buffer,mmsStreamInf->asfChunkDataBuffer,remainDataSize);
                memcpy(mmsStreamInf->buffer,mmsStreamInf->asfChunkDataBuffer+remainDataSize,
                  wantSize-remainDataSize);
            }
            headerBuffptr = mmsStreamInf->asfChunkDataBuffer;
        }
        else
        {
            if(AsfReadWrapper(fd, buffer, chunkSize, mmsStreamInf) <= 0)
            {
                CDX_LOGD("-- 22 AsfReadWrapper error");
                return -1;
            }
            if(dropChunk == 1)
            {
                continue;
            }
            memset(buffer+chunkSize, 0, wantSize-chunkSize);
            headerBuffptr = buffer;
        }
        break;
    }

    if(chunk.type == ASF_STREAMING_HEADER)
    {
        pos = MmshFindAsfGuid(headerBuffptr, asfFileHeaderGuid, 0, chunkSize);
        if(pos >= 0)
        {
            uint8_t *fileh = (uint8_t *)headerBuffptr + pos;
            asfHttpCtrl->packetSize = GetLE32(fileh + OFFSET_ASF_HEADER(maxPacketSize));
            mmsStreamInf->fileSize  = GetLE64(fileh + OFFSET_ASF_HEADER(fileSize));
            mmsStreamInf->fileDuration =
                    GetLE64(fileh + OFFSET_ASF_HEADER(playDuration))/10000;
        }
        else
        {
            CDX_LOGW("can not find File_Properties_Object");
        }
    }
    return wantSize;
}

//****************************************************************************************//
//****************************************************************************************//

int MmshStreamingReadFunc(aw_mms_inf_t* mmsStreamInf, int wantSize)
{
    int remainDataSize = 64*1024;
    int readLen = 0;

    if((0 < wantSize) && (wantSize < remainDataSize))
    {
        remainDataSize = wantSize;
    }

    readLen = MmshStreamingRead( mmsStreamInf->sockFd,mmsStreamInf->bufWritePtr,
                               remainDataSize,mmsStreamInf);
    if(readLen < 0)
    {
        return STREAM_READ_END;
    }
    else
    {
#ifdef SAVE_MMS
        fwrite(mmsStreamInf->bufWritePtr, 1, readLen, mmsStreamInf->fp);
#endif
        pthread_mutex_lock(&mmsStreamInf->bufferMutex);

        mmsStreamInf->bufWritePtr += readLen;
        if(mmsStreamInf->bufWritePtr > mmsStreamInf->bufEndPtr)
        {
            mmsStreamInf->bufWritePtr -= mmsStreamInf->stream_buf_size;
        }
        mmsStreamInf->bufferDataSize += readLen;
        mmsStreamInf->buf_pos += readLen;
        mmsStreamInf->validDataSize  += readLen;
        pthread_mutex_unlock(&mmsStreamInf->bufferMutex);
        return STREAM_READ_OK;
    }
    return STREAM_READ_OK;
}

//**************************************************************************************//
//*************************************************************************************//

int MmshNopStreamingReadFunc(aw_mms_inf_t* mmsStreamInf, int wantSize)
{
    int remainDataSize = 64*1024;
    int readLen = 0;

    if((0 < wantSize) && (wantSize < remainDataSize))
    {
        remainDataSize = wantSize;
    }

    readLen = NopStreamingRead(mmsStreamInf->fd, mmsStreamInf->bufWritePtr,
                        remainDataSize, mmsStreamInf);

    pthread_mutex_lock(&mmsStreamInf->bufferMutex);
    mmsStreamInf->bufWritePtr+= readLen;
    if(mmsStreamInf->bufWritePtr > mmsStreamInf->bufEndPtr)
    {
        mmsStreamInf->bufWritePtr -= (mmsStreamInf->bufEndPtr-mmsStreamInf->buffer+1);
    }
    mmsStreamInf->bufferDataSize += readLen;
    mmsStreamInf->buf_pos += readLen;
    mmsStreamInf->validDataSize  += readLen;
    pthread_mutex_unlock(&mmsStreamInf->bufferMutex);
    if(readLen != remainDataSize)
    {
        return STREAM_READ_END;
    }
    return STREAM_READ_OK;
}

//****************************************************************************************//
//****************************************************************************************//
int CdxNopStreamingSeek( aw_mms_inf_t* mmsStreamInf)
{
    int fd = 0;
    int i = 0;
    int r = 0;
    char buffer[HTTP_BUFFER_SIZE];
    CdxUrlT*url = NULL;
    aw_mmsh_ctrl_t *asfHttpCtrl = NULL;
    CdxHttpHeaderT *httpHdr = NULL;

    fd = mmsStreamInf->fd;
    url = mmsStreamInf->awUrl;
    asfHttpCtrl = (aw_mmsh_ctrl_t*)mmsStreamInf->data;

    if(mmsStreamInf->sockFd > 0)
    {
        closesocket(mmsStreamInf->sockFd);
    }
    if(!strcasecmp(url->protocol, "http_proxy"))
    {
        if(url->port == 0)
        {
            url->port = 8080;
        }
    }
    else
    {
        if(url->port == 0)
        {
            url->port = 80;
        }
    }

    fd = Connect2Server(mmsStreamInf, url->hostname, url->port, 1);
    if(fd <= 0)
    {
        printf("*********************connect the stream fd=%d\n", fd);
    }
    httpHdr = MmshRequest(mmsStreamInf,0,0, mmsStreamInf->buf_pos);
    for(i=0; i < (int)httpHdr->bufferSize; )
    {
        r = send(fd, httpHdr->buffer+i, httpHdr->bufferSize-i, DEFAULT_SEND_FLAGS);
        if(r <0)
        {
            CDX_LOGV("**************MSGTR_MPDEMUX_ASF_SocketWriteError\n");
            goto err_out;
        }
        i += r;
        usleep(MMS_WAIT_DATA_TIME);
    }
    do
    {
        i = MmsGetNetworkData(mmsStreamInf, fd, buffer,HTTP_BUFFER_SIZE);
        if(i<=0)
        {
            goto err_out;
        }
        CdxMmshResponseAppend(httpHdr, buffer, i);
    } while(!CdxMmshIsHeaderEntire(httpHdr));
    httpHdr->buffer[httpHdr->bufferSize] = '\0';
    mmsStreamInf->fd = fd;
    CdxMmshFree(httpHdr);
    return 0;

err_out:
      mmsStreamInf->fd = -1;
    if(httpHdr != NULL)
    {
        CdxMmshFree(httpHdr);
    }
    if(asfHttpCtrl != NULL)
    {
        free(asfHttpCtrl);
        asfHttpCtrl = NULL;
        mmsStreamInf->data = NULL;
    }
    return -1;
}

//*****************************************************************************************//

int CdxMmshStreamingSeek(aw_mms_inf_t* mmsStreamInf)
{
    aw_mmsh_ctrl_t *asfHttpCtrl = NULL;
    CdxHttpHeaderT *httpHdr = NULL;
    char buffer[HTTP_BUFFER_SIZE];
    aw_asf_stream_chunck_t chunk;
    char*bufPtr = NULL;
    int chunkSize = 0;
    CdxUrlT*url = NULL;
    int readLen = 0;
    int dropChunk = 0;
    int fd = 0;
    int pos = 0;
    int i = 0;
    int r = 0;
    int findValidChunkFlag = 0;
    int resetSize = 0;
    char buf[12] = {0};

    fd = mmsStreamInf->fd;
    url = mmsStreamInf->awUrl;
    asfHttpCtrl = (aw_mmsh_ctrl_t*)mmsStreamInf->data;

    if(mmsStreamInf->sockFd > 0)
    {
        closesocket(mmsStreamInf->sockFd);
    }
    if(!strcasecmp(url->protocol, "http_proxy"))
    {
        if(url->port == 0)
        {
            url->port = 8080;
        }
    }
    else
    {
        if(url->port == 0)
        {
            url->port = 80;
        }
    }

    fd = Connect2Server(mmsStreamInf, url->hostname, url->port, 1);
    if(fd <= 0)
    {
        CDX_LOGW("*********************connect the stream fd=%d\n", fd);
    }
    httpHdr = MmshRequest(mmsStreamInf,(mmsStreamInf->buf_pos>>32)&0xffffffff,
                        mmsStreamInf->buf_pos&0xffffffff, 0);
    for(i=0; i < (int)httpHdr->bufferSize; )
    {
        r = send(fd, httpHdr->buffer+i, httpHdr->bufferSize-i, DEFAULT_SEND_FLAGS);
        if(r <0)
        {
            CDX_LOGV("**************MSGTR_MPDEMUX_ASF_SocketWriteError\n");
            goto err_out;
        }
        i += r;
        usleep(MMS_WAIT_DATA_TIME);
    }
    do
    {
        i = MmsGetNetworkData(mmsStreamInf, fd, buffer,HTTP_BUFFER_SIZE);
        if(i<=0)
        {
            goto err_out;
        }
        CdxMmshResponseAppend(httpHdr, buffer, i);
    } while(!CdxMmshIsHeaderEntire(httpHdr));
    httpHdr->buffer[httpHdr->bufferSize] = '\0';
    mmsStreamInf->fd = fd;
    CdxMmshFree(httpHdr);

    while(1)
    {
        if(mmsStreamInf->exitFlag == 1)
        {
            return 0;
        }
        memset(&chunk, 0, sizeof(aw_asf_stream_chunck_t));
        if(AsfReadWrapper(fd, buf, 4, mmsStreamInf) <= 0)
        {
            return -1;
        }
        // Endian handling of the stream chunk
        chunk.type = (buf[1]<<8)|buf[0];
        chunk.size = (buf[3]<<8)|buf[2];
        mmsStreamInf->buf_pos += 4;
        mmsStreamInf->dmx_read_pos += 4;
        if(findValidChunkFlag==0)
        {
            if((chunk.type==ASF_STREAMING_HEADER)||(chunk.type == ASF_STREAMING_DATA))
            {
                findValidChunkFlag = 1;
            }
            else
            {
                continue;
            }
        }
        resetSize = (chunk.size>8)?  8 : chunk.size;
        if(AsfReadWrapper(fd, buf,resetSize, mmsStreamInf) <= 0)
        {
            return -1;
        }
        chunk.sequenceNumber = (buf[3]<<24)|(buf[2]<<16)|(buf[1]<<8)|buf[0];
        chunk.unknown = (buf[5]<<8)|buf[4];
        if(resetSize < 8)
        {
            chunk.sizeConfirm = 8;
        }
        else
        {
            chunk.sizeConfirm = (buf[7]<<8)|buf[6];
        }
        AsfStreamingType(&chunk,&dropChunk);
        chunkSize = chunk.sizeConfirm-8;
        mmsStreamInf->buf_pos += resetSize;
        mmsStreamInf->dmx_read_pos += resetSize;

        bufPtr = mmsStreamInf->bufWritePtr;
        readLen = AsfReadWrapper(fd, bufPtr,chunkSize, mmsStreamInf);
        if(readLen <= 0)
        {
            goto err_out;
        }

        if(chunk.type == ASF_STREAMING_HEADER)
        {
            pos = MmshFindAsfGuid(bufPtr, asfFileHeaderGuid, 0, chunkSize);
            if(pos >= 0)
            {
                uint8_t *fileh = (uint8_t *)bufPtr + pos;
                asfHttpCtrl->packetSize = GetLE32(fileh + OFFSET_ASF_HEADER(maxPacketSize));
                mmsStreamInf->fileSize  = GetLE64(fileh + OFFSET_ASF_HEADER(fileSize));
                mmsStreamInf->fileDuration =
                    GetLE64(fileh + OFFSET_ASF_HEADER(playDuration))/10000;
            }
            mmsStreamInf->buf_pos += chunkSize;
            mmsStreamInf->dmx_read_pos += chunkSize;
            continue;
        }
        else if(dropChunk == 1)
        {
            mmsStreamInf->buf_pos += chunkSize;
            mmsStreamInf->dmx_read_pos += chunkSize;
            continue;
        }
        else
        {
            break;
        }
    }
    pthread_mutex_lock(&mmsStreamInf->bufferMutex);
    memset(bufPtr+chunkSize, 0, asfHttpCtrl->packetSize-chunkSize);
    mmsStreamInf->bufWritePtr += asfHttpCtrl->packetSize;
    if(mmsStreamInf->bufWritePtr > mmsStreamInf->bufEndPtr)
    {
        mmsStreamInf->bufWritePtr -= mmsStreamInf->stream_buf_size;
    }
    mmsStreamInf->bufferDataSize += asfHttpCtrl->packetSize;
    mmsStreamInf->buf_pos += asfHttpCtrl->packetSize;
    mmsStreamInf->validDataSize  += asfHttpCtrl->packetSize;
    pthread_mutex_unlock(&mmsStreamInf->bufferMutex);
    return 0;

err_out:
    mmsStreamInf->fd = -1;
    if(httpHdr != NULL)
    {
        CdxMmshFree(httpHdr);
    }
    if(asfHttpCtrl != NULL)
    {
        free(asfHttpCtrl);
        asfHttpCtrl = NULL;
        mmsStreamInf->data = NULL;
    }
    return -1;
}

