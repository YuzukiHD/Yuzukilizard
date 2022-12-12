/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : xxx.c
* Description : DataSourceStreamOpen
* History :
*
*/
#define LOG_TAG "DataSourceStream"
#include <cdx_log.h>
#include <CdxStream.h>

CdxStreamT *DataSourceStreamOpen(CdxDataSourceT *dataSource)
{
    void* handle = NULL;
    int ret;
    ret = sscanf(dataSource->uri, "datasource://%p", &handle);
    CDX_LOGD("++++dataSource->uri: %s ret: %d handle: %p", dataSource->uri, ret,
        handle);
    if (ret != 1)
    {
        CDX_LOGE("sscanf failure...(%s)", dataSource->uri);
        return NULL;
    }
    return (CdxStreamT *)handle;
}

CdxStreamCreatorT dataSourceStreamCtor =
{
    .create = DataSourceStreamOpen
};
