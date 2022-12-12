
#define LOG_TAG "sample_g2d"
#include <utils/plat_log.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <linux/g2d_driver.h>

#include <vo/hwdisplay.h>
#include <mpi_vo.h>
#include <media/mpi_sys.h>
#include <media/mm_comm_vi.h>
#include <mpi_videoformat_conversion.h>
#include <SystemBase.h>
#include <VideoFrameInfoNode.h>
#include "media/mpi_vi.h"
#include "media/mpi_isp.h"
#include <utils/PIXEL_FORMAT_E_g2d_format_convert.h>
#include <utils/VIDEO_FRAME_INFO_S.h>
#include <confparser.h>
#include "sample_g2d.h"
#include "sample_g2d_mem.h"
#include "sample_g2d_config.h"
#include <cdx_list.h>


static int ParseCmdLine(int argc, char **argv, SampleG2dCmdLineParam *pCmdLinePara)
{
    alogd("sample virvi path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleG2dCmdLineParam));
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if(++i >= argc)
            {
                aloge("fatal error! use -h to learn how to set parameter!!!");
                ret = -1;
                break;
            }
            if(strlen(argv[i]) >= MAX_FILE_PATH_SIZE)
            {
                aloge("fatal error! file path[%s] too long: [%d]>=[%d]!", argv[i], strlen(argv[i]), MAX_FILE_PATH_SIZE);
            }
            strncpy(pCmdLinePara->mConfigFilePath, argv[i], MAX_FILE_PATH_SIZE-1);
            pCmdLinePara->mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
        }
        else if(!strcmp(argv[i], "-h"))
        {
            alogd("CmdLine param:\n"
                "\t-path /mnt/extsd/sample_vi_g2d.conf");
            ret = 1;
            break;
        }
        else
        {
            alogd("ignore invalid CmdLine param:[%s], type -h to get how to set parameter!", argv[i]);
        }
        i++;
    }
    return ret;
}



static ERRORTYPE loadSampleG2dConfig(SampleG2dConfig *pConfig, const char *pConfPath)
{
    int ret = SUCCESS;

    pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    pConfig->mSrcWidth = 1920;
    pConfig->mSrcHeight = 1080;
    pConfig->mSrcRectX = 0;
    pConfig->mSrcRectY = 0;
    pConfig->mSrcRectW = 1920;
    pConfig->mSrcRectH = 1080;
    pConfig->mDstRotate = 90;
    pConfig->mDstWidth = 1080;
    pConfig->mDstHeight = 1920;
    pConfig->mDstRectX = 0;
    pConfig->mDstRectY = 0;
    pConfig->mDstRectW = 1080;
    pConfig->mDstRectH = 1920;

    if(pConfPath != NULL)
    {
        CONFPARSER_S stConfParser;
        ret = createConfParser(pConfPath, &stConfParser);
        if(ret < 0)
        {
            aloge("load conf fail"); 
            return FAILURE;
        }
        char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_G2D_KEY_PIC_FORMAT, NULL);
        if(!strcmp(pStrPixelFormat, "nv21"))
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        else if(!strcmp(pStrPixelFormat, "nv12"))
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        }
        else if (!strcmp(pStrPixelFormat, "nv16"))  // yuv422sp
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422;
        } 
        else if (!strcmp(pStrPixelFormat, "nv61"))  // yvu422sp
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422;
        }
        else if (!strcmp(pStrPixelFormat, "rgb888"))
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_RGB_888;
        }
        else if (!strcmp(pStrPixelFormat, "rgb8888"))
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_RGB_8888;
        }
        else
        {
            aloge("fatal error! conf file pic_format[%s] is unsupported", pStrPixelFormat);
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        } 
        
        pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_G2D_DST_PIC_FORMAT, NULL);
        if(!strcmp(pStrPixelFormat, "nv21"))
        {
            pConfig->mDstPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        else if(!strcmp(pStrPixelFormat, "nv12"))
        {
            pConfig->mDstPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        }
        else if(!strcmp(pStrPixelFormat, "rgb888"))
        {
            pConfig->mDstPicFormat = MM_PIXEL_FORMAT_RGB_888;
        }
        else if(!strcmp(pStrPixelFormat, "rgb8888"))
        {
            pConfig->mDstPicFormat = MM_PIXEL_FORMAT_RGB_8888;
        }
        else if (!strcmp(pStrPixelFormat, "nv16"))  // yuv422sp
        {
            pConfig->mDstPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422;
        } 
        else if (!strcmp(pStrPixelFormat, "nv61"))  // yvu422sp
        {
            pConfig->mDstPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422;
        } 
        else if (!strcmp(pStrPixelFormat, "yvu422pakage"))  // yvu422sp
        {
            pConfig->mDstPicFormat = MM_PIXEL_FORMAT_YVYU_AW_PACKAGE_422;
        }
        else if (!strcmp(pStrPixelFormat, "yuv422pakage"))  // yuv422sp
        {
            pConfig->mDstPicFormat = MM_PIXEL_FORMAT_YUYV_PACKAGE_422;
        }
        else if (!strcmp(pStrPixelFormat, "uyvy422pakage"))  
        {
            pConfig->mDstPicFormat = MM_PIXEL_FORMAT_UYVY_PACKAGE_422;
        }
        else if (!strcmp(pStrPixelFormat, "vyuy422pakage"))   
        {
            pConfig->mDstPicFormat = MM_PIXEL_FORMAT_VYUY_PACKAGE_422;
        }
        else
        {
            aloge("fatal error! conf dst pic_format[%s] is unsupported", pStrPixelFormat);
            pConfig->mDstPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
		
		aloge("config_pixel_fmt:src:%d,dst:%d",pConfig->mPicFormat,pConfig->mDstPicFormat);
        pConfig->mSrcWidth  = GetConfParaInt(&stConfParser, SAMPLE_G2D_KEY_SRC_WIDTH, 0);
        pConfig->mSrcHeight  = GetConfParaInt(&stConfParser, SAMPLE_G2D_KEY_SRC_HEIGHT, 0);
        pConfig->mSrcRectX  = GetConfParaInt(&stConfParser, SAMPLE_G2D_KEY_SRC_RECT_X, 0);
        pConfig->mSrcRectY  = GetConfParaInt(&stConfParser, SAMPLE_G2D_KEY_SRC_RECT_Y, 0);
        pConfig->mSrcRectW  = GetConfParaInt(&stConfParser, SAMPLE_G2D_KEY_SRC_RECT_W, 0);
        pConfig->mSrcRectH  = GetConfParaInt(&stConfParser, SAMPLE_G2D_KEY_SRC_RECT_H, 0);
        pConfig->mDstRotate = GetConfParaInt(&stConfParser, SAMPLE_G2D_KEY_DST_ROTATE, 0);
        pConfig->mDstWidth  = GetConfParaInt(&stConfParser, SAMPLE_G2D_KEY_DST_WIDTH, 0);
        pConfig->mDstHeight = GetConfParaInt(&stConfParser, SAMPLE_G2D_KEY_DST_HEIGHT, 0);
        pConfig->mDstRectX  = GetConfParaInt(&stConfParser, SAMPLE_G2D_KEY_DST_RECT_X, 0);
        pConfig->mDstRectY  = GetConfParaInt(&stConfParser, SAMPLE_G2D_KEY_DST_RECT_Y, 0);
        pConfig->mDstRectW  = GetConfParaInt(&stConfParser, SAMPLE_G2D_KEY_DST_RECT_W, 0);
        pConfig->mDstRectH  = GetConfParaInt(&stConfParser, SAMPLE_G2D_KEY_DST_RECT_H, 0);
        aloge("width:%d,height:%d",pConfig->mDstWidth,pConfig->mDstHeight);
        
        pConfig->g2d_mod  = GetConfParaInt(&stConfParser, SAMPLE_G2D_MODE, 0);  // 0:roate, 1: scale, 2:roate & scale

        char *tmp_ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_G2D_SRC_FILE_STR, NULL);
        strcpy(pConfig->SrcFile,tmp_ptr);

        tmp_ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_G2D_KEY_DST_FLIP, NULL);
        if(!strcmp(tmp_ptr, "H"))
            pConfig->flip_flag = 'H';
        else if(!strcmp(tmp_ptr, "V"))
            pConfig->flip_flag = 'V';
        else if (!strcmp(tmp_ptr, "N"))
            pConfig->flip_flag = 'N';

        tmp_ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_G2D_DST_FILE_STR, NULL);
        strcpy(pConfig->DstFile,tmp_ptr); 

        pConfig->g2d_bld_mod = GetConfParaInt(&stConfParser, SAMPLE_G2D_BLD_MODE, 0);

        aloge("src_file:%s, dst_file:%s",pConfig->SrcFile,pConfig->DstFile);
        destroyConfParser(&stConfParser);
    }
    return ret;
}

static int releaseSrcFile(SampleG2dMixerTaskFrmInfo *pYUVFrmInfo, SampleG2dMixerTaskFrmInfo *pRGBFrmInfo)
{
    for (int i = 0; i < 3; i++)
    {
        if (NULL != pYUVFrmInfo->mpSrcVirFrmAddr[i])
        {
            g2d_freeMem(pYUVFrmInfo->mpSrcVirFrmAddr[i]);
            pYUVFrmInfo->mpSrcVirFrmAddr[i] = NULL;
        }

        if (NULL != pRGBFrmInfo->mpSrcVirFrmAddr[i])
        {
            g2d_freeMem(pRGBFrmInfo->mpSrcVirFrmAddr[i]);
            pRGBFrmInfo->mpSrcVirFrmAddr[i] = NULL;
        }
    }

    return 0;
}

static int releaseBuf(SampleG2dMixerTaskContext *pContext)
{
    int ret = 0;
    SampleG2dMixerTaskFrmInfo *pFrmInfo = NULL;

    releaseSrcFile(&pContext->mYUVFrmInfo, &pContext->mRGBFrmInfo);

    for (int i = 0; i < FRAME_TO_BE_PROCESS; i++)
    {
        pFrmInfo = &pContext->mFrmInfo[i];
        for (int i = 0; i < 3; i++)
        {
            if (NULL != pFrmInfo->mpSrcVirFrmAddr[i])
            {
                g2d_freeMem(pFrmInfo->mpSrcVirFrmAddr[i]);
                pFrmInfo->mpSrcVirFrmAddr[i] = NULL;
            }

            if (NULL != pFrmInfo->mpDstVirFrmAddr[i])
            {
                g2d_freeMem(pFrmInfo->mpDstVirFrmAddr[i]);
                pFrmInfo->mpDstVirFrmAddr[i] = NULL;
            }
        }
    }

    return ret;
}

static int FreeFrmBuff(SAMPLE_G2D_CTX *p_g2d_ctx)
{
    if(p_g2d_ctx->mConfigPara.g2d_mod != 4)
    {
        if(NULL != p_g2d_ctx->src_frm_info.p_vir_addr[0])
        {
            g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[0]);
        }

        if(NULL != p_g2d_ctx->src_frm_info.p_vir_addr[1])
        {
            g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[1]);
        }

        if(NULL != p_g2d_ctx->dst_frm_info.p_vir_addr[0])
        {
            g2d_freeMem(p_g2d_ctx->dst_frm_info.p_vir_addr[0]);
        }

        if(NULL != p_g2d_ctx->dst_frm_info.p_vir_addr[1])
        {
            g2d_freeMem(p_g2d_ctx->dst_frm_info.p_vir_addr[1]);
        }
    }else{
        releaseBuf(&p_g2d_ctx->mixertask);
    }

    return 0; 
} 

static int PrepareFrmBuff(SAMPLE_G2D_CTX *p_g2d_ctx)
{
    SampleG2dConfig *pConfig = NULL;
    unsigned int size = 0;

    pConfig = &p_g2d_ctx->mConfigPara;
    
    p_g2d_ctx->src_frm_info.frm_width = pConfig->mSrcWidth;
    p_g2d_ctx->src_frm_info.frm_height =  pConfig->mSrcHeight;

    p_g2d_ctx->dst_frm_info.frm_width = pConfig->mDstWidth;
    p_g2d_ctx->dst_frm_info.frm_height = pConfig->mDstHeight;

    size = ALIGN(p_g2d_ctx->src_frm_info.frm_width, 16)*ALIGN(p_g2d_ctx->src_frm_info.frm_height, 16);
    if(pConfig->mPicFormat == MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420 || pConfig->mPicFormat == MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420)
    {
        p_g2d_ctx->src_frm_info.p_vir_addr[0] = (void *)g2d_allocMem(size);
        if(NULL == p_g2d_ctx->src_frm_info.p_vir_addr[0])
        {
            aloge("malloc_src_frm_y_mem_failed");
            return -1;
        }

        p_g2d_ctx->src_frm_info.p_vir_addr[1] = (void *)g2d_allocMem(size/2);
        if(NULL == p_g2d_ctx->src_frm_info.p_vir_addr[1])
        {
            g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[0]);
            aloge("malloc_src_frm_c_mem_failed");    
            return -1;
        }

        p_g2d_ctx->src_frm_info.p_phy_addr[0] = (void *)g2d_getPhyAddrByVirAddr(p_g2d_ctx->src_frm_info.p_vir_addr[0]); 
        p_g2d_ctx->src_frm_info.p_phy_addr[1] = (void *)g2d_getPhyAddrByVirAddr(p_g2d_ctx->src_frm_info.p_vir_addr[1]);
    }
    else if(pConfig->mPicFormat == MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422 || pConfig->mPicFormat == MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422)
    {
        p_g2d_ctx->src_frm_info.p_vir_addr[0] = (void *)g2d_allocMem(size);
        if(NULL == p_g2d_ctx->src_frm_info.p_vir_addr[0])
        {
            aloge("malloc_src_frm_y_mem_failed");
            return -1;
        }

        p_g2d_ctx->src_frm_info.p_vir_addr[1] = (void *)g2d_allocMem(size);
        if(NULL == p_g2d_ctx->src_frm_info.p_vir_addr[1])
        {
            g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[0]);
            aloge("malloc_src_frm_c_mem_failed");    
            return -1;
        }

        p_g2d_ctx->src_frm_info.p_phy_addr[0] = (void *)g2d_getPhyAddrByVirAddr(p_g2d_ctx->src_frm_info.p_vir_addr[0]);
        p_g2d_ctx->src_frm_info.p_phy_addr[1] = (void *)g2d_getPhyAddrByVirAddr(p_g2d_ctx->src_frm_info.p_vir_addr[1]);
    }
    else if (MM_PIXEL_FORMAT_RGB_888 == pConfig->mPicFormat)
    {
        p_g2d_ctx->src_frm_info.p_vir_addr[0] = (void *)g2d_allocMem(size*3);
        if(NULL == p_g2d_ctx->src_frm_info.p_vir_addr[0])
        {
            aloge("malloc_src_frm_y_mem_failed");
            return -1;
        }

        p_g2d_ctx->src_frm_info.p_phy_addr[0] = (void *)g2d_getPhyAddrByVirAddr(p_g2d_ctx->src_frm_info.p_vir_addr[0]);
    }
    else if (MM_PIXEL_FORMAT_RGB_8888 == pConfig->mPicFormat)
    {
        p_g2d_ctx->src_frm_info.p_vir_addr[0] = (void *)g2d_allocMem(size*4);
        if(NULL == p_g2d_ctx->src_frm_info.p_vir_addr[0])
        {
            aloge("malloc_src_frm_y_mem_failed");
            return -1;
        }

        p_g2d_ctx->src_frm_info.p_phy_addr[0] = (void *)g2d_getPhyAddrByVirAddr(p_g2d_ctx->src_frm_info.p_vir_addr[0]);
    }

	if(pConfig->mDstPicFormat == MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420 ||
			pConfig->mDstPicFormat == MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420)
	{
		size = ALIGN(p_g2d_ctx->dst_frm_info.frm_width, 16)*ALIGN(p_g2d_ctx->dst_frm_info.frm_height, 16);
		p_g2d_ctx->dst_frm_info.p_vir_addr[0] = (void *)g2d_allocMem(size);
		if(NULL == p_g2d_ctx->dst_frm_info.p_vir_addr[0])
		{
			g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[0]); 
			g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[1]);
			aloge("malloc_dst_frm_y_mem_failed");
			return -1;
		}
		p_g2d_ctx->dst_frm_info.p_vir_addr[1] = (void *)g2d_allocMem(size/2);
		if(NULL == p_g2d_ctx->dst_frm_info.p_vir_addr[1])
		{
			g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[0]); 
			g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[1]); 
			g2d_freeMem(p_g2d_ctx->dst_frm_info.p_vir_addr[0]); 
			aloge("malloc_dst_frm_c_mem_failed");    
			return -1;
		} 
		p_g2d_ctx->dst_frm_info.p_phy_addr[0] = (void *)g2d_getPhyAddrByVirAddr(p_g2d_ctx->dst_frm_info.p_vir_addr[0]); 
		p_g2d_ctx->dst_frm_info.p_phy_addr[1] = (void *)g2d_getPhyAddrByVirAddr(p_g2d_ctx->dst_frm_info.p_vir_addr[1]); 
	}
    else if(pConfig->mDstPicFormat == MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422 || pConfig->mDstPicFormat == MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422)
    {
		size = ALIGN(p_g2d_ctx->dst_frm_info.frm_width, 16)*ALIGN(p_g2d_ctx->dst_frm_info.frm_height, 16);
		p_g2d_ctx->dst_frm_info.p_vir_addr[0] = (void *)g2d_allocMem(size);
		if(NULL == p_g2d_ctx->dst_frm_info.p_vir_addr[0])
		{
			g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[0]); 
			g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[1]);
			aloge("malloc_dst_frm_y_mem_failed");
			return -1;
		}

		p_g2d_ctx->dst_frm_info.p_vir_addr[1] = (void *)g2d_allocMem(size);
		if(NULL == p_g2d_ctx->dst_frm_info.p_vir_addr[1])
		{
			g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[0]); 
			g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[1]); 
			g2d_freeMem(p_g2d_ctx->dst_frm_info.p_vir_addr[0]); 
			aloge("malloc_dst_frm_c_mem_failed");    
			return -1;
		} 
		p_g2d_ctx->dst_frm_info.p_phy_addr[0] = (void *)g2d_getPhyAddrByVirAddr(p_g2d_ctx->dst_frm_info.p_vir_addr[0]); 
		p_g2d_ctx->dst_frm_info.p_phy_addr[1] = (void *)g2d_getPhyAddrByVirAddr(p_g2d_ctx->dst_frm_info.p_vir_addr[1]); 
    }
    else if(pConfig->mDstPicFormat == MM_PIXEL_FORMAT_UYVY_PACKAGE_422 || pConfig->mDstPicFormat == MM_PIXEL_FORMAT_VYUY_PACKAGE_422 ||
            pConfig->mDstPicFormat == MM_PIXEL_FORMAT_YUYV_PACKAGE_422 || pConfig->mDstPicFormat == MM_PIXEL_FORMAT_YVYU_AW_PACKAGE_422)
    {
		size = ALIGN(p_g2d_ctx->dst_frm_info.frm_width, 16)*ALIGN(p_g2d_ctx->dst_frm_info.frm_height, 16);
		p_g2d_ctx->dst_frm_info.p_vir_addr[0] = (void *)g2d_allocMem(size*2);
		if(NULL == p_g2d_ctx->dst_frm_info.p_vir_addr[0])
		{
			g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[0]); 
			g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[1]);
			aloge("malloc_dst_frm_y_mem_failed");
			return -1;
		}

		p_g2d_ctx->dst_frm_info.p_phy_addr[0] = (void *)g2d_getPhyAddrByVirAddr(p_g2d_ctx->dst_frm_info.p_vir_addr[0]); 
    }
	else if(pConfig->mDstPicFormat == MM_PIXEL_FORMAT_RGB_888)
	{
		size = p_g2d_ctx->dst_frm_info.frm_width * p_g2d_ctx->dst_frm_info.frm_height * 3;
		p_g2d_ctx->dst_frm_info.p_vir_addr[0] = (void *)g2d_allocMem(size);
		if(NULL == p_g2d_ctx->dst_frm_info.p_vir_addr[0])
		{
			if(p_g2d_ctx->src_frm_info.p_vir_addr[0] != NULL)
			{
				g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[0]); 
			}
			if(p_g2d_ctx->src_frm_info.p_vir_addr[1] != NULL)
			{
				g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[1]);
			}
			aloge("malloc_dst_frm_y_mem_failed");
			return -1;
		}
		p_g2d_ctx->dst_frm_info.p_phy_addr[0] = (void *)g2d_getPhyAddrByVirAddr(p_g2d_ctx->dst_frm_info.p_vir_addr[0]); 
	}
    else if(pConfig->mDstPicFormat == MM_PIXEL_FORMAT_RGB_8888)
	{
		size = p_g2d_ctx->dst_frm_info.frm_width * p_g2d_ctx->dst_frm_info.frm_height * 4;
		p_g2d_ctx->dst_frm_info.p_vir_addr[0] = (void *)g2d_allocMem(size);
		if(NULL == p_g2d_ctx->dst_frm_info.p_vir_addr[0])
		{
			if(p_g2d_ctx->src_frm_info.p_vir_addr[0] != NULL)
			{
				g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[0]);
			}
			if(p_g2d_ctx->src_frm_info.p_vir_addr[1] != NULL)
			{
				g2d_freeMem(p_g2d_ctx->src_frm_info.p_vir_addr[1]);
			}
			aloge("malloc_dst_frm_y_mem_failed");
			return -1;
		}
		p_g2d_ctx->dst_frm_info.p_phy_addr[0] = (void *)g2d_getPhyAddrByVirAddr(p_g2d_ctx->dst_frm_info.p_vir_addr[0]);
	}

    return 0; 
}

static int SampleG2d_G2dOpen(SAMPLE_G2D_CTX *p_g2d_ctx)
{
    int ret = 0;
    p_g2d_ctx->mG2dFd = open("/dev/g2d", O_RDWR, 0);
    if (p_g2d_ctx->mG2dFd < 0)
    {
        aloge("fatal error! open /dev/g2d failed");
        ret = -1;
    }
    return ret;
}

static int SampleG2d_G2dClose(SAMPLE_G2D_CTX *p_g2d_ctx)
{
    if(p_g2d_ctx->mG2dFd >= 0)
    {
        close(p_g2d_ctx->mG2dFd);
        p_g2d_ctx->mG2dFd = -1;
    }
    return 0;
}

static int SampleG2d_G2dConvert_rotate(SAMPLE_G2D_CTX *p_g2d_ctx)
{
    int ret = 0;
    g2d_blt_h blit;
    g2d_fmt_enh eSrcFormat, eDstFormat; 
    SampleG2dConfig *pConfig = NULL;

    pConfig = &p_g2d_ctx->mConfigPara;

    
    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pConfig->mPicFormat, &eSrcFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! src pixel format[0x%x] is invalid!", pConfig->mPicFormat);
        return -1;
    }
    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pConfig->mDstPicFormat, &eDstFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! dst pixel format[0x%x] is invalid!", pConfig->mPicFormat);
        return -1;
    }

    //config blit
    memset(&blit, 0, sizeof(g2d_blt_h));
    if(pConfig->mDstRotate >= 0){
        switch(pConfig->mDstRotate)
        {
            case 0:
                blit.flag_h = G2D_BLT_NONE_H;   //G2D_ROT_0, G2D_BLT_NONE_H
                break;
            case 90:
                blit.flag_h = G2D_ROT_90;
                break;
            case 180:
                blit.flag_h = G2D_ROT_180;
                break;
            case 270:
                blit.flag_h = G2D_ROT_270;
                break;
            default:
                aloge("fatal error! rotation[%d] is invalid!", pConfig->mDstRotate);
                blit.flag_h = G2D_BLT_NONE_H;
                break;
        }
    }else if(pConfig->flip_flag == 'H' || pConfig->flip_flag == 'V'){
        switch(pConfig->flip_flag)
        {
            case 'H':
                blit.flag_h = G2D_ROT_H;
                break;
            case 'V':
                blit.flag_h = G2D_ROT_V;
                break;
            case 'N':
                break;
            default:
                aloge("fatal error! dst_flip[%c] is invalid!", pConfig->flip_flag);
                break;
        }
    }else{
        aloge("fatal error! invalid rotate value %d or flip %d", pConfig->mDstRotate,pConfig->flip_flag);
        return -1;
    }
    
    //blit.src_image_h.bbuff = 1;
    //blit.src_image_h.color = 0xff;
    blit.src_image_h.format = eSrcFormat;
    blit.src_image_h.laddr[0] = (unsigned int)p_g2d_ctx->src_frm_info.p_phy_addr[0];
    blit.src_image_h.laddr[1] = (unsigned int)p_g2d_ctx->src_frm_info.p_phy_addr[1];
    blit.src_image_h.laddr[2] = (unsigned int)p_g2d_ctx->src_frm_info.p_phy_addr[2];
    //blit.src_image_h.haddr[] = 
    blit.src_image_h.width = p_g2d_ctx->src_frm_info.frm_width;
    blit.src_image_h.height = p_g2d_ctx->src_frm_info.frm_height;
    blit.src_image_h.align[0] = 0;
    blit.src_image_h.align[1] = 0;
    blit.src_image_h.align[2] = 0;
    blit.src_image_h.clip_rect.x = pConfig->mSrcRectX;
    blit.src_image_h.clip_rect.y = pConfig->mSrcRectY;
    blit.src_image_h.clip_rect.w = pConfig->mSrcRectW;
    blit.src_image_h.clip_rect.h = pConfig->mSrcRectH;
    blit.src_image_h.gamut = G2D_BT601;
    blit.src_image_h.bpremul = 0;
    //blit.src_image_h.alpha = 0xff;
    blit.src_image_h.mode = G2D_PIXEL_ALPHA;   //G2D_PIXEL_ALPHA, G2D_GLOBAL_ALPHA
    blit.src_image_h.fd = -1;
    blit.src_image_h.use_phy_addr = 1;

    //blit.dst_image_h.bbuff = 1;
    //blit.dst_image_h.color = 0xff;
    blit.dst_image_h.format = eDstFormat;
    blit.dst_image_h.laddr[0] = (unsigned int)p_g2d_ctx->dst_frm_info.p_phy_addr[0];
    blit.dst_image_h.laddr[1] = (unsigned int)p_g2d_ctx->dst_frm_info.p_phy_addr[1];
    blit.dst_image_h.laddr[2] = (unsigned int)p_g2d_ctx->dst_frm_info.p_phy_addr[2];
    //blit.dst_image_h.haddr[] = 
    blit.dst_image_h.width = p_g2d_ctx->dst_frm_info.frm_width;
    blit.dst_image_h.height = p_g2d_ctx->dst_frm_info.frm_height;
    blit.dst_image_h.align[0] = 0;
    blit.dst_image_h.align[1] = 0;
    blit.dst_image_h.align[2] = 0;
    blit.dst_image_h.clip_rect.x = pConfig->mDstRectX;
    blit.dst_image_h.clip_rect.y = pConfig->mDstRectY;
    blit.dst_image_h.clip_rect.w = pConfig->mDstRectW;
    blit.dst_image_h.clip_rect.h = pConfig->mDstRectH;
    blit.dst_image_h.gamut = G2D_BT601;
    blit.dst_image_h.bpremul = 0;
    //blit.dst_image_h.alpha = 0xff;
    blit.dst_image_h.mode = G2D_PIXEL_ALPHA;   //G2D_PIXEL_ALPHA, G2D_GLOBAL_ALPHA
    blit.dst_image_h.fd = -1;
    blit.dst_image_h.use_phy_addr = 1;

    ret = ioctl(p_g2d_ctx->mG2dFd, G2D_CMD_BITBLT_H, (unsigned long)&blit);
    if(ret < 0)
    {
        aloge("fatal error! bit-block(image) transfer failed[%d]", ret);
        system("cd /sys/class/sunxi_dump;echo 0x14A8000,0x14A8100 > dump;cat dump");
    }

    return ret;
} 

static int SampleG2d_G2dConvert_scale(SAMPLE_G2D_CTX *p_g2d_ctx)
{
    int ret = 0;
    g2d_blt_h blit;
    g2d_fmt_enh eSrcFormat, eDstFormat; 
    SampleG2dConfig *pConfig = NULL;

    pConfig = &p_g2d_ctx->mConfigPara;

    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pConfig->mPicFormat, &eSrcFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! src pixel format[0x%x] is invalid!", pConfig->mPicFormat);
        return -1;
    }
    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pConfig->mDstPicFormat, &eDstFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! dst pixel format[0x%x] is invalid!", pConfig->mPicFormat);
        return -1;
    }

    //config blit
    memset(&blit, 0, sizeof(g2d_blt_h));

    if(0 != pConfig->mDstRotate)
    {
        aloge("fatal_err: rotation can't be performed when do scaling");
    }

    blit.flag_h = G2D_BLT_NONE_H;       // angle rotation used
//    switch(pConfig->mDstRotate)
//    {
//        case 0:
//            blit.flag_h = G2D_BLT_NONE_H;   //G2D_ROT_0, G2D_BLT_NONE_H
//            break;
//        case 90:
//            blit.flag_h = G2D_ROT_90;
//            break;
//        case 180:
//            blit.flag_h = G2D_ROT_180;
//            break;
//        case 270:
//            blit.flag_h = G2D_ROT_270;
//            break;
//        default:
//            aloge("fatal error! rotation[%d] is invalid!", pConfig->mDstRotate);
//            blit.flag_h = G2D_BLT_NONE_H;
//            break;
//    }
    //blit.src_image_h.bbuff = 1;
    //blit.src_image_h.color = 0xff;
    blit.src_image_h.format = eSrcFormat;
    blit.src_image_h.laddr[0] = (unsigned int)p_g2d_ctx->src_frm_info.p_phy_addr[0];
    blit.src_image_h.laddr[1] = (unsigned int)p_g2d_ctx->src_frm_info.p_phy_addr[1];
    blit.src_image_h.laddr[2] = (unsigned int)p_g2d_ctx->src_frm_info.p_phy_addr[2];
    //blit.src_image_h.haddr[] = 
    blit.src_image_h.width = p_g2d_ctx->src_frm_info.frm_width;
    blit.src_image_h.height = p_g2d_ctx->src_frm_info.frm_height;
    blit.src_image_h.align[0] = 0;
    blit.src_image_h.align[1] = 0;
    blit.src_image_h.align[2] = 0;
    blit.src_image_h.clip_rect.x = pConfig->mSrcRectX;
    blit.src_image_h.clip_rect.y = pConfig->mSrcRectY;
    blit.src_image_h.clip_rect.w = pConfig->mSrcRectW;
    blit.src_image_h.clip_rect.h = pConfig->mSrcRectH;
    blit.src_image_h.gamut = G2D_BT601;
    blit.src_image_h.bpremul = 0;
    //blit.src_image_h.alpha = 0xff;
    blit.src_image_h.mode = G2D_PIXEL_ALPHA;   //G2D_PIXEL_ALPHA, G2D_GLOBAL_ALPHA
    blit.src_image_h.fd = -1;
    blit.src_image_h.use_phy_addr = 1;

    //blit.dst_image_h.bbuff = 1;
    //blit.dst_image_h.color = 0xff;
    blit.dst_image_h.format = eDstFormat;
    blit.dst_image_h.laddr[0] = (unsigned int)p_g2d_ctx->dst_frm_info.p_phy_addr[0];
    blit.dst_image_h.laddr[1] = (unsigned int)p_g2d_ctx->dst_frm_info.p_phy_addr[1];
    blit.dst_image_h.laddr[2] = (unsigned int)p_g2d_ctx->dst_frm_info.p_phy_addr[2];
    //blit.dst_image_h.haddr[] = 
    blit.dst_image_h.width = p_g2d_ctx->dst_frm_info.frm_width;
    blit.dst_image_h.height = p_g2d_ctx->dst_frm_info.frm_height;
    blit.dst_image_h.align[0] = 0;
    blit.dst_image_h.align[1] = 0;
    blit.dst_image_h.align[2] = 0;
    blit.dst_image_h.clip_rect.x = pConfig->mDstRectX;
    blit.dst_image_h.clip_rect.y = pConfig->mDstRectY;
    blit.dst_image_h.clip_rect.w = pConfig->mDstRectW;
    blit.dst_image_h.clip_rect.h = pConfig->mDstRectH;
    blit.dst_image_h.gamut = G2D_BT601;
    blit.dst_image_h.bpremul = 0;
    //blit.dst_image_h.alpha = 0xff;
    blit.dst_image_h.mode = G2D_PIXEL_ALPHA;   //G2D_PIXEL_ALPHA, G2D_GLOBAL_ALPHA
    blit.dst_image_h.fd = -1;
    blit.dst_image_h.use_phy_addr = 1;

    ret = ioctl(p_g2d_ctx->mG2dFd, G2D_CMD_BITBLT_H, (unsigned long)&blit);
    if(ret < 0)
    {
        aloge("fatal error! bit-block(image) transfer failed[%d]", ret);
        system("cd /sys/class/sunxi_dump;echo 0x14A8000,0x14A8100 > dump;cat dump");
    }

    return ret;
} 

static int SampleG2d_G2dConvert_formatconversion(SAMPLE_G2D_CTX *p_g2d_ctx)
{
    int ret = 0;
    g2d_blt_h blit;
    g2d_fmt_enh eSrcFormat, eDstFormat;
    SampleG2dConfig *pConfig = NULL;

    pConfig = &p_g2d_ctx->mConfigPara;

    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pConfig->mPicFormat, &eSrcFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! src pixel format[0x%x] is invalid!", pConfig->mPicFormat);
        return -1;
    }
    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pConfig->mDstPicFormat, &eDstFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! dst pixel format[0x%x] is invalid!", pConfig->mPicFormat);
        return -1;
    }

    //config blit
    memset(&blit, 0, sizeof(g2d_blt_h));

    if(0 != pConfig->mDstRotate)
    {
        aloge("fatal_err: rotation can't be performed when do scaling");
    }

    blit.flag_h = G2D_BLT_NONE_H;       // angle rotation used
//    switch(pConfig->mDstRotate)
//    {
//        case 0:
//            blit.flag_h = G2D_BLT_NONE_H;   //G2D_ROT_0, G2D_BLT_NONE_H
//            break;
//        case 90:
//            blit.flag_h = G2D_ROT_90;
//            break;
//        case 180:
//            blit.flag_h = G2D_ROT_180;
//            break;
//        case 270:
//            blit.flag_h = G2D_ROT_270;
//            break;
//        default:
//            aloge("fatal error! rotation[%d] is invalid!", pConfig->mDstRotate);
//            blit.flag_h = G2D_BLT_NONE_H;
//            break;
//    }
    //blit.src_image_h.bbuff = 1;
    //blit.src_image_h.color = 0xff;
    blit.src_image_h.format = eSrcFormat;
    blit.src_image_h.laddr[0] = (unsigned int)p_g2d_ctx->src_frm_info.p_phy_addr[0];
    blit.src_image_h.laddr[1] = (unsigned int)p_g2d_ctx->src_frm_info.p_phy_addr[1];
    blit.src_image_h.laddr[2] = (unsigned int)p_g2d_ctx->src_frm_info.p_phy_addr[2];
    //blit.src_image_h.haddr[] =
    blit.src_image_h.width = p_g2d_ctx->src_frm_info.frm_width;
    blit.src_image_h.height = p_g2d_ctx->src_frm_info.frm_height;
    blit.src_image_h.align[0] = 0;
    blit.src_image_h.align[1] = 0;
    blit.src_image_h.align[2] = 0;
    blit.src_image_h.clip_rect.x = pConfig->mSrcRectX;
    blit.src_image_h.clip_rect.y = pConfig->mSrcRectY;
    blit.src_image_h.clip_rect.w = pConfig->mSrcRectW;
    blit.src_image_h.clip_rect.h = pConfig->mSrcRectH;
    blit.src_image_h.gamut = G2D_BT601;
    blit.src_image_h.bpremul = 0;
    //blit.src_image_h.alpha = 0xff;
    blit.src_image_h.mode = G2D_PIXEL_ALPHA;   //G2D_PIXEL_ALPHA, G2D_GLOBAL_ALPHA
    blit.src_image_h.fd = -1;
    blit.src_image_h.use_phy_addr = 1;

    //blit.dst_image_h.bbuff = 1;
    //blit.dst_image_h.color = 0xff;
    blit.dst_image_h.format = eDstFormat;
    blit.dst_image_h.laddr[0] = (unsigned int)p_g2d_ctx->dst_frm_info.p_phy_addr[0];
    blit.dst_image_h.laddr[1] = (unsigned int)p_g2d_ctx->dst_frm_info.p_phy_addr[1];
    blit.dst_image_h.laddr[2] = (unsigned int)p_g2d_ctx->dst_frm_info.p_phy_addr[2];
    //blit.dst_image_h.haddr[] =
    blit.dst_image_h.width = p_g2d_ctx->dst_frm_info.frm_width;
    blit.dst_image_h.height = p_g2d_ctx->dst_frm_info.frm_height;
    blit.dst_image_h.align[0] = 0;
    blit.dst_image_h.align[1] = 0;
    blit.dst_image_h.align[2] = 0;
    blit.dst_image_h.clip_rect.x = pConfig->mDstRectX;
    blit.dst_image_h.clip_rect.y = pConfig->mDstRectY;
    blit.dst_image_h.clip_rect.w = pConfig->mDstRectW;
    blit.dst_image_h.clip_rect.h = pConfig->mDstRectH;
    blit.dst_image_h.gamut = G2D_BT601;
    blit.dst_image_h.bpremul = 0;
    //blit.dst_image_h.alpha = 0xff;
    blit.dst_image_h.mode = G2D_PIXEL_ALPHA;   //G2D_PIXEL_ALPHA, G2D_GLOBAL_ALPHA
    blit.dst_image_h.fd = -1;
    blit.dst_image_h.use_phy_addr = 1;

    ret = ioctl(p_g2d_ctx->mG2dFd, G2D_CMD_BITBLT_H, (unsigned long)&blit);
    if(ret < 0)
    {
        aloge("fatal error! bit-block(image) transfer failed[%d]", ret);
        system("cd /sys/class/sunxi_dump;echo 0x14A8000,0x14A8100 > dump;cat dump");
    }

    return ret;
}

static int SampleG2d_G2dBld(SAMPLE_G2D_CTX *p_g2d_ctx)
{
    int ret = 0;
    g2d_bld bld;
    g2d_fmt_enh eSrcFormat, eDstFormat;
    SampleG2dConfig *pConfig = NULL;

    pConfig = &p_g2d_ctx->mConfigPara;

    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pConfig->mPicFormat, &eSrcFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! src pixel format[0x%x] is invalid!", pConfig->mPicFormat);
        return -1;
    }
    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pConfig->mDstPicFormat, &eDstFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! dst pixel format[0x%x] is invalid!", pConfig->mPicFormat);
        return -1;
    }
    memset(&bld, 0, sizeof(g2d_bld));

    alogd("size[%dx%d], size[%dx%d]", p_g2d_ctx->src_frm_info.frm_width, \
        p_g2d_ctx->src_frm_info.frm_height, p_g2d_ctx->dst_frm_info.frm_width, \
        p_g2d_ctx->dst_frm_info.frm_height);

    bld.bld_cmd = pConfig->g2d_bld_mod;

    bld.src_image_h.format = eSrcFormat;
    bld.src_image_h.laddr[0] = (unsigned int)p_g2d_ctx->src_frm_info.p_phy_addr[0];
    bld.src_image_h.laddr[1] = (unsigned int)p_g2d_ctx->src_frm_info.p_phy_addr[1];
    bld.src_image_h.laddr[2] = (unsigned int)p_g2d_ctx->src_frm_info.p_phy_addr[2];
    bld.src_image_h.width = p_g2d_ctx->src_frm_info.frm_width;
    bld.src_image_h.height = p_g2d_ctx->src_frm_info.frm_height;
    bld.src_image_h.align[0] = 0;
    bld.src_image_h.align[1] = 0;
    bld.src_image_h.align[2] = 0;
    bld.src_image_h.clip_rect.x = pConfig->mSrcRectX;
    bld.src_image_h.clip_rect.y = pConfig->mSrcRectY;
    bld.src_image_h.clip_rect.w = pConfig->mSrcRectW;
    bld.src_image_h.clip_rect.h = pConfig->mSrcRectH;
    //bld.src_image_h.gamut = G2D_BT601;
    //bld.src_image_h.bpremul = 0;
    bld.src_image_h.mode = G2D_GLOBAL_ALPHA;
    bld.src_image_h.alpha = 128;
    bld.src_image_h.fd = -1;
    bld.src_image_h.use_phy_addr = 1;

    bld.dst_image_h.format = eDstFormat;
    bld.dst_image_h.laddr[0] = (unsigned int)p_g2d_ctx->dst_frm_info.p_phy_addr[0];
    bld.dst_image_h.laddr[1] = (unsigned int)p_g2d_ctx->dst_frm_info.p_phy_addr[1];
    bld.dst_image_h.laddr[2] = (unsigned int)p_g2d_ctx->dst_frm_info.p_phy_addr[2];
    bld.dst_image_h.width = p_g2d_ctx->dst_frm_info.frm_width;
    bld.dst_image_h.height = p_g2d_ctx->dst_frm_info.frm_height;
    bld.dst_image_h.align[0] = 0;
    bld.dst_image_h.align[1] = 0;
    bld.dst_image_h.align[2] = 0;
    bld.dst_image_h.clip_rect.x = pConfig->mDstRectX;
    bld.dst_image_h.clip_rect.y = pConfig->mDstRectY;
    bld.dst_image_h.clip_rect.w = pConfig->mDstRectW;
    bld.dst_image_h.clip_rect.h = pConfig->mDstRectH;
    //bld.dst_image_h.gamut = G2D_BT601;
    //bld.dst_image_h.bpremul = 0;
    bld.dst_image_h.mode = G2D_GLOBAL_ALPHA;
    bld.dst_image_h.alpha = 128;
    bld.dst_image_h.fd = -1;
    bld.dst_image_h.use_phy_addr = 1;

    ret = ioctl(p_g2d_ctx->mG2dFd, G2D_CMD_BLD_H, (unsigned long)&bld);
    if(ret < 0)
    {
        aloge("fatal error! bit-block(image) transfer failed[%d]", ret);
        system("cd /sys/class/sunxi_dump;echo 0x14A8000,0x14A8100 > dump;cat dump");
    }

    return ret;
}

static int readSrcFile(SampleG2dMixerTaskFrmInfo *pFrmInfo, char *pFilePath)
{
    int read_len = 0;
    FILE *fp = fopen(pFilePath, "rb");
    if(NULL == fp)
    {
        aloge("open src file failed");
        return -1;
    }

    if (fp)
    {
        if (G2D_FORMAT_RGB888 == pFrmInfo->mSrcPicFormat)
        {
            read_len = pFrmInfo->mSrcWidth * pFrmInfo->mSrcHeight * 3;
            memset(pFrmInfo->mpSrcVirFrmAddr[0], 0, read_len);
            fread(pFrmInfo->mpSrcVirFrmAddr[0], read_len, 1, fp);
            g2d_flushCache(pFrmInfo->mpSrcVirFrmAddr[0], read_len);
        }
        else if (G2D_FORMAT_YUV420UVC_V1U1V0U0 == pFrmInfo->mSrcPicFormat)
        {
            read_len = pFrmInfo->mSrcWidth * pFrmInfo->mSrcHeight;
            memset(pFrmInfo->mpSrcVirFrmAddr[0], 0, read_len);
            memset(pFrmInfo->mpSrcVirFrmAddr[1], 0, read_len/2);
            fread(pFrmInfo->mpSrcVirFrmAddr[0], read_len, 1, fp);
            fread(pFrmInfo->mpSrcVirFrmAddr[1], read_len/2, 1, fp);
            g2d_flushCache(pFrmInfo->mpSrcVirFrmAddr[0], read_len);
            g2d_flushCache(pFrmInfo->mpSrcVirFrmAddr[1], read_len/2);
        }
    }
    else
    {
        return -1;
    }

    fclose(fp);
    fp = NULL;
    return 0;
}

static int prepareSrcFile(SAMPLE_G2D_CTX *p_g2d_ctx)
{
    int len = 0;
    int ret = 0;
    SampleG2dMixerTaskFrmInfo *pYUVFrmInfo = &p_g2d_ctx->mixertask.mYUVFrmInfo;
    SampleG2dMixerTaskFrmInfo *pRGBFrmInfo = &p_g2d_ctx->mixertask.mRGBFrmInfo;
    if(p_g2d_ctx->mConfigPara.mPicFormat == MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420)
    {
        memset(pYUVFrmInfo, 0, sizeof(SampleG2dMixerTaskFrmInfo));
        pYUVFrmInfo->mSrcPicFormat = G2D_FORMAT_YUV420UVC_V1U1V0U0;
        pYUVFrmInfo->mSrcWidth = p_g2d_ctx->mConfigPara.mSrcWidth;
        pYUVFrmInfo->mSrcHeight = p_g2d_ctx->mConfigPara.mSrcHeight;
        len = pYUVFrmInfo->mSrcWidth * pYUVFrmInfo->mSrcHeight;
        pYUVFrmInfo->mpSrcVirFrmAddr[0] = g2d_allocMem(len);
        if (NULL == pYUVFrmInfo->mpSrcVirFrmAddr[0])
        {
            aloge("fatal error! prepare src file y data mem fail!");
            return -1;
        }
        pYUVFrmInfo->mpSrcVirFrmAddr[1] = g2d_allocMem(len/2);
        if (NULL == pYUVFrmInfo->mpSrcVirFrmAddr[0])
        {
            aloge("fatal error! prepare src file c data mem fail!");
            return -1;
        }
        ret = readSrcFile(pYUVFrmInfo, p_g2d_ctx->mConfigPara.SrcFile);
        if (ret)
        {
            aloge("fatal error! prepare src yuv file fail!");
            return -1;
        }
    }
    else if(p_g2d_ctx->mConfigPara.mPicFormat == MM_PIXEL_FORMAT_RGB_888)
    {
        memset(pRGBFrmInfo, 0, sizeof(SampleG2dMixerTaskFrmInfo));
        pRGBFrmInfo->mSrcPicFormat = G2D_FORMAT_RGB888;
        pRGBFrmInfo->mSrcWidth = p_g2d_ctx->mConfigPara.mDstWidth;
        pRGBFrmInfo->mSrcHeight = p_g2d_ctx->mConfigPara.mDstHeight;
        len = pRGBFrmInfo->mSrcWidth * pRGBFrmInfo->mSrcHeight * 3;
        pRGBFrmInfo->mpSrcVirFrmAddr[0] = g2d_allocMem(len);
        if (NULL == pRGBFrmInfo->mpSrcVirFrmAddr[0])
        {
            aloge("fatal error! prepare dst file data mem fail!");
            return -1;
        }
        ret = readSrcFile(pRGBFrmInfo, p_g2d_ctx->mConfigPara.SrcFile);
        if (ret)
        {
            aloge("fatal error! prepare dst rgb file fail!");
            return -1;
        }
    }

    return 0;
}

static int saveDstFile(SampleG2dMixerTaskFrmInfo *pFrmInfo, char *pFilePath)
{
    FILE *fp = NULL;
    int write_len = 0;

    fp = fopen(pFilePath, "wb");
    if (fp)
    {
        if (G2D_FORMAT_RGB888 == pFrmInfo->mDstPicFormat)
        {
            write_len = pFrmInfo->mDstWidth * pFrmInfo->mDstHeight * 3;
            g2d_flushCache(pFrmInfo->mpDstVirFrmAddr[0], write_len);
            fwrite(pFrmInfo->mpDstVirFrmAddr[0], write_len, 1, fp);
        }
        else if (G2D_FORMAT_YUV420UVC_V1U1V0U0 == pFrmInfo->mDstPicFormat)
        {
            write_len = pFrmInfo->mDstWidth * pFrmInfo->mDstHeight;
            g2d_flushCache(pFrmInfo->mpDstVirFrmAddr[0], write_len);
            g2d_flushCache(pFrmInfo->mpDstVirFrmAddr[1], write_len/2);
            fwrite(pFrmInfo->mpDstVirFrmAddr[0], write_len, 1, fp);
            fwrite(pFrmInfo->mpDstVirFrmAddr[1], write_len/2, 1, fp);
        }
    }
    else
    {
        aloge("fatal error! open file[%s] fail!", pFilePath);
        return -1;
    }

    fclose(fp);
    fp = NULL;
    return 0;
}

static int saveDstFrm(SampleG2dMixerTaskContext *pContext)
{
    int ret = 0;
    char filePath[128] = {0};
    SampleG2dMixerTaskFrmInfo *pFrmInfo;

    for (int i = 0; i < FRAME_TO_BE_PROCESS; i++)
    {
        pFrmInfo = &pContext->mFrmInfo[i];
        if (G2D_FORMAT_RGB888 == pFrmInfo->mDstPicFormat)
        {
            memset(filePath, 0, sizeof(filePath));
            sprintf(filePath, "/mnt/extsd/mixer%d_%dx%d_rgb888.rgb", \
                i, pFrmInfo->mDstWidth, pFrmInfo->mDstHeight);
            ret = saveDstFile(pFrmInfo, filePath);
            if (ret)
            {
                aloge("fatal error! mixer[%d] save dst file fail!", i);
                return -1;
            }
        }
        else if (G2D_FORMAT_YUV420UVC_V1U1V0U0 == pFrmInfo->mDstPicFormat)
        {
            memset(filePath, 0, sizeof(filePath));
            sprintf(filePath, "/mnt/extsd/mixer%d_%dx%d_nv21.yuv", \
                i, pFrmInfo->mDstWidth, pFrmInfo->mDstHeight);
            ret = saveDstFile(pFrmInfo, filePath);
            if (ret)
            {
                aloge("fatal error! mixer[%d] save dst file fail!", i);
                return -1;
            }
        }
    }
    return ret;
}

static int prepareBuf(SAMPLE_G2D_CTX *p_g2d_ctx)
{
    int ret = 0;
    int len = 0;
    struct mixer_para *pMixerPara = NULL;
    struct SampleG2dMixerTaskFrmInfo *pFrmInfo = NULL;
    g2d_fmt_enh eSrcFormat;
    if(p_g2d_ctx->mConfigPara.mPicFormat == MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420)
        eSrcFormat = G2D_FORMAT_YUV420UVC_V1U1V0U0;
    else if(p_g2d_ctx->mConfigPara.mPicFormat == MM_PIXEL_FORMAT_RGB_888)
        eSrcFormat = G2D_FORMAT_RGB888;
    else
        eSrcFormat = G2D_FORMAT_YUV420UVC_V1U1V0U0;

    for (int i = 0; i < FRAME_TO_BE_PROCESS; i++)
    {
        pMixerPara = &p_g2d_ctx->mixertask.stMixPara[i];
        pFrmInfo = &p_g2d_ctx->mixertask.mFrmInfo[i];
        memset(pMixerPara, 0, sizeof(p_g2d_ctx->mixertask.stMixPara[i]));
        if (i < FRAME_TO_BE_PROCESS)
        {
            pMixerPara->flag_h = G2D_BLT_NONE_H;
            pMixerPara->op_flag = OP_BITBLT;
            pMixerPara->src_image_h.format = eSrcFormat;
            pMixerPara->src_image_h.width =  p_g2d_ctx->mConfigPara.mSrcWidth;
            pMixerPara->src_image_h.height = p_g2d_ctx->mConfigPara.mSrcHeight;
            pMixerPara->src_image_h.clip_rect.x = p_g2d_ctx->mConfigPara.mSrcRectX;
            pMixerPara->src_image_h.clip_rect.y = p_g2d_ctx->mConfigPara.mSrcRectY;
            pMixerPara->src_image_h.clip_rect.w = p_g2d_ctx->mConfigPara.mSrcRectW;
            pMixerPara->src_image_h.clip_rect.h = p_g2d_ctx->mConfigPara.mSrcRectH;
            pMixerPara->src_image_h.mode = G2D_PIXEL_ALPHA;
            pMixerPara->src_image_h.alpha = 255;
            pMixerPara->src_image_h.use_phy_addr = 1;
            pMixerPara->src_image_h.fd = -1;

            pMixerPara->dst_image_h.format = eSrcFormat;
            pMixerPara->dst_image_h.width = p_g2d_ctx->mConfigPara.mDstWidth;
            pMixerPara->dst_image_h.height = p_g2d_ctx->mConfigPara.mDstHeight;
            pMixerPara->dst_image_h.clip_rect.x = p_g2d_ctx->mConfigPara.mDstRectX;
            pMixerPara->dst_image_h.clip_rect.y = p_g2d_ctx->mConfigPara.mDstRectY;
            pMixerPara->dst_image_h.clip_rect.w = p_g2d_ctx->mConfigPara.mDstRectW;
            pMixerPara->dst_image_h.clip_rect.h = p_g2d_ctx->mConfigPara.mDstRectH;
            pMixerPara->dst_image_h.mode = G2D_PIXEL_ALPHA;
            pMixerPara->dst_image_h.alpha = 255;
            pMixerPara->dst_image_h.use_phy_addr = 1;
            pMixerPara->dst_image_h.fd = -1;
        }
            pFrmInfo->mSrcPicFormat = pMixerPara->src_image_h.format;
            pFrmInfo->mSrcWidth = pMixerPara->src_image_h.width;
            pFrmInfo->mSrcHeight = pMixerPara->src_image_h.height;
            pFrmInfo->mDstPicFormat = pMixerPara->dst_image_h.format;
            pFrmInfo->mDstWidth = pMixerPara->dst_image_h.width;
            pFrmInfo->mDstHeight = pMixerPara->dst_image_h.height;
    }

    memset(&p_g2d_ctx->mixertask.mYUVFrmInfo, 0, sizeof(SampleG2dMixerTaskFrmInfo));
    memset(&p_g2d_ctx->mixertask.mRGBFrmInfo, 0, sizeof(SampleG2dMixerTaskFrmInfo));
    ret = prepareSrcFile(p_g2d_ctx);
    if (ret)
    {
        aloge("fatal error! prepare src file fail!");
        goto _release_src_file;
    }

    for (int i = 0; i < FRAME_TO_BE_PROCESS; i++)
    {
        pMixerPara = &p_g2d_ctx->mixertask.stMixPara[i];
        pFrmInfo = &p_g2d_ctx->mixertask.mFrmInfo[i];
        switch (pMixerPara->src_image_h.format)
        {
            case G2D_FORMAT_RGB888:
            {
                pMixerPara->src_image_h.laddr[0] = g2d_getPhyAddrByVirAddr(p_g2d_ctx->mixertask.mRGBFrmInfo.mpSrcVirFrmAddr[0]);
                break;
            }
            case G2D_FORMAT_YUV420UVC_V1U1V0U0:
            {
                pMixerPara->src_image_h.laddr[0] = g2d_getPhyAddrByVirAddr(p_g2d_ctx->mixertask.mYUVFrmInfo.mpSrcVirFrmAddr[0]);
                pMixerPara->src_image_h.laddr[1] = g2d_getPhyAddrByVirAddr(p_g2d_ctx->mixertask.mYUVFrmInfo.mpSrcVirFrmAddr[1]);
                break;
            }
            default :
            {
                aloge("mixer para[%d] src format[%d] is invalid!", i, \
                    pMixerPara->src_image_h.format);
                return -1;
            }
        }

        switch (pMixerPara->dst_image_h.format)
        {
            case G2D_FORMAT_RGB888:
            {
                len = pFrmInfo->mDstWidth * pFrmInfo->mDstHeight * 3;
                pFrmInfo->mpDstVirFrmAddr[0] = g2d_allocMem(len);
                if (NULL == pFrmInfo->mpDstVirFrmAddr[0])
                {
                    aloge("fatal error! malloc mixer[%d] mem fail!", i);
                    return -1;
                }
                pMixerPara->dst_image_h.laddr[0] = g2d_getPhyAddrByVirAddr(pFrmInfo->mpDstVirFrmAddr[0]);
                break;
            }
            case G2D_FORMAT_YUV420UVC_V1U1V0U0:
            {
                len = pFrmInfo->mDstWidth * pFrmInfo->mDstHeight;
                pFrmInfo->mpDstVirFrmAddr[0] = g2d_allocMem(len);
                if (NULL == pFrmInfo->mpDstVirFrmAddr[0])
                {
                    aloge("fatal error! malloc mixer[%d] mem fail!", i);
                    return -1;
                }
                pFrmInfo->mpDstVirFrmAddr[1] = g2d_allocMem(len/2);
                if (NULL == pFrmInfo->mpDstVirFrmAddr[1])
                {
                    aloge("fatal error! malloc mixer[%d] mem fail!", i);
                    return -1;
                }
                pMixerPara->dst_image_h.laddr[0] = g2d_getPhyAddrByVirAddr(pFrmInfo->mpDstVirFrmAddr[0]);
                pMixerPara->dst_image_h.laddr[1] = g2d_getPhyAddrByVirAddr(pFrmInfo->mpDstVirFrmAddr[1]);
                break;
            }
            default :
            {
                aloge("mixer para[%d] dst format[%d] is invalid!", i, \
                    pMixerPara->src_image_h.format);
                return -1;
            }
        }
    }

    return ret;
_release_src_file:
    releaseSrcFile(&p_g2d_ctx->mixertask.mYUVFrmInfo, &p_g2d_ctx->mixertask.mRGBFrmInfo);

    return ret;
}

static int SampleG2d_G2dMixter_Task(SAMPLE_G2D_CTX *p_g2d_ctx)
{
    int ret = 0;
    ret = prepareBuf(p_g2d_ctx);
    if(ret < 0)
    {
        aloge("prepare buf failed!!\n",ret);
        return -1;
    }

    unsigned long arg[2];
    arg[0] = (unsigned long)p_g2d_ctx->mixertask.stMixPara;
    arg[1] = FRAME_TO_BE_PROCESS;
    ret = ioctl(p_g2d_ctx->mG2dFd, G2D_CMD_MIXER_TASK, arg);
    if (ret < 0)
    {
       aloge("fatal error! G2D_CMD_MIXER_TASK fail!");
       goto _release_buf;
    }
#if 0
    ret = saveDstFrm(&p_g2d_ctx->mixertask);
    if (ret)
    {
        aloge("fatal error! save dst file fail!");
    }
#endif
_release_buf:
    releaseBuf(&p_g2d_ctx->mixertask);
    return 0;
}

static int SampleG2d_G2dConvert(SAMPLE_G2D_CTX *p_g2d_ctx)
{
    int ret = 0;

    SampleG2dConfig *pConfig = &p_g2d_ctx->mConfigPara;
    if(0 == pConfig->g2d_mod)
    {
        ret = SampleG2d_G2dConvert_rotate(p_g2d_ctx);
    }
    else if(1 == pConfig->g2d_mod)
    {
        ret = SampleG2d_G2dConvert_scale(p_g2d_ctx);
    }
    else if(2 == pConfig->g2d_mod)
    {
        ret = SampleG2d_G2dConvert_formatconversion(p_g2d_ctx);
    }
    else if (3 == pConfig->g2d_mod)
    {
        ret = SampleG2d_G2dBld(p_g2d_ctx);
    }
    else if (4 == pConfig->g2d_mod)
    {
        ret = SampleG2d_G2dMixter_Task(p_g2d_ctx);
    }

    return ret;
}


int main(int argc, char *argv[])
{
    int ret = 0;
    unsigned int size1 = 0;
    unsigned int size2 = 0; 
    unsigned int read_len = 0;
    unsigned int out_len = 0;
    SAMPLE_G2D_CTX g2d_ctx;
    SAMPLE_G2D_CTX *p_g2d_ctx = &g2d_ctx;

    memset(p_g2d_ctx,0,sizeof(SAMPLE_G2D_CTX)); 
    
    SampleG2dConfig *pConfig = &p_g2d_ctx->mConfigPara;
    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 0,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 1,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "IPC-");
    log_init(argv[0], &stGLogConfig);


    if(ParseCmdLine(argc, argv, &p_g2d_ctx->mCmdLinePara) != 0)
    {
        ret = -1;
        return ret;
    }
    char *pConfigFilePath = NULL; 
    if(strlen(p_g2d_ctx->mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = p_g2d_ctx->mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL;
    }

    if(loadSampleG2dConfig(&p_g2d_ctx->mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        ret = -1;
        return ret;
    }

    memset(&p_g2d_ctx->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    p_g2d_ctx->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&p_g2d_ctx->mSysConf);
    ret = AW_MPI_SYS_Init();
    if (ret < 0)
    {
        aloge("sys Init failed!");
        ret = -1;
        goto _exit;
    }

    g2d_MemOpen();

    if(pConfig->g2d_mod != 4)
    {
        ret = PrepareFrmBuff(p_g2d_ctx);
        if(0 != ret)
        {
            aloge("malloc frm buffer failed");
            ret = -1;
            goto _err1;
        }

        p_g2d_ctx->fd_in = fopen(p_g2d_ctx->mConfigPara.SrcFile,"r");
        if(NULL == p_g2d_ctx->fd_in)
        {
            aloge("open src file failed");
            ret = -1;
            goto _err2;
        }
        fseek(p_g2d_ctx->fd_in, 0, SEEK_SET);

        if (3 == pConfig->g2d_mod)
        {
            p_g2d_ctx->fd_out = fopen(p_g2d_ctx->mConfigPara.DstFile, "rb");
            if (NULL == p_g2d_ctx->fd_out)
            {
                aloge("open out file failed");
                ret = -1;
                goto _err2;
            }
            fseek(p_g2d_ctx->fd_out, 0, SEEK_SET);
        }
        else
        {
            p_g2d_ctx->fd_out = fopen(p_g2d_ctx->mConfigPara.DstFile, "wb");
            if (NULL == p_g2d_ctx->fd_out)
            {
                aloge("open out file failed");
                ret = -1;
                goto _err2;
            }
            fseek(p_g2d_ctx->fd_out, 0, SEEK_SET);
        }

        read_len = p_g2d_ctx->src_frm_info.frm_width * p_g2d_ctx->src_frm_info.frm_height;
        if(pConfig->mPicFormat == MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420|| pConfig->mPicFormat == MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420)
        {
            size1 = fread(p_g2d_ctx->src_frm_info.p_vir_addr[0] , 1, read_len, p_g2d_ctx->fd_in);
            if(size1 != read_len)
            {
                aloge("read_y_data_frm_src_file_invalid");
            }
            size2 = fread(p_g2d_ctx->src_frm_info.p_vir_addr[1], 1, read_len /2, p_g2d_ctx->fd_in);
            if(size2 != read_len/2)
            {
                aloge("read_c_data_frm_src_file_invalid");
            }

            fclose(p_g2d_ctx->fd_in);

            g2d_flushCache((void *)p_g2d_ctx->src_frm_info.p_vir_addr[0], read_len);
            g2d_flushCache((void *)p_g2d_ctx->src_frm_info.p_vir_addr[1], read_len/2);
        }
        else if(pConfig->mPicFormat ==  MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422|| pConfig->mPicFormat == MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422)
        {
            size1 = fread(p_g2d_ctx->src_frm_info.p_vir_addr[0] , 1, read_len, p_g2d_ctx->fd_in);
            if(size1 != read_len)
            {
                aloge("read_y_data_frm_src_file_invalid");
            }
            size2 = fread(p_g2d_ctx->src_frm_info.p_vir_addr[1], 1, read_len , p_g2d_ctx->fd_in);
            if(size2 != read_len)
            {
                aloge("read_c_data_frm_src_file_invalid");
            }

            fclose(p_g2d_ctx->fd_in);

            g2d_flushCache((void *)p_g2d_ctx->src_frm_info.p_vir_addr[0], read_len);
            g2d_flushCache((void *)p_g2d_ctx->src_frm_info.p_vir_addr[1], read_len);
        }
        else if (MM_PIXEL_FORMAT_RGB_888 == pConfig->mPicFormat)
        {
            size1 = fread(p_g2d_ctx->src_frm_info.p_vir_addr[0] , 1, read_len * 3, p_g2d_ctx->fd_in);
            fclose(p_g2d_ctx->fd_in);
            g2d_flushCache((void *)p_g2d_ctx->src_frm_info.p_vir_addr[0], read_len*3);
        }
        else if (MM_PIXEL_FORMAT_RGB_8888 == pConfig->mPicFormat)
        {
            size1 = fread(p_g2d_ctx->src_frm_info.p_vir_addr[0] , 1, read_len * 4, p_g2d_ctx->fd_in);
            fclose(p_g2d_ctx->fd_in);
            g2d_flushCache((void *)p_g2d_ctx->src_frm_info.p_vir_addr[0], read_len*4);
        }

        if (3 == p_g2d_ctx->mConfigPara.g2d_mod)
        {
            if (MM_PIXEL_FORMAT_RGB_888 == pConfig->mDstPicFormat)
            {
                int read_len = pConfig->mDstWidth * pConfig->mDstHeight * 3;
                fread(p_g2d_ctx->dst_frm_info.p_vir_addr[0], read_len, 1, p_g2d_ctx->fd_out);
                g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[0], read_len);
            }
            else if (MM_PIXEL_FORMAT_RGB_8888 == pConfig->mDstPicFormat)
            {
                int read_len = pConfig->mDstWidth * pConfig->mDstHeight * 4;
                fread(p_g2d_ctx->dst_frm_info.p_vir_addr[0], read_len, 1, p_g2d_ctx->fd_out);
                g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[0], read_len);
            }
            else if (MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420 == pConfig->mDstPicFormat || \
                MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420 == pConfig->mDstPicFormat)
            {
                int read_len = pConfig->mDstWidth * pConfig->mDstHeight;
                fread(p_g2d_ctx->dst_frm_info.p_vir_addr[0], read_len, 1, p_g2d_ctx->fd_out);
                fread(p_g2d_ctx->dst_frm_info.p_vir_addr[1], read_len/2, 1, p_g2d_ctx->fd_out);
                g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[0], read_len);
                g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[1], read_len/2);
            }
        }
    }
    // g2d related operations start ->
    ret = SampleG2d_G2dOpen(p_g2d_ctx);
    if (ret < 0)
    {
        aloge("fatal error! open /dev/g2d fail!");
        goto _err2;
    }
    ret = SampleG2d_G2dConvert(p_g2d_ctx);
    if (ret < 0)
    {
        aloge("fatal error! g2d convert fail!");
        goto _close_g2d;
    }
    // g2d related operations end <-
    if (3 == pConfig->g2d_mod)
    {
        FILE *fp = fopen("/mnt/extsd/g2d_bld_test.rgb", "wb+");
        if (fp)
        {
            alogd("save file size[%dx%d]", p_g2d_ctx->dst_frm_info.frm_width, \
                p_g2d_ctx->dst_frm_info.frm_height);
            if(pConfig->mDstPicFormat == MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420 ||
                    pConfig->mDstPicFormat == MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420)
            {
                out_len = p_g2d_ctx->dst_frm_info.frm_width * p_g2d_ctx->dst_frm_info.frm_height;
                g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[0], out_len);
                g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[1], out_len/2);

                fwrite(p_g2d_ctx->dst_frm_info.p_vir_addr[0], 1, out_len, fp);
                fwrite(p_g2d_ctx->dst_frm_info.p_vir_addr[1], 1, out_len/2, fp);
            }
            else if (pConfig->mDstPicFormat == MM_PIXEL_FORMAT_RGB_888)
            {
                out_len = p_g2d_ctx->dst_frm_info.frm_width * p_g2d_ctx->dst_frm_info.frm_height * 3;
                g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[0], out_len);
                fwrite(p_g2d_ctx->dst_frm_info.p_vir_addr[0], 1, out_len, fp);
            }
            else if (pConfig->mDstPicFormat == MM_PIXEL_FORMAT_RGB_8888)
            {
                out_len = p_g2d_ctx->dst_frm_info.frm_width * p_g2d_ctx->dst_frm_info.frm_height * 4;
                g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[0], out_len);
                fwrite(p_g2d_ctx->dst_frm_info.p_vir_addr[0], 1, out_len, fp);
            }
            fclose(fp);
            fp = NULL;
        }
        goto _close_g2d;
    }
    if(pConfig->g2d_mod != 4)
    {
        if(pConfig->mDstPicFormat == MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420 ||
                pConfig->mDstPicFormat == MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420)
        {
            out_len = p_g2d_ctx->dst_frm_info.frm_width * p_g2d_ctx->dst_frm_info.frm_height;
            g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[0], out_len);
            g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[1], out_len/2);

            fwrite(p_g2d_ctx->dst_frm_info.p_vir_addr[0], 1, out_len, p_g2d_ctx->fd_out);
            fwrite(p_g2d_ctx->dst_frm_info.p_vir_addr[1], 1, out_len/2, p_g2d_ctx->fd_out);
        }
        else if(pConfig->mDstPicFormat == MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422 || pConfig->mDstPicFormat == MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422)
        {
            out_len = p_g2d_ctx->dst_frm_info.frm_width * p_g2d_ctx->dst_frm_info.frm_height;
            g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[0], out_len);
            g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[1], out_len);

            fwrite(p_g2d_ctx->dst_frm_info.p_vir_addr[0], 1, out_len, p_g2d_ctx->fd_out);
            fwrite(p_g2d_ctx->dst_frm_info.p_vir_addr[1], 1, out_len, p_g2d_ctx->fd_out);
        }
        else if(pConfig->mDstPicFormat == MM_PIXEL_FORMAT_UYVY_PACKAGE_422 || pConfig->mDstPicFormat == MM_PIXEL_FORMAT_VYUY_PACKAGE_422 ||
                pConfig->mDstPicFormat == MM_PIXEL_FORMAT_YUYV_PACKAGE_422 || pConfig->mDstPicFormat == MM_PIXEL_FORMAT_YVYU_AW_PACKAGE_422)
        {
            out_len = p_g2d_ctx->dst_frm_info.frm_width * p_g2d_ctx->dst_frm_info.frm_height;
            g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[0], out_len*2);

            fwrite(p_g2d_ctx->dst_frm_info.p_vir_addr[0], 1, out_len*2, p_g2d_ctx->fd_out);
        }
        else if(pConfig->mDstPicFormat == MM_PIXEL_FORMAT_RGB_888)
        {
            out_len = p_g2d_ctx->dst_frm_info.frm_width * p_g2d_ctx->dst_frm_info.frm_height *3;
            g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[0], out_len);

            fwrite(p_g2d_ctx->dst_frm_info.p_vir_addr[0], 1, out_len, p_g2d_ctx->fd_out);
        }
        else if (MM_PIXEL_FORMAT_RGB_8888)
        {
            out_len = p_g2d_ctx->dst_frm_info.frm_width * p_g2d_ctx->dst_frm_info.frm_height *4;
            g2d_flushCache((void *)p_g2d_ctx->dst_frm_info.p_vir_addr[0], out_len);

            fwrite(p_g2d_ctx->dst_frm_info.p_vir_addr[0], 1, out_len, p_g2d_ctx->fd_out);
        }
    }

_close_g2d:
    SampleG2d_G2dClose(p_g2d_ctx);
_close_out_fd:
    if(p_g2d_ctx->fd_out > 0)
        fclose(p_g2d_ctx->fd_out);
_err2:
    FreeFrmBuff(p_g2d_ctx);
_err1:
    g2d_MemClose();
_mpp_exit:
    AW_MPI_SYS_Exit();
_exit:
    alogd("%s test result: %s", argv[0], ((0 == ret) ? "success" : "fail"));

    return 0;
}


