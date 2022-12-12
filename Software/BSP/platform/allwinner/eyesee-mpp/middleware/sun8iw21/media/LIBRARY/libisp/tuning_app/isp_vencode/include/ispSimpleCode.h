/*
* Copyright (c) 2008-2020 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : ispSimpleCode.h
* Description :
* History :
*   Author  :
*   Date    :
*   Comment :
*
*
*/

#ifndef ISPSIMPLECODE_H
#define ISPSIMPLECODE_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "vencoder.h"

#define VENCODE_ROI_NUM 4

#ifdef __cplusplus
extern "C" {
#endif

typedef enum vencode_command_e {
	VENCODE_CMD_HEAD_PPSSPS = 0x00,
	VENCODE_CMD_STREAM		= 0x01,
} vencode_command;

/****************************************************************************************************************
 * structure
 ***************************************************************************************************************/
typedef struct {
	VencHeaderData			sps_pps_data;
	VencH264Param			h264Param;
	VencMBModeCtrl			h264MBMode;
	VencMBInfo				MBInfo;
	VencFixQP				fixQP;
	VencSuperFrameConfig	sSuperFrameCfg;
	VencH264SVCSkip			SVCSkip; // set SVC and skip_frame
	VencH264AspectRatio		sAspectRatio;
	VencH264VideoSignal		sVideoSignal;
	VencCyclicIntraRefresh	sIntraRefresh;
	VencROIConfig			sRoiConfig[VENCODE_ROI_NUM];
	VeProcSet				sVeProcInfo;
	VencOverlayInfoS		sOverlayInfo;
	VencSmartFun			sH264Smart;
} h264_func_t;

typedef struct {
	VencH265Param			h265Param;
	VencH265GopStruct		h265Gop;
	VencHVS					h265Hvs;
	VencH265TendRatioCoef	h265Trc;
	VencSmartFun			h265Smart;
	VencMBModeCtrl			h265MBMode;
	VencMBInfo				MBInfo;
	VencFixQP				fixQP;
	VencSuperFrameConfig	sSuperFrameCfg;
	VencH264SVCSkip			SVCSkip; // set SVC and skip_frame
	VencH264AspectRatio		sAspectRatio;
	VencH264VideoSignal		sVideoSignal;
	VencCyclicIntraRefresh	sIntraRefresh;
	VencROIConfig			sRoiConfig[VENCODE_ROI_NUM];
	VencAlterFrameRateInfo	sAlterFrameRateInfo;
	int						h265_rc_frame_total;
	VeProcSet				sVeProcInfo;
	VencOverlayInfoS		sOverlayInfo;
}h265_func_t;

typedef struct {
    EXIFInfo                exifinfo;
    int                     quality;
    int                     jpeg_mode;
    VencJpegVideoSignal     vs;
    int                     jpeg_biteRate;
    int                     jpeg_frameRate;
    VencBitRateRange        bitRateRange;
    VencOverlayInfoS        sOverlayInfo;
    VencCopyROIConfig       pRoiConfig;
}jpeg_func_t;

typedef struct Fixqp {
	int mIQp;
	int mPQp;
} Fixqp;

typedef struct
{
   int (*EventHandler)(
        VideoEncoder* pEncoder,
        void* pAppData,
        VencEventType eEvent,
        unsigned int nData1,
        unsigned int nData2,
        void* pEventData);
} EncoderCbType;

typedef struct {
	unsigned int src_width;
	unsigned int src_height;
	unsigned int dst_width;
	unsigned int dst_height;
	unsigned int encode_frame_num;
	unsigned int encode_format;
	int bit_rate;
	int frame_rate;
	int SbmBufSize;
	int maxKeyFrame;
	unsigned int qp_min;
	unsigned int qp_max;
      unsigned int mInitQp;
	int                 bFastEncFlag;
	int                 mbPintraEnable;
	int                 n3DNR;
	int                 IQpOffset;

	int Rotate;
	int RCEnable;
	int ThrdI;
	int ThrdP;
	int MaxReEncodeTimes;
#ifdef INPUTSOURCE_FILE
	unsigned int src_size;
	unsigned int dts_size;
	FILE *in_file;
	FILE *out_file;
	char input_path[256];
	char output_path[256];
	VencAllocateBufferParam bufferParam;
#endif

      Fixqp fixqp;
	VencInputBuffer inputBuffer;
	VencOutputBuffer outputBuffer;
	VencHeaderData sps_pps_data;
	VENC_PIXEL_FMT picture_format;
	unsigned char bLbcLossyComEnFlag1_5x;
    unsigned char bLbcLossyComEnFlag2x;
    unsigned char bLbcLossyComEnFlag2_5x;
    unsigned int   bEnableGetWbYuv;
	unsigned int eColorSpace;
    unsigned int bColorSpaceFullFlag;
	unsigned int debug_gdc_en;

      jpeg_func_t jpeg_func;
	h264_func_t h264_func;
	h265_func_t h265_func;
    EncoderCbType* pCallbacks;
    void* pAppData;
}encode_param_t;

int EncoderSetCallbacks(encode_param_t *encode_param, EncoderCbType* pCallbacks, void* pAppData);

int EncoderOpen(VENC_CODEC_TYPE type);

int EncoderPrepare(encode_param_t *encode_param);

int EncoderStart(encode_param_t *encode_param, VencInputBuffer *input, VencOutputBuffer *output, vencode_command type);

int EncoderFreeOutputBuffer(VencOutputBuffer *outputBuffer);

int EncoderClose(encode_param_t *encode_param);

int EncoderGetWbYuv(encode_param_t *encode_param, unsigned char* dst_buf, unsigned int buf_size);

int EncoderSetParamSharpConfig(encode_param_t *encode_param, sEncppSharpParam* pSharpParam);

int EncoderSetParamEnableSharp(encode_param_t *encode_param, unsigned int bEnable);

int EncoderSetParamColorSpace(encode_param_t *encode_param, VENC_COLOR_SPACE eColorSpace, unsigned int bFullFlag);

int EncoderSetParam3DFliter(encode_param_t *encode_param, s3DfilterParam *p3dFilterParam);

int EncoderSetParam2DFliter(encode_param_t *encode_param, s2DfilterParam *p2dFilterParam);

int EncoderSetParamSuperFrame(encode_param_t *encode_param, VencSuperFrameConfig *pSuperFrameConfig);

#ifdef __cplusplus
}
#endif

#endif

