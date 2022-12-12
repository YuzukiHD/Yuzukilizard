/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : iniparserapi.c
 * Description : iniparserapi
 * History :
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cdc_log.h"
#include "cdcIniparser.h"
#include "CdcIniparserapi.h"

#ifdef __ANDROID__
#define CONF_FILE_NAME "/vendor/etc/cedarc.conf"
#else
#define CONF_FILE_NAME "/etc/cedarc.conf"
#endif

static dictionary* g_ini = NULL;

int  CdcIniParserInit()
{
    logv("CdcIniParserInit");
    char ini_name[265] = CONF_FILE_NAME;

    g_ini = cdc_iniparser_load(ini_name);
    if (g_ini == NULL)
    {
        logd("cannot parse file: %s\n", ini_name);
        return -1 ;
    }
    else
    {
        logd("load conf file %s ok!\n", ini_name);
    }

    return 0;
}

void CdcIniParserDestory()
{
    if (g_ini == NULL)
    {
        cdc_iniparser_freedict(g_ini);
        g_ini = NULL;
    }
}

const char * CdcIniParserGetString(const char * key, const char * def)
{
    if (g_ini == NULL)
    {
        int ret = CdcIniParserInit();
        if (ret == -1)
            return NULL;
    }
    return cdc_iniparser_getstring(g_ini,key,def);
}

int CdcIniParserGetInt(const char * key, int notfound)
{
    if (g_ini == NULL)
    {
        int ret = CdcIniParserInit();
        if (ret == -1)
            return notfound;
    }
    return cdc_iniparser_getint(g_ini,key,notfound);;
}

double CdcIniparserGetDouble(const char * key, double notfound)
{
    if (g_ini == NULL)
    {
        int ret = CdcIniParserInit();
        if (ret == -1)
            return notfound;
    }
    return cdc_iniparser_getdouble(g_ini,key,notfound);
}

int CdcIniParserGetBoolean(const char * key, int notfound)
{
    if (g_ini == NULL)
    {
        int ret = CdcIniParserInit();
        if (ret == -1)
            return notfound;
    }
    return cdc_iniparser_getboolean(g_ini,key,notfound);
}

int CdcIniParserFindEntry(const char * entry)
{
    if (g_ini == NULL)
    {
        int ret = CdcIniParserInit();
        if (ret == -1)
            return 0;
    }
    return cdc_iniparser_find_entry(g_ini,entry);
}

int CdcGetConfigParamterInt(const char * key, int notfound)
{
    char paramterkey[128];
    sprintf(paramterkey,"paramter:%s",key);
    return CdcIniParserGetInt(paramterkey,notfound);
}

const char * CdcGetConfigParamterString(const char * key, const char * def)
{
    char paramterkey[128];
    sprintf(paramterkey,"paramter:%s",key);
    return CdcIniParserGetString(paramterkey,def);
}
double CdcGetConfigParamterDouble(const char * key, double notfound)
{
    char paramterkey[128];
    sprintf(paramterkey,"paramter:%s",key);
    return CdcIniparserGetDouble(paramterkey,notfound);
}

int CdcGetSpecifyFilePathLogLevel(const char * FilePath)
{
    FILE * in = NULL;
    int loglevel;
    char chr;
    if ((in=fopen(FilePath, "r"))==NULL) {
        fprintf(stderr, "iniparser: cannot open %s\n", FilePath);
        return 0;
    }
    chr = (char)fgetc(in);
    loglevel = atoi(&chr);
    fclose(in);
    return loglevel;
}

int CdcGetConfigParamterBoolean(const char * key, int notfound)
{
    char paramterkey[128];
    sprintf(paramterkey,"paramter:%s",key);
    return CdcIniParserGetBoolean(paramterkey,notfound);
}
