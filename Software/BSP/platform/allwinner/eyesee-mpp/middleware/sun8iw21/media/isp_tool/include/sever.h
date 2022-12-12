#ifndef _HW_SEVER_H_
#define _HW_SEVER_H_

#define SAFE_DELETE_OBJ(obj) \
	if (obj) { free(obj); obj = NULL; }	

// option for debug level.
#define OPTION_LOG_LEVEL_CLOSE      0
#define OPTION_LOG_LEVEL_ERROR      1
#define OPTION_LOG_LEVEL_WARNING    2
#define OPTION_LOG_LEVEL_DEFAULT    3
#define OPTION_LOG_LEVEL_DETAIL     4


#define LOG_LEVEL_ERROR     "error  "
#define LOG_LEVEL_WARNING   "warning"
#define LOG_LEVEL_INFO      "info   "
#define LOG_LEVEL_DEBUG     "debug  "

#define AWLOG(fd, level, fmt, arg...)  \
    dprintf(fd, "%s: %s <%s:%u>: "fmt"\n", level, LOG_TAG, __FUNCTION__, __LINE__, ##arg)

#if CONFIG_LOG_LEVEL == OPTION_LOG_LEVEL_CLOSE

#define mesge(fd, fmt, arg...)
#define mesgw(fd, fmt, arg...)
#define mesgi(fd, fmt, arg...)
#define mesgd(fd, fmt, arg...)

#elif CONFIG_LOG_LEVEL == OPTION_LOG_LEVEL_ERROR

#define mesge(fd, fmt, arg...) AWLOG(fd, LOG_LEVEL_ERROR, "\033[40;31m" fmt "\033[0m", ##arg)
#define mesgw(fd, fmt, arg...)
#define mesgi(fd, fmt, arg...)
#define mesgd(fd, fmt, arg...)

#elif CONFIG_LOG_LEVEL == OPTION_LOG_LEVEL_WARNING

#define mesge(fd, fmt, arg...) AWLOG(fd, LOG_LEVEL_ERROR, "\033[40;31m" fmt "\033[0m", ##arg)
#define mesgw(fd, fmt, arg...) AWLOG(fd, LOG_LEVEL_WARNING, fmt, ##arg)
#define mesgi(fd, fmt, arg...)
#define mesgd(fd, fmt, arg...)

#elif CONFIG_LOG_LEVEL == OPTION_LOG_LEVEL_DEFAULT

#define mesge(fd, fmt, arg...) AWLOG(fd, LOG_LEVEL_ERROR, "\033[40;31m" fmt "\033[0m", ##arg)
#define mesgw(fd, fmt, arg...) AWLOG(fd, LOG_LEVEL_WARNING, fmt, ##arg)
#define mesgi(fd, fmt, arg...) AWLOG(fd, LOG_LEVEL_INFO, fmt, ##arg)
#define mesgd(fd, fmt, arg...)

#elif CONFIG_LOG_LEVEL == OPTION_LOG_LEVEL_DETAIL

#define mesge(fd, fmt, arg...) AWLOG(fd, LOG_LEVEL_ERROR, "\033[40;31m" fmt "\033[0m", ##arg)
#define mesgw(fd, fmt, arg...) AWLOG(fd, LOG_LEVEL_WARNING, fmt, ##arg)
#define mesgi(fd, fmt, arg...) AWLOG(fd, LOG_LEVEL_INFO, fmt, ##arg)
#define mesgd(fd, fmt, arg...) AWLOG(fd, LOG_LEVEL_DEBUG, fmt, ##arg)


#else
    #error "invalid configuration of debug level."
#endif

struct hw_comm_package 
{
	unsigned char head_flag[5];
	unsigned char cmd_ids[6];
	unsigned int  ret;
	unsigned int  data_length;
//	unsigned int  reserved[2]; 
};

struct hw_sever_receive
{
	short receive_command;
	unsigned int receive_group_id;
	unsigned int receive_cfg_ids;
	unsigned int receive_ret;
};

/*typedef struct awVI_FRAME_BUF_INFO_S
{
	AW_U32          u32PoolId;
	AW_U32          u32Width;
	AW_U32          u32Height;
	//VI_FIELD_E	u32Field;
	//VI_OUTPUT_FMT_E	enPixelFormat;
	AW_U32		u32PixelFormat;
	AW_U32		u32field;

	AW_U32          u32PhyAddr[3];
	AW_VOID         *pVirAddr[3];
	AW_U32          u32Stride[3];

	//AW_U32          u32TimeRef;
	struct timeval stTimeStamp;
} VI_FRAME_BUF_INFO_S;

typedef struct awVI_PrivCap_S
{
	AW_DEV Dev;
	AW_CHN Chn;
	AW_S32 s32MilliSec;
	VI_ATTR_S pstAttr;
	VI_DEV_ATTR_S pstDevAttr;
	VI_CAP_MODE_E enViCapMode;
	VI_INT_SEL_E enInterrupt;
	VI_FRAME_BUF_INFO_S pstFrameInfo;

} VI_PrivCap_S;*/


/* sever sendto client */
typedef enum {

	SEVER_HEADFLAG_SEND_SUCCESS = 1,	
	SEVER_HEADFLAG_SEND_FAILED,					
	SEVER_DATA_SEND_SUCCESS,	
	SEVER_DATA_SEND_FAILED,		
	SEVER_CFG_OPT_GET_SUCCESS,	
	SEVER_CFG_OPT_GET_FAILED,	
	SEVER_CFG_OPT_SET_SUCCESS,
	SEVER_CFG_OPT_SET_FAILED,
	SEVER_GET_YUV_SUCCESS,
	SEVER_GET_YUV_FAILED,
	SEVER_GET_JPEG_SUCCESS,
	SEVER_GET_JPEG_FAILED,
	SEVER_GET_RAW_SUCCESS,
	SEVER_GET_RAW_FAILED,
	SEVER_GET_ISE_SUCCESS,
	SEVER_GET_ISE_FAILED,
	SEVER_GET_CEV_SUCCESS,
	SEVER_GET_CEV_FAILED,
	SEVER_GET_EVE_SUCCESS,
	SEVER_GET_EVE_FAILED
}hw_isp_sever_sendto_client;

/* client sendto sever */
typedef enum {

	CLIENT_HEADFLAG_SEND_SUCCESS = 1,	
	CLIENT_HEADFLAG_SEND_FAILED,		
	CLIENT_DATA_SEND_SUCCESS,		
	CLIENT_DATA_SEND_FAILED,	
	CLIENT_CFG_OPT_GET,
	CLIENT_CFG_OPT_GET_SUCCESS,
	CLIENT_CFG_OPT_GET_FAILED,	
	CLIENT_CFG_OPT_SET,
	CLIENT_CFG_OPT_SET_SUCCESS,
	CLIENT_CFG_OPT_SET_FAILED	//CLIENT_CFG_OPT_SET_FAILED
}hw_isp_client_sendto_sever;


#endif /* _HW_SEVER_H_ */

