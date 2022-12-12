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
#include <cdx_log.h>

#include "iniparser.h"
#include "iniparserapi.h"

static dictionary* g_ini = NULL;

int  IniParserInit()
{
    logd("IniParserInit");
    char ini_name[265] = "/etc/cedarx.conf";
    char vendor_init_name[] =
            "/vendor/etc/cedarx.conf";

    g_ini = iniparser_load(ini_name);
    if (g_ini == NULL)
    {
        g_ini = iniparser_load(vendor_init_name);
        if(g_ini == NULL)
        {
            logd("cannot parse file: %s\n", ini_name);
            return -1 ;
        }
    }

    if(g_ini)
    {
        logd("load conf file %s ok!\n", ini_name);
    }

    log_set_level(GetConfigParamterInt("log_level", (int)LOG_LEVEL_WARNING));

    return 0;
}

void IniParserDestory()
{
    if (g_ini == NULL)
    {
        iniparser_freedict(g_ini);
        g_ini = NULL;
    }
}

const char * IniParserGetString(const char * key, const char * def)
{
    int ret = 0;
    if (g_ini == NULL)
    {
        ret = IniParserInit();
        if (ret == -1)
            return NULL;
    }
    return iniparser_getstring(g_ini,key,def);
}

int IniParserGetInt(const char * key, int notfound)
{
    int ret = 0;
    if (g_ini == NULL)
    {
        ret = IniParserInit();
        if (ret == -1)
            return notfound;
    }
    return iniparser_getint(g_ini,key,notfound);;
}

double IniparserGetDouble(const char * key, double notfound)
{
    int ret = 0;
    if (g_ini == NULL)
    {
        ret = IniParserInit();
        if (ret == -1)
            return notfound;
    }
    return iniparser_getdouble(g_ini,key,notfound);
}

int IniParserGetBoolean(const char * key, int notfound)
{
    int ret = 0;
    if (g_ini == NULL)
    {
        ret = IniParserInit();
        if (ret == -1)
            return notfound;
    }
    return iniparser_getboolean(g_ini,key,notfound);
}

int IniParserFindEntry(const char * entry)
{
    int ret = 0;
    if (g_ini == NULL)
    {
        ret = IniParserInit();
        if (ret == -1)
            return 0;
    }
    return iniparser_find_entry(g_ini,entry);
}

int GetConfigParamterInt(const char * key, int notfound)
{
    char paramterkey[128];
    sprintf(paramterkey,"paramter:%s",key);
    return IniParserGetInt(paramterkey,notfound);
}

const char * GetConfigParamterString(const char * key, const char * def)
{
    char paramterkey[128];
    sprintf(paramterkey,"paramter:%s",key);
    return IniParserGetString(paramterkey,def);
}
double GetConfigParamterDouble(const char * key, double notfound)
{
    char paramterkey[128];
    sprintf(paramterkey,"paramter:%s",key);
    return IniparserGetDouble(paramterkey,notfound);
}
int GetConfigParamterBoolean(const char * key, int notfound)
{
    char paramterkey[128];
    sprintf(paramterkey,"paramter:%s",key);
    return IniParserGetBoolean(paramterkey,notfound);
}
