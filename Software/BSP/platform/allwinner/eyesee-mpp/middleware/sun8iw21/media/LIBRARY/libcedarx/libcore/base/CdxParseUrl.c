/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : CdxParseUrl.c
 * Description : ParseUrl
 * History :
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CdxUrl.h>
#include <ctype.h>
#include <cdx_log.h>

#ifdef SIZE_MAX
#undef SIZE_MAX
#endif
#define SIZE_MAX ((size_t)-1)
void CdxUrlPrintf(CdxUrlT* url)
{
    CDX_LOGD("**********print the url container.");
    CDX_LOGD("**********ur->url=(%s)", url->url);
    CDX_LOGD("**********ur->protocol=%s", url->protocol);
    CDX_LOGD("**********ur->hostname=%s", url->hostname);
    CDX_LOGD("**********ur->file=%s", url->file);
    CDX_LOGD("**********ur->port=%u", url->port);
    CDX_LOGD("**********ur->username=%s", url->username);
    CDX_LOGD("**********ur->password=%s", url->password);
}
CdxUrlT* CdxUrlNew(char* url)
{
    int pos1 = 0;
    int pos2 = 0;
    int v6addr = 0;
    int len = 0;
    int len2 = 0;
    char *p1 = NULL;
    char *p2 = NULL;
    char *p3 = NULL;
    char *p4 = NULL;
    int nJump = 3;
    CdxUrlT* curUrl = NULL;
    char *pEscName = NULL;

    if(url == NULL)
    {
        CDX_LOGE("url null");
        return NULL;
    }
    if(strlen(url) >(SIZE_MAX/3 - 1))
    {
        CDX_LOGE("the length of the url is too longer.");
        goto err_out;
    }
    pEscName = malloc(strlen(url)*3+1);
    if(pEscName == NULL)
    {
        CDX_LOGE("malloc memory for pEscName failed.");
        goto err_out;
    }

    // Create the URL container
    curUrl = malloc(sizeof(CdxUrlT));
    if(curUrl == NULL)
    {
        CDX_LOGE("malloc memory for curUrl failed.");
        goto err_out;
    }

    //Initialisation of the URL container members
    memset(curUrl, 0, sizeof(CdxUrlT));
    CdxUrlEscapeString(pEscName,url);
    // Copy the url in the URL container
    curUrl->url = strdup(pEscName);
    if(curUrl->url == NULL)
    {
        CDX_LOGE("curUrl->url is NULL.");
        goto err_out;
    }

    // extract the protocol
    p1 = strstr(pEscName, "://");

    if(p1 == NULL)
    {
        // Check for a special case: "sip:" (without "//"):
        if(strstr(pEscName, "sip:") == pEscName)
        {
            p1 = (char *)&url[3]; // points to ':'
            nJump = 1;
        }
        else
        {
            CDX_LOGE("the url (%s) is not a URL.", pEscName);
            goto err_out;
        }
    }

    pos1 = p1-pEscName;
    curUrl->protocol = malloc(pos1+1);
    if(curUrl->protocol == NULL)
    {
        CDX_LOGE("curUrl->protocol is NULL.");
        goto err_out;
    }

    strncpy(curUrl->protocol, pEscName, pos1);
    curUrl->protocol[pos1] = '\0';

    // jump the "://"
    p1 += nJump;
    pos1 += nJump;

    //check if a username:password is given
    p2 = strrchr(p1, '@');
    p3 = strstr(p1, "/");
    if(p3!=NULL && p3<p2)
    {
        // it isn't really a username but rather a part of the path
        p2 = NULL;
    }

    if(p2 != NULL)
    {
        // We got something, at least a username...
        len = p2-p1;
        curUrl->username = malloc(len+1);
        if(curUrl->username == NULL )
        {
            CDX_LOGE("curUrl->username  is faile.");
            goto err_out;
        }
        strncpy(curUrl->username, p1, len);
        curUrl->username[len] = '\0';

        p3 = strstr(p1, ":");
        if(p3!=NULL && p3<p2)
        {
            // We also have a password
            len2 = p2-p3-1;
            curUrl->username[p3-p1]='\0';
            curUrl->password = malloc(len2+1);
            if(curUrl->password == NULL)
            {
                CDX_LOGE("curUrl->password is failed.");
                goto err_out;
            }
            strncpy(curUrl->password, p3+1, len2);
            curUrl->password[len2]='\0';
        }
        p1 = p2+1;
        pos1 = p1-pEscName;
    }

    // before looking for a port number check if we have an IPv6 type numeric address
    // in IPv6 URL the numeric address should be inside square braces.
    char *sLeft = "[";
    char *sRight = "]";
    char *fSlash = "/";

    p2 = strstr(p1, sLeft);
    p3 = strstr(p1, sRight);
    p4 = strstr(p1, fSlash);
    if((p2 != NULL) && (p3 != NULL) && (p2 < p3) && (!p4 || (p4 > p3)))
    {
        // we have an IPv6 numeric address
        p1++;
        pos1++;
        p2 = p3;
        v6addr = 1;
    }
    else
    {
        p2 = p1;
    }

    // look if the port is given
    p2 = strstr(p2, ":");
    // If the : is after the first / it isn't the port
    p3 = strstr(p1, "/");
    if(p3 && p3 - p2 < 0)
    {
        p2 = NULL;
    }
    if(p2==NULL)
    {
        // No port is given, Look if a path is given
        if(p3==NULL)
        {
            // No path/filename, So we have an URL like http://www.hostname.com
            pos2 = strlen(pEscName);
        }
        else
        {
            // We have an URL like http://www.hostname.com/file.txt
            pos2 = p3-pEscName;
        }
    }
    else
    {
        // We have an URL beginning like http://www.hostname.com:1212, Get the port number
        curUrl->port = atoi(p2+1);
        pos2 = p2-pEscName;
    }

    if(v6addr)
    {
        pos2--;
    }

    // copy the hostname in the URL container
    curUrl->hostname = malloc(pos2-pos1+1);
    if(curUrl->hostname==NULL)
    {
        CDX_LOGE("curUrl->hostname is NULL.");
        goto err_out;
    }

    strncpy(curUrl->hostname, p1, pos2-pos1);
    curUrl->hostname[pos2-pos1] = '\0';

    // Look if a path is given
    p2 = strstr(p1, "/");

    if(p2 != NULL)
    {
        //A path/filename is given, check if it's not a trailing '/'
        if(strlen(p2) > 1)
        {
            // copy the path/filename in the URL container
            curUrl->file = strdup(p2);
            if(curUrl->file==NULL)
            {
                CDX_LOGE("curURL is NULL.");
                goto err_out;
            }
        }
    }

    // Check if a filename was given or set, else set it with '/'
    if(curUrl->file==NULL)
    {
        curUrl->file = malloc(2);
        if(curUrl->file==NULL)
        {
            CDX_LOGE("curURL file is NULL.");
            goto err_out;
        }
        strcpy(curUrl->file, "/");
    }
    free(pEscName);
    pEscName = NULL;
    return curUrl;

err_out:
    if(pEscName)
    {
        free(pEscName);
        pEscName = NULL;
    }
    if(curUrl)
    {
        CdxUrlFree(curUrl);
    }
    return NULL;
}
//*******************************************************************************************//
/* Replace specific characters in the URL string by an escape sequence */
/* works like strcpy(), but without return argument */
//*******************************************************************************************//
void CdxUrlEscapeString(char *pOutbuf, const char *pInbuf)
{
    unsigned char a;
    int i = 0;
    int j = 0;
    int len = 0;
    char *pTmp = NULL;
    char *in = NULL;
    char *pUnesc = NULL;

    len = strlen(pInbuf);

    // Look if we have an ip6 address, if so skip it there is no need to escape anything in there.
    pTmp = strstr(pInbuf,"://[");
    if(pTmp)
    {
        pTmp = strchr(pTmp + 4, ']');
        if(pTmp && ('/' == pTmp[1] || ':' == pTmp[1] || '\0' == pTmp[1]))
        {
            i = pTmp + 1 - pInbuf;
            strncpy(pOutbuf, pInbuf, i);
            pOutbuf += i;
            pTmp = NULL;
        }
    }

    pTmp = NULL;
    while(i < len)
    {
        // look for the next char that must be kept
        for(j=i; j<len; j++)
        {
            a = pInbuf[j];
            if(a=='-' || a=='_' || a=='.'  || a=='!' ||  // mark characters, see RFC 2396.
               a=='~' || a=='*' || a=='\'' || a=='(' ||
               a==')' || a==';' || a=='/'  || a=='?' ||
               a==':' || a=='@' || a=='&'  || a=='=' ||
               a=='+' || a=='$' || a==','  || a=='[' ||
               a==']')
               break;
        }
        // we are on a reserved char, write it out
        if(j == i)
        {
            *pOutbuf++ = a;
            i++;
            continue;
        }
        // we found one, take that part of the string
        if(j < len)
        {
            if(!pTmp)
            {
                pTmp = malloc(len+1);
            }
            strncpy(pTmp,pInbuf+i,j-i);
            pTmp[j-i] = '\0';
            in = pTmp;
        }
        else // take the rest of the string
        {
            in = (char*)pInbuf+i;
        }

        if(!pUnesc)
        {
            pUnesc = malloc(len+1);
        }
        // unescape first to avoid escaping escape
        //CdxUrlUnescapeString(pUnesc, in);
        // then escape, including mark and other reserved char that can come from escape sequences
        CdxUrlEscapeStringPart(pOutbuf, in);
        pOutbuf += strlen(pOutbuf);
        i += strlen(in);
    }
    *pOutbuf = '\0';

    if(pTmp)
    {
        free(pTmp);
        pTmp = NULL;
    }
    if(pUnesc)
    {
        free(pUnesc);
        pUnesc = NULL;
    }
}

//*******************************************************************************************//
/* Replace escape sequences in an URL (or a part of an URL) */
/* works like strcpy(), but without return argument */
//*******************************************************************************************//

void CdxUrlUnescapeString(char *pOutbuf, const char *pInbuf)
{
    int i = 0;
    int len = 0;
    unsigned char c,c1,c2;

    len = strlen(pInbuf);
    for (i=0;i<len;i++)
    {
        c = pInbuf[i];
        if ((c == '%') && (i < len-2))    //must have 2 more chars
        {
            c1 = toupper(pInbuf[i+1]);  // we need uppercase characters
            c2 = toupper(pInbuf[i+2]);
            //if (((c1>='0' && c1<='9') || (c1>='A' && c1<='F')) &&
            if ((c1>='0' && c1<='7') &&
                ((c2>='0' && c2<='9') || (c2>='A' && c2<='F')))
            {
                if (c1>='0' && c1<='9')
                {
                    c1-='0';
                }
                else
                {
                    c1-='A'-10;
                }
                if(c2>='0' && c2<='9')
                {
                    c2-='0';
                }
                else
                {
                    c2-='A'-10;
                }
                c = (c1<<4) + c2;
                i= i+2;               //only skip next 2 chars if valid esc
            }
        }
        *pOutbuf++ = c;
    }
    *pOutbuf++='\0'; //add nullterm to string
}

//*******************************************************************************************//
//*******************************************************************************************//

void CdxUrlEscapeStringPart(char *pOutbuf, const char *pInbuf)

{
    int i = 0;
    int len = 0;
    unsigned char c, c_1, c_2;

    len=strlen(pInbuf);

    for(i=0; i<len; i++)
    {
        c = pInbuf[i];
        if (('%' == c) && (i<len-2) )
        {
            //need 2 more characters
             c_1 = toupper(pInbuf[i+1]);
             c_2 = toupper(pInbuf[i+2]); // need uppercase chars
        }
        else
        {
            c_1 = 129;
            c_2 = 129; //not escape chars
        }

        //if((c >= 'A' && c <= 'Z') ||(c >= 'a' && c <= 'z') ||(c >= '0' && c <= '9') ||(c >= 0x7f))
        if((c >= 'A' && c <= 'Z') ||(c >= 'a' && c <= 'z') ||(c >= '0' && c <= '9'))
        {
            *pOutbuf++ = c;
        }
        else if(c == '%' && ((c_1 >= '0' && c_1 <= '9') || (c_1 >= 'A' && c_1 <= 'F')) &&
               ((c_2 >= '0' && c_2 <= '9') || (c_2 >= 'A' && c_2 <= 'F')))
        {
            // check if part of an escape sequence already
            *pOutbuf++ = c;
            // dont escape again error as this should not happen
            // against RFC 2396 to escape a string twice
        }
        else
        {
            // all others will be escaped
            c_1 = ((c & 0xf0) >> 4);
            c_2 = (c & 0x0f);
            if(c_1 < 10)
            {
                c_1 += '0';
            }
            else
            {
                c_1 += 'A'-10;
            }
            if(c_2 < 10)
            {
                c_2 += '0';
            }
            else
            {
                c_2 += 'A'-10;
            }
            *pOutbuf++ = '%';
            *pOutbuf++ = c_1;
            *pOutbuf++ = c_2;
        }
    }
    *pOutbuf++ = '\0';
}
void CdxUrlFree(CdxUrlT* curUrl)
{
    if(curUrl == NULL)
    {
        return;
    }
    if(curUrl->url)
    {
        free(curUrl->url);
    }
    if(curUrl->protocol)
    {
        free(curUrl->protocol);
    }
    if(curUrl->hostname)
    {
        free(curUrl->hostname);
    }
    if(curUrl->file)
    {
        free(curUrl->file);
    }
    if(curUrl->username)
    {
        free(curUrl->username);
    }
    if(curUrl->password)
    {
        free(curUrl->password);
    }
    if(curUrl->noauth_url)
    {
        free(curUrl->noauth_url);
    }
    free(curUrl);
}
CdxUrlT* CdxCheck4Proxies(CdxUrlT *url)
{
    int len = 0;
    CdxUrlT *urlOut = NULL;
    char *proxy = NULL;
    char *newUrl = NULL;
    CdxUrlT *tmpUrl = NULL;
    CdxUrlT *proxyUrl = NULL;

    if(url == NULL)
    {
        return NULL;
    }
    urlOut = CdxUrlNew(url->url);
    if(!strcasecmp(url->protocol, "http_proxy"))
    {
        CDX_LOGI("Using HTTP proxy: http://%s:%d\n", url->hostname, url->port);
        return urlOut;
    }
    // Check if the http_proxy environment variable is set.
    if(!strcasecmp(url->protocol, "http"))
    {
        proxy = getenv("http_proxy");
        if(proxy != NULL)
        {
            // We got a proxy, build the URL to use it
            proxyUrl = CdxUrlNew(proxy);
            if(proxyUrl == NULL)
            {
                CDX_LOGI("proxy_url is NULL.");
                return urlOut;
            }

#ifdef HAVE_AF_INET6
            if(network_ipv4_only_proxy && (gethostbyname(url->hostname)==NULL))
            {
                CdxUrlFree(proxyUrl);
                return urlOut;
            }
#endif
            // 20 = http_proxy:// + port
            len = strlen(proxyUrl->hostname) + strlen(url->url) + 20;
            newUrl = malloc(len+1);
            if (newUrl == NULL)
            {
                CdxUrlFree(proxyUrl);
                return urlOut;
            }
            sprintf(newUrl, "http_proxy://%s:%d/%s", proxyUrl->hostname, proxyUrl->port, url->url);
            tmpUrl = CdxUrlNew(newUrl);
            if(tmpUrl == NULL)
            {
                free(newUrl);
                newUrl = NULL;
                CdxUrlFree(proxyUrl);
                return urlOut;
            }
            CdxUrlFree(urlOut);
            urlOut = tmpUrl;
            free(newUrl);
            newUrl = NULL;
            CdxUrlFree(proxyUrl);
        }
    }
    return urlOut;
}
#if 0
char *aw_mp_asprintf(char *fmt, ...)
{
    char *p = NULL;
    va_list va;
    int len;

    va_start(va, fmt);
    len = vsnprintf(NULL, 0, fmt, va);
    va_end(va);
    if(len < 0)
    {
        goto end;
    }
    p = malloc(len + 1);
    if(!p)
    {
        goto end;
    }
    va_start(va, fmt);
    len = vsnprintf(p, len + 1, fmt, va);
    va_end(va);
    if(len < 0)
    {
        free(p);
        p = NULL;
    }
end:
    return p;
}

char *CdxGetNoauthUrl(CdxUrlT *url)
{
    if(url->port)
    {
        return aw_mp_asprintf("%s://%s:%d%s",
                            url->protocol, url->hostname, url->port, url->file);
    }
    else
    {
        return aw_mp_asprintf("%s://%s%s",
                            url->protocol, url->hostname, url->file);
    }
}
#endif
CdxUrlT *CdxUrlRedirect(CdxUrlT **url, char *pRedir)
{
    CdxUrlT *u = *url;
    CdxUrlT *res;

    if(!strstr(pRedir, "://")|| *pRedir == '/')
    {
        char *pTmp;
        char *newurl = malloc(strlen(u->url) + strlen(pRedir) + 1);
        strcpy(newurl, u->url);
        if (*pRedir == '/')
        {
            pRedir++;
            pTmp = strstr(newurl, "://");
            if(pTmp)
            {
                pTmp = strchr(pTmp + 3, '/');
            }
        }
        else
        {
            pTmp = strrchr(newurl, '/');
        }

        if(pTmp)
        {
            pTmp[1] = 0;
        }
        strcat(newurl, pRedir);
        res = CdxUrlNew(newurl);
        free(newurl);
   }
   else
   {
        res = CdxUrlNew(pRedir);
   }
   CdxUrlFree(u);
   *url = res;
   return res;
}

