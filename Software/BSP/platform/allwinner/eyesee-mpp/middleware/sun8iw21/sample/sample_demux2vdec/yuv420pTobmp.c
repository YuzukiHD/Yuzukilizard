//#define LOG_NDEBUG 0
#define LOG_TAG "YUV420pToBmp"
 
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "plat_log.h"


#define DEFAULT_BMP_SAVE_PATH  "/mnt/extsd/yuv_bmp_0.bmp"


#define GETR(y,u,v) ((y) + ((v)-128) + ((((v)-128)*103)>>8) )
#define GETG(y,u,v) ((y) - ( ((((u)-128)*88)>>8) + ((((v)-128)*183)>>8) ))
#define GETB(y,u,v) ((y) + ((u)-128) + ((((u)-128)*198)>>8))

#define CLIP(v)  ( ((v)<0) ? 0 : (((v)>255) ? 255 : (v)) )

#define COMBRGBValue(r,g, b) ( ((r)<<16) | ((g) << 8) | ((b) << 0) )


typedef unsigned char BYTE;
typedef int LONG;
typedef unsigned int DWORD;
typedef unsigned short WORD;


typedef struct {
    WORD bfType;
    DWORD bfSize;
    WORD  bfReserved1;
    WORD  bfReserved2;
    DWORD bfOffBits;
}BMPFILEHEADER_T; //14

typedef struct {
    DWORD biSize;
    LONG  biWidth;
    LONG  biHeight;
    WORD  biPlanes;
    WORD  biBitCount;
    DWORD  biCompression;
    DWORD  biSizeImage;
    LONG  biXPelsPerMeter;
    LONG  biYPelsPerMeter;
    DWORD  biClrUsed;
    DWORD  biClrImportant;
}BMPINFOHEADER_T; //40

typedef struct
{
    BYTE   rgbBlue;
    BYTE   rgbGreen;
    BYTE   rgbRed;
    BYTE   rgbReserved;
}RGBMIXPLATE_T;


static int saveRGB2BMP(unsigned char *pRgbData, FILE *fd, int width, int height, int bpp)
{
    int size;
    BMPFILEHEADER_T bfh = {0};
    BMPINFOHEADER_T bih = {0};

    if (fd == NULL)
    {
        aloge("bmp out fd null");
        return -1;
    }

    size = width * height * bpp/8;

    bfh.bfType = (WORD)0x4d42;
    bfh.bfOffBits = 54;
    //bfh.bfSize = size + sizeof(BMPFILEHEADER_T) + sizeof(BMPINFOHEADER_T);
    bfh.bfSize = size + bfh.bfOffBits;
    bfh.bfReserved1 = 0;
    bfh.bfReserved2 = 0;

    bih.biSize = 40;//sizeof(BMPINFOHEADER_T);
    bih.biWidth = width;
    bih.biHeight = -height;//height;
    bih.biPlanes = 1;
    bih.biBitCount = bpp;//24;
    bih.biCompression = 0;
    bih.biSizeImage = size;
    bih.biXPelsPerMeter = 0;
    bih.biYPelsPerMeter = 0;
    bih.biClrUsed = 0;
    bih.biClrImportant = 0;

#if 0 //size error
    fwrite(&bfh, sizeof(BMPFILEHEADER_T), 1, fd);
    fwrite(&bih, sizeof(BMPINFOHEADER_T),1, fd);
#else
    fwrite(&bfh.bfType, 1, sizeof(bfh.bfType), fd);
    fwrite(&bfh.bfSize, 1, sizeof(bfh.bfSize), fd);
    fwrite(&bfh.bfReserved1, 1, sizeof(bfh.bfReserved1), fd);
    fwrite(&bfh.bfReserved2, 1, sizeof(bfh.bfReserved2), fd);
    fwrite(&bfh.bfOffBits, 1, sizeof(bfh.bfOffBits), fd);

    fwrite(&bih.biSize, 1, sizeof(bih.biSize), fd);
    fwrite(&bih.biWidth, 1, sizeof(bih.biWidth), fd);
    fwrite(&bih.biHeight, 1, sizeof(bih.biHeight), fd);
    fwrite(&bih.biPlanes, 1, sizeof(bih.biPlanes), fd);
    fwrite(&bih.biBitCount, 1, sizeof(bih.biBitCount), fd);
    fwrite(&bih.biCompression, 1, sizeof(bih.biCompression), fd);
    fwrite(&bih.biSizeImage, 1, sizeof(bih.biSizeImage), fd);
    fwrite(&bih.biXPelsPerMeter, 1, sizeof(bih.biXPelsPerMeter), fd);
    fwrite(&bih.biYPelsPerMeter, 1, sizeof(bih.biYPelsPerMeter), fd);
    fwrite(&bih.biClrUsed, 1, sizeof(bih.biClrUsed), fd);
    fwrite(&bih.biClrImportant, 1, sizeof(bih.biClrImportant), fd);
#endif

    fwrite(pRgbData, size, 1, fd);
    //fclose(fd);

    ALOGD("bmp file has done\n");

    return 0;
}

static void YUV420pToRGB888(unsigned char *pRgb888Data, int w, int h, unsigned char *pY, unsigned char *pU, unsigned char *pV)
{
    unsigned char y, u, v;
    unsigned char r, g, b;
    unsigned int row, col;
    int off = 0, pos;

    for (row=0; row<h; row++)
    {
        off = row * w;

        for (col=0; col < w; col++)
        {
             y = *(pY + off + col);
             pos = (row/2) * (w/2) + col/2;
             u = *(pU + pos);
             v = *(pV + pos);

             r = CLIP(GETR(y,u,v));
             g = CLIP(GETG(y,u,v));
             b = CLIP(GETB(y,u,v));

             *pRgb888Data++ = b;
             *pRgb888Data++ = g;
             *pRgb888Data++ = r;
        }
    }
}

static unsigned int get_file_size(const char *path)
{
    unsigned long filesize = -1;
    struct stat statbuff;

    if (stat(path, &statbuff) < 0)
    {
        return filesize;
    }
    else
    {
        filesize = statbuff.st_size;
    }

    return filesize;
} 

enum {
    PIX_FMT_YUV420P = 0,
    PIX_FMT_YVU420P,
};

typedef struct yuv2bmp_t
{
    char yuv_src_path[256];
    char bmp_dst_path[256];
    FILE *yuv_fd;
    FILE *bmp_fd;

    int src_yuv_pixfmt; //yuv420p or yvu420p

    int width;
    int height;

    int total_frame;
    int get_frame;

    unsigned char *pY;
    unsigned char *pU;
    unsigned char *pV;

    unsigned char *pRGB888;
}YUV2BMP_T;

static YUV2BMP_T s_yuv_para;


int main(int argc, char **argv)
{
    int i, ret;
    int size, size2;
    int file_size;

    memset(&s_yuv_para, 0, sizeof(s_yuv_para));

    if (argc < 2)
    {
        alogd("error call para\n");
        return -1;
    }

    argv++;
    while (*argv)
    {
        if (!strcmp(*argv, "-i"))
        {
            argv++;
            if (*argv)
            {
                strcpy(s_yuv_para.yuv_src_path, *argv);
            }
        }
        else if (!strcmp(*argv, "-o"))
        {
            argv++;
            if (*argv)
            {
                strcpy(s_yuv_para.bmp_dst_path, *argv);
            }
        }
        else if (!strcmp(*argv, "-w"))
        {
            argv++;
            if (*argv)
            {
                s_yuv_para.width = atoi(*argv);
            }
        }
        else if (!strcmp(*argv, "-h"))
        {
            argv++;
            if (*argv)
            {
                s_yuv_para.height = atoi(*argv);
            }
        }
        else if (!strcmp(*argv, "-p"))
        {
            argv++;
            if (*argv)
            {
                s_yuv_para.src_yuv_pixfmt = atoi(*argv);
            }
        }
        else if (!strcmp(*argv, "-g"))
        {
            argv++;
            if (*argv)
            {
                s_yuv_para.get_frame = atoi(*argv);
            }
        }
        else if (!strcmp(*argv, "-n"))
        {
            argv++;
            if (*argv)
            {
                s_yuv_para.total_frame = atoi(*argv);
            }
        }
        else if (!strcmp(*argv, "-help"))
        {
            alogd("usage:\n"
               "\t-i: yuv src file path\n"
               "\t-o: bmp output file path\n"
               "\t-w: src frame width\n"
               "\t-h: src frame height\n"
               "\t-g: to get src frame no, from 0\n"
               "\t-n: src file total frame number\n"
               "\t-example_2: yuv2bmp -i /mnt/extsd/720p.yuv -o 720p.bmp -n 10 -w 1280 -h 720 -g 0\n"
              );

            return -1;
        }
        else if (*argv)
        {
            argv++;
        }
    }

    s_yuv_para.yuv_fd = fopen(s_yuv_para.yuv_src_path, "r");
    if (s_yuv_para.yuv_fd == NULL)
    {
        aloge("open yuv src file[%s] fail", s_yuv_para.yuv_src_path);
        goto err_out_0;
    }

    if (!s_yuv_para.bmp_dst_path[0])
    {
        strcpy(s_yuv_para.bmp_dst_path, s_yuv_para.yuv_src_path);
        int len = strlen(s_yuv_para.bmp_dst_path);
        s_yuv_para.bmp_dst_path[len-4] = 0;
        sprintf(s_yuv_para.bmp_dst_path, "%s_%d.bmp", s_yuv_para.bmp_dst_path, s_yuv_para.get_frame);
    }

    alogd("bmp dst file:%s", s_yuv_para.bmp_dst_path);
    s_yuv_para.bmp_fd = fopen(s_yuv_para.bmp_dst_path, "wb");
    if (s_yuv_para.bmp_fd == NULL)
    {
        aloge("create bmp dst file fail");
        goto err_out_1;
    }

    if (s_yuv_para.width <= 0)
    {
        s_yuv_para.width = 1920;
    }
    if (s_yuv_para.height <= 0)
    {
        s_yuv_para.height = 1080;
    }
    size = s_yuv_para.width * s_yuv_para.height;
    s_yuv_para.pY = (unsigned char *)malloc(size);
    if (s_yuv_para.pY == NULL)
    {
        aloge("malloc pY buffer fail");
        goto err_out_2;
    }

    s_yuv_para.pU = (unsigned char *)malloc(size/4);
    if (s_yuv_para.pU == NULL)
    {
        aloge("malloc pY buffer fail");
        goto err_out_3;
    }

    s_yuv_para.pV = (unsigned char *)malloc(size/4);
    if (s_yuv_para.pV == NULL)
    {
        aloge("malloc pY buffer fail");
        goto err_out_4;
    }

    s_yuv_para.pRGB888 = (unsigned char *)malloc(size*3);
    if (s_yuv_para.pRGB888 == NULL)
    {
        aloge("malloc pRGB888 buffer fail");
        goto err_out_5;
    }

    file_size = get_file_size(s_yuv_para.yuv_src_path);
    if ((file_size == (unsigned )-1) || !file_size)
    {
        aloge("yuv src fail not exsit or file size=0");
        goto err_out_6;
    }

    size2 = size + size / 2;
    int frame_num = file_size / size2;
    alogd("frame_num=%d, f_size=%ld", frame_num, file_size);
    if (frame_num != s_yuv_para.total_frame)
    {
        s_yuv_para.total_frame = frame_num;
    }

    if (s_yuv_para.total_frame < s_yuv_para.get_frame)
    {
        s_yuv_para.get_frame = 0;
    }

    int get_offset = size2 * s_yuv_para.get_frame; //start from frame 0

    fseek(s_yuv_para.yuv_fd, get_offset, SEEK_SET);
    fread(s_yuv_para.pY, 1, size, s_yuv_para.yuv_fd);
    if (s_yuv_para.src_yuv_pixfmt == PIX_FMT_YUV420P)
    {
        fread(s_yuv_para.pU, 1, size/4, s_yuv_para.yuv_fd);
        fread(s_yuv_para.pV, 1, size/4, s_yuv_para.yuv_fd);
    }
    else
    {
        fread(s_yuv_para.pV, 1, size/4, s_yuv_para.yuv_fd);
        fread(s_yuv_para.pU, 1, size/4, s_yuv_para.yuv_fd);
    }

    YUV420pToRGB888(s_yuv_para.pRGB888, s_yuv_para.width, s_yuv_para.height, s_yuv_para.pY, s_yuv_para.pU, s_yuv_para.pV);

    fseek(s_yuv_para.bmp_fd, 0, SEEK_SET);
    saveRGB2BMP(s_yuv_para.pRGB888, s_yuv_para.bmp_fd, s_yuv_para.width, s_yuv_para.height, 24);

err_out_6:
    if (s_yuv_para.pRGB888 != NULL)
    {
        free(s_yuv_para.pRGB888);
        s_yuv_para.pRGB888 = NULL;
    }
err_out_5:
    if (s_yuv_para.pV != NULL)
    {
        free(s_yuv_para.pV);
        s_yuv_para.pV = NULL;
    }
err_out_4:
    if (s_yuv_para.pU != NULL)
    {
        free(s_yuv_para.pU);
        s_yuv_para.pU = NULL;
    }
err_out_3:
    if (s_yuv_para.pY != NULL)
    {
        free(s_yuv_para.pY);
        s_yuv_para.pY = NULL;
    }
err_out_2:
    if (s_yuv_para.bmp_fd != NULL)
    {
        fclose(s_yuv_para.bmp_fd);
        s_yuv_para.bmp_fd = NULL;
    }
err_out_1:
    if (s_yuv_para.yuv_fd != NULL)
    {
        fclose(s_yuv_para.yuv_fd);
        s_yuv_para.yuv_fd = NULL;
    }
err_out_0:

    return 0;
}
