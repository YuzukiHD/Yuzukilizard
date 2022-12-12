//
// Copyright 2010 The Android Open Source Project
//
// The Display dispatcher.
//

//#define LOG_NDEBUG 0
#define LOG_TAG "sub_render_android"
#include <CDX_Debug.h>

#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <utils/Errors.h>


#include <binder/IInterface.h>
#include <binder/IPCThreadState.h>
#include <binder/Parcel.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
//#include <ui/Region.h>
//#include <ui/Rect.h>
//#include <ui/PixelFormat.h>
//#include <EGL/egl.h>
//#include <SkCanvas.h>
//#include <SkBitmap.h>
//#include <SkXfermode.h>
//#include <SkRegion.h>
//#include <SkTypeface.h>
//#include <SkGlyphCache.h>
//#include <SkUtils.h>
//#include <SkAutoKern.h>
//#include <cutils/properties.h>
//#include "unicode/ucnv.h"
//#include "unicode/ustring.h"
#include <CDX_CallbackType.h>
#include "subtitle_render_android.h"
#include "CDX_SubRender.h"
//#include "sub_total_inf.h"
//#include "subTotalInf.h"
#include "sdecoder.h"
//#include "subReader.h"

//#include "ui/DisplayInfo.h"
//
#include "CDX_Subtitle.h"
//#include "CDX_PlayerAPI.h"

#include <include_omx/OMX_Core.h>

//#define  BASELAYER                  8  //5, 3, higher number layer is on topper.
//#define  TOPBASELAYER				8  //5, 3
//#define  BOTTOMBASELAYER			8
//#define  LAYER_MULTIPLIER			10000
//#define  LAYER_OFFSET				1000
//#define  TRANSPARENT_COLOR			0x00050107
//#define  MAX_TEXTLINE				3
//#define  MAX_FONTHEIGHT				36
//#define  SUB_DISPLAY				0
//#define  FONT_SIZE_UNIT         	16
//#define  DEFAULT_TEXT_SIZE          24

#define  SUB_SHOW_NEW_VALID         1   //set new valid subinfo, not show or show at first.
#define  SUB_SHOW_OLD_VALID         2   //already show, and caller want to hide, but not hide because it is still in duration.
#define  SUB_SHOW_INVALID           0   //invalid item.
//#define  SUB_HAS_NONE_COLOR         0
//#define  SUB_HAS_DEFINED_COLOR      1
//#define  SUB_HAS_DEFINED_FONTSIZE   1
//#define  SUB_HAS_NONE_FONTSIZE      0
//#define  SUB_HAS_DEFINED_STYLE      1
//#define  SUB_HAS_NONE_STYLE         0
//#define  SUB_INIT_SET_FONT_INFO     1
//#define  SUB_USER_SET_FONT_INFO     2
//
//#define PROP_SUBTITLE_MAXFONTSIZE_KEY  "media.stagefright.maxsubfont"

namespace android 
{
//************************************************************************************************************//
//************************************************************************************************************//
//AMRWriter::AMRWriter
CedarXTimedTextRender::CedarXTimedTextRender(void *callback_info)
    :mCallbackInfo(callback_info)
{
	LOGV("CedarXTimedTextRender::CedarXTimedTextRender");
    //size_t  size;
    //size_t  size2;
    sp<CedarXTimedText>	  spCedarXTimedText;

    //size = mCedarXTimedTexts.size();
	LOGV("CedarXSubRender::CedarXSubRender2");
    spCedarXTimedText		= new CedarXTimedText();
	LOGV("CedarXSubRender::CedarXSubRender3");
	
	/*create main CedarXTimedText*/
	mCedarXTimedTexts.add(spCedarXTimedText);
    //size2 = mCedarXTimedTexts.size();
    //LOGD("(f:%s, l:%d) size[%d], size2[%d]", __FUNCTION__, __LINE__, size, size2);

    mCharset = SUBTITLE_TEXT_FORMAT_GBK;    //SUB_CHARSET_GBK;
}

CedarXTimedTextRender::~CedarXTimedTextRender()
{
	LOGV("CedarXTimedTextRender::~CedarXTimedTextRender!\n");
}

//int	CedarXSubRender::updateSubPara(sub_item_inf *sub_info) 
int	CedarXTimedTextRender::updateTimedTextPara(SubtitleItem *sub_info) 
{
	int			  	count = 0;  //mCedarXTimedTexts idx
	size_t			size;
    size_t          size2;
	SubtitleItem 	*current;
	sp<CedarXTimedText>	spCedarXTimedText;
	
	if(sub_info == NULL)
	{
		LOGW("Invalid sub information!\n");
		
		return  -1;
	}
	Mutex::Autolock autoLock(mLock);
	current = sub_info;
	
	size	= mCedarXTimedTexts.size();
	
	while(current != NULL)
	{   
		if(count >= (int)size)
		{
            spCedarXTimedText		= new CedarXTimedText();
	        spCedarXTimedText->setCharset(mCharset);
			/*create main CedarXTimedText*/
			mCedarXTimedTexts.add(spCedarXTimedText);
		}
        if(mCedarXTimedTexts[count]->getSubShowFlag() == SUB_SHOW_INVALID)
        {   
            mCedarXTimedTexts[count]->setSubInf(current);
		    //current = (SubtitleItem *)current->nextSubItem;
            current = NULL;
        }
		count++;
	}

//        size2	= mCedarXTimedTexts.size();
//        LOGD("(f:%s, l:%d), size[%d], size2[%d]", __FUNCTION__, __LINE__, size, size2);
	return NO_ERROR;
}
int CedarXTimedTextRender::CedarXTimedTextShow()
{
    //callback to cedarxPlayer
    size_t		    count;
    CedarXTimedText *pCedarXTimedText = NULL;

    Mutex::Autolock autoLock(mLock);
    count	= mCedarXTimedTexts.size();

    for(size_t i = 0;i < count;i++)
    {   
        if(mCedarXTimedTexts[i]->getSubShowFlag() == SUB_SHOW_NEW_VALID)
    	{   
            pCedarXTimedText = mCedarXTimedTexts[i].get();
            ((CedarXPlayerCallbackType*)mCallbackInfo)->callback(((CedarXPlayerCallbackType*)mCallbackInfo)->cookie,CDX_EVENT_TIMED_TEXT, (void*)pCedarXTimedText);
            mCedarXTimedTexts[i]->setSubShowFlag(SUB_SHOW_OLD_VALID);
        }
    }

    return NO_ERROR;
}


int CedarXTimedTextRender::CedarXTimedTextHide(unsigned int mediaTime, int* hasSubShowFlag)
{
    size_t		count;
	unsigned  int remainSubShowFlag;
    CedarXTimedText *pCedarXTimedText = NULL;

    Mutex::Autolock autoLock(mLock);
	count	= mCedarXTimedTexts.size();
	
    remainSubShowFlag = 0;

    if(mediaTime == 0xFFFFFFFF)
	{
        LOGV("(f:%s, l:%d) mediaTime = 0xFFFFFFFF, hide all [%d]subtitle", __FUNCTION__, __LINE__, count);
        for(size_t i = 0;i < count;i++)
	    {   
            mCedarXTimedTexts[i]->setSubShowFlag(SUB_SHOW_INVALID);
	    }
        ((CedarXPlayerCallbackType*)mCallbackInfo)->callback(((CedarXPlayerCallbackType*)mCallbackInfo)->cookie,CDX_EVENT_TIMED_TEXT, (void*)NULL);
    }
    else
    {
        LOGV("(f:%s, l:%d), subtitle count is [%d]", __FUNCTION__, __LINE__, count);
        if(1 == count)  //one subtitle, prefer to send NULL.
        {
            //LOGD("hehehaha: only one subtitle elem in array");
            if(mCedarXTimedTexts[0]->getSubShowFlag()!=SUB_SHOW_INVALID)
            {
                if(hasSubShowFlag)
                {
                    if((mCedarXTimedTexts[0]->endTime-200)<=mediaTime)
                    {   
                        LOGV("(f:%s, l:%d) mediaTime[%d]>=endTime[%d]-200, hide subtitle[%d]", __FUNCTION__, __LINE__, mediaTime, mCedarXTimedTexts[0]->endTime, 0);
                        mCedarXTimedTexts[0]->setSubShowFlag(SUB_SHOW_INVALID);
                        //pCedarXTimedText = mCedarXTimedTexts[0].get();
                        ((CedarXPlayerCallbackType*)mCallbackInfo)->callback(((CedarXPlayerCallbackType*)mCallbackInfo)->cookie,CDX_EVENT_TIMED_TEXT, (void*)NULL);
                    }
                    else
                    {   
                        LOGV("(f:%s, l:%d) mediaTime[%d]<endTime[%d]-200, remain subtitle[%d]", __FUNCTION__, __LINE__, mediaTime, mCedarXTimedTexts[0]->endTime, 0);
                        remainSubShowFlag = 1;
                        mCedarXTimedTexts[0]->setSubShowFlag(SUB_SHOW_OLD_VALID);
                    }
                }
                else    //hasSubShowFlag is NULL indicate that before draw, hide subtitle first.
                {
                    //pgs subtitle is bitmap, but its endtime is not accurate, so we process here. 
                    //we think bitmap subtitle can't has multi, so when subtitle thread want to hide before draw a new subtitle, we clear with no condition.
                    if(mCedarXTimedTexts[0]->subMode == 0)
                    {
                        LOGV("(f:%s, l:%d) bitmap subtitle is hidden before draw a new one", __FUNCTION__, __LINE__);
                        mCedarXTimedTexts[0]->setSubShowFlag(SUB_SHOW_INVALID);
                        ((CedarXPlayerCallbackType*)mCallbackInfo)->callback(((CedarXPlayerCallbackType*)mCallbackInfo)->cookie,CDX_EVENT_TIMED_TEXT, (void*)NULL);
                    }
                }
            }
        }
        else
        {
            for(size_t i = 0;i < count;i++)
		    {  
			   if(mCedarXTimedTexts[i]->getSubShowFlag()!=SUB_SHOW_INVALID)
			   {    
                    if((mCedarXTimedTexts[i]->endTime-200)<=mediaTime)
                    {   
                        LOGV("(f:%s, l:%d) mediaTime[%d]>=endTime[%d]-200, hide subtitle[%d]", __FUNCTION__, __LINE__, mediaTime, mCedarXTimedTexts[i]->endTime, i);
                        mCedarXTimedTexts[i]->setSubShowFlag(SUB_SHOW_INVALID);
                        pCedarXTimedText = mCedarXTimedTexts[i].get();
                        ((CedarXPlayerCallbackType*)mCallbackInfo)->callback(((CedarXPlayerCallbackType*)mCallbackInfo)->cookie,CDX_EVENT_TIMED_TEXT, (void*)pCedarXTimedText);
                    }
                    else
                    {   
                        LOGV("(f:%s, l:%d) mediaTime[%d]<endTime[%d]-200, remain subtitle[%d]", __FUNCTION__, __LINE__, mediaTime, mCedarXTimedTexts[i]->endTime, i);
                        remainSubShowFlag = 1;
                        mCedarXTimedTexts[i]->setSubShowFlag(SUB_SHOW_OLD_VALID);
                    }
               }
		    }
        }
    }
    
    if(hasSubShowFlag != NULL)
    {
        *hasSubShowFlag = remainSubShowFlag;
    }
    
    if(remainSubShowFlag == 1)
    {
        for(size_t i = 0;i < count;i++)
	    {  
//			   if(mCedarXSubs[i]->getSubShowFlag()!=SUB_SHOW_INVALID)
//			   {
//                  mCedarXSubs[i]->processSpecialEffect(mediaTime);
//               }
	    }
    }
	return  NO_ERROR;
}
int CedarXTimedTextRender::CedarXSubSetCharset(int charset)
{
	size_t		count;
	mCharset = charset;

    Mutex::Autolock autoLock(mLock);
	count	= mCedarXTimedTexts.size();
		
	for(size_t i = 0;i < count;i++)
	{
		mCedarXTimedTexts[i]->setCharset(charset);
	}
		
	return NO_ERROR;
}

int CedarXTimedTextRender::CedarXSubGetCharset()
{
    Mutex::Autolock autoLock(mLock);
	return mCedarXTimedTexts[0]->getCharset();
}
int CedarXTimedTextRender::GetEarliestEndItem(SubtitleItem *sub_info)
{
    Mutex::Autolock autoLock(mLock);
    size_t count = mCedarXTimedTexts.size();
    if(count <= 0)
    {
        return UNKNOWN_ERROR;
    }
    int nFlag = 0;
    unsigned int minEndTime;
    size_t minIndex;
    for(size_t i=0; i<count; i++)
    {
        if(mCedarXTimedTexts[i]->getSubShowFlag()!=SUB_SHOW_INVALID)
        {
            if(0 == nFlag)
            {
                minEndTime = mCedarXTimedTexts[i]->endTime;
                minIndex = i;
                nFlag = 1;
            }
            else
            {
                if(mCedarXTimedTexts[i]->endTime < minEndTime)
                {
                    minEndTime = mCedarXTimedTexts[i]->endTime;
                    minIndex = i;
                }
            }
        }
    }
    if(nFlag)
    {
        sub_info->nPts = (int64_t)mCedarXTimedTexts[minIndex]->startTime * 1000;
        sub_info->nDuration = (int64_t)(mCedarXTimedTexts[minIndex]->endTime - mCedarXTimedTexts[minIndex]->startTime) * 1000;
        return NO_ERROR;
    }
    else
    {
        return UNKNOWN_ERROR;
    }
}

CedarXTimedText::CedarXTimedText()
{
	subMode         = 1;
//    OMX_S32  startx;             // the invalid value is -1
//    OMX_S32  starty;             // the invalid value is -1
//    OMX_S32  endx;               // the invalid value is -1
//    OMX_S32  endy;               // the invalid value is -1
//    OMX_S32  subDispPos;         // the disply position of the subItem
    startTime       = 0;
    endTime         = 0;
    subTextBuf      = NULL;
    subBitmapBuf    = NULL;
    subTextLen      = 0;
    subPicWidth     = 0;
    subPicHeight    = 0;
    //mSubPicPixelFormat = PIXEL_FORMAT_UNKNOWN;  //PIXEL_FORMAT_RGBA_8888
    mSubPicPixelFormat = 0;
    //OMX_U8   alignment;          // the alignment of the subtitle
    //encodingType = SUB_ENCODING_UNKNOWN;
    mSubShowFlag = SUB_SHOW_INVALID;
    mCharset     = SUBTITLE_TEXT_FORMAT_GBK;    //SUB_CHARSET_GBK;
}
CedarXTimedText::~CedarXTimedText()
{
	if(subTextBuf)
    {
        free(subTextBuf);
        subTextBuf = NULL;
    }
    if(subBitmapBuf)
    {
        free(subBitmapBuf);
        subBitmapBuf = NULL;
    }
	
	LOGV("CedarXTimedText::~CedarXTimedText!\n");
}
int CedarXTimedText::setSubShowFlag(int subShowFlag)
{   
    mSubShowFlag = subShowFlag;
    return NO_ERROR;
}
int CedarXTimedText::getSubShowFlag()
{
    return mSubShowFlag;
}
int CedarXTimedText::setCharset(int Charset)
{
    mCharset = Charset;

    return NO_ERROR;
}
int CedarXTimedText::getCharset()
{
    return mCharset;
}
int CedarXTimedText::setSubInf(void *pInf)
{
    SubtitleItem *sub_info = (SubtitleItem*)pInf;
    if(mSubShowFlag != SUB_SHOW_INVALID)
    {
        LOGE("fatal error! why mSubShowFlag[%d] != SUB_SHOW_INVALID", mSubShowFlag);
    }
    if(sub_info == NULL)
	{
		LOGW("Invalid sub information!\n");
		return  -1;
	}
    mSubShowFlag       = SUB_SHOW_NEW_VALID;
    //subMode     = sub_info->bText ? SUB_MODE_TEXT : SUB_MODE_BITMAP;
    subMode     = sub_info->bText;
    startx      = sub_info->nStartX;
    starty      = sub_info->nStartY;
    endx        = sub_info->nEndX;
    endy        = sub_info->nEndY;
    //subDispPos  = sub_info->subDispPos;
    //nScreenWidth    = sub_info->nScreenWidth;
    //nScreenHeight   = sub_info->nScreenHeight;
    startTime   = sub_info->nPts/1000;
    endTime     = (sub_info->nPts + sub_info->nDuration)/1000;
    if(subMode == 1)
    {
        if(NULL == subTextBuf)
        {
            subTextBuf = (OMX_U8*)malloc(SUB_TEXT_BUFSIZE);
            if(NULL == subTextBuf)
            {
                ALOGE("(f:%s, l:%d) fatal error! malloc fail!", __FUNCTION__, __LINE__);
            }
        }
        memset(subTextBuf, 0, SUB_TEXT_BUFSIZE);
        memcpy(subTextBuf, sub_info->pText, sub_info->nTextLength);
        subTextLen     = sub_info->nTextLength;
        //convertUniCode(sub_info);
        LOGV("sub_text:%s, subTextLen[%d]", subTextBuf, subTextLen);
    }
    else
    {
        if(NULL == subBitmapBuf)
        {
            subBitmapBuf = (OMX_U8*)malloc(SUB_FRAME_BUFSIZE);
            if(NULL == subBitmapBuf)
            {
                ALOGE("(f:%s, l:%d) fatal error! malloc fail!", __FUNCTION__, __LINE__);
            }
        }
        memcpy(subBitmapBuf, sub_info->pBitmapData, SUB_FRAME_BUFSIZE);
        subPicWidth    = sub_info->nBitmapWidth;
        subPicHeight   = sub_info->nBitmapHeight;
        //mSubPicPixelFormat  = PIXEL_FORMAT_RGBA_8888;
        mSubPicPixelFormat  = 1;
    }
    //encodingType        = (char)sub_info->encodingType;
    //subHasFontInfFlag   = sub_info->subHasFontInfFlag;
    //fontStyle           = sub_info->fontStyle;
    fontSize            = sub_info->nFontSize;
    primaryColor        = sub_info->nPrimaryColor;
    secondaryColor      = sub_info->nSecondaryColor;

//    subStyle            = 0;
//    if (sub_info->bBold) {
//        subStyle |= SUB_STYLE_BOLD;
//    }
//    if (sub_info->bItalic) {
//        subStyle |= SUB_STYLE_ITALIC;
//    }
//    if (sub_info->bUnderlined) {
//        subStyle |= SUB_STYLE_UNDERLINE;
//    }
    return NO_ERROR;
}
//int CedarXSub::convertUniCode(sub_item_inf *sub_info)
int CedarXTimedText::convertUniCode(void *pInf)
{
    ALOGE("(f:%s, l:%d) fatal error! no icu4c, sorry!", __FUNCTION__, __LINE__);
    return INVALID_OPERATION;
    #if 0
	int			charset;
	const char* enc = NULL;
	sub_item_inf *sub_info = (sub_item_inf*)pInf;

	LOGV("******************CedarXTimedText::convertUniCode sub_info->encodingType = %d mCharset=%d",sub_info->encodingType,mCharset);
	charset	= sub_info->encodingType; //mapDecToRender(sub_info->encodingType);
	if(charset == SUB_CHARSET_UNKNOWN)  //if subtitle decoder don't know charset, then use app's setting.
	{
        charset = mCharset;
	}
    
	//LOGV("CedarXSub::convertUniCode charset = %d",charset);
	//LOGV("CedarXSub::convertUniCode!sub_info->subTextBuf = %s",sub_info->subTextBuf);
    switch (charset) 
    {
        case SUB_CHARSET_BIG5:
            enc = "Big5";
            break;
            
        case SUB_CHARSET_UTF_16LE:
        	enc = "UTF-16LE";
        	break;
        	
       	case SUB_CHARSET_UTF_32LE:
       		enc = "UTF-32LE";
       		break;
       		
       	case SUB_CHARSET_UTF_32BE:
       		enc = "UTF-32BE";
       		break;
       		
       	case SUB_CHARSET_GBK:
       		enc = "GBK";
       		break;
       		
        case SUB_CHARSET_UTF_16BE:
            enc = "UTF-16BE";
            break;
            
        case SUB_CHARSET_BIG5_HKSCS:
            enc = "Big5-HKSCS";
            break;
            
        case SUB_CHARSET_BOCU_1:
        	enc = "BOCU-1";
        	break;
        	
       	case SUB_CHARSET_CESU_8:
       		enc = "CESU-8";
       		break;
       		
       	case SUB_CHARSET_CP864:
       		enc = "cp864";
       		break;
       		
       	case SUB_CHARSET_EUC_JP:
       		enc = "EUC-JP";
       		break;
       		
        case SUB_CHARSET_EUC_KR:
            enc = "EUC-KR";
            break;
            
        case SUB_CHARSET_GB18030:
            enc = "GB18030";
            break;
            
        case SUB_CHARSET_HZ_GB_2312:
        	enc = "HZ-GB-2312";
        	break;
        	
       	case SUB_CHARSET_ISO_2022_CN:
       		enc = "ISO-2022-CN";
       		break;
       		
       	case SUB_CHARSET_ISO_2022_CN_EXT:
       		enc = "ISO-2022-CN-EXT";
       		break;
       		
       	case SUB_CHARSET_ISO_2022_JP:
       		enc = "ISO-2022-JP";
       		break;
       		
        case SUB_CHARSET_ISO_2022_KR:
            enc = "ISO-2022-KR";
            break;
            
        case SUB_CHARSET_ISO_8859_1:
            enc = "ISO-8859-1";
            break;
            
        case SUB_CHARSET_ISO_8859_10:
        	enc = "ISO-8859-10";
        	break;
        	
       	case SUB_CHARSET_ISO_8859_13:
       		enc = "ISO-8859-13";
       		break;
       		
       	case SUB_CHARSET_ISO_8859_14:
       		enc = "ISO-8859-14";
       		break;
       		
       	case SUB_CHARSET_ISO_8859_15:
       		enc = "ISO-8859-15";
       		break;
       		
        case SUB_CHARSET_ISO_8859_16:
            enc = "ISO-8859-16";
            break;     
            
        case SUB_CHARSET_ISO_8859_2:
            enc = "ISO-8859-2";
            break;
            
        case SUB_CHARSET_ISO_8859_3:
        	enc = "ISO-8859-3";
        	break;
        	
       	case SUB_CHARSET_ISO_8859_4:
       		enc = "ISO-8859-4";
       		break;
       		
       	case SUB_CHARSET_ISO_8859_5:
       		enc = "ISO-8859-5";
       		break;
       		
       	case SUB_CHARSET_ISO_8859_6:
       		enc = "ISO-8859-6";
       		break;
       		
        case SUB_CHARSET_ISO_8859_7:
            enc = "ISO-8859-7";
            break;
            
        case SUB_CHARSET_ISO_8859_8:
            enc = "Big5-HKSCS";
            break;
            
        case SUB_CHARSET_ISO_8859_9:
        	enc = "ISO-8859-9";
        	break;
        	
       	case SUB_CHARSET_KOI8_R:
       		enc = "KOI8-R";
       		break;
       		
       	case SUB_CHARSET_KOI8_U:
       		enc = "KOI8-U";
       		break;
       		
       	case SUB_CHARSET_MACINTOSH:
       		enc = "macintosh";
       		break;
       		
        case SUB_CHARSET_SCSU:
            enc = "SCSU";
            break;
            
        case SUB_CHARSET_SHIFT_JIS:
            enc = "Shift_JIS";
            break;
            
        case SUB_CHARSET_TIS_620:
        	enc = "TIS-620";
        	break;
        	
       	case SUB_CHARSET_US_ASCII:
       		enc = "US-ASCII";
       		break;
       		
       	case SUB_CHARSET_UTF_16:
       		enc = "UTF-16";
       		break;
       		
        case SUB_CHARSET_UTF_32:
            enc = "UTF-32";
            break;
            
        case SUB_CHARSET_UTF_7:
            enc = "UTF-7";
            break;
            
        case SUB_CHARSET_WINDOWS_1250:
        	enc = "windows-1250";
        	break;
        	
       	case SUB_CHARSET_WINDOWS_1251:
       		enc = "windows-1251";
       		break;
       		
       	case SUB_CHARSET_WINDOWS_1252:
       		enc = "windows-1252";
       		break;
       		
       	case SUB_CHARSET_WINDOWS_1253:
       		enc = "windows-1253";
       		break;
       		
        case SUB_CHARSET_WINDOWS_1254:
            enc = "windows-1254";
            break;       
  
       case SUB_CHARSET_WINDOWS_1255:
            enc = "windows-1255";
            break;
            
        case SUB_CHARSET_WINDOWS_1256:
        	enc = "windows-1256";
        	break;
        	
       	case SUB_CHARSET_WINDOWS_1257:
       		enc = "windows-1257";
       		break;
       		
       	case SUB_CHARSET_WINDOWS_1258:
       		enc = "windows-1258";
       		break;
       		
       	case SUB_CHARSET_X_DOCOMO_SHIFT_JIS_2007:
       		enc = "x-docomo-shift_jis-2007";
       		break;
       		
        case SUB_CHARSET_X_GSM_03_38_2000:
            enc = "x-gsm-03.38-2000";
            break;
            
        case SUB_CHARSET_X_IBM_1383_P110_1999:
            enc = "x-ibm-1383_P110-1999";
            break;
            
        case SUB_CHARSET_X_IMAP_MAILBOX_NAME:
        	enc = "x-IMAP-mailbox-name";
        	break;
        	
       	case SUB_CHARSET_X_ISCII_BE:
       		enc = "x-iscii-be";
       		break;
       		
       	case SUB_CHARSET_X_ISCII_DE:
       		enc = "x-iscii-de";
       		break;
       		
       	case SUB_CHARSET_X_ISCII_GU:
       		enc = "x-iscii-gu";
       		break;
       		
        case SUB_CHARSET_X_ISCII_KA:
            enc = "x-iscii-ka";
            break;
            
        case SUB_CHARSET_X_ISCII_MA:
            enc = "x-iscii-ma";
            break;
            
        case SUB_CHARSET_X_ISCII_OR:
        	enc = "x-iscii-or";
        	break;
        	
       	case SUB_CHARSET_X_ISCII_PA:
       		enc = "x-iscii-pa";
       		break;
       		
       	case SUB_CHARSET_X_ISCII_TA:
       		enc = "x-iscii-ta";
       		break;
       		
        case SUB_CHARSET_X_ISCII_TE:
            enc = "x-iscii-te";
            break;
            
        case SUB_CHARSET_X_ISO_8859_11_2001:
            enc = "x-iso-8859_11-2001";
            break;
            
        case SUB_CHARSET_X_JAVAUNICODE:
        	enc = "x-JavaUnicode";
        	break;
        	
       	case SUB_CHARSET_X_KDDI_SHIFT_JIS_2007:
       		enc = "x-kddi-shift_jis-2007";
       		break;
       		
       	case SUB_CHARSET_X_MAC_CYRILLIC:
       		enc = "x-mac-cyrillic";
       		break;
       		
       	case SUB_CHARSET_X_SOFTBANK_SHIFT_JIS_2007:
       		enc = "x-softbank-shift_jis-2007";
       		break;
       		
        case SUB_CHARSET_X_UNICODEBIG:
            enc = "x-UnicodeBig";
            break;                          
            
       case SUB_CHARSET_X_UTF_16LE_BOM:
       		enc = "x-UTF-16LE-BOM";
       		break;
       		
       	case SUB_CHARSET_X_UTF16_OPPOSITEENDIAN:
       		enc = "x-UTF16_OppositeEndian";
       		break;
       		
        case SUB_CHARSET_X_UTF16_PLATFORMENDIAN:
            enc = "x-UTF16_PlatformEndian";
            break;
            
        case SUB_CHARSET_X_UTF32_OPPOSITEENDIAN:
            enc = "x-UTF32_OppositeEndian";
            break;     
             
        default:
        	enc = "UTF-8";
        	break;
    }

    if (enc) 
    {
        UErrorCode status = U_ZERO_ERROR;

        UConverter *conv = ucnv_open(enc, &status);
        if (U_FAILURE(status)) 
        {
            LOGW("could not create UConverter for %s\n", enc);
            
            return -1;
        }
        
        UConverter *utf8Conv = ucnv_open("UTF-8", &status);
        if (U_FAILURE(status)) 
        {
            LOGW("could not create UConverter for UTF-8\n");
            
            ucnv_close(conv);
            return -1;
        }

        // first we need to untangle the utf8 and convert it back to the original bytes
        // since we are reducing the length of the string, we can do this in place
        const char* src = (const char*)sub_info->subTextBuf;
        //LOGV("CedarXTimedText::convertUniCode src = %s\n",src);
        int len = strlen((char *)src);
		//LOGV("CedarXTimedText::convertUniCode len = %d\n",len);
        // now convert from native encoding to UTF-8
        int targetLength = len * 3 + 1;
        if(targetLength > SUB_TEXT_BUFSIZE)
        {
            LOGW("(f:%s, l:%d) fatal error! targetLength[%d]>SUB_TEXT_BUFSIZE[%d], need cut!", __FUNCTION__, __LINE__, targetLength, SUB_TEXT_BUFSIZE);
            targetLength = SUB_TEXT_BUFSIZE;
        }
        memset(subTextBuf,0,SUB_TEXT_BUFSIZE);
        char* target = (char*)&subTextBuf[0];
        
		//LOGV("(f:%s, l:%d), target[%p], targetLength[%d], src[%p], len[%d]", __FUNCTION__, __LINE__, target, targetLength, src, len);
        ucnv_convertEx(utf8Conv, conv, &target, (const char*)target + targetLength,&src, (const char*)src + len, NULL, NULL, NULL, NULL, true, true, &status);
        if (U_FAILURE(status)) 
        {
            LOGW("ucnv_convertEx failed: %d\n", status);
            
            memset(subTextBuf,0,SUB_TEXT_BUFSIZE);
        } 
        subTextLen = target - (char*)subTextBuf;
        //LOGV("(f:%s, l:%d)_2, target[%p], targetLength[%d], src[%p]", __FUNCTION__, __LINE__, target, (int)(target - (char*)subTextBuf), src, (int)(src - (char*)sub_info->subTextBuf));
        LOGV("CedarXTimedText::convertUniCode src = %s,target = %s\n", sub_info->subTextBuf, subTextBuf);
        
//	        int targetlen2 = strlen((char *)subTextBuf);
//            LOGV("(f:%s, l:%d), targetLength2[%d], subTextLen[%d]", __FUNCTION__, __LINE__, targetlen2, subTextLen);
        
        ucnv_close(conv);
        ucnv_close(utf8Conv);
    }
    return NO_ERROR;
    #endif
}
} // namespace android

using namespace android;
extern "C" {

#if 0
void PrintSubItemInf(SubtitleItem *sub_info)
{
    int num = 0;
    SubtitleItem 	*current;
    LOGD("(f:%s, l:%d) subMode[%d], startx[%d], starty[%d], endx[%d], endy[%d], subDispPos[0x%x], nScreenWidth[%d], nScreenHeight[%d], \
    \n startTime[%d], endTime[%d], subBitmapBuf[0p%p], \
    \n subPicWidth[%d], subPicHeight[%d], alignment[%d], encodingType[%d], \
    \n nextSubItem[0p%p], dispBufIdx[%d], subScaleWidth[%d], subScaleHeight[%d], \
    \n fontStyle[%d], fontSize[%d], primaryColor[%d], secondaryColor[%d], \
    \n subStyle[%d], subEffectFlag[%d], effectStartxPos[%d], effectEndxPos[%d], effectStartyPos[%d], effectEndyPos[%d], \
    \n effectTimeDelay[%d], subKarakoEffectInf[0p%p]", 
    __FUNCTION__, __LINE__, sub_info->subMode, sub_info->startx, sub_info->starty, sub_info->endx, sub_info->endy, sub_info->subDispPos, sub_info->nScreenWidth, sub_info->nScreenHeight,
    sub_info->startTime, sub_info->endTime, sub_info->subBitmapBuf, 
    sub_info->subPicWidth, sub_info->subPicHeight, sub_info->alignment, sub_info->encodingType,
    sub_info->nextSubItem, sub_info->dispBufIdx, sub_info->subScaleWidth, sub_info->subScaleHeight,
    sub_info->fontStyle, sub_info->fontSize, sub_info->primaryColor, sub_info->secondaryColor,
    sub_info->subStyle, sub_info->subEffectFlag, sub_info->effectStartxPos, sub_info->effectEndxPos, sub_info->effectStartyPos, sub_info->effectEndyPos,
    sub_info->effectTimeDelay, sub_info->subKarakoEffectInf);
    if(sub_info->subMode == 1)
    {
        LOGD("subText:[%s]\n", sub_info->subTextBuf);
    }
    
    current = sub_info;
    while(current)
    {
        num++;
        current = (SubtitleItem *)current->nextSubItem;
    }
    LOGD("subtitle number is [%d]", num);

    current = (SubtitleItem *)sub_info->nextSubItem;
    num = 2;
    while(current)
    {
        LOGD("sub_sub_item[%d]:", num);
        LOGD("(f:%s, l:%d) subMode[%d], startx[%d], starty[%d], endx[%d], endy[%d], subDispPos[0x%x], nScreenWidth[%d], nScreenHeight[%d], \
        \n startTime[%d], endTime[%d], subBitmapBuf[0p%p], \
        \n subPicWidth[%d], subPicHeight[%d], alignment[%d], encodingType[%d], \
        \n nextSubItem[0p%p], dispBufIdx[%d], subScaleWidth[%d], subScaleHeight[%d], \
        \n fontStyle[%d], fontSize[%d], primaryColor[%d], secondaryColor[%d], \
        \n subStyle[%d], subEffectFlag[%d], effectStartxPos[%d], effectEndxPos[%d], effectStartyPos[%d], effectEndyPos[%d], \
        \n effectTimeDelay[%d], subKarakoEffectInf[0p%p]", 
        __FUNCTION__, __LINE__, current->subMode, current->startx, current->starty, current->endx, current->endy, current->subDispPos, current->nScreenWidth, current->nScreenHeight,
        current->startTime, current->endTime, current->subBitmapBuf, 
        current->subPicWidth, current->subPicHeight, current->alignment, current->encodingType,
        current->nextSubItem, current->dispBufIdx, current->subScaleWidth, current->subScaleHeight,
        current->fontStyle, current->fontSize, current->primaryColor, current->secondaryColor,
        current->subStyle, current->subEffectFlag, current->effectStartxPos, current->effectEndxPos, current->effectStartyPos, current->effectEndyPos,
        current->effectTimeDelay, current->subKarakoEffectInf);
        if(current->subMode == 1)
        {
            LOGD("subText:[%s]\n", current->subTextBuf);
        }
        current = (SubtitleItem *)current->nextSubItem;
        num++;
    }
}
#endif
//    int SubRenderGetScreenWidth();
//    int SubRenderGetScreenHeight();
//    int SubRenderGetScreenDirection();
//    int SubRenderSetTextColor(int color);
//    int SubRenderSetBackColor(int color);
//    int SubRenderSetCharset(int charset);
//	int SubRenderSetAlign(int align);
//	int SubRenderSetZorderTop();
//	int SubRenderSetZorderBottom();
//	int SubRenderSetFontStyle(int style);
//	int SubRenderSetFontSize(int fontsize);

//CedarXTimedTextRender* gCedarXTimedTextRender = NULL;

static bool checkCedarXTimedTextRenderUnitialized(CDX_SubRenderHAL* pHandle)
{
    if (pHandle == NULL) 
    {
        //LOGW("CedarXTimedTextRender not initialized.");
        
        return true;
    }
    
    return false;
}

CDX_SubRenderHAL* SubRenderCreate(void *callback_info)   //CedarXPlayerCallbackType
{
	LOGV("(f:%s, l:%d) SubRenderCreate!\n", __FUNCTION__, __LINE__);
	return new CedarXTimedTextRender(callback_info);
}

int SubRenderDestory(CDX_SubRenderHAL* pHandle)
{
    LOGV("(f:%s, l:%d) SubRenderDestory!\n", __FUNCTION__, __LINE__);
	if (checkCedarXTimedTextRenderUnitialized(pHandle)) 
	{
        return -1;
    } 
    LOGV("SubRenderDestory1!\n");
    
    CedarXTimedTextRender *pCedarXTimedTextRender = (CedarXTimedTextRender*)pHandle;
    delete pCedarXTimedTextRender;
    
    LOGV("SubRenderDestory2!\n");
    
    //endRenderSession();
    
    //return NO_ERROR;
    return OMX_ErrorNone;
}

int SubRenderDraw(CDX_SubRenderHAL* pHandle, SubtitleItem *sub_info)
{
    int ret;
    OMX_ERRORTYPE eError;
    LOGV("(f:%s, l:%d)\n", __FUNCTION__, __LINE__);
	if (checkCedarXTimedTextRenderUnitialized(pHandle))
	{
		LOGW("CedarXTimedTextRender not initalize!\n");
		
        return -1;
    } 
    CedarXTimedTextRender *pCedarXTimedTextRender = (CedarXTimedTextRender*)pHandle;
    //debug sub to bmp.
    {
        //PrintSubItemInf(sub_info);
    }
    //-----------------------------
    ret = pCedarXTimedTextRender->updateTimedTextPara(sub_info);
    switch(ret)
    {
    case NO_ERROR:
        eError = OMX_ErrorNone;
        break;
    default:
        eError = OMX_ErrorUndefined;
        break;
    }
    return eError;
}

int SubRenderShow(CDX_SubRenderHAL* pHandle)
{
    int ret;
    OMX_ERRORTYPE eError;
    LOGV("(f:%s, l:%d)\n", __FUNCTION__, __LINE__);
	if (checkCedarXTimedTextRenderUnitialized(pHandle)) 
	{
		LOGW("CedarXTimedTextRender not initalize!\n");
		
        return -1;
    } 
    CedarXTimedTextRender *pCedarXTimedTextRender = (CedarXTimedTextRender*)pHandle;
    ret = pCedarXTimedTextRender->CedarXTimedTextShow();
    switch(ret)
    {
    case NO_ERROR:
        eError = OMX_ErrorNone;
        break;
    default:
        eError = OMX_ErrorUndefined;
        break;
    }
    return eError;
}

int SubRenderHide(CDX_SubRenderHAL* pHandle, unsigned int mediaTime, int* hasSubShowFlag)
{
    int ret;
    OMX_ERRORTYPE eError;
	LOGV("(f:%s, l:%d)\n", __FUNCTION__, __LINE__);
	if (checkCedarXTimedTextRenderUnitialized(pHandle)) 
	{
		LOGW("[2]CedarXTimedTextRender not initalize!\n");
		
        return -1;
    } 
    CedarXTimedTextRender *pCedarXTimedTextRender = (CedarXTimedTextRender*)pHandle;
    ret = pCedarXTimedTextRender->CedarXTimedTextHide(mediaTime, hasSubShowFlag);
    switch(ret)
    {
    case NO_ERROR:
        eError = OMX_ErrorNone;
        break;
    default:
        eError = OMX_ErrorUndefined;
        break;
    }
    return eError;
}

//	int SubRenderSetTextColor(int color)
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int SubRenderGetTextColor()
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int SubRenderSetBackColor(int color)
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int SubRenderGetBackColor()
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int SubRenderSetFontSize(int fontsize)
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int SubRenderGetFontSize()
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int SubRenderSetPosition(int index,int posx,int posy)
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int SubRenderSetYPositionPercent(int index,int percent)
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int SubRenderGetPositionX(int index)
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int SubRenderGetPositionY(int index)
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int SubRenderGetWidth(int index)
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int SubRenderGetHeight(int index)
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

int SubRenderSetCharset(CDX_SubRenderHAL* pHandle, int charset)
{
    int ret;
    OMX_ERRORTYPE eError;
	LOGV("(f:%s, l:%d) charset[%d]!\n", __FUNCTION__, __LINE__, charset);
    if (checkCedarXTimedTextRenderUnitialized(pHandle)) 
	{
		//LOGW("cedarXSubRender not initalize!\n");
		
        return OMX_ErrorUndefined;
    } 
    
	if(charset == -1)
	{
		return OMX_ErrorUndefined;
	}
    CedarXTimedTextRender *pCedarXTimedTextRender = (CedarXTimedTextRender*)pHandle;
	//gFontCharset = charset;

    ret = pCedarXTimedTextRender->CedarXSubSetCharset(charset);
    switch(ret)
    {
    case NO_ERROR:
        eError = OMX_ErrorNone;
        break;
    default:
        eError = OMX_ErrorUndefined;
        break;
    }
    return eError;
}

int SubRenderGetCharset(CDX_SubRenderHAL* pHandle)
{
	LOGV("(f:%s, l:%d)!\n", __FUNCTION__, __LINE__);
    if (checkCedarXTimedTextRenderUnitialized(pHandle)) 
	{
		//LOGW("cedarXSubRender not initalize!\n");
		
        return -1;
    } 
    CedarXTimedTextRender *pCedarXTimedTextRender = (CedarXTimedTextRender*)pHandle;
    return pCedarXTimedTextRender->CedarXSubGetCharset();
}

//	int SubRenderGetAlign()
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int SubRenderSetAlign(int align)
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int SubRenderSetZorderTop()
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int SubRenderSetZorderBottom()
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int   SubRenderSetFontStyle(int style)
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//	int   SubRenderGetFontStyle()
//	{
//		LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}

//    int SubRenderGetScreenWidth()
//	{   
//        LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}
//
//    int SubRenderGetScreenDirection()
//    {
//    	LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//    }
//
//	int SubRenderGetScreenHeight()
//	{
//        LOGV("(f:%s, l:%d) obsolete!\n", __FUNCTION__, __LINE__);
//		return 0;
//	}
int SubRenderGetEarliestEndItem(CDX_SubRenderHAL* pHandle, SubtitleItem *sub_info)
{
    int ret;
    OMX_ERRORTYPE eError;
	LOGV("(f:%s, l:%d)", __FUNCTION__, __LINE__);
    if (checkCedarXTimedTextRenderUnitialized(pHandle)) 
	{
		//ALOGW("cedarXSubRender not initalize!\n");
        return OMX_ErrorUndefined;
    } 
    CedarXTimedTextRender *pCedarXTimedTextRender = (CedarXTimedTextRender*)pHandle;
	//gFontCharset = charset;

    ret = pCedarXTimedTextRender->GetEarliestEndItem(sub_info);
    switch(ret)
    {
    case NO_ERROR:
        eError = OMX_ErrorNone;
        break;
    default:
        eError = OMX_ErrorUndefined;
        break;
    }
    return eError;
}
}

