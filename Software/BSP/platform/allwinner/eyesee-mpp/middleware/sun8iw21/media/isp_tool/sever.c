
#ifndef CONFIG_LOG_LEVEL
#define CONFIG_LOG_LEVEL  OPTION_LOG_LEVEL_DETAIL
//#define CONFIG_LOG_LEVEL  OPTION_LOG_LEVEL_ERROR
#endif
    
#ifndef LOG_TAG
#define LOG_TAG "sever.c"
#endif
    
/*systrem header file*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

/* ISP header file */
#include "isp_tuning_cfg_api.h"
#include "isp_tuning_cfg.h"
#include "sever.h"

/* vin header file yuv*/
#include "mm_comm_vi.h"
#include "mm_comm_ise.h"

#include "vencoder.h"
#include "sc_interface.h"
#include "memoryAdapter.h"
#include "mpi_vi.h"
#include "mpi_ise.h"

#define PORT 5588	//port
#define QUEUE 20	
#define CHECK_SWITCH 		//switch for check out
//#define child_thread_count 5
#define child_thread_count 1
//#define Test_1
//#define Test_2
//#define Sever_Test 
//#define LOCK
#define MAX_VIDEO_NUM 	2  
#define NO_ERROR	    0	  // No errors.
#define Error           1
#define UNKNOWN_ERROR	0x80000000

//#define Test

typedef struct awVI_PrivCap_S
{
	AW_DEV Dev;
	AW_CHN Chn;
	AW_S32 s32MilliSec;
	VI_ATTR_S pstAttr;
	VI_DEV_ATTR_S pstDevAttr;
	VI_CAP_MODE_E enViCapMode;
	VI_INT_SEL_E enInterrupt;
	VIDEO_FRAME_INFO_S pstFrameInfo;

} VI_PrivCap_S;

struct VI_Parameter
{
    VI_PrivCap_S *privCap;
    AW_DEV ViDev;
    AW_CHN ViCh;
};

struct time_mesg {
    int    tm_sec;   /* seconds [0,61] */
    int    tm_min;   /* minutes [0,59] */
    int    tm_hour;  /* hour [0,23] */
    int    tm_mday;  /* day of month [1,31] */
    int    tm_mon;   /* month of year [0,11] */
    int    tm_year;  /* years since 1900 */
//    int    tm_wday;  /* day of week [0,6] (Sunday = 0) */
//    int    tm_yday;  /* day of year [0,365] */
//    int    tm_isdst; /* daylight savings flag */
};

struct JpegEncConfig
{
    unsigned int nVbvBufferSize;
};

struct Jpeg_BufferSize
{
	int nWidth;
	int nHeight;
	unsigned int ThumbWidth;
	unsigned int ThumbHeight;
	unsigned int nInputWidth;
    unsigned int nInputHeight;
    unsigned int nDstWidth;
    unsigned int nDstHeight;
};

struct YUV_Buffer
{
	unsigned char *yuv;
	int yuv_size;
};

struct JpegPicture_Buffer
{
	struct JpegEncConfig *Jpeg_Config;
	struct JpegEncInfo *Jpeg_EncInfo; 
	struct EXIFInfo *Jpeg_EXIFInfo;
	unsigned char **jpeg;
};

/*struct ISE_Buffer
{
    unsigned int mId; 
    unsigned int ise_mPhyAddr[3];
    unsigned int mStride[3];
};*/

struct ISE_Send_Buffer 
{
    unsigned char *ise_yuv; 
//    struct ISE_Buffer ise_data;
};

struct ISE_Parameter
{
    ISE_GRP IseGrp;
    ISE_CHN IseChn;
    AW_S32 s32MilliSec;
    int ise_size;
    VIDEO_FRAME_INFO_S ise_data;
    struct JpegPicture_Buffer *ise_chn0_jpeg_pst_buffer;
    struct JpegPicture_Buffer *ise_chn1_jpeg_pst_buffer;
    struct JpegPicture_Buffer *ise_splice_jpeg_pst_buffer;
    struct ISE_Send_Buffer *ise_send_buffer;
};

struct Image_Buffer
{
    unsigned int group_id; 
    void *pArg_Chn0;
    void *pArg_Chn1;
    struct YUV_Buffer *YUV_Pst;
    struct JpegPicture_Buffer *Jpeg_Pst;
    struct ISE_Parameter *ISE_Pst;
};

static struct isp_lib_context g_content;
static struct isp_tuning g_tuningParams;
static struct hw_isp_device g_isp_test;

void initParams()
{
	memset(&g_content, 0, sizeof(g_content));
	memset(&g_tuningParams, 0, sizeof(g_tuningParams));
	memset(&g_isp_test, 0, sizeof(g_isp_test));

    g_isp_test.id = 0;
	g_isp_test.ctx = &g_content;
	g_isp_test.tuning = &g_tuningParams;
	g_tuningParams.ctx = &g_content;

#ifdef Sever_Test	
	    /* client want to get some cfg */
	#ifdef Test_1
		//test group_1 group_id = HW_ISP_CFG_TEST;  cfg_ids = HW_ISP_CFG_TEST_PUB      
		g_tuningParams.params.isp_test_settings.isp_test_mode = 0x01;
		g_tuningParams.params.isp_test_settings.isp_gain = 88;
		g_tuningParams.params.isp_test_settings.isp_exp_line = 10000;
		g_tuningParams.params.isp_test_settings.isp_color_temp = 0x04;
		g_tuningParams.params.isp_test_settings.isp_log_param = 0x05;
	#endif

	#ifdef Test_2
	    unsigned short testbuffer[6] = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26};
		//test group_2 group_id = HW_ISP_CFG_TUNING; cfg_ids = HW_ISP_CFG_TUNING_LSC  
		g_tuningParams.params.isp_tunning_settings.ff_mod = 0x01;
		g_tuningParams.params.isp_tunning_settings.lsc_center_x = 1000;
		g_tuningParams.params.isp_tunning_settings.lsc_center_y = 2000;
		g_tuningParams.params.isp_tunning_settings.lsc_tbl[0][0] = 0x11; 
		memcpy(g_tuningParams.params.isp_tunning_settings.lsc_trig_cfg, testbuffer, sizeof(testbuffer));
	#endif
	
#endif
}

typedef enum {

	/* receive flag */
	HW_Sever_HEADFLAG_RECEIVE = 1,
	HW_Sever_HEADFLAG_RECEIVE_SUCCESS,	 
	HW_Sever_HEADFLAG_RECEIVE_FAILED,	
	HW_Sever_HEADFLAG_CHECK_SUCCESS,	 
	HW_Sever_DATA_RECEIVE,  
	HW_Sever_DATA_CHECK, 	//reserved
	HW_Sever_DATA_CHECK_SUCCESS,  
	HW_Sever_DATA_CHECK_FAILED,  
	
	HW_Sever_INTERMEDIATE_TRANSIT,	//intermediate transit   

	/* send flag */
	HW_Sever_HEADFLAG_SEND,
	HW_Sever_DATA_SEND,

	/* sever quit */
	HW_Sever_DISCONNECT,

	/* sever get rgb */
	HW_Sever_GET_RGB,
	HW_Sever_SEND_RGB,
	
	HW_Sever_Wait_Next_step
		
}hw_isp_sever_status;

int log_fd = 0;
int child_thread_exit_flag[child_thread_count+1] = {0};
int child_thread_running_flag[child_thread_count+1] = {0};
int yuv_fd = 0;
int ise_yuv_fd = 0;
signed int Count;
int Vin_Initialize;
int Vin_Exit;
pthread_mutex_t socket_lock;

#ifdef LOCK
pthread_mutex_t data_lock;
#endif

static void getCurrentDateTime(char *buf, int bufsize)
{
	time_t t;
	//struct time_mesg *tm_t; 
	struct tm *tm_t; 
    time(&t);
    tm_t = localtime(&t);
    snprintf(buf, bufsize, "%4d:%02d:%02d %02d:%02d:%02d",
        tm_t->tm_year+1900, tm_t->tm_mon+1, tm_t->tm_mday,
        tm_t->tm_hour, tm_t->tm_min, tm_t->tm_sec);	
}

void AW_JPEG_Init(void *pArg, struct JpegPicture_Buffer* pst)
{
	VI_PrivCap_S *privCap = (VI_PrivCap_S *)pArg;  
	pst->Jpeg_EncInfo->sBaseInfo.nInputWidth = privCap->pstAttr.format.width;
	pst->Jpeg_EncInfo->sBaseInfo.nInputHeight = privCap->pstAttr.format.height;
	pst->Jpeg_EncInfo->sBaseInfo.nDstWidth = privCap->pstAttr.format.width;
	pst->Jpeg_EncInfo->sBaseInfo.nDstHeight = privCap->pstAttr.format.height;
	pst->Jpeg_EncInfo->sBaseInfo.memops = MemAdapterGetOpsS();
	pst->Jpeg_EncInfo->sCropInfo.nTop = 0;
	pst->Jpeg_EncInfo->sCropInfo.nLeft = 0;
	pst->Jpeg_EncInfo->sCropInfo.nWidth =  privCap->pstAttr.format.width;
	pst->Jpeg_EncInfo->sCropInfo.nHeight = privCap->pstAttr.format.height;
	pst->Jpeg_EncInfo->quality = 90;
	pst->Jpeg_EncInfo->bEnableCorp = 1;
	pst->Jpeg_EncInfo->sBaseInfo.eInputFormat = VENC_PIXEL_YUV420SP; 

	pst->Jpeg_EXIFInfo->Orientation = 0;
	pst->Jpeg_EXIFInfo->ThumbWidth = (privCap->pstAttr.format.width >> 1);	//640
	pst->Jpeg_EXIFInfo->ThumbHeight = (privCap->pstAttr.format.height >> 1); //360
	pst->Jpeg_EXIFInfo->enableGpsInfo = 0;
	pst->Jpeg_EXIFInfo->WhiteBalance = 0;
	pst->Jpeg_EXIFInfo->MeteringMode = AVERAGE_AW_EXIF;
	pst->Jpeg_EXIFInfo->FocalLengthIn35mmFilm = 18;
	pst->Jpeg_EXIFInfo->FNumber.num = 26;
	pst->Jpeg_EXIFInfo->FNumber.den = 10;
	pst->Jpeg_EXIFInfo->FocalLength.num = 228;
	pst->Jpeg_EXIFInfo->FocalLength.den = 100;
	strcpy((char*)pst->Jpeg_EXIFInfo->ImageName, "aw-photo");
	strcpy((char*)pst->Jpeg_EXIFInfo->ImageDescription, "This photo is taken by AllWinner");
	getCurrentDateTime((char*)pst->Jpeg_EXIFInfo->DateTime, DATA_TIME_LENGTH);

	pst->Jpeg_Config->nVbvBufferSize = ((((pst->Jpeg_EncInfo->sBaseInfo.nDstWidth * pst->Jpeg_EncInfo->sBaseInfo.nDstHeight * 3) >> 2) + 1023) >> 10) << 10;
}

static int JpegEncode(JpegEncInfo *pJpegInfo, VideoEncoder *JpegEnc)
{
    VencAllocateBufferParam bufferParam;
    VencInputBuffer inputBuffer;

    if (pJpegInfo->bNoUseAddrPhy) {
    
		bufferParam.nSizeY = pJpegInfo->sBaseInfo.nStride * pJpegInfo->sBaseInfo.nInputHeight;
        bufferParam.nSizeC = bufferParam.nSizeY >> 1;
        bufferParam.nBufferNum = 1;

        if (AllocInputBuffer(JpegEnc, &bufferParam) < 0) {
        
            mesge(log_fd, "AllocInputBuffer error!!");
            return UNKNOWN_ERROR;
        }

        GetOneAllocInputBuffer(JpegEnc, &inputBuffer);
        memcpy(inputBuffer.pAddrVirY, pJpegInfo->pAddrPhyY, bufferParam.nSizeY);
        memcpy(inputBuffer.pAddrVirC, pJpegInfo->pAddrPhyC, bufferParam.nSizeC);

        FlushCacheAllocInputBuffer(JpegEnc, &inputBuffer);
    } else {
    
        inputBuffer.pAddrPhyY = pJpegInfo->pAddrPhyY;
        inputBuffer.pAddrPhyC = pJpegInfo->pAddrPhyC;
    }

    inputBuffer.bEnableCorp = pJpegInfo->bEnableCorp;
    inputBuffer.sCropInfo.nLeft = pJpegInfo->sCropInfo.nLeft;
    inputBuffer.sCropInfo.nTop = pJpegInfo->sCropInfo.nTop;
    inputBuffer.sCropInfo.nWidth = pJpegInfo->sCropInfo.nWidth;
    inputBuffer.sCropInfo.nHeight = pJpegInfo->sCropInfo.nHeight;
	
    AddOneInputBuffer(JpegEnc, &inputBuffer);
    if (VideoEncodeOneFrame(JpegEnc) != 0) {
    
        mesge(log_fd, "jpeg encoder error");
    }

    AlreadyUsedInputBuffer(JpegEnc, &inputBuffer);

    if (pJpegInfo->bNoUseAddrPhy) {
    
        ReturnOneAllocInputBuffer(JpegEnc, &inputBuffer);
    }

    return NO_ERROR;
}

static int getDataToBuffer(void *buffer, VencOutputBuffer *OutBuffer)
{
    char *p = (char *)buffer;
    memcpy(p, OutBuffer->pData0, OutBuffer->nSize0);
    p += OutBuffer->nSize0;
    if (OutBuffer->nSize1 > 0) {
        memcpy(p, OutBuffer->pData1, OutBuffer->nSize1);
    }
    return NO_ERROR;
}

static int getThumbOffset(off_t offset, VideoEncoder *JpegEnc, VencOutputBuffer *OutBuffer)
{
    EXIFInfo exif;

    memset(&exif, 0, sizeof(EXIFInfo));
    VideoEncGetParameter(JpegEnc, VENC_IndexParamJpegExifInfo, &exif);
    //len = exif.ThumbLen;
    if (exif.ThumbAddrVir >= OutBuffer->pData0 && exif.ThumbAddrVir < OutBuffer->pData0+OutBuffer->nSize0) {
        offset = exif.ThumbAddrVir - OutBuffer->pData0;
    } else if (exif.ThumbAddrVir >= OutBuffer->pData1 && exif.ThumbAddrVir < OutBuffer->pData1+OutBuffer->nSize1) {
        offset = OutBuffer->nSize0 + exif.ThumbAddrVir - OutBuffer->pData1;
    } else {
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

#ifdef Sever_Test
static void onPictureTaken(int chnId, const void *data, int size)
{
	mesgi(log_fd, "channel %d picture data size %d", chnId, size);
	char name[256];
    snprintf(name, 256, "/home/test%d.jpg", chnId);
    int fd = open(name, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        mesge(log_fd, "open file /home/yuvdata.dat failed(%s)!", strerror(errno));
        return;
    }
    write(fd, data, size);
    close(fd);
}
#endif

static int AW_Great_Jpeg(struct JpegPicture_Buffer *Jpeg_Pst)
{
	int ret = 0;
	VideoEncoder *JpegEncoder_Enc;
	VencOutputBuffer JpegEncoder_OutBuffer;
	size_t Jpeg_Size = 0, Buffer_Size = 0;
	off_t thumboffset = 0;

	JpegEncoder_Enc = VideoEncCreate(VENC_CODEC_JPEG);
    if (JpegEncoder_Enc == NULL) { 
	
        mesge(log_fd,"VideoEncCreate error!!\n");
        goto JPEG_INIT_ERR;
    }
    VideoEncSetParameter(JpegEncoder_Enc, VENC_IndexParamJpegExifInfo, Jpeg_Pst->Jpeg_EXIFInfo);
    VideoEncSetParameter(JpegEncoder_Enc, VENC_IndexParamJpegQuality, &Jpeg_Pst->Jpeg_EncInfo->quality);
    if (Jpeg_Pst->Jpeg_Config) {
    
        VideoEncSetParameter(JpegEncoder_Enc, VENC_IndexParamSetVbvSize, &Jpeg_Pst->Jpeg_Config->nVbvBufferSize);
    }

    if (VideoEncInit(JpegEncoder_Enc, &Jpeg_Pst->Jpeg_EncInfo->sBaseInfo) < 0) {   
        mesge(log_fd, "VideoEncInit error!!\n");
        VideoEncDestroy(JpegEncoder_Enc);
        JpegEncoder_Enc = NULL;
       	goto JPEG_INIT_ERR;
    }

	ret = JpegEncode(Jpeg_Pst->Jpeg_EncInfo, JpegEncoder_Enc);
	if (ret != NO_ERROR) { 	
		mesge(log_fd, "CameraJpegEncoder encode error!");
		goto JPEG_ENCODE_ERR;
	}
	
	if (GetOneBitstreamFrame(JpegEncoder_Enc, &JpegEncoder_OutBuffer) != 0) {
		mesge(log_fd, "CameraJpegEncoder getFrame error!");
		goto JPEG_GET_FRAME_ERR;
	}

	Jpeg_Size = JpegEncoder_OutBuffer.nSize0 + JpegEncoder_OutBuffer.nSize1;
	Buffer_Size = Jpeg_Size + sizeof(off_t) + sizeof(size_t) + sizeof(size_t);

	Jpeg_Pst->jpeg[0] = (unsigned char*)malloc(Buffer_Size);
	getDataToBuffer(Jpeg_Pst->jpeg[0], &JpegEncoder_OutBuffer);
	
	mesgi(log_fd, "Jpeg Encoder buffer_Size= %d\n", Buffer_Size);

	getThumbOffset((off_t)&thumboffset, JpegEncoder_Enc, &JpegEncoder_OutBuffer);
	FreeOneBitStreamFrame(JpegEncoder_Enc, &JpegEncoder_OutBuffer);	
	
	if (JpegEncoder_Enc != NULL) {
        VideoEncDestroy(JpegEncoder_Enc);
        JpegEncoder_Enc = NULL;
    }

	return Buffer_Size;

//ALLOC_MEM_ERR:
//	FreeOneBitStreamFrame(JpegEncoder_Enc, &JpegEncoder_OutBuffer);
JPEG_GET_FRAME_ERR:
//	return Error;
JPEG_ENCODE_ERR:
	if (JpegEncoder_Enc != NULL) {
        VideoEncDestroy(JpegEncoder_Enc);
        JpegEncoder_Enc = NULL;
    }
JPEG_INIT_ERR:
	return Error;
}
								
static int AW_MPI_VI_Get_Image(struct Image_Buffer *image_buffer)	
{
    struct Image_Buffer *buffer =  (struct Image_Buffer *)image_buffer;    
    int ret = 0; 
    int data_length = 0;

    VIDEO_FRAME_INFO_S ise_buffer;    
    VIDEO_FRAME_INFO_S *ise_pst_buffer;    
    ise_pst_buffer = &ise_buffer;   
    int chn0_data_length = 0, chn1_data_length = 0, splice_data_length = 0;
    
    struct VI_Parameter *VI_Par[2];
    struct VI_Parameter VI_Chn0_Parameter;
    struct VI_Parameter VI_Chn1_Parameter;

    if(!buffer->pArg_Chn0 && !buffer->pArg_Chn1) {
        mesge(log_fd,"error vi parameter!!\n");
        return -1;}
    
    if(!buffer->YUV_Pst && !buffer->Jpeg_Pst && !buffer->ISE_Pst) {
        mesge(log_fd,"error pst parameter!!\n");
        return -2;}

    if(buffer->group_id != HW_ISP_CFG_YUV && buffer->group_id != HW_ISP_CFG_JPEG && buffer->group_id != HW_ISP_CFG_ISE) {
        mesge(log_fd,"error group_id!!\n");
        return -3;}
    
    VI_PrivCap_S *privCap_Chn0 = (VI_PrivCap_S *)buffer->pArg_Chn0;  //pst = &privCap[0];
    VI_PrivCap_S *privCap_Chn1 = (VI_PrivCap_S *)buffer->pArg_Chn1;
   
    VI_Chn0_Parameter.privCap = privCap_Chn0;
    VI_Chn0_Parameter.ViCh = privCap_Chn0->Chn;
    VI_Chn0_Parameter.ViDev = privCap_Chn0->Dev;
    VI_Par[0] = &VI_Chn0_Parameter;

    VI_Chn1_Parameter.privCap = privCap_Chn1;
    VI_Chn1_Parameter.ViCh = privCap_Chn1->Chn;
    VI_Chn1_Parameter.ViDev = privCap_Chn1->Dev;
    VI_Par[1] = &VI_Chn1_Parameter;
    
	pthread_mutex_lock(&socket_lock);
	if (Vin_Initialize == 0) {		
		Vin_Initialize = 1;
		Vin_Exit = 1;
		pthread_mutex_unlock(&socket_lock);       

        int i = 0; 
        for(i = 0;i < 2;i++)
        {
            ret = AW_MPI_VI_Init();
    		if (SUCCESS != ret) {				
                mesge(log_fd, "AW_MPI_VI_Init failed\n");
    			return -1;
    		}
    	
    		ret = AW_MPI_VI_InitCh((*VI_Par[i]).ViCh);
    		if (SUCCESS != ret) {
    			mesge(log_fd, "AW_MPI_VI_InitCh failed\n");
    			//goto vi_exit;
    			return Error;
    		}

    		ret = AW_MPI_VI_SetDevAttr((*VI_Par[i]).ViDev, &((*VI_Par[i]).privCap->pstDevAttr));
    		if (SUCCESS != ret) {
    			mesge(log_fd, "AW_MPI_VI_SetDevAttr failed\n");
    			//goto exit_vich;
    			AW_MPI_VI_ExitCh((*VI_Par[i]).ViCh);
	            return Error;
    		}

    		ret = AW_MPI_VI_GetDevAttr((*VI_Par[i]).ViDev, &((*VI_Par[i]).privCap->pstDevAttr));
    		if (SUCCESS != ret) {
    			mesge(log_fd, "AW_MPI_VI_GetDevAttr failed\n");
    			//goto vi_exit;
    			return Error;
    		}

    		ret = AW_MPI_VI_EnableDev((*VI_Par[i]).ViDev);
    		if (SUCCESS != ret) {
    			mesge(log_fd, "AW_MPI_VI_EnableDev failed\n");
    			//goto disabledev;
            	AW_MPI_VI_DisableDev((*VI_Par[i]).ViDev);
            	AW_MPI_VI_ExitCh((*VI_Par[i]).ViCh);
            	return Error; 
    		}

    		ret = AW_MPI_VI_SetChnAttr((*VI_Par[i]).ViCh, &((*VI_Par[i]).privCap->pstAttr));
    		if (SUCCESS != ret) {
    			mesge(log_fd, "AW_MPI_VI_SetChnAttr failed\n");
                //goto disabledev;
    			AW_MPI_VI_DisableDev((*VI_Par[i]).ViDev);
            	AW_MPI_VI_ExitCh((*VI_Par[i]).ViCh);
            	return Error; 
    		}

    		ret = AW_MPI_VI_GetChnAttr((*VI_Par[i]).ViCh, &((*VI_Par[i]).privCap->pstAttr));
    		if (SUCCESS != ret) {
    			mesge(log_fd, "AW_MPI_VI_GetChnAttr failed\n");
                //goto disabledev;    
    			AW_MPI_VI_DisableDev((*VI_Par[i]).ViDev);
            	AW_MPI_VI_ExitCh((*VI_Par[i]).ViCh);
            	return Error; 
    		}

    		ret = AW_MPI_VI_CapStart((*VI_Par[i]).ViDev, (*VI_Par[i]).privCap->enViCapMode);
    		if (SUCCESS != ret) {
    			mesge(log_fd, "AW_MPI_VI_EnableChn failed\n");
    			//goto disabledev;
    			AW_MPI_VI_DisableDev((*VI_Par[i]).ViDev);
            	AW_MPI_VI_ExitCh((*VI_Par[i]).ViCh);
            	return Error; 
    		}

    		ret = AW_MPI_VI_EnableChnInterrupt((*VI_Par[i]).ViDev, (*VI_Par[i]).ViCh, (*VI_Par[i]).privCap->enInterrupt);
    		if (SUCCESS != ret) {
    			mesge(log_fd, "AW_MPI_VI_EnableChn failed\n");
    			//goto capstop;
    			AW_MPI_VI_CapStop((*VI_Par[i]).ViDev, (*VI_Par[i]).privCap->enViCapMode);
            	AW_MPI_VI_DisableDev((*VI_Par[i]).ViDev);
            	AW_MPI_VI_ExitCh((*VI_Par[i]).ViCh);
            	return Error;
    		}

    		ret = AW_MPI_VI_EnableChn((*VI_Par[i]).ViCh);
    		if (SUCCESS != ret) {
    			mesge(log_fd, "AW_MPI_VI_EnableChn failed\n");
    			//goto disableint;
    		    AW_MPI_VI_DisableChnInterrupt((*VI_Par[i]).ViDev, (*VI_Par[i]).ViCh, (*VI_Par[i]).privCap->enInterrupt);
            	AW_MPI_VI_CapStop((*VI_Par[i]).ViDev, (*VI_Par[i]).privCap->enViCapMode);
            	AW_MPI_VI_DisableDev((*VI_Par[i]).ViDev);
            	AW_MPI_VI_ExitCh((*VI_Par[i]).ViCh);
            	return Error;
    		}
        }
	} else {
		pthread_mutex_unlock(&socket_lock);
    }

#ifdef LOCK
	pthread_mutex_lock(&data_lock);
	if (AW_MPI_VI_GetFrame((*VI_Par[0]).ViCh, &((*VI_Par[0]).privCap->pstFrameInfo), (*VI_Par[0]).privCap->s32MilliSec) < 0) {	
		mesge(log_fd, "VI Get Frame failed!\n");
		AW_MPI_VI_ReleaseFrame((*VI_Par[0]).ViCh, &((*VI_Par[0]).privCap->pstFrameInfo));
		pthread_mutex_unlock(&data_lock);
		return Error;
	}

	/* get yuv */
	if (buffer->group_id == HW_ISP_CFG_YUV) {	
		memcpy(buffer->YUV_Pst->yuv, (*VI_Par[0]).privCap->pstFrameInfo.pVirAddr[0], buffer->YUV_Pst->yuv_size); 		//Y
		buffer->YUV_Pst->yuv +=  buffer->YUV_Pst->yuv_size;
		memcpy(buffer->YUV_Pst->yuv, (*VI_Par[0]).privCap->pstFrameInfo.pVirAddr[1], (buffer->YUV_Pst->yuv_size >> 1));	//VU
		data_length += (buffer->YUV_Pst->yuv_size + (buffer->YUV_Pst->yuv_size >> 1));
	}

	/* get jpeg */
	if (buffer->group_id == HW_ISP_CFG_JPEG) { 
		buffer->Jpeg_Pst->Jpeg_EncInfo->pAddrPhyY = (unsigned char*)(*VI_Par[0]).privCap->pstFrameInfo.u32PhyAddr[0];
		buffer->Jpeg_Pst->Jpeg_EncInfo->pAddrPhyC = (unsigned char*)(*VI_Par[0]).privCap->pstFrameInfo.u32PhyAddr[1];		
		
        data_length = AW_Great_Jpeg(buffer->Jpeg_Pst);
		if (data_length == Error) {
			mesge(log_fd, "Jpeg encoder failed!\n");
			AW_MPI_VI_ReleaseFrame((*VI_Par[0]).ViCh, &(*VI_Par[0]).privCap->pstFrameInfo);
			pthread_mutex_unlock(&data_lock);
			return Error;
		}
	}

    /* get ise */
    if (buffer->group_id == HW_ISP_CFG_ISE) {

        if (AW_MPI_VI_GetFrame((*VI_Par[1]).ViCh, &((*VI_Par[1]).privCap->pstFrameInfo), (*VI_Par[1]).privCap->s32MilliSec) < 0) {	
    		mesge(log_fd, "VI Get Frame failed!\n");
    		AW_MPI_VI_ReleaseFrame((*VI_Par[1]).ViCh, &((*VI_Par[1]).privCap->pstFrameInfo));
    		pthread_mutex_unlock(&data_lock);
    		return Error;
	    }

        buffer->ISE_Pst->ise_chn0_jpeg_pst_buffer->Jpeg_EncInfo->pAddrPhyY = (unsigned char*)(*VI_Par[0]).privCap->pstFrameInfo.u32PhyAddr[0];
        buffer->ISE_Pst->ise_chn0_jpeg_pst_buffer->Jpeg_EncInfo->pAddrPhyC = (unsigned char*)(*VI_Par[0]).privCap->pstFrameInfo.u32PhyAddr[1];        
        buffer->ISE_Pst->ise_chn1_jpeg_pst_buffer->Jpeg_EncInfo->pAddrPhyY = (unsigned char*)(*VI_Par[1]).privCap->pstFrameInfo.u32PhyAddr[0];
        buffer->ISE_Pst->ise_chn1_jpeg_pst_buffer->Jpeg_EncInfo->pAddrPhyC = (unsigned char*)(*VI_Par[1]).privCap->pstFrameInfo.u32PhyAddr[1];

        AW_MPI_ISE_SendPic(buffer->ISE_Pst->IseGrp, &((*VI_Par[0]).privCap->pstFrameInfo), &((*VI_Par[1]).privCap->pstFrameInfo), buffer->ISE_Pst->s32MilliSec);
        AW_MPI_ISE_GetData(buffer->ISE_Pst->IseGrp, buffer->ISE_Pst->IseChn, ise_pst_buffer, buffer->ISE_Pst->s32MilliSec);

        /* ISE yuv */
        memcpy(buffer->ISE_Pst->ise_send_buffer->ise_yuv, ise_pst_buffer->VFrame.mpVirAddr, buffer->ISE_Pst->ise_size); 		 //Y
		buffer->ISE_Pst->ise_send_buffer->ise_yuv +=  buffer->ISE_Pst->ise_size;
		memcpy(buffer->ISE_Pst->ise_send_buffer->ise_yuv, ise_pst_buffer->VFrame.mpVirAddr, (buffer->ISE_Pst->ise_size >> 1)); //VU
        data_length += (buffer->ISE_Pst->ise_size + (buffer->ISE_Pst->ise_size >> 1));

        /* ISE Phyaddress */
        buffer->ISE_Pst->ise_splice_jpeg_pst_buffer->Jpeg_EncInfo->pAddrPhyY = (unsigned char*)ise_pst_buffer->VFrame.mPhyAddr[0];
        buffer->ISE_Pst->ise_splice_jpeg_pst_buffer->Jpeg_EncInfo->pAddrPhyC = (unsigned char*)ise_pst_buffer->VFrame.mPhyAddr[1];

        chn0_data_length = AW_Great_Jpeg(buffer->ISE_Pst->ise_chn0_jpeg_pst_buffer);
	    if (chn0_data_length == Error) {
		    mesge(log_fd, "Jpeg encoder failed!\n");
            AW_MPI_ISE_ReleaseData(buffer->ISE_Pst->IseGrp, buffer->ISE_Pst->IseChn,ise_pst_buffer); 
		    AW_MPI_VI_ReleaseFrame((*VI_Par[0]).ViCh, &((*VI_Par[0]).privCap->pstFrameInfo));
            AW_MPI_VI_ReleaseFrame((*VI_Par[1]).ViCh, &((*VI_Par[1]).privCap->pstFrameInfo));
		    pthread_mutex_unlock(&data_lock);
		    return Error;
        }        
        #ifdef Sever_Test
		onPictureTaken(1, buffer->ISE_Pst->ise_chn0_jpeg_pst_buffer->jpeg[0], chn0_data_length);
        #endif
        
        chn1_data_length = AW_Great_Jpeg(buffer->ISE_Pst->ise_chn1_jpeg_pst_buffer);
	    if (chn1_data_length == Error) {
		    mesge(log_fd, "Jpeg encoder failed!\n");
		    AW_MPI_ISE_ReleaseData(buffer->ISE_Pst->IseGrp, buffer->ISE_Pst->IseChn,ise_pst_buffer); 
		    AW_MPI_VI_ReleaseFrame((*VI_Par[0]).ViCh, &((*VI_Par[0]).privCap->pstFrameInfo));
            AW_MPI_VI_ReleaseFrame((*VI_Par[1]).ViCh, &((*VI_Par[1]).privCap->pstFrameInfo));
		    pthread_mutex_unlock(&data_lock);
		    return Error;
        }
        #ifdef Sever_Test
        onPictureTaken(2, buffer->ISE_Pst->ise_chn1_jpeg_pst_buffer->jpeg[0], chn1_data_length);
        #endif

        splice_data_length = AW_Great_Jpeg(buffer->ISE_Pst->ise_splice_jpeg_pst_buffer);
	    if (splice_data_length == Error) {
		    mesge(log_fd, "Jpeg encoder failed!\n");
            AW_MPI_ISE_ReleaseData(buffer->ISE_Pst->IseGrp, buffer->ISE_Pst->IseChn,ise_pst_buffer); 
		    AW_MPI_VI_ReleaseFrame((*VI_Par[0]).ViCh, &((*VI_Par[0]).privCap->pstFrameInfo));
            AW_MPI_VI_ReleaseFrame((*VI_Par[1]).ViCh, &((*VI_Par[1]).privCap->pstFrameInfo));
		    pthread_mutex_unlock(&data_lock);
		    return Error;
        }        
        #ifdef Sever_Test
		onPictureTaken(3, buffer->ISE_Pst->ise_splice_jpeg_pst_buffer->jpeg[0], splice_data_length);
        #endif
        
        AW_MPI_ISE_ReleaseData(buffer->ISE_Pst->IseGrp, buffer->ISE_Pst->IseChn,ise_pst_buffer); 
        AW_MPI_VI_ReleaseFrame((*VI_Par[1]).ViCh, &((*VI_Par[1]).privCap->pstFrameInfo));
    }
    
	AW_MPI_VI_ReleaseFrame((*VI_Par[0]).ViCh, &((*VI_Par[0]).privCap->pstFrameInfo));
   
	pthread_mutex_unlock(&data_lock);
	
	return data_length;
#endif

#ifndef LOCK
	
	if (AW_MPI_VI_GetFrame((*VI_Par[0]).ViCh, &((*VI_Par[0]).privCap->pstFrameInfo), (*VI_Par[0]).privCap->s32MilliSec) < 0) {	
		mesge(log_fd, "VI Get Frame failed!\n");
		AW_MPI_VI_ReleaseFrame((*VI_Par[0]).ViCh, &((*VI_Par[0]).privCap->pstFrameInfo));
		return Error;
	}

	/* get yuv */
	if (buffer->group_id == HW_ISP_CFG_YUV) {	
		memcpy(buffer->YUV_Pst->yuv, (*VI_Par[0]).privCap->pstFrameInfo.pVirAddr[0], buffer->YUV_Pst->yuv_size); 		//Y
		buffer->YUV_Pst->yuv +=  buffer->YUV_Pst->yuv_size;
		memcpy(buffer->YUV_Pst->yuv, (*VI_Par[0]).privCap->pstFrameInfo.pVirAddr[1], (buffer->YUV_Pst->yuv_size >> 1));	//VU
		data_length += (buffer->YUV_Pst->yuv_size + (buffer->YUV_Pst->yuv_size >> 1));
	}

	/* get jpeg */
	if (buffer->group_id == HW_ISP_CFG_JPEG) { 
		buffer->Jpeg_Pst->Jpeg_EncInfo->pAddrPhyY = (unsigned char*)(*VI_Par[0]).privCap->pstFrameInfo.u32PhyAddr[0];
		buffer->Jpeg_Pst->Jpeg_EncInfo->pAddrPhyC = (unsigned char*)(*VI_Par[0]).privCap->pstFrameInfo.u32PhyAddr[1];		
		
        data_length = AW_Great_Jpeg(buffer->Jpeg_Pst);
		if (data_length == Error) {
			mesge(log_fd, "Jpeg encoder failed!\n");
			AW_MPI_VI_ReleaseFrame((*VI_Par[0]).ViCh, &(*VI_Par[0]).privCap->pstFrameInfo);
			return Error;
		}
	}

    /* get ise */
    if (buffer->group_id == HW_ISP_CFG_ISE) {

        if (AW_MPI_VI_GetFrame((*VI_Par[1]).ViCh, &((*VI_Par[1]).privCap->pstFrameInfo), (*VI_Par[1]).privCap->s32MilliSec) < 0) {	
    		mesge(log_fd, "VI Get Frame failed!\n");
    		AW_MPI_VI_ReleaseFrame((*VI_Par[1]).ViCh, &((*VI_Par[1]).privCap->pstFrameInfo));
    		return Error;
	    }

        /* Chn0 Ch1 Phyaddress */
        buffer->ISE_Pst->ise_chn0_jpeg_pst_buffer->Jpeg_EncInfo->pAddrPhyY = (unsigned char*)(*VI_Par[0]).privCap->pstFrameInfo.u32PhyAddr[0];
        buffer->ISE_Pst->ise_chn0_jpeg_pst_buffer->Jpeg_EncInfo->pAddrPhyC = (unsigned char*)(*VI_Par[0]).privCap->pstFrameInfo.u32PhyAddr[1];        
        buffer->ISE_Pst->ise_chn1_jpeg_pst_buffer->Jpeg_EncInfo->pAddrPhyY = (unsigned char*)(*VI_Par[1]).privCap->pstFrameInfo.u32PhyAddr[0];
        buffer->ISE_Pst->ise_chn1_jpeg_pst_buffer->Jpeg_EncInfo->pAddrPhyC = (unsigned char*)(*VI_Par[1]).privCap->pstFrameInfo.u32PhyAddr[1];

        AW_MPI_ISE_SendPic(buffer->ISE_Pst->IseGrp, &((*VI_Par[0]).privCap->pstFrameInfo), &((*VI_Par[1]).privCap->pstFrameInfo), buffer->ISE_Pst->s32MilliSec);
        AW_MPI_ISE_GetData(buffer->ISE_Pst->IseGrp, buffer->ISE_Pst->IseChn, ise_pst_buffer, buffer->ISE_Pst->s32MilliSec);

        /* ISE yuv */
        memcpy(buffer->ISE_Pst->ise_send_buffer->ise_yuv, ise_pst_buffer->VFrame.mpVirAddr, buffer->ISE_Pst->ise_size); 	   //Y
		buffer->ISE_Pst->ise_send_buffer->ise_yuv +=  buffer->ISE_Pst->ise_size;
		memcpy(buffer->ISE_Pst->ise_send_buffer->ise_yuv, ise_pst_buffer->VFrame.mpVirAddr, (buffer->ISE_Pst->ise_size >> 1)); //VU
        data_length += (buffer->ISE_Pst->ise_size + (buffer->ISE_Pst->ise_size >> 1));

        /* ISE Phyaddress */
        buffer->ISE_Pst->ise_splice_jpeg_pst_buffer->Jpeg_EncInfo->pAddrPhyY = (unsigned char*)ise_pst_buffer->VFrame.mPhyAddr[0];
        buffer->ISE_Pst->ise_splice_jpeg_pst_buffer->Jpeg_EncInfo->pAddrPhyC = (unsigned char*)ise_pst_buffer->VFrame.mPhyAddr[1];

        chn0_data_length = AW_Great_Jpeg(buffer->ISE_Pst->ise_chn0_jpeg_pst_buffer);
	    if (chn0_data_length == Error) {
		    mesge(log_fd, "Jpeg encoder failed!\n");
            AW_MPI_ISE_ReleaseData(buffer->ISE_Pst->IseGrp, buffer->ISE_Pst->IseChn,ise_pst_buffer); 
		    AW_MPI_VI_ReleaseFrame((*VI_Par[0]).ViCh, &((*VI_Par[0]).privCap->pstFrameInfo));
            AW_MPI_VI_ReleaseFrame((*VI_Par[1]).ViCh, &((*VI_Par[1]).privCap->pstFrameInfo));
		    return Error;
        }
        #ifdef Sever_Test
		onPictureTaken(1, buffer->ISE_Pst->ise_chn0_jpeg_pst_buffer->jpeg[0], chn0_data_length);
        #endif
    
        chn1_data_length = AW_Great_Jpeg(buffer->ISE_Pst->ise_chn1_jpeg_pst_buffer);
	    if (chn1_data_length == Error) {
		    mesge(log_fd, "Jpeg encoder failed!\n");
		    AW_MPI_ISE_ReleaseData(buffer->ISE_Pst->IseGrp, buffer->ISE_Pst->IseChn,ise_pst_buffer); 
		    AW_MPI_VI_ReleaseFrame((*VI_Par[0]).ViCh, &((*VI_Par[0]).privCap->pstFrameInfo));
            AW_MPI_VI_ReleaseFrame((*VI_Par[1]).ViCh, &((*VI_Par[1]).privCap->pstFrameInfo));
		    return Error;
        }
        #ifdef Sever_Test
        onPictureTaken(2, buffer->ISE_Pst->ise_chn1_jpeg_pst_buffer->jpeg[0], chn1_data_length);
        #endif

        splice_data_length = AW_Great_Jpeg(buffer->ISE_Pst->ise_splice_jpeg_pst_buffer);
	    if (splice_data_length == Error) {
		    mesge(log_fd, "Jpeg encoder failed!\n");
            AW_MPI_ISE_ReleaseData(buffer->ISE_Pst->IseGrp, buffer->ISE_Pst->IseChn,ise_pst_buffer); 
		    AW_MPI_VI_ReleaseFrame((*VI_Par[0]).ViCh, &((*VI_Par[0]).privCap->pstFrameInfo));
            AW_MPI_VI_ReleaseFrame((*VI_Par[1]).ViCh, &((*VI_Par[1]).privCap->pstFrameInfo));
		    return Error;
        }
        #ifdef Sever_Test
		onPictureTaken(3, buffer->ISE_Pst->ise_splice_jpeg_pst_buffer->jpeg[0], splice_data_length);
        #endif
        
        AW_MPI_ISE_ReleaseData(buffer->ISE_Pst->IseGrp, buffer->ISE_Pst->IseChn,ise_pst_buffer); 
        AW_MPI_VI_ReleaseFrame((*VI_Par[1]).ViCh, &((*VI_Par[1]).privCap->pstFrameInfo));
    }
   
	AW_MPI_VI_ReleaseFrame((*VI_Par[0]).ViCh, &((*VI_Par[0]).privCap->pstFrameInfo));
	
	return data_length;
#endif

//disablech:
	//AW_MPI_VI_DisableChn(ViCh);	
/*disableint:
	AW_MPI_VI_DisableChnInterrupt((*VI_Par[0]).ViDev, (*VI_Par[0]).ViCh, (*VI_Par[0]).privCap->enInterrupt);
capstop:
	AW_MPI_VI_CapStop((*VI_Par[0]).ViDev, privCap_Chn0->enViCapMode);
disabledev:
	AW_MPI_VI_DisableDev((*VI_Par[0]).ViDev);
exit_vich:
	AW_MPI_VI_ExitCh((*VI_Par[0]).ViCh);
vi_exit:
	return Error; */

}

/* logical processing status machine */
static void Sever_Status_Machine(void *socket_id, int num)
{
	/* client socketfd */
	int client_id = (int)socket_id;	

	int try_count = 0, recv_count = 0, count = 0, left_len = 0;		
	
	signed int send_status = 0, Rev_len = 0, data_length = 0;	

	unsigned char cmdBuffer[10] = {0};
	
	/* Sever send to Client */
	short send_command = 0;
	unsigned int send_group_id = 0, send_cfg_ids = 0;

	struct hw_comm_package *str =  NULL;
	struct hw_comm_package *send_str =  NULL;
	struct hw_sever_receive *recv_str =  NULL;

	unsigned char *Get_DataBuffer =  NULL;
	unsigned char *Receive_Headflag = NULL;
	unsigned char *Set_DataBuffer = NULL;

	send_str = (struct hw_comm_package *)malloc(sizeof(struct hw_comm_package));
	memset(send_str, 0, sizeof(struct hw_comm_package));

	recv_str = (struct hw_sever_receive *)malloc(sizeof(struct hw_sever_receive));
	memset(recv_str, 0, sizeof(struct hw_sever_receive));

	Receive_Headflag = (unsigned char *)malloc(sizeof(struct hw_comm_package));
	memset(Receive_Headflag, 0, sizeof(struct hw_comm_package));

	Get_DataBuffer = (unsigned char *)malloc(sizeof(struct isp_param_config));
	memset(Get_DataBuffer, 0, sizeof(struct isp_param_config));

	Set_DataBuffer = (unsigned char *)malloc(sizeof(struct isp_param_config));
	memset(Set_DataBuffer, 0, sizeof(struct isp_param_config));
	
	unsigned char Head_Flag[] = {"HWV5"};	//head flag send

	/* Sever Status flag */
	char Sever_Status = HW_Sever_HEADFLAG_RECEIVE;  

	/* test */
	unsigned char *ttt = NULL;	
    int *kkk = NULL;	
	int jjj = 0;
	
	VI_PrivCap_S privCap[3];
	int i = 0;

	/* VI initialize */
	for(i = 0; i < MAX_VIDEO_NUM; i++)
	{
		memset(&privCap[i], 0, sizeof(VI_PrivCap_S));
		
		/*Set Dev ID and Chn ID*/
		privCap[i].Dev = i;
		privCap[i].Chn = i;
		privCap[i].s32MilliSec = 5000;

		/*Set VI Channel Attribute*/
		privCap[i].pstAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		privCap[i].pstAttr.memtype = V4L2_MEMORY_MMAP;
		privCap[i].pstAttr.format.pixelformat = V4L2_PIX_FMT_NV21M;
		privCap[i].pstAttr.format.field = V4L2_FIELD_NONE;
		privCap[i].pstAttr.format.width = 1280;  //720p/1080p/640*480 
		privCap[i].pstAttr.format.height= 720;
		privCap[i].pstAttr.nbufs = 5;
	  //privCap[i].pstAttr.nplanes = 3;
		privCap[i].pstAttr.nplanes = 2;
	}
	
	/* YUV initialize */
	int camera_group = 0; 
	int width = privCap[0].pstAttr.format.width;
	int height = privCap[0].pstAttr.format.height;
	int YUV_ImgSize = width * height;
	int YUV_FrameSize = YUV_ImgSize + (YUV_ImgSize >> 1);
	unsigned char *yuv = NULL;
	struct YUV_Buffer yuv_buffer;
	struct YUV_Buffer *yuv_pst_buffer; 
	yuv_pst_buffer = &yuv_buffer;
	
	/* JPGE initialize */
	JpegEncInfo Jpeg;
	EXIFInfo   ExitInfo;
	struct JpegEncConfig JpegSize;
    
	unsigned char *Jpeg_Data = NULL;
	struct JpegPicture_Buffer jpeg_buffer;
	struct JpegPicture_Buffer *jpeg_pst_buffer;
	jpeg_pst_buffer = &jpeg_buffer;
	jpeg_pst_buffer->Jpeg_Config = &JpegSize;
	jpeg_pst_buffer->Jpeg_EncInfo = &Jpeg;
	jpeg_pst_buffer->Jpeg_EXIFInfo = &ExitInfo;
    jpeg_pst_buffer->jpeg = &Jpeg_Data;
	
	memset(jpeg_pst_buffer->Jpeg_EncInfo, 0, sizeof(struct JpegEncInfo));
	memset(jpeg_pst_buffer->Jpeg_EXIFInfo, 0, sizeof(struct EXIFInfo));
	memset(jpeg_pst_buffer->Jpeg_Config, 0, sizeof(struct JpegEncConfig));
	AW_JPEG_Init(&privCap[0], jpeg_pst_buffer);

    /* ISE initialize */
    struct ISE_Parameter ise_parameter;
    struct ISE_Parameter *ise_pst_parameter;
    struct ISE_Send_Buffer ise_buffer;
    struct ISE_Send_Buffer *ise_pst_buffer;
    
    int ISE_ImgSize = (width * 2) * height;
	int ISE_FrameSize = ISE_ImgSize + (ISE_ImgSize >> 1);
    unsigned char *ise_yuv = NULL;
    ise_pst_parameter = &ise_parameter;
    ise_pst_buffer = &ise_buffer;
    ise_pst_parameter->IseChn = 0;
    ise_pst_parameter->IseGrp = 0;
    ise_pst_parameter->s32MilliSec = privCap[0].s32MilliSec;  //5000
    ise_pst_parameter->ise_size = ISE_ImgSize;
    ise_pst_parameter->ise_send_buffer = ise_pst_buffer;
 // ise_pst_buffer->ise_mpVirAddr = (unsigned char*)malloc(ISE_FrameSize);

    unsigned char *ISE_Chn0_Jpeg_Data = NULL, *ISE_Chn1_Jpeg_Data = NULL, *ISE_Splice_Jpeg_Data = NULL;
    struct JpegPicture_Buffer ISE_Chn0_Jpeg_Buffer, ISE_Chn1_Jpeg_Buffer, ISE_Splice_Jpeg_Buffer;
    JpegEncInfo ISE_Chn0_Jpeg, ISE_Chn1_Jpeg, ISE_Splice_Jpeg;
	EXIFInfo ISE_Chn0_ExitInfo, ISE_Chn1_ExitInfo, ISE_Splice_ExitInfo;
	struct JpegEncConfig ISE_Chn0_JpegSize, ISE_Chn1_JpegSize, ISE_Splice_JpegSize;
    
    ise_pst_parameter->ise_chn0_jpeg_pst_buffer = &ISE_Chn0_Jpeg_Buffer;
	ise_pst_parameter->ise_chn0_jpeg_pst_buffer->Jpeg_Config = &ISE_Chn0_JpegSize;
	ise_pst_parameter->ise_chn0_jpeg_pst_buffer->Jpeg_EncInfo = &ISE_Chn0_Jpeg;
	ise_pst_parameter->ise_chn0_jpeg_pst_buffer->Jpeg_EXIFInfo = &ISE_Chn0_ExitInfo;
    ise_pst_parameter->ise_chn0_jpeg_pst_buffer->jpeg = &ISE_Chn0_Jpeg_Data;	
	memset(ise_pst_parameter->ise_chn0_jpeg_pst_buffer->Jpeg_EncInfo, 0, sizeof(struct JpegEncInfo));
	memset(ise_pst_parameter->ise_chn0_jpeg_pst_buffer->Jpeg_EXIFInfo, 0, sizeof(struct EXIFInfo));
	memset(ise_pst_parameter->ise_chn0_jpeg_pst_buffer->Jpeg_Config, 0, sizeof(struct JpegEncConfig));
	AW_JPEG_Init(&privCap[0],  ise_pst_parameter->ise_chn0_jpeg_pst_buffer);

    ise_pst_parameter->ise_chn1_jpeg_pst_buffer = &ISE_Chn1_Jpeg_Buffer;
    ise_pst_parameter->ise_chn1_jpeg_pst_buffer->Jpeg_Config = &ISE_Chn1_JpegSize;
	ise_pst_parameter->ise_chn1_jpeg_pst_buffer->Jpeg_EncInfo = &ISE_Chn1_Jpeg;
	ise_pst_parameter->ise_chn1_jpeg_pst_buffer->Jpeg_EXIFInfo = &ISE_Chn1_ExitInfo;
    ise_pst_parameter->ise_chn1_jpeg_pst_buffer->jpeg = &ISE_Chn1_Jpeg_Data;	
	memset(ise_pst_parameter->ise_chn1_jpeg_pst_buffer->Jpeg_EncInfo, 0, sizeof(struct JpegEncInfo));
	memset(ise_pst_parameter->ise_chn1_jpeg_pst_buffer->Jpeg_EXIFInfo, 0, sizeof(struct EXIFInfo));
	memset(ise_pst_parameter->ise_chn1_jpeg_pst_buffer->Jpeg_Config, 0, sizeof(struct JpegEncConfig));
	AW_JPEG_Init(&privCap[1],  ise_pst_parameter->ise_chn1_jpeg_pst_buffer);

    privCap[2].pstAttr.format.width = privCap[0].pstAttr.format.width * 2;
    privCap[2].pstAttr.format.height = privCap[0].pstAttr.format.height;    
    ise_pst_parameter->ise_splice_jpeg_pst_buffer = &ISE_Splice_Jpeg_Buffer;
    ise_pst_parameter->ise_splice_jpeg_pst_buffer->Jpeg_Config = &ISE_Splice_JpegSize;
    ise_pst_parameter->ise_splice_jpeg_pst_buffer->Jpeg_EncInfo = &ISE_Splice_Jpeg;
    ise_pst_parameter->ise_splice_jpeg_pst_buffer->Jpeg_EXIFInfo = &ISE_Splice_ExitInfo;
    ise_pst_parameter->ise_splice_jpeg_pst_buffer->jpeg = &ISE_Splice_Jpeg_Data;    
    memset(ise_pst_parameter->ise_splice_jpeg_pst_buffer->Jpeg_EncInfo, 0, sizeof(struct JpegEncInfo));
    memset(ise_pst_parameter->ise_splice_jpeg_pst_buffer->Jpeg_EXIFInfo, 0, sizeof(struct EXIFInfo));
    memset(ise_pst_parameter->ise_splice_jpeg_pst_buffer->Jpeg_Config, 0, sizeof(struct JpegEncConfig));
    AW_JPEG_Init(&privCap[2], ise_pst_parameter->ise_splice_jpeg_pst_buffer);
   
    struct Image_Buffer image_buffer;

    /*test*/
    //recv_str->receive_command = HW_ISP_CFG_SET;  
    /*recv_str->receive_command = HW_ISP_CFG_GET;  
	recv_str->receive_group_id = HW_ISP_CFG_TEST_PUB;
	recv_str->receive_cfg_ids = 0;
    Sever_Status = HW_Sever_INTERMEDIATE_TRANSIT;*/
  //  recv_str->receive_ret = SEVER_GET_ISE_SUCCESS,;

	while (!child_thread_exit_flag[num]) 
	{	
		switch (Sever_Status)
		{

		    case HW_Sever_HEADFLAG_RECEIVE:
            {   
				/* receive head flag from client */				
				Rev_len = 0, left_len = 0, recv_count = 0; 
				memset(Receive_Headflag, 0, sizeof(struct hw_comm_package));
						
				Rev_len = recv(client_id, Receive_Headflag, sizeof(struct hw_comm_package), MSG_DONTWAIT);
				
				while (Rev_len < sizeof(struct hw_comm_package)) 
				{
					left_len = sizeof(struct hw_comm_package) - Rev_len;
					Rev_len += recv(client_id, Receive_Headflag + Rev_len, left_len, MSG_DONTWAIT);  
					recv_count++;
					
					/* beyond the limit of time,receive fail */
					if (recv_count > 20000) {   					
						recv_count = 0;
						Sever_Status = HW_Sever_HEADFLAG_RECEIVE;  
						mesgw(log_fd, "Sever receive headflag fail,beyond the limit of time!!\n");
						break;
					}
				}

				if (Rev_len == sizeof(struct hw_comm_package)) {	
					Sever_Status = HW_Sever_HEADFLAG_RECEIVE_SUCCESS; //turn to check out headflag
					mesgi(log_fd, "Sever receive headflag success!!\n");
				}

				if (Rev_len == -1) {				
					if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {     				
						Sever_Status = HW_Sever_HEADFLAG_RECEIVE;  		/* try again */				
				    } else if (errno == EBADF || errno == EFAULT 
                        || errno == ENOTCONN || errno == ENOTSOCK) {  
					    mesge(log_fd, "Invalid socket\n");  /* invalid socket */
					    goto Sever_Status_Machine_Exit_Tag;
					} else if (errno == EINVAL) {  					
						mesge(log_fd, "Invalid argument passed\n");  /* invalid argument passed */	
						goto Sever_Status_Machine_Exit_Tag;
					} else if (errno == ENOMEM) { 
						mesge(log_fd, "No memory available\n"); /* no memory available */		
						goto Sever_Status_Machine_Exit_Tag;						
					}
				}				
			break;									
             }
			case HW_Sever_HEADFLAG_RECEIVE_SUCCESS:  
            {
				/* receive head flag success, turn to check out head flag */
				send_status = 0, data_length = 0;

				memset(send_str, 0, sizeof(struct hw_comm_package));
				memset(recv_str, 0, sizeof(struct hw_sever_receive));
				
				str = (struct hw_comm_package*)Receive_Headflag; 
				
                //check out head flag success  command = get or set or quit 						
				if (strcmp((char*)Head_Flag, (char*)str->head_flag) == 0) {				
					if ((str->cmd_ids[0] == HW_ISP_CFG_GET) || 
                        (str->cmd_ids[0] == HW_ISP_CFG_SET) || (str->cmd_ids[0] == HW_ISP_CFG_QUIT)) {
						 		
						/* server receive the command and unpackage it */
						recv_str->receive_command = str->cmd_ids[0];
						recv_str->receive_group_id = str->cmd_ids[1];
						recv_str->receive_cfg_ids = (str->cmd_ids[2] << 24) 
                            | (str->cmd_ids[3] << 16) | (str->cmd_ids[4] << 8) | (str->cmd_ids[5]);
						recv_str->receive_ret = htonl(str->ret);
						data_length = htonl(str->data_length);

						/* package headflag send to client */
						strcpy((char*)send_str->head_flag, (char*)Head_Flag);
						strcpy((char*)send_str->cmd_ids, (char*)str->cmd_ids);
						send_str->data_length = 0;
                        //send CLIENT_HEADFLAG_SEND_SUCCESS to client
						send_str->ret = htonl(CLIENT_HEADFLAG_SEND_SUCCESS); 
                        
						send_status = send(client_id,send_str, sizeof(struct hw_comm_package),0);	
						
						if (str->cmd_ids[0] == HW_ISP_CFG_GET && send_status == sizeof(struct hw_comm_package)) { 
							Sever_Status = HW_Sever_HEADFLAG_CHECK_SUCCESS;	//get cfg 		
							try_count = 0;
						}
						
						if (str->cmd_ids[0] == HW_ISP_CFG_SET && send_status == sizeof(struct hw_comm_package)) { 
							Sever_Status = HW_Sever_DATA_RECEIVE;	//receive data	
							try_count = 0;
						}
			
						if (str->cmd_ids[0] == HW_ISP_CFG_QUIT && send_status == sizeof(struct hw_comm_package)) { 
							Sever_Status = HW_Sever_DISCONNECT;	    //disconnect 
							try_count = 0;
						}

						/* printf receive data */
						mesgi(log_fd, "Sever receive package and check out command success!!\n");
						mesgi(log_fd, "Sever_receive_package:");
						mesgi(log_fd, "receive_command=%d	", recv_str->receive_command);
						mesgi(log_fd, "receive_group_id=%d ", recv_str->receive_group_id);
						mesgi(log_fd, "receive_cfg_ids=%d	", recv_str->receive_cfg_ids);
						mesgi(log_fd, "receive_ret=%d	", recv_str->receive_ret);
						mesgi(log_fd, "data_length=%d\n	", data_length);
				
				}else { //check out command failed
			
						mesgw(log_fd, "Sever check out command failed!!\n");
						
						/* package headflag send to client */
						strcpy((char*)send_str->head_flag, (char*)Head_Flag);
						strcpy((char*)send_str->cmd_ids, (char*)str->cmd_ids);
						send_str->data_length = 0;
						send_str->ret = htonl(CLIENT_HEADFLAG_SEND_FAILED); 	
						send_status = send(client_id, send_str, sizeof(struct hw_comm_package), 0);  //send CLIENT_HEADFLAG_SEND_FAILED to client
											
						if (send_status == sizeof(struct hw_comm_package)) {
							Sever_Status = HW_Sever_HEADFLAG_RECEIVE; 	//check out command failed,receive head flag again
							try_count = 0;
						}		
			
					}
			
					/* send error,package headflag send to client */
					if (send_status == -1) {
					
						if (errno == EACCES) {  /* Permission denied */
				
							mesge(log_fd, "Permission denied\n");
							try_count = 0;
						    goto Sever_Status_Machine_Exit_Tag;
	
						} else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) { /* try again */
			
							Sever_Status = HW_Sever_HEADFLAG_RECEIVE_SUCCESS;	
							try_count++;
							if(try_count > 20) {
			
								try_count = 0;
								Sever_Status = HW_Sever_HEADFLAG_RECEIVE;		
							}			
						}
						/* invalid socket */
						else if (errno == EBADF || errno == ECONNRESET || errno == EDESTADDRREQ || errno == EISCONN 
								|| errno == EFAULT || errno == ENOTCONN || errno == ENOTSOCK) {  
				
							mesge(log_fd, "Invalid socket\n");
							try_count = 0;
						    goto Sever_Status_Machine_Exit_Tag;

						}
						/* invalid argument passed */
						else if (errno == EINVAL) {  
				
							mesge(log_fd, "Invalid argument passed\n");
							try_count = 0;
						    goto Sever_Status_Machine_Exit_Tag;

						}	
						/* no memory available */
						else if (errno == ENOMEM) { 
				
							mesge(log_fd, "No memory available\n");
							try_count = 0;
						    goto Sever_Status_Machine_Exit_Tag;

						}
					}
				} else {
			
					mesgw(log_fd, "Sever receive package and check out failed!!\n");
					Sever_Status = HW_Sever_HEADFLAG_RECEIVE;  //check out head flag failed,receive head flag again

				}
			break;
            }
			case HW_Sever_HEADFLAG_CHECK_SUCCESS:
			{	
				/* data check success,turn to HW_Sever_INTERMEDIATE_TRANSIT */
				Sever_Status = HW_Sever_INTERMEDIATE_TRANSIT;
				
			break;
            }

			case HW_Sever_DATA_RECEIVE:
			{	
				left_len = 0, Rev_len = 0, recv_count = 0;
			
				memset(Set_DataBuffer, 0, sizeof(struct isp_param_config));
				
				Rev_len = recv(client_id, Set_DataBuffer, data_length, MSG_DONTWAIT);
				
				while (Rev_len < data_length) 
				{
					left_len = data_length - Rev_len;
					Rev_len += recv(client_id, Set_DataBuffer + Rev_len, left_len, MSG_DONTWAIT);  
					recv_count++;
					
					/* beyond the limit of time,receive fail */
					if (recv_count > 20000) {	//up to the system running time
					
						recv_count = 0;
						count++;
						Sever_Status = HW_Sever_DATA_RECEIVE;  
						if (count > 20) {
						
							count = 0;
							Sever_Status = HW_Sever_HEADFLAG_RECEIVE;  
							mesgw(log_fd,"Sever receive data fail,beyond the limit of time!!\n");
						}
						break;
					}
				}

				if (Rev_len == data_length) { 
				
					mesgi(log_fd, "Sever receive DataBuffer success!!\n");
					Sever_Status = HW_Sever_INTERMEDIATE_TRANSIT; 

					/* test */
					mesgd(log_fd, "Sever receive Set_DataBuffer:");
					kkk = (int *)Set_DataBuffer;
					
					for (jjj = 0; jjj < (data_length >> 2); jjj++)
					{
						mesgd(log_fd, "%d", ntohl(kkk[jjj]));
					}
					
					mesgd(log_fd, "\n");
					try_count = 0;
					count = 0;
				}

				if (Rev_len == -1) {  

                     /* try again */
					if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
					
						Sever_Status = HW_Sever_DATA_RECEIVE;

						try_count++;
						
						if(try_count > 20) {
						
							try_count = 0;
							count = 0;
							Sever_Status = HW_Sever_HEADFLAG_RECEIVE;
						}
					} else if (errno == EBADF || errno == EFAULT || errno == ENOTCONN || errno == ENOTSOCK) {
			
						mesge(log_fd, "Invalid socket\n");
						count = 0;
						try_count = 0;
					    goto Sever_Status_Machine_Exit_Tag;

					} else if (errno == EINVAL) {
						
						mesge(log_fd, "Invalid argument passed\n");
						count = 0;
						try_count = 0;
					    goto Sever_Status_Machine_Exit_Tag;
					} else if (errno == ENOMEM) {
						 
						mesge(log_fd, "No memory available\n");
						count = 0;
						try_count = 0;
					    goto Sever_Status_Machine_Exit_Tag;
					}
				}
			break;
			}	
			case HW_Sever_INTERMEDIATE_TRANSIT:
            {
				memset(send_str, 0, sizeof(struct hw_comm_package));

				/* according to the command(get cfg/set cfg/get yuv/jpeg/raw) for conditional jumps */
				if (recv_str->receive_command == HW_ISP_CFG_GET  && recv_str->receive_group_id != HW_ISP_CFG_RAW 
					&& recv_str->receive_group_id != HW_ISP_CFG_YUV && recv_str->receive_group_id != HW_ISP_CFG_JPEG 
					&& recv_str->receive_group_id != HW_ISP_CFG_ISE ) { 	
				
					memset(Get_DataBuffer, 0, sizeof(struct isp_param_config));
					data_length = hw_get_isp_cfg(&g_isp_test, recv_str->receive_group_id, recv_str->receive_cfg_ids, Get_DataBuffer);
						
					/* a1. client wants to get some cfg */
					send_command = recv_str->receive_command;  /* command, assume 1 for get cfg, 2 for set cfg */
					send_group_id = recv_str->receive_group_id;
					send_cfg_ids = recv_str->receive_cfg_ids;	 /* get isp_test_pub_cfg */
				
					/* send head_flag to client */
					strcpy((char*)send_str->head_flag, (char*)Head_Flag);
				
					/* package data */
					cmdBuffer[0] = (send_command & 0xFF);
					cmdBuffer[1] = (send_group_id & 0xFF);	/* group_id */
					cmdBuffer[2] = ((send_cfg_ids >> 24) & 0xFF);
					cmdBuffer[3] = ((send_cfg_ids >> 16) & 0xFF);
					cmdBuffer[4] = ((send_cfg_ids >> 8) & 0xFF);
					cmdBuffer[5] = (send_cfg_ids & 0xFF);  /* buffer[2:5], cfg_ids */
					cmdBuffer[6] = 0x0;  
				
					memcpy(send_str->cmd_ids, cmdBuffer, 6);
					send_str->data_length = htonl(data_length);
				
					if (data_length >= 0) {	//get success
					
						mesgi(log_fd, "Get Success!!\n");
						send_str->ret = htonl(SEVER_CFG_OPT_GET_SUCCESS);
						
						/* send cfg to client */
						Sever_Status = HW_Sever_HEADFLAG_SEND;
						 
					} else {  //get fail
						
						mesgw(log_fd, "Get Failed!!\n");
						send_str->ret = htonl(SEVER_CFG_OPT_GET_FAILED);
						
						/* send get fail message to client */
						Sever_Status = HW_Sever_HEADFLAG_SEND;
					}
			
					mesgd(log_fd, "Get data_length = %d\n", data_length);
				} else if (recv_str->receive_command == HW_ISP_CFG_SET) { //client send set cfg command  receive_command == HW_ISP_CFG_SET
			
				//	data_length = 0;				
					data_length = hw_set_isp_cfg(&g_isp_test, recv_str->receive_group_id, recv_str->receive_cfg_ids,Set_DataBuffer);

					/* a2. client wants to set some cfg */
					send_command = recv_str->receive_command;;	/* command, assume 1 for get cfg, 2 for set cfg */
					send_group_id = recv_str->receive_group_id;
					send_cfg_ids = recv_str->receive_cfg_ids;	/* set AE table */
				
					/* send head_flag to client */
					strcpy((char*)send_str->head_flag,(char*)Head_Flag);
				
					/* package	data */
					cmdBuffer[0] = (send_command & 0xFF);
					cmdBuffer[1] = (send_group_id & 0xFF);	/* group_id */
					cmdBuffer[2] = ((send_cfg_ids >> 24) & 0xFF);
					cmdBuffer[3] = ((send_cfg_ids >> 16) & 0xFF);
					cmdBuffer[4] = ((send_cfg_ids >> 8) & 0xFF);
					cmdBuffer[5] = (send_cfg_ids & 0xFF);  /* buffer[2:5], cfg_ids */
					cmdBuffer[6] = 0x0;  
					memcpy(send_str->cmd_ids, cmdBuffer, 6);
					
					send_str->data_length = htonl(0);
						
					if(data_length >= 0) {	//set success
					
						mesgi(log_fd, "Set Success!!\n");
						send_str->ret = htonl(SEVER_CFG_OPT_SET_SUCCESS);
						
						/*send set success message to Client*/
						Sever_Status = HW_Sever_HEADFLAG_SEND;
					} else {	//set fail
					
						mesgw(log_fd, "Set Failed!!\n");
						send_str->ret = htonl(SEVER_CFG_OPT_SET_FAILED);
						
						/*send set fail message to Client*/
						Sever_Status = HW_Sever_HEADFLAG_SEND;
					}
	
					memset(Set_DataBuffer, 0, sizeof(struct isp_param_config));
					
		        } else if (recv_str->receive_command == HW_ISP_CFG_GET && recv_str->receive_group_id == HW_ISP_CFG_YUV) {
				
					camera_group = 0;
					yuv =(unsigned char*)malloc(YUV_FrameSize);
					yuv_pst_buffer->yuv = yuv;
					yuv_pst_buffer->yuv_size = YUV_ImgSize;
					memset(yuv, 0, YUV_FrameSize);

					//mesgi(log_fd, "ViDev = %d, ViCh = %d\n", privCap[camera_group].Dev, privCap[camera_group].Chn);

                    image_buffer.pArg_Chn0 = &privCap[0];
                    image_buffer.pArg_Chn1 = &privCap[1];                 
                    image_buffer.group_id = recv_str->receive_group_id;
                    image_buffer.YUV_Pst = yuv_pst_buffer;
                    image_buffer.Jpeg_Pst = NULL;
                    image_buffer.ISE_Pst = NULL;

                    data_length = AW_MPI_VI_Get_Image(&image_buffer);
					
					//AW_MPI_VI_Exit();
					
					/* a1. client wants to get some cfg */
					send_command = recv_str->receive_command;  /* command, assume 1 for get cfg, 2 for set cfg */
					send_group_id = recv_str->receive_group_id;
					send_cfg_ids = recv_str->receive_cfg_ids;	 /* get isp_test_pub_cfg */
				
					/* send head_flag to client */
					strcpy((char*)send_str->head_flag, (char*)Head_Flag);
				
					/* package data */
					cmdBuffer[0] = (send_command & 0xFF);
					cmdBuffer[1] = (send_group_id & 0xFF);	/* group_id */
					cmdBuffer[2] = ((send_cfg_ids >> 24) & 0xFF);
					cmdBuffer[3] = ((send_cfg_ids >> 16) & 0xFF);
					cmdBuffer[4] = ((send_cfg_ids >> 8) & 0xFF);
					cmdBuffer[5] = (send_cfg_ids & 0xFF);  /* buffer[2:5], cfg_ids */
					cmdBuffer[6] = 0x0;  
				
					memcpy(send_str->cmd_ids, cmdBuffer, 6);
					send_str->data_length = htonl(data_length);

					if (data_length >= 0) {	//get success
					
						mesgi(log_fd, "Get YUV Success!!\n");
						send_str->ret = htonl(SEVER_GET_YUV_SUCCESS);

						/* test */	
                        #ifdef Sever_Test					
						yuv_fd = open("yuv_data.yuv", O_RDWR | O_CREAT | O_TRUNC);
						write(yuv_fd,yuv,data_length);
						mesgi(log_fd,"write yuv success!!\n");
                        #endif						

						/* send yuv to client */
						Sever_Status = HW_Sever_HEADFLAG_SEND;
					} else {	//get fail
					
						mesgw(log_fd, "Get YUV Failed!!\n");
						send_str->ret = htonl(SEVER_GET_YUV_FAILED);
						
						/* send get fail message to client */
						Sever_Status = HW_Sever_HEADFLAG_SEND;
					}
		
				} else if (recv_str->receive_command == HW_ISP_CFG_GET && recv_str->receive_group_id == HW_ISP_CFG_JPEG) { /* get jpeg */
				
					camera_group = 0;

					mesgi(log_fd, "ViDev = %d, ViCh = %d\n", privCap[camera_group].Dev, privCap[camera_group].Chn);

                    image_buffer.pArg_Chn0 = &privCap[0];
                    image_buffer.pArg_Chn1 = &privCap[1];
                    image_buffer.group_id = recv_str->receive_group_id;
                    image_buffer.YUV_Pst = NULL;
                    image_buffer.Jpeg_Pst = jpeg_pst_buffer;
                    image_buffer.ISE_Pst = NULL;

                    data_length = AW_MPI_VI_Get_Image(&image_buffer);
		
					//AW_MPI_VI_Exit();

					mesgi(log_fd, "data_length = %d\n",data_length);

					#ifdef Sever_Test
					/* test */
				    onPictureTaken(1, Jpeg_Data, data_length);
					#endif
					
					/* a1. client wants to get some cfg */
					send_command = recv_str->receive_command;  /* command, assume 1 for get cfg, 2 for set cfg */
					send_group_id = recv_str->receive_group_id;
					send_cfg_ids = recv_str->receive_cfg_ids;	 /* get isp_test_pub_cfg */
				
					/* send head_flag to client */
					strcpy((char*)send_str->head_flag, (char*)Head_Flag);
				
					/* package data */
					cmdBuffer[0] = (send_command & 0xFF);
					cmdBuffer[1] = (send_group_id & 0xFF);	/* group_id */
					cmdBuffer[2] = ((send_cfg_ids >> 24) & 0xFF);
					cmdBuffer[3] = ((send_cfg_ids >> 16) & 0xFF);
					cmdBuffer[4] = ((send_cfg_ids >> 8) & 0xFF);
					cmdBuffer[5] = (send_cfg_ids & 0xFF);  /* buffer[2:5], cfg_ids */
					cmdBuffer[6] = 0x0;  				
					memcpy(send_str->cmd_ids, cmdBuffer, 6);

                    send_str->data_length = htonl(data_length);

					if (data_length >= 0) {	//get success
					
						mesgi(log_fd, "Get Jpeg Success!!\n");
						send_str->ret = htonl(SEVER_GET_JPEG_SUCCESS);
						
						/* send yuv to client */
						Sever_Status = HW_Sever_HEADFLAG_SEND;
					} else {	//get fail
				
						mesgw(log_fd, "Get Jpeg Failed!!\n");
						
						send_str->ret = htonl(SEVER_GET_JPEG_FAILED);
						
						/* send get fail message to client */
						Sever_Status = HW_Sever_HEADFLAG_SEND;
					}
								
				} else if (recv_str->receive_command == HW_ISP_CFG_GET && recv_str->receive_group_id == HW_ISP_CFG_ISE) { /* get ISE */
				
					camera_group = 0;

					mesgi(log_fd, "ViDev = %d, ViCh = %d\n", privCap[camera_group].Dev, privCap[camera_group].Chn);

                    image_buffer.pArg_Chn0 = &privCap[0];
                    image_buffer.pArg_Chn1 = &privCap[1];
                    image_buffer.group_id = recv_str->receive_group_id;
                    image_buffer.YUV_Pst = NULL;
                    image_buffer.Jpeg_Pst = NULL;  
                    image_buffer.ISE_Pst = ise_pst_parameter;
                    ise_yuv = (unsigned char*)malloc(ISE_FrameSize);
                    image_buffer.ISE_Pst->ise_send_buffer->ise_yuv = ise_yuv;
                    memset(ise_yuv, 0, ISE_FrameSize);
               
                    data_length = AW_MPI_VI_Get_Image(&image_buffer);
		
					//AW_MPI_VI_Exit();                  

					mesgi(log_fd, "data_length = %d\n",data_length);
	
					/* a1. client wants to get some cfg */
					send_command = recv_str->receive_command;  /* command, assume 1 for get cfg, 2 for set cfg */
					send_group_id = recv_str->receive_group_id;
					send_cfg_ids = recv_str->receive_cfg_ids;	 /* get isp_test_pub_cfg */
				
					/* send head_flag to client */
					strcpy((char*)send_str->head_flag, (char*)Head_Flag);
				
					/* package data */
					cmdBuffer[0] = (send_command & 0xFF);
					cmdBuffer[1] = (send_group_id & 0xFF);	/* group_id */
					cmdBuffer[2] = ((send_cfg_ids >> 24) & 0xFF);
					cmdBuffer[3] = ((send_cfg_ids >> 16) & 0xFF);
					cmdBuffer[4] = ((send_cfg_ids >> 8) & 0xFF);
					cmdBuffer[5] = (send_cfg_ids & 0xFF);  /* buffer[2:5], cfg_ids */
					cmdBuffer[6] = 0x0;  				
					memcpy(send_str->cmd_ids, cmdBuffer, 6);

                    send_str->data_length = htonl(data_length);
					if (data_length >= 0) {	//get success
					
						mesgi(log_fd, "Get ise_yuv Success!!\n");
						send_str->ret = htonl(SEVER_GET_ISE_SUCCESS);

                        /* test */	
                        #ifdef Sever_Test					
						ise_yuv_fd = open("yuv_data.yuv", O_RDWR | O_CREAT | O_TRUNC);
						write(ise_yuv_fd, image_buffer.ISE_Pst->ise_send_buffer->ise_yuv, data_length);
						mesgi(log_fd,"write yuv success!!\n");
                        #endif		
             
						/* send yuv to client */
						Sever_Status = HW_Sever_HEADFLAG_SEND;
					} else {	//get fail
				
						mesgw(log_fd, "Get ise_yuv Failed!!\n");
						
						send_str->ret = htonl(SEVER_GET_ISE_FAILED);
						
						/* send get fail message to client */
						Sever_Status = HW_Sever_HEADFLAG_SEND;
					}
								
				}
		
			break;
            }

			case HW_Sever_HEADFLAG_SEND:  
            {
				/* send headflag to client */		
				send_status = send(client_id, send_str, sizeof(struct hw_comm_package), 0);  
				
				if (send_status == -1) { //send error
					if (errno == EACCES) {  /* Permission denied */
					
						mesge(log_fd,"Permission denied\n");
						try_count = 0;
						if ((send_str->ret == htonl(SEVER_GET_YUV_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_YUV_FAILED))){
							free(yuv);
						}
						if ((send_str->ret == htonl(SEVER_GET_JPEG_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_JPEG_FAILED))){
							free(Jpeg_Data);
						}
                        if ((send_str->ret == htonl(SEVER_GET_ISE_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_ISE_FAILED))){
							free(ise_yuv);
						}
					    goto Sever_Status_Machine_Exit_Tag;
					} else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {  /* try again */
					
						Sever_Status = HW_Sever_HEADFLAG_SEND;  
						try_count++;
						if(try_count > 20) {
						
							try_count = 0;
							if((send_str->ret == htonl(SEVER_GET_YUV_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_YUV_FAILED))) {
								free(yuv);
							}
							if((send_str->ret == htonl(SEVER_GET_JPEG_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_JPEG_FAILED))) {
								free(Jpeg_Data);
							}
                            if ((send_str->ret == htonl(SEVER_GET_ISE_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_ISE_FAILED))){
							    free(ise_yuv);
						    }
							Sever_Status = HW_Sever_HEADFLAG_RECEIVE; 
						}
					} else if (errno == EBADF || errno == ECONNRESET || errno == EDESTADDRREQ || errno == EISCONN 
							|| errno == EFAULT || errno == ENOTCONN || errno == ENOTSOCK) {  /* invalid socket */
					
						mesge(log_fd, "Invalid socket\n");
						try_count = 0;
						if((send_str->ret == htonl(SEVER_GET_YUV_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_YUV_FAILED))) {
							free(yuv);
						}
						if((send_str->ret == htonl(SEVER_GET_JPEG_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_JPEG_FAILED))) {
							free(Jpeg_Data);
						}
                        if ((send_str->ret == htonl(SEVER_GET_ISE_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_ISE_FAILED))){
							free(ise_yuv);
						}
					    goto Sever_Status_Machine_Exit_Tag;
					} else if (errno == EINVAL) {  /* invalid argument passed */
					
						mesge(log_fd,"Invalid argument passed\n");
						try_count = 0;
						if((send_str->ret == htonl(SEVER_GET_YUV_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_YUV_FAILED))) {
							free(yuv);
						}
						if((send_str->ret == htonl(SEVER_GET_JPEG_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_JPEG_FAILED))) {
							free(Jpeg_Data);
						}
                        if ((send_str->ret == htonl(SEVER_GET_ISE_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_ISE_FAILED))){
							free(ise_yuv);
						}
					    goto Sever_Status_Machine_Exit_Tag;
					} else if (errno == ENOMEM) { /* no memory available */	 
					
						mesge(log_fd,"No memory available\n");
						try_count = 0;
						if((send_str->ret == htonl(SEVER_GET_YUV_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_YUV_FAILED))) {
							free(yuv);
						}
						if((send_str->ret == htonl(SEVER_GET_JPEG_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_JPEG_FAILED))) {
							free(Jpeg_Data);
						}
                        if ((send_str->ret == htonl(SEVER_GET_ISE_SUCCESS)) || (send_str->ret == htonl(SEVER_GET_ISE_FAILED))){
							free(ise_yuv);
						}
					    goto Sever_Status_Machine_Exit_Tag;
					}
				} else if (send_status == sizeof(struct hw_comm_package)) {

					mesgi(log_fd, "Sever send headflag success!!\n");
					try_count = 0;
					
					/* client get cfg */
					if (send_str->ret == htonl(SEVER_CFG_OPT_GET_SUCCESS) || send_str->ret == htonl(SEVER_GET_YUV_SUCCESS) 
                        || send_str->ret == htonl(SEVER_GET_JPEG_SUCCESS) || send_str->ret == htonl(SEVER_GET_ISE_SUCCESS)) {					
						Sever_Status = HW_Sever_DATA_SEND;
					} else if (send_str->ret == htonl(SEVER_CFG_OPT_GET_FAILED) || send_str->ret == htonl(SEVER_GET_YUV_FAILED) 
                        || send_str->ret == htonl(SEVER_GET_JPEG_FAILED) || send_str->ret == htonl(SEVER_GET_ISE_FAILED)) {
					
						Sever_Status = HW_Sever_HEADFLAG_RECEIVE;

						if (send_str->ret == htonl(SEVER_GET_YUV_FAILED)) {
							free(yuv);
						}
						if (send_str->ret == htonl(SEVER_GET_JPEG_FAILED)) {
							free(Jpeg_Data);
						}
                        if (send_str->ret == htonl(SEVER_GET_ISE_FAILED)) {
							free(ise_yuv);
						}
					} else if ((send_str->ret == htonl(SEVER_CFG_OPT_SET_SUCCESS)) /* client set cfg */ 
                        || (send_str->ret == htonl(SEVER_CFG_OPT_SET_FAILED))) {
						Sever_Status = HW_Sever_HEADFLAG_RECEIVE;
					}
				}
			break;
			}	
		
			case HW_Sever_DATA_SEND:
			{
				if (send_str->ret == htonl(SEVER_CFG_OPT_GET_SUCCESS)) {
				
					/* test */				
					ttt = (unsigned char *)Get_DataBuffer;	
					
					mesgd(log_fd, "Sever send Get_DataBuffer:");
					for (jjj = 0; jjj < data_length; jjj++)				
					{								
						mesgd(log_fd,"%02x ", ttt[jjj]);
					}				
	
					mesgd(log_fd, "\n");

					/* sever send data to client */
					send_status = send(client_id, Get_DataBuffer, data_length, 0);
				} else if (send_str->ret == htonl(SEVER_GET_YUV_SUCCESS)) {
				
					send_status = send(client_id, yuv,data_length, 0);
					mesgi(log_fd, "Send YUV to Client!!\n");
					
				} else if (send_str->ret == htonl(SEVER_GET_JPEG_SUCCESS)) {
				
					send_status = send(client_id, Jpeg_Data,data_length, 0);
					mesgi(log_fd, "Send JPEG to Client!!\n");
			
				} else if (send_str->ret == htonl(SEVER_GET_ISE_SUCCESS)) {
				
					send_status = send(client_id,ise_pst_parameter->ise_send_buffer->ise_yuv,data_length,0);
					mesgi(log_fd, "Send ISE YUV to Client!!\n");

				}
				
				if (send_status == -1) { //send error
				
					if (errno == EACCES) {  /* Permission denied */
				
						mesge(log_fd, "Permission denied\n");
						try_count = 0;
						if (send_str->ret == htonl(SEVER_GET_YUV_SUCCESS)) {
							free(yuv);
						}
						if (send_str->ret == htonl(SEVER_GET_JPEG_SUCCESS)) {
							free(Jpeg_Data);
						}
                        if (send_str->ret == htonl(SEVER_GET_ISE_SUCCESS)) {
							free(ise_yuv);
						}
					    goto Sever_Status_Machine_Exit_Tag;

					} else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) { /* try again */
					
						Sever_Status = HW_Sever_DATA_SEND;  
						try_count++;
						if (try_count > 20) {
						
							try_count = 0;
							if (send_str->ret == htonl(SEVER_GET_YUV_SUCCESS)) {
							free(yuv);
							}
							if (send_str->ret == htonl(SEVER_GET_JPEG_SUCCESS)) {
								free(Jpeg_Data);
							}
                            if (send_str->ret == htonl(SEVER_GET_ISE_SUCCESS)) {
							    free(ise_yuv);
						    }
							Sever_Status = HW_Sever_HEADFLAG_RECEIVE;
						}
					} else if (errno == EBADF || errno == ECONNRESET || errno == EDESTADDRREQ || errno == EISCONN 
							|| errno == EFAULT || errno == ENOTCONN || errno == ENOTSOCK) {  /* invalid socket */
				
						mesge(log_fd, "Invalid socket\n");
						try_count = 0;
						if (send_str->ret == htonl(SEVER_GET_YUV_SUCCESS)) {
							free(yuv);
						}
						if (send_str->ret == htonl(SEVER_GET_JPEG_SUCCESS)) {
							free(Jpeg_Data);
						}
                        if (send_str->ret == htonl(SEVER_GET_ISE_SUCCESS)) {
							free(ise_yuv);
						}
					    goto Sever_Status_Machine_Exit_Tag;
					} else if (errno == EINVAL) {  /* invalid argument passed */
					
						mesge(log_fd, "Invalid argument passed\n");
						try_count = 0;
						if(send_str->ret == htonl(SEVER_GET_YUV_SUCCESS)) {
							free(yuv);
						}
						if(send_str->ret == htonl(SEVER_GET_JPEG_SUCCESS)) {
							free(Jpeg_Data);
						}
                        if (send_str->ret == htonl(SEVER_GET_ISE_SUCCESS)) {
						    free(ise_yuv);
					    }
					    goto Sever_Status_Machine_Exit_Tag;
					} else if (errno == ENOMEM) { /* no memory available */
					
						mesge(log_fd, "No memory available\n");
						try_count = 0;
						if (send_str->ret == htonl(SEVER_GET_YUV_SUCCESS)) {
							free(yuv);
						}
						if (send_str->ret == htonl(SEVER_GET_JPEG_SUCCESS)) {
							free(Jpeg_Data);
						}
                        if (send_str->ret == htonl(SEVER_GET_ISE_SUCCESS)) {
						    free(ise_yuv);
					    }
					    goto Sever_Status_Machine_Exit_Tag;
					}
				} else if (send_status == data_length) {
	
					mesgi(log_fd, "Sever send data success!!\n");
					try_count = 0;
					if (send_str->ret == htonl(SEVER_GET_YUV_SUCCESS)) {
						free(yuv);
					}
					if (send_str->ret == htonl(SEVER_GET_JPEG_SUCCESS)) {
						free(Jpeg_Data);
					}
                    if (send_str->ret == htonl(SEVER_GET_ISE_SUCCESS)) {
					    free(ise_yuv);
				    }
					Sever_Status = HW_Sever_HEADFLAG_RECEIVE;
					data_length = 0;
				}
					
			break;
			}
	
			case HW_Sever_DISCONNECT:
			{	
				mesge(log_fd, "Sever disconnect!!\n");
				recv_count = 0;
				count = 0;
				try_count = 0;
				child_thread_exit_flag[num] = 1;
				goto Sever_Status_Machine_Exit_Tag;
			//break;
			}	

			default: break;
					
		}
	}

Sever_Status_Machine_Exit_Tag:
								SAFE_DELETE_OBJ(send_str);
								SAFE_DELETE_OBJ(recv_str);
								SAFE_DELETE_OBJ(Receive_Headflag);
								SAFE_DELETE_OBJ(Get_DataBuffer);
								SAFE_DELETE_OBJ(Set_DataBuffer);	
								//SAFE_DELETE_OBJ(yuv);		
								//AW_MPI_VI_Exit();
								close(client_id);

}

/* Sever communicate to client */
static void *Socket_Process_Thread(void *socket_id)
{
	int thread_count = 0;

	pthread_mutex_lock(&socket_lock);

	Count++;
	if (Count > child_thread_count) {
		mesgw(log_fd, "The number of threads reaches the upper limit,close the one you are using!\n");
		Count = child_thread_count;
		pthread_mutex_unlock(&socket_lock);
		pthread_exit(0);
		return 0;
	}

	thread_count = Count;

	if (thread_count == 1 && Vin_Initialize != 1)
		Vin_Initialize = 0;

	pthread_mutex_unlock(&socket_lock);
	
	child_thread_exit_flag[thread_count] = 0;

	child_thread_running_flag[thread_count] = 1;

	Sever_Status_Machine(socket_id,thread_count);

	child_thread_running_flag[thread_count] = 0;

	pthread_mutex_lock(&socket_lock);
	Count--;
	pthread_mutex_unlock(&socket_lock);
	
	pthread_exit(0);

	return 0;

}

void KillChildThreads()
{
	int i = 0;
	for (i = 1; i <= child_thread_count; i++) 
	{
		child_thread_exit_flag[i] = 1;  // tell child thread to exit
		while (child_thread_running_flag[i]) // wait for child thread to exit
			usleep(50000);
	}
}
	
int main(int argc,char *argv[])	
{
    //int TEST = 1;
    
	log_fd = open("mylog.log", O_WRONLY | O_TRUNC); 	

	/*Sever socket message*/
	struct sockaddr_in Sever_socket_message;
	int Sever_socketfd = 0;	

	/*Client_socket_message*/
	struct sockaddr_in Client_socket_message;
	//signed int Client_socketfd = 0;
	
	Sever_socket_message.sin_family = AF_INET;
	Sever_socket_message.sin_port = htons(PORT);
	Sever_socket_message.sin_addr.s_addr = htons(INADDR_ANY); 
	Sever_socketfd = socket(AF_INET, SOCK_STREAM, 0);	//creat sever socket

	pthread_mutex_init(&socket_lock, NULL);

    #ifdef LOCK
	pthread_mutex_init(&data_lock, NULL);
    #endif
    
	pthread_t id_1;
	int ret = 0;
	Count = 0;

	if (bind(Sever_socketfd, (struct sockaddr*)&Sever_socket_message, sizeof(Sever_socket_message)) == -1) {	//bind
	
		mesge(log_fd,"bind error\n");
		exit(1);	
	}

	if (listen(Sever_socketfd, QUEUE) == -1) {	//listen
	
		mesge(log_fd,"listen error\n");
		exit(1);
	}
	
	socklen_t Client_len = sizeof(Client_socket_message);

	initParams();  

	while(1)
	{
     #ifndef Test       
        signed int Client_socketfd = 0;
		
		Client_socketfd = accept(Sever_socketfd, (struct sockaddr*)&Client_socket_message, &Client_len);	 //get socket

		if (Client_socketfd >= 0) {
		
			ret = pthread_create(&id_1, NULL, Socket_Process_Thread, (void *)Client_socketfd);
			if (ret != 0) {
			
				void *tret;
				pthread_join(id_1, &tret); 

			}
		} else if (Client_socketfd == -1) {
		
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {  /* try again */
			
				continue;
			} else if (errno == EBADF || errno == EFAULT || errno == ENOTSOCK 
                || errno == EOPNOTSUPP || errno == ECONNABORTED) {  /* invalid socket */
			
				mesge(log_fd, "Invalid socket\n");
				close(Client_socketfd);
				
			} else if (errno == ENOBUFS || errno == ENOMEM || errno == EPROTO) { /* sever error */ 
			
				mesge(log_fd, "Sever error\n");
				close(Client_socketfd); 
				break;
			}
	    }	
    
    #endif

    #ifdef Test
       int test = 1;
       if(test  == 1)		
       {            
            Sever_Status_Machine(&test,0);            
            test  = 0;      
       }   
    #endif   

    }
	//close child threads which is running
	KillChildThreads();
	if(Vin_Exit == 1)
	{
        AW_MPI_VI_Exit();
        Vin_Exit = 0;
	}
	
	pthread_mutex_destroy(&socket_lock);
    
    #ifdef LOCK
	pthread_mutex_destroy(&data_lock);
	#endif
    
	close(Sever_socketfd);
	return 0;	

    
}


