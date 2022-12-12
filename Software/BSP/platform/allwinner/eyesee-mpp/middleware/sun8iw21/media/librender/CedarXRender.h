/*
********************************************************************************
*                           Android multimedia module
*
*          (c) Copyright 2010-2015, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : CedarXRender.h
* Version: V1.0
* By     : eric_wang
* Date   : 2015-6-16
* Description:
********************************************************************************
*/
#ifndef _CEDARXRENDER_H_
#define _CEDARXRENDER_H_

//#include <utils/RefBase.h>

#include <CdxANativeWindowBuffer.h>
//#include <CDX_PlayerAPI.h>
#include <vdecoder.h>
#include <CDX_MediaFormat.h>

namespace EyeseeLinux {

enum VIDEORENDER_CMD
{
    VIDEORENDER_CMD_UNKOWN = 0,
    VIDEORENDER_CMD_ROTATION_DEG    = 1,    //HWC_LAYER_ROTATION_DEG, //HWC_LAYER_ROTATION_DEG    ,
    VIDEORENDER_CMD_DITHER          = 3,    //HWC_LAYER_DITHER, //HWC_LAYER_DITHER          ,
    VIDEORENDER_CMD_SETINITPARA     = 4,    //HWC_LAYER_SETINITPARA, //HWC_LAYER_SETINITPARA     ,
    VIDEORENDER_CMD_SETVIDEOPARA    = 5,    //HWC_LAYER_SETVIDEOPARA, //HWC_LAYER_SETVIDEOPARA    ,
    VIDEORENDER_CMD_SETFRAMEPARA    = 6,    //HWC_LAYER_SETFRAMEPARA, //HWC_LAYER_SETFRAMEPARA    ,
    VIDEORENDER_CMD_GETCURFRAMEPARA = 7,    //HWC_LAYER_GETCURFRAMEPARA, //HWC_LAYER_GETCURFRAMEPARA ,
    VIDEORENDER_CMD_QUERYVBI        = 8,    //HWC_LAYER_QUERYVBI, //HWC_LAYER_QUERYVBI        ,
    VIDEORENDER_CMD_SETSCREEN       = 9,    //HWC_LAYER_SETMODE,
    VIDEORENDER_CMD_SHOW            = 0xa, //HWC_LAYER_SHOW, //HWC_LAYER_SHOW            ,
    VIDEORENDER_CMD_RELEASE         = 0xb, //HWC_LAYER_RELEASE, //HWC_LAYER_RELEASE         ,
    VIDEORENDER_CMD_SET3DMODE       = 0xc,    //HWC_LAYER_SET3DMODE, //HWC_LAYER_SET3DMODE       ,
    VIDEORENDER_CMD_SETFORMAT       = 0xd,    //HWC_LAYER_SETFORMAT, //HWC_LAYER_SETFORMAT       ,
    VIDEORENDER_CMD_VPPON           = 0xe,    //HWC_LAYER_VPPON, //HWC_LAYER_VPPON           ,
    VIDEORENDER_CMD_VPPGETON        = 0xf,    //HWC_LAYER_VPPGETON, //HWC_LAYER_VPPGETON        ,
    VIDEORENDER_CMD_SETLUMASHARP    = 0x10,    //HWC_LAYER_SETLUMASHARP,//HWC_LAYER_SETLUMASHARP    ,
    VIDEORENDER_CMD_GETLUMASHARP    = 0x11,    //HWC_LAYER_GETLUMASHARP, //HWC_LAYER_GETLUMASHARP    ,
    VIDEORENDER_CMD_SETCHROMASHARP  = 0x12,    //HWC_LAYER_SETCHROMASHARP, //HWC_LAYER_SETCHROMASHARP  ,
    VIDEORENDER_CMD_GETCHROMASHARP  = 0x13,    //HWC_LAYER_GETCHROMASHARP,//HWC_LAYER_GETCHROMASHARP  ,
    VIDEORENDER_CMD_SETWHITEEXTEN   = 0x14,    //HWC_LAYER_SETWHITEEXTEN, //HWC_LAYER_SETWHITEEXTEN   ,
    VIDEORENDER_CMD_GETWHITEEXTEN   = 0x15,    //HWC_LAYER_GETWHITEEXTEN, //HWC_LAYER_GETWHITEEXTEN   ,
    VIDEORENDER_CMD_SETBLACKEXTEN   = 0x16,    //HWC_LAYER_SETBLACKEXTEN, //HWC_LAYER_SETBLACKEXTEN   ,
    VIDEORENDER_CMD_GETBLACKEXTEN   = 0x17,    //HWC_LAYER_GETBLACKEXTEN, //HWC_LAYER_GETBLACKEXTEN   ,

    VIDEORENDER_CMD_SETLAYERORDER   = 0x100,                   //DISP_CMD_LAYER_TOP, DISP_CMD_LAYER_BOTTOM ,
    VIDEORENDER_CMD_SET_CROP        = 0x1000,   //RECT_S
    VIDEORENDER_CMD_SET_SCALING_MODE= 0x1001,
    VIDEORENDER_CMD_GET_ANATIVEWINDOWBUFFERS,
    VIDEORENDER_CMD_UPDATE_DISPLAY_SIZE,
};

struct CedarXRenderer {
    CedarXRenderer() {}
    virtual ~CedarXRenderer() {};

    virtual void render(const void *data, size_t size) = 0;
    virtual int control(int cmd, int para, void *pData) = 0;
    virtual int dequeueFrame(ANativeWindowBufferCedarXWrapper *pObject) = 0;
    virtual int enqueueFrame(ANativeWindowBufferCedarXWrapper *pObject) = 0;
    virtual int cancelFrame(ANativeWindowBufferCedarXWrapper *pObject) = 0;
    
private:
    CedarXRenderer(const CedarXRenderer &);
    CedarXRenderer &operator=(const CedarXRenderer &);
};

}
#endif  /* _CEDARXRENDER_H_ */

