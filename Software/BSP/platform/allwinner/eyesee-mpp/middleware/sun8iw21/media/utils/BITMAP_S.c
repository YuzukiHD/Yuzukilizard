//#define LOG_NDEBUG 0
#define LOG_TAG "BITMAP_S"
#include <utils/plat_log.h>

#include "plat_errno.h"
#include "plat_type.h"

#include <BITMAP_S.h>

#define SWAP(px, py)        \
    do {                    \
        unsigned int tmp;   \
        tmp = *(px);        \
        *(px) = *(py);      \
        *(py) = tmp;        \
    } while (0)

int BITMAP_S_GetdataSize(PARAM_IN const BITMAP_S *pBitmap)
{
    int nSize = 0;
    if(NULL == pBitmap)
    {
        aloge("fatal error! pBitmap == NULL, return!");
        return 0;
    }
    switch(pBitmap->mPixelFormat)
    {
        case MM_PIXEL_FORMAT_RGB_1555:
        case MM_PIXEL_FORMAT_RGB_4444:
            nSize = pBitmap->mWidth * pBitmap->mHeight * 2;
            break;
        case MM_PIXEL_FORMAT_RGB_8888:
            nSize = pBitmap->mWidth * pBitmap->mHeight * 4;
            break;
        default:
            aloge("fatal error! unknown pixel format[0x%x]", pBitmap->mPixelFormat);
            break;
    }
    return nSize;
}

ERRORTYPE BITMAP_S_FlipData(PARAM_INOUT BITMAP_S *pBitmap, PARAM_IN BMP_FLIP_FLAG_E flip_dir)
{
    ERRORTYPE ret = SUCCESS;
    int width = pBitmap->mWidth;
    int height = pBitmap->mHeight;

    if (flip_dir < BMP_FLIP_NONE || flip_dir > BMP_FLIP_BOTH_FLIP)
    {
        aloge("fatal error! unknown flip direction[%d]!", flip_dir);
        ret = FAILURE;
        return ret;
    }

    switch (pBitmap->mPixelFormat)
    {
        case MM_PIXEL_FORMAT_RGB_1555:
        {
            unsigned short *data = pBitmap->mpData;
            if (flip_dir & BMP_FLIP_HFLIP)
            {
                for (int i = 0; i < height; i++)
                {
                    for (int j = 0; j < width/2; j++)
                        SWAP(data+i*width+j, data+(i+1)*width-1-j);
                }
            }
            if (flip_dir & BMP_FLIP_VFLIP)
            {
                for (int i = 0; i < width; i++)
                {
                    for (int j = 0; j < height/2; j++)
                        SWAP(data+j*width+i, data+(height-j-1)*width+i);
                }
            }
            break;
        }
        case MM_PIXEL_FORMAT_RGB_8888:
        {
            unsigned int *data = pBitmap->mpData;
            if (flip_dir & BMP_FLIP_HFLIP)
            {
                for (int i = 0; i < height; i++)
                {
                    for (int j = 0; j < width/2; j++)
                        SWAP(data+i*width+j, data+(i+1)*width-1-j);
                }
            }
            if (flip_dir & BMP_FLIP_VFLIP)
            {
                for (int i = 0; i < width; i++)
                {
                    for (int j = 0; j < height/2; j++)
                        SWAP(data+j*width+i, data+(height-j-1)*width+i);
                }
            }
            break;
        }
        default:
        {
            aloge("fatal error! unknown pixel format[0x%x]", pBitmap->mPixelFormat);
            ret = FAILURE;
            break;
        }
    }

    return ret;
}
