/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : xxx.c
* Description : CustomerStreamOpen
* History :
*
*/
#define LOG_TAG "CustomerStream"
#include <cdx_log.h>
#include <CdxStream.h>

CdxStreamT *CustomerStreamOpen(CdxDataSourceT *dataSource)
{
    void* handle;
    int ret;
    ret = sscanf(dataSource->uri, "customer://%p", &handle);
    if (ret != 1)
    {
        CDX_LOGE("sscanf failure...(%s)", dataSource->uri);
        return NULL;
    }
    return (CdxStreamT *)handle;
}

CdxStreamCreatorT customerStreamCtor =
{
    .create = CustomerStreamOpen
};
