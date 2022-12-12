/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxHttpField.c
 * Description : Part of http stream.
 * History :
 *
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "CdxHttpField"       //* prefix of the printed messages.
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <CdxStream.h>
#include <CdxHttpStream.h>

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

char* CdxHttpGetNextField(CdxHttpHeaderT* httpHdr)
{
    char *p;
    CdxHttpFieldT *field;

    if(httpHdr==NULL )
    {
        return NULL;
    }

    field = httpHdr->fieldSearchPos;
    while(field!=NULL)
    {
        p = strstr( field->fieldName, ":" );
        if(p==NULL)
        {
            return NULL;
        }
        if(!strncasecmp( field->fieldName, httpHdr->fieldSearch, p-(field->fieldName)))
        {
            p++;    // Skip the column
            while(p[0]==' ')
            {
                p++; // Skip the spaces if there is some
            }
            httpHdr->fieldSearchPos = field->next;
            return p;    // return the value without the field name
        }
        field = field->next;
    }
    return NULL;
}
char *CdxHttpGetFieldFromSearchPos(CdxHttpHeaderT *httpHdr, const char *fieldName,
    CdxHttpFieldT *searchPos)
{
    if(httpHdr==NULL || fieldName==NULL)
    {
        return NULL;
    }
    httpHdr->fieldSearchPos = searchPos;
    httpHdr->fieldSearch = realloc(httpHdr->fieldSearch, strlen(fieldName)+1 );
    if(httpHdr->fieldSearch==NULL)
    {
        return NULL;
    }
    strcpy(httpHdr->fieldSearch, fieldName);
    return CdxHttpGetNextField(httpHdr);
}

char *CdxHttpGetField(CdxHttpHeaderT *httpHdr, const char *fieldName)
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
    return CdxHttpGetNextField(httpHdr);
}

void CdxHttpFree(CdxHttpHeaderT *httpHdr)
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
    free(httpHdr->cookies);
    free(httpHdr);
}

CdxHttpHeaderT *CdxHttpNewHeader(void)
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

int CdxHttpResponseAppend(CdxHttpHeaderT *httpHdr, char *response, int length)
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

int CdxHttpIsHeaderEntire(CdxHttpHeaderT *httpHdr)
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

void CdxHttpSetField(CdxHttpHeaderT *httpHdr, const char *fieldName)
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
char * TrimChar(char *str)
{
    CDX_CHECK(str);
    cdx_uint32 len = strlen(str);
    CDX_CHECK(len);

    cdx_uint32 i = 0;
    while (i < len && isspace(str[i]))
    {
        ++i;
    }
    cdx_uint32 j = len;
    while (j > i && isspace(str[j - 1]))
    {
        --j;
    }
    str[j] = '\0';
    return str + i;
}
int ParseCookie(char *cookie, char **name_value)
{
    *name_value = NULL;
    char *copy = strdup(cookie);
    if(!copy)
    {
        CDX_LOGE("malloc fail.");
        return -1;
    }
    cdx_uint32 offset = 0;
    cdx_uint32 len = strlen(copy);
    int ret = 0;
    //
    {
        char *end = strchr(copy + offset, ';');
        if(end)
        {
            *end = '\0';
        }

        char *attr = TrimChar(copy + offset);
        if(end)
        {
            offset = end - copy + 1;
        }
        else
        {
            offset = len;
        }

        char *equalPos = strstr(attr, "=");
        if (equalPos == NULL)
        {
            goto _exit;
        }
        *name_value = strdup(attr);
    }
    //
    while (offset < len)
    {
        char *end = strchr(copy + offset, ';');
        if(end)
        {
            *end = '\0';
        }

        char *attr = TrimChar(copy + offset);
        if(end)
        {
            offset = end - copy + 1;
        }
        else
        {
            offset = len;
        }

        char *equalPos = strstr(attr, "=");
        if (equalPos == NULL)
        {
            continue;
        }
        *equalPos = '\0';

        char *key = TrimChar(attr);
        char *val = TrimChar(equalPos + 1);
        CDX_LOGD("key=%s value=%s", key, val);
        //tolower_str(key);
        if(!strcasecmp("path", key))
        {
            ret = strlen(val);
            break;
        }
    }
_exit:
    free(copy);
    return ret;

}
void ProcessCookies(CdxHttpHeaderT *httpHdr)
{
#define MaxCookieNum 10
    char *cookies[MaxCookieNum];
    int pathLen[MaxCookieNum];
    int sortIndex[MaxCookieNum] = {0};
    int count = 0;
    CdxHttpFieldT *searchPos = httpHdr->firstField;
    while(searchPos)
    {
        char *cookie = CdxHttpGetFieldFromSearchPos(httpHdr, "Set-Cookie", searchPos);
        if(!cookie || count >= MaxCookieNum)
        {
            break;
        }
        pathLen[count] = ParseCookie(cookie, &cookies[count]);
        if(pathLen[count] <= 0 || cookies[count] == NULL)
        {
            CDX_LOGE("it is a bug.");
            break;
        }
        count++;
        searchPos = httpHdr->fieldSearchPos;
    }

    if(count)
    {
        int i,j,k;

        for(i=0; i<count; i++)
        {
            k=0;
            for (j=1; j<count; j++)
            {
                if (pathLen[j]>pathLen[k])
                    k=j;
            }
            sortIndex[i]=k;
            pathLen[k]=0;
        }
        char *field = (char *)malloc(4096);
        memset(field, 0, 4096);
        //strcpy(field, "Cookie: ");
        char *ptr = field;
        for(i=0; i<count; i++)
        {
            char *cookie = cookies[sortIndex[i]];
            if(i)
            {
                ptr += sprintf( ptr, "; ");
            }
            ptr += sprintf( ptr, "%s", cookie);
        }
        //CdxHttpSetField(httpHdr, field);
        httpHdr->cookies = field;

        for(i=0; i<count; i++)
        {
            free(cookies[i]);
        }
    }
    return;
}
int CdxHttpResponseParse(CdxHttpHeaderT *httpHdr)
{
    char *hdrPtr = NULL;
    char *ptr    = NULL;
    char *field = NULL;
    char *tempField = NULL;
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
        CDX_LOGE("No space separator found.");
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
        if(sscanf(httpHdr->protocol+5,"1.%d", &(httpHdr->httpMinorVersion))!=1)
            //--write to httpHdr->httpMinorVersion
        {
            CDX_LOGE("No HTTP minor version.");
            return -1;
        }
    }

    // Get the status code
    if(sscanf(++hdrPtr, "%d", &(httpHdr->statusCode)) != 1)
    {
        CDX_LOGE("No status code.");
        return -1;
    }
    hdrPtr += 4;

    // Get the reason phrase
    ptr = strstr(hdrPtr, "\n" );
    if(ptr == NULL)
    {
        CDX_LOGE("No reason phrase.");
        return -1;
    }
    len = ptr-hdrPtr;
    httpHdr->reasonPhrase = malloc(len+1);
    if(httpHdr->reasonPhrase==NULL)
    {
        CDX_LOGE("malloc failed.");
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
            CDX_LOGE("No CRLF CRLF found.");
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
        tempField = realloc(field, len+1);
        if(tempField == NULL)
        {
            if(field != NULL)
            {
                free(field);
            }
            return -1;
        }
        field = tempField;
        strncpy( field, hdrPtr, len );
        field[len]='\0';
        CdxHttpSetField( httpHdr, field );
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
//        CDX_LOGD("*************** httpHdr->bodySize = %zu",httpHdr->bodySize);
    }
    httpHdr->posHdrSep = posHdrSep;

    //* print response header
    if(httpHdr->bufferSize > 0)
    {
        char *tmpBuf = calloc(1, httpHdr->bufferSize);
        if(tmpBuf == NULL)
        {
            CDX_LOGE("malloc fail.");
            return -1;
        }
        memcpy(tmpBuf, httpHdr->buffer, posHdrSep);
        CDX_LOGV("===================================================");
        CDX_LOGV("%s", tmpBuf);
        CDX_LOGV("===================================================");
        free(tmpBuf);
    }
    ProcessCookies(httpHdr);

    httpHdr->isParsed = 1;
    return 0;
}

void CdxHttpSetUri(CdxHttpHeaderT *httpHdr, const char *uri)
{
    if(httpHdr == NULL || uri == NULL)
    {
        return;
    }
    httpHdr->uri = malloc(strlen(uri)+1);
    if(httpHdr->uri == NULL )
    {
        CDX_LOGE("malloc failed.");
        return;
    }
    strcpy(httpHdr->uri, uri);
}

static int CdxPase64Encode(const void *pEnc, int encLen, char *out, int nOutMax)
{
    const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char *pEncBuf = NULL;
    int            nOutLen = 0;
    unsigned int uBits = 0;
    unsigned int uShift = 0;

    pEncBuf = (unsigned char *)pEnc;
    nOutLen = 0;
    uBits = 0;
    uShift = 0;
    nOutMax &= ~3;

    while(1)
    {
        if(encLen > 0)
        {
            // Shift in byte
            uBits <<= 8;
            uBits |= *pEncBuf;
            uShift += 8;
            // Next byte
            pEncBuf++;
            encLen--;
        }
        else if(uShift>0)
        {
            // Pad last bits to 6 bits - will end next loop
            uBits <<= 6 - uShift;
            uShift = 6;
        }
        else
        {
            // As per RFC 2045, section 6.8,pad output as necessary: 0 to 2 '=' chars.
            while(nOutLen &3)
            {
                *out++ = '=';
                nOutLen++;
            }
            return nOutLen;
        }

        // Encode 6 bit segments
        while(uShift>=6)
        {
            if(nOutLen >= nOutMax)
            {
                return -1;
            }
            uShift -= 6;
            *out = b64[(uBits >> uShift)&0x3F];
            out++;
            nOutLen++;
        }
    }
}

int CdxHttpAddBasicAuthentication(CdxHttpHeaderT *httpHdr, const char *username,
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
        CDX_LOGE("malloc failed.");
        goto out;
    }

    sprintf(usrPass, "%s:%s", username, (password==NULL) ? "" : password);

    // Base 64 encode with at least 33% more data than the original size
    encodedLen = strlen(usrPass)*2;
    b64UsrPass = malloc(encodedLen);
    if(b64UsrPass==NULL)
    {
        CDX_LOGE("malloc failed.");
        goto out;
    }

    outLen = CdxPase64Encode(usrPass, strlen(usrPass), b64UsrPass, encodedLen);
    if(outLen < 0)
    {
        CDX_LOGE("malloc failed.");
        goto out;
    }

    b64UsrPass[outLen]='\0';

    auth = malloc(encodedLen+22);
    if(auth == NULL)
    {
        CDX_LOGE("malloc failed.");
        goto out;
    }

    sprintf(auth, "Authorization: Basic %s", b64UsrPass);
    CdxHttpSetField(httpHdr, auth);
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

static void CdxHttpSetMethod(CdxHttpHeaderT *httpHdr, const char *method)
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

char* CdxHttpBuildRequest(CdxHttpHeaderT *httpHdr)
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
        CdxHttpSetMethod(httpHdr, "GET");
    }
    if(httpHdr->uri == NULL)
    {
        CdxHttpSetUri(httpHdr, "/");
    }

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
    len = strlen(httpHdr->method) + strlen(httpHdr->uri) + 12;//GET uri HTTP/1.1\r\n
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
        CDX_LOGE("malloc failed.");
        if(uri)
        {
            free(uri);
            uri = NULL;
        }
        return NULL;
    }

    httpHdr->bufferSize = len;

    //-- Building the request
    ptr = httpHdr->buffer;
    // Add the method line
    ptr += sprintf( ptr, "%s %s HTTP/1.%u\r\n", httpHdr->method, uri,
                httpHdr->httpMinorVersion);//---minorVersion

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

int CdxHttpAuthenticate(CdxHttpHeaderT *httpHdr, CdxUrlT *url, int *authRetry)
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

    aut = CdxHttpGetField(httpHdr, "WWW-Authenticate");
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

