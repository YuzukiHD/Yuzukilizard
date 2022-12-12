/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mm_component.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/03/15
  Last Modified :
  Description   : The common implement for multimedia component.
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "mm_component"
#include <utils/plat_log.h>

//ref platform headers
#include <stdlib.h>
#include <string.h>
#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"

//media api headers to app

//media internal common headers.
#include "mm_component.h"
#include "cdx_list.h"

extern CDX_COMPONENTREGISTRY cdx_comp_table[];
extern const unsigned int SIZE_OF_COMP;

static int get_cmp_index(char* cmp_name)
{
    int rc = -1;
    unsigned int i;
    for(i=0; i< SIZE_OF_COMP; i++)
    {
        alogv("get_cmp_index: cmp_name = %s , cdx_comp_table[%d].name = %s", cmp_name, i, cdx_comp_table[i].name);
        if(!strcmp(cmp_name, cdx_comp_table[i].name))
        {
            rc = i;
            break;
        }
    }
    return rc;
}

ERRORTYPE COMP_GetHandle(
    PARAM_OUT COMP_HANDLETYPE* pHandle, 
    PARAM_IN  char* cComponentName,
    PARAM_IN  void* pAppData,
    PARAM_IN  COMP_CALLBACKTYPE* pCallBacks)
{
    ERRORTYPE eRet = SUCCESS;
    alogv("COMP CORE API - COMP_GetHandle");
    int compIndex = get_cmp_index(cComponentName);
    if(compIndex >= 0)
    {
        COMP_HANDLETYPE compHandle;
        compHandle = (COMP_HANDLETYPE) malloc(sizeof(MM_COMPONENTTYPE));
        if (!compHandle) 
        {
            aloge("Component Creation failed");
            return ERR_COMPCORE_NOMEM;
        }
        memset(compHandle, 0, sizeof(MM_COMPONENTTYPE));
        cdx_comp_table[compIndex].comp_init_fn(compHandle);
        ((MM_COMPONENTTYPE *)compHandle)->SetCallbacks(compHandle, pCallBacks, pAppData);
        *pHandle = compHandle;
    }
    else
    {
        alogw("component name[%s] is not found", cComponentName);
        eRet = ERR_COMPCORE_UNEXIST;
    }
    return eRet;
}

ERRORTYPE COMP_FreeHandle(
    PARAM_IN  COMP_HANDLETYPE hComponent)
{
    ERRORTYPE eRet = SUCCESS;
    alogv("COMP CORE API - Free Handle %p", hComponent);
    if(NULL == hComponent)
    {
        return SUCCESS;
    }
    MM_COMPONENTTYPE *pComp = (MM_COMPONENTTYPE*)hComponent;
    COMP_STATETYPE state;
    pComp->GetState(hComponent, &state);
    if(state!=COMP_StateLoaded)
    {
        aloge("fatal error! Calling FreeHandle in state %d", state);
    }
    pComp->ComponentDeInit(hComponent);
    free(hComponent);
    return eRet;
}

ERRORTYPE COMP_SetupTunnel(
    PARAM_IN  COMP_HANDLETYPE hOutput,
    PARAM_IN  unsigned int nPortOutput,
    PARAM_IN  COMP_HANDLETYPE hInput,
    PARAM_IN  unsigned int nPortInput)
{
    alogv("COMP CORE API - COMP_SetupTunnel %p %d %p %d", hOutput, nPortOutput, hInput, nPortInput);
    ERRORTYPE eError;
    COMP_TUNNELSETUPTYPE oTunnelSetup;
    eError = ((MM_COMPONENTTYPE *)hOutput)->ComponentTunnelRequest(hOutput, nPortOutput, hInput, nPortInput, &oTunnelSetup);
    if(eError!=SUCCESS)
    {
        aloge("fatal error! setup tunnel on output port fail[0x%x]!", eError);
        return ERR_COMPCORE_NOT_SUPPORT;
    }
    eError = ((MM_COMPONENTTYPE *)hInput)->ComponentTunnelRequest(hInput, nPortInput, hOutput, nPortOutput, &oTunnelSetup);
    if(eError!=SUCCESS)
    {
        aloge("fatal error! setup tunnel on input port fail[0x%x]!", eError);
        ((MM_COMPONENTTYPE *)hOutput)->ComponentTunnelRequest(hOutput, nPortOutput, NULL, 0, NULL);
        return ERR_COMPCORE_NOT_SUPPORT;
    }
    return SUCCESS;
}

ERRORTYPE COMP_ResetTunnel(
    PARAM_IN  COMP_HANDLETYPE hOutput,
    PARAM_IN  unsigned int nPortOutput,
    PARAM_IN  COMP_HANDLETYPE hInput,
    PARAM_IN  unsigned int nPortInput)
{
    alogv("COMP CORE API - COMP_ResetTunnel %p %d %p %d", hOutput, nPortOutput, hInput, nPortInput);
    ERRORTYPE eError;
    eError = ((MM_COMPONENTTYPE *)hOutput)->ComponentTunnelRequest(hOutput, nPortOutput, NULL, 0, NULL);
    if(eError!=SUCCESS)
    {
        aloge("fatal error! setup tunnel on output port fail[0x%x]!", eError);
        eError = ERR_COMPCORE_NOT_SUPPORT;
    }
    eError = ((MM_COMPONENTTYPE *)hInput)->ComponentTunnelRequest(hInput, nPortInput, NULL, 0, NULL);
    if(eError!=SUCCESS)
    {
        aloge("fatal error! setup tunnel on input port fail[0x%x]!", eError);
        eError = ERR_COMPCORE_NOT_SUPPORT;
    }
    return SUCCESS;
}

ERRORTYPE COMP_UpdateTunnelSlavePortDefine(
    PARAM_IN  COMP_HANDLETYPE hOutput,
    PARAM_IN  unsigned int nPortOutput,
    PARAM_IN  COMP_HANDLETYPE hInput,
    PARAM_IN  unsigned int nPortInput){

    COMP_PARAM_PORTDEFINITIONTYPE out_port_def,in_port_def;

    out_port_def.nPortIndex = nPortOutput;
    ((MM_COMPONENTTYPE *)hOutput)->GetConfig(hOutput, COMP_IndexParamPortDefinition,&out_port_def);

    if(out_port_def.eDir != COMP_DirOutput)
    {
        aloge("fatal error! update tunnel slave port define fail!");
        return ERR_COMPCORE_NOT_SUPPORT;
    }

    in_port_def.nPortIndex = nPortInput;
    ((MM_COMPONENTTYPE *)hInput)->GetConfig(hInput,COMP_IndexParamPortDefinition,&in_port_def);
    if(in_port_def.eDir != COMP_DirInput)
    {
        aloge("fatal error! update tunnel slave port define fail!");
        return ERR_COMPCORE_NOT_SUPPORT;
    }

    if(in_port_def.eDomain != out_port_def.eDomain)
    {
        aloge("fatal error! update tunnel slave port define fail!");
        return ERR_COMPCORE_NOT_SUPPORT;
    }

    //------------update portdef infomation --------------
    in_port_def.format = out_port_def.format;
    //LOGV("==== OMX_UpdateTunnelSlavePortDefine format:%d nPortOutput:%d nPortInput:%d",out_port_def.format.audio.eEncoding,nPortOutput,nPortInput);
    ((MM_COMPONENTTYPE *)hInput)->SetConfig(hInput, COMP_IndexParamPortDefinition, &in_port_def);

    return SUCCESS;
}

ERRORTYPE COMP_AddTunnelInfoChain(
    CDX_TUNNELLINKTYPE *cdx_tunnel_link,
    PARAM_IN  COMP_HANDLETYPE hOutput,
    PARAM_IN  unsigned int nPortOutput,
    PARAM_IN  COMP_HANDLETYPE hInput,
    PARAM_IN  unsigned int nPortInput)
{
    CDX_TUNNELINFOTYPE *tunnel_info = (CDX_TUNNELINFOTYPE *)malloc(sizeof(CDX_TUNNELINFOTYPE));

    if(!tunnel_info){
        aloge("malloc fail!");
        return ERR_COMPCORE_NOMEM;
    }

    tunnel_info->hTunnelMaster = hOutput;
    tunnel_info->hTunnelSlave = hInput;
    tunnel_info->nMasterPortIndex = nPortOutput;
    tunnel_info->nSlavePortIndex = nPortInput;
    tunnel_info->hNext = NULL;

    if(cdx_tunnel_link->head == NULL){
        cdx_tunnel_link->head = cdx_tunnel_link->tail = tunnel_info;
    }
    else{
        cdx_tunnel_link->tail->hNext = tunnel_info;
        cdx_tunnel_link->tail = tunnel_info;
    }

    return SUCCESS;
}

ERRORTYPE COMP_DeleteTunnelInfoChain(
        CDX_TUNNELLINKTYPE *cdx_tunnel_link)
{
    cdx_tunnel_link->tail = cdx_tunnel_link->head;
    while(cdx_tunnel_link->tail){
//      LOGV("tail:%p",cdx_tunnel_link->tail);
        cdx_tunnel_link->tail = cdx_tunnel_link->tail->hNext;
        free(cdx_tunnel_link->head);
        cdx_tunnel_link->head = cdx_tunnel_link->tail;
    }
    return SUCCESS;
}

ERRORTYPE COMP_QueryTunnelInfoChain(
        PARAM_IN CDX_TUNNELLINKTYPE *cdx_tunnel_link,
        PARAM_IN COMP_HANDLETYPE master,
        PARAM_IN COMP_HANDLETYPE slave,
        PARAM_OUT CDX_TUNNELINFOTYPE *cdx_tunnel_info)
{
    CDX_TUNNELINFOTYPE *curr;

    curr = cdx_tunnel_link->head;
    while(curr){
        if(curr->hTunnelMaster == master && curr->hTunnelSlave == slave){
            break;
        }
        curr = curr->hNext;
    }

    if(curr){
        memcpy(cdx_tunnel_info,curr,sizeof(CDX_TUNNELINFOTYPE));
        return SUCCESS;
    }
    else{
        aloge("fatal error! query tunnel info fail!");
        return ERR_COMPCORE_UNEXIST;
    }
}
