/*
********************************************************************************
*                                    eMOD
*                   the Easy Portable/Player Develop Kits
*                               mod_herb sub-system
*                          (module name, e.g.mpeg4 decoder plug-in) module
*
*          (c) Copyright 2009-2010, Allwinner Microelectronic Co., Ltd.
*                              All Rights Reserved
*
* File   : aenc_sw_lib.h
* Version: V1.0
* By     : Eric_wang
* Date   : 2010-4-26
* Description:
********************************************************************************
*/
#ifdef __cplusplus
extern "C" {
#endif

#include "aencoder.h"

#ifndef _AENC_SW_LIB_H_
#define _AENC_SW_LIB_H_

// 8k sample_rate with fifo=32  can cache 32*128ms = 4096ms
// 22.05k                   128          128*46.4ms = 5939ms
// 44.1k                    192          192*23.2ms = 4454ms
#define FIFO_LEVEL	(64)    // 32 -> 128 -> 192 for higher sample_rate

#define BS_BUFFER_SIZE       (64*1024) //128*1024, 128*1024*3, 1024*1024, 
#define PCM_FRAME_SIZE       (32*1024)
#define BS_HEADER_SIZE       (1*1024)

#define OUT_BUFFER_SIZE         (512*1024)
#define OUT_ENCODE_BUFFER_SIZE  (4*1024)    //byte, max_bs_pkt_size:4*1024 for uncompress encoder(PCM_TYPE), input param: s16le+2track
#define MAXDECODESAMPLE (1024)

//encode input data information
typedef struct __audio_enc_inf_t
{
    int     InSamplerate;   //输入fs
    int     InChan;         //输入pcm chan 1:mon 2:stereo
    int     bitrate;        //bs
    int     SamplerBits;    //only for 16bits
    int     OutSamplerate;  //输出fs,now OutSamplerate must equal InSamplerate
    int     OutChan;        //编码输出 chan
    int     frame_style;    //for aac: 0:add head,1:raw data; for pcm: 2:mpegTs pcm(big endian), other: common pcm(little endian)

	int g726_enc_law;     // 1:a law; 0: u law;
	//int target_bit_rate;	// normally setting:40kbs/s(16:5) 32kbs/s(4:1) 24kbs/s(16:3) 16kbs/s(8:1)
}__audio_enc_inf_t;

#define BsBuffLEN 20*1024
typedef struct aenc_pcm_frm_info_s
{
    void *p_frm_buff;
    long long frm_pts;
}AENC_PCM_FRM_INFO;
typedef struct pcm_chunk_s
{
    long long  chunk_pts;  //unit: us
    unsigned int chunk_data_size;

    unsigned char *p_addr0;
    unsigned int size0;

    unsigned char *p_addr1;
    unsigned int size1;
}PCM_CHUNK;
#define PCM_CHUNK_NUM 1024
typedef struct __PCM_BUF_MANAGER
{
    unsigned char       *pBufStart;         //buffer首地址
    int                 uBufTotalLen;       //总长度
    unsigned char       *pBufReadPtr;       //正在读的指针
    int                 uDataLen;           //有效数据长度
    unsigned char       *pBufWritPtr;       //正在写的指针
    int                 uFreeBufSize;       //空余长度
	int                 uDataFlowflag;      //数据是否超过预定解码，1：超过补0帧，0：没有超过，正常编码
	void				*parent;			// encode use,not lib use;

} __pcm_buf_manager_t;


typedef struct __PCM_BUF_CHUNK_MANAGER
{
    PCM_CHUNK pcm_chunks[PCM_CHUNK_NUM]; 
    unsigned int chunks_num;
    unsigned int chunks_rd;
    unsigned int chunks_wt;
    unsigned int chunks_num_usable;

    long long last_chunk_pts;       // used to rebuild current chunk pts when no valid pts info carried
} __pcm_buf_chunk_manager_t;

typedef struct __COM_INTERNAL_PARAMETER
{
    unsigned int        ulNowTimeMS; 
    unsigned int        ulPCMgainSet;
    unsigned int        framecount;         //frames

    unsigned int        ulEncCom;           //0: normal enc 1: ff end   5: stop
    unsigned char       BsHeaderBuf[BS_HEADER_SIZE];
    unsigned int        ValidHeaderLen;
	unsigned int        *pEncInfoSet;		// encode lib use;

} __com_internal_prameter_t;


typedef struct __AudioENC_AC320
{
	__pcm_buf_manager_t         *pPcmBufManager;
	__audio_enc_inf_t           *AudioBsEncInf;
	__com_internal_prameter_t   *EncoderCom;
	int                         Encinitedflag;

	int	(*EncInit)(struct __AudioENC_AC320 *p);
	int	(*EncFrame)(struct __AudioENC_AC320 *p,char *OutBuffer,int *OutBuffLen);
	int	(*EncExit)(struct __AudioENC_AC320 *p);

}AudioEnc_AC320;

//extern struct __AudioENC_AC320 *EncInit(void);
//extern int  EncExit(struct __AudioENC_AC320 *p);

//struct __AudioENC_AC320 *AudioMP3ENCEncInit(void);
//int  AudioMP3ENCEncExit(struct __AudioENC_AC320 *p);

//编码库所需要的需上层模块实现的函数
extern int GetPcmDataSize(__pcm_buf_manager_t *pPcmMan);
extern int ReadPcmDataForEnc(void *pBuf, int uGetLen, __pcm_buf_manager_t *pPcmMan);

#endif  /* _AENC_SW_LIB_H_ */

#ifdef __cplusplus
}
#endif
