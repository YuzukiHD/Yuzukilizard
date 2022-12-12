/************************************************************************************************/
/* Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.                                          */
/************************************************************************************************/
/**
 * @file aw_osd.c
 * @brief
 * @author id: guixing
 * @version v0.1
 * @date 2016-08-28
 */

/************************************************************************************************/
/*                                      Include Files                                           */
/************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "rgb_ctrl.h"


/************************************************************************************************/
/*                                     Macros & Typedefs                                        */
/************************************************************************************************/
#define FONT_PATH_PREFFIX "/usr/share/osd/fonts/"

#define GB2312_FONT_16_PATH    FONT_PATH_PREFFIX "hzk16.lz4"
#define GB2312_FONT_32_PATH    FONT_PATH_PREFFIX "hzk32.lz4"
#define GB2312_FONT_64_PATH    FONT_PATH_PREFFIX "hzk64.lz4"


#define ASCII_FONT_16_PATH     FONT_PATH_PREFFIX "asc16.lz4"
#define ASCII_FONT_32_PATH     FONT_PATH_PREFFIX "asc32.lz4"
#define ASCII_FONT_64_PATH     FONT_PATH_PREFFIX "asc64.lz4"

#define ALIGN(x, a)            ((a) * (((x) + (a) - 1) / (a)))


typedef int LONG;
typedef unsigned int DWORD;
typedef unsigned short WORD;


/************************************************************************************************/
/*                                    Structure Declarations                                    */
/************************************************************************************************/
//typedef union {
//    struct rgb {
//	    unsigned char blue;
//	    unsigned char green;
//	    unsigned char red;
//	};
//}RGB888;
//
//typedef union {
//    unsigned int argb8888;
//    struct argb {
//	    unsigned char blue;
//	    unsigned char green;
//	    unsigned char red;
//        unsigned char alpha;
//	};
//}ARGB8888;

#pragma pack(2)
typedef struct {
        WORD    bfType;
        DWORD   bfSize;
        WORD    bfReserved1;
        WORD    bfReserved2;
        DWORD   bfOffBits;
} BMPFILEHEADER_T;

#pragma pack(2)
typedef struct{
        DWORD      biSize;
        LONG       biWidth;
        LONG       biHeight;
        WORD       biPlanes;
        WORD       biBitCount;
        DWORD      biCompression;
        DWORD      biSizeImage;
        LONG       biXPelsPerMeter;
        LONG       biYPelsPerMeter;
        DWORD      biClrUsed;
        DWORD      biClrImportant;
} BMPINFOHEADER_T;


/************************************************************************************************/
/*                                      Global Variables                                        */
/************************************************************************************************/
static unsigned char *g_asc_16 = NULL; // bytes 128 x 16 x 1 size 8x16
static unsigned char *g_asc_20 = NULL; // bytes 128 x 20 x 2 size 10x20
static unsigned char *g_asc_24 = NULL; // bytes 128 x 24 x 2 size 12x24
static unsigned char *g_asc_32 = NULL; // bytes 128 x 32 x 2 size 16x32
static unsigned char *g_asc_40 = NULL; // bytes 128 x 40 x 3 size 20x40
static unsigned char *g_asc_64 = NULL; // bytes 128 x 64 x 4 size 64x64


static unsigned char *g_gb2312_16 = NULL; // bytes 87 x 94 x 16 x 2 size 16x16
static unsigned char *g_gb2312_20 = NULL; // bytes 87 x 94 x 20 x 3 size 20x20
static unsigned char *g_gb2312_24 = NULL; // bytes 87 x 94 x 24 x 3 size 24x24
static unsigned char *g_gb2312_32 = NULL; // bytes 87 x 94 x 32 x 4 size 32x32
static unsigned char *g_gb2312_40 = NULL; // bytes 87 x 94 x 40 x 5 size 40x40
static unsigned char *g_gb2312_64 = NULL; // bytes 87 x 94 x 64 x 8 size 64x64



/************************************************************************************************/
/*                                    Function Declarations                                     */
/************************************************************************************************/
extern size_t decompress_file_to_mem(FILE *in, char **outMem);


/************************************************************************************************/
/*                                     Function Definitions                                     */
/************************************************************************************************/
int create_rectangle_rgb(RGB_PIC_S *rgb_pic)
{
    int size = 0;
    int byte_size = 0;
    int i = 0, addr_cnt = 0;
    int w_cnt = 0, h_cnt = 0;
    int w_tmp = 0, h_tmp = 0;

    if (NULL == rgb_pic) {
        RGB_ERR_PRT("Input rgb_pic is NULL!\n");
        return -1;
    }

    if (rgb_pic->enable_mosaic && rgb_pic->mosaic_size <= 0) {
        RGB_ERR_PRT("When enable_mosaic=1 so must rgb_pic->mosaic_size > 0 is ok! mosaic_size:%d\n",
                                                         rgb_pic->mosaic_size);
        return -1;
    }

    switch (rgb_pic->rgb_type)
    {
        case OSD_RGB_24:
            byte_size = 3;
            size = rgb_pic->wide * rgb_pic->high * 3;
            break;

        case OSD_RGB_555:
        case OSD_RGB_565:
            RGB_DB_PRT("Don't support this type:%d !\n", rgb_pic->rgb_type);
            return -1;
            break;

        case OSD_RGB_32:
            byte_size = 4;
            size = rgb_pic->wide * rgb_pic->high * 4;
            break;

        default:
            RGB_ERR_PRT("Input rgb_type:%d error!\n", rgb_pic->rgb_type);
            return -1;
            break;
    }

    rgb_pic->pic_size = 0;
    rgb_pic->pic_addr = NULL;
    rgb_pic->pic_addr = (char *)malloc(size);
    if (NULL == rgb_pic->pic_addr) {
        RGB_ERR_PRT("Do malloc size:%d fail! error:%d  %s\n", size, errno, strerror(errno));
        return -1;
    }
    rgb_pic->pic_size = size;

    if (0 == rgb_pic->enable_mosaic) {
        /* Draw normal picture */
        for (addr_cnt = 0; addr_cnt < size; ) {
            for (i = 0; i < byte_size; i++) {
                rgb_pic->pic_addr[addr_cnt++] = rgb_pic->background[i];
            }
        }
    } else {
        /* Draw mosaic picture */
        for (h_cnt = 0; h_cnt < rgb_pic->high; h_cnt++) {
            h_tmp = h_cnt % (rgb_pic->mosaic_size * 2);
            if (h_tmp < rgb_pic->mosaic_size) {
                for (w_cnt = 0; w_cnt < rgb_pic->wide; w_cnt++) {
                    w_tmp = w_cnt % (rgb_pic->mosaic_size * 2);
                    if (w_tmp < rgb_pic->mosaic_size) {
                        for (i = 0; i < byte_size; i++) {
                            rgb_pic->pic_addr[addr_cnt++] = rgb_pic->background[i];
                        }
                    } else {
                        for (i = 0; i < byte_size; i++) {
                            rgb_pic->pic_addr[addr_cnt++] = rgb_pic->mosaic_color[i];
                        }
                    }
                }
            } else {
                for (w_cnt = 0; w_cnt < rgb_pic->wide; w_cnt++) {
                    w_tmp = w_cnt % (rgb_pic->mosaic_size * 2);
                    if (w_tmp < rgb_pic->mosaic_size) {
                        for (i = 0; i < byte_size; i++) {
                            rgb_pic->pic_addr[addr_cnt++] = rgb_pic->mosaic_color[i];
                        }
                    } else {
                        for (i = 0; i < byte_size; i++) {
                            rgb_pic->pic_addr[addr_cnt++] = rgb_pic->background[i];
                        }
                    }
                }
            }
        }
    }

    return 0;
}


int fill_rectangle_rgb(RGB_PIC_S *rgb_pic, const unsigned char *color)
{
    int size = 0;
    int w = 0;
    int h = 0;
    int cnt = 0;

    if (NULL == rgb_pic || NULL == color) {
        RGB_ERR_PRT("Input rgb_pic or color is NULL!\n");
        return -1;
    }

    if (NULL == rgb_pic->pic_addr) {
        RGB_ERR_PRT("Input rgb_pic->pic_addr is NULL!\n");
        return -1;
    }

    switch (rgb_pic->rgb_type)
    {
        case OSD_RGB_24:
            size = rgb_pic->wide * rgb_pic->high * 3;
            for (cnt = 0; cnt < size; cnt += 3) {
                rgb_pic->pic_addr[cnt]     = color[0];
                rgb_pic->pic_addr[cnt + 1] = color[1];
                rgb_pic->pic_addr[cnt + 2] = color[2];
            }
            break;

        case OSD_RGB_555:
        case OSD_RGB_565:
            RGB_ERR_PRT("Input rgb_type:%d Don't support now!\n", rgb_pic->rgb_type);
            return -1;
            break;

        case OSD_RGB_32:
            size = rgb_pic->wide * rgb_pic->high * 4;
            for (cnt = 0; cnt < size; cnt += 4) {
                rgb_pic->pic_addr[cnt]     = color[0];
                rgb_pic->pic_addr[cnt + 1] = color[1];
                rgb_pic->pic_addr[cnt + 2] = color[2];
                rgb_pic->pic_addr[cnt + 3] = color[3];
            }
            break;

        default:
            RGB_ERR_PRT("Input rgb_type:%d error!\n", rgb_pic->rgb_type);
            return -1;
            break;
    }

    return 0;
}


static int get_rgb_bytesize(OSD_RGB_TYPE rgb_type, int *byte_size)
{
    if (NULL == byte_size) {
        RGB_ERR_PRT("Input byte_size is NULL!\n");
        return -1;
    }

    switch (rgb_type) {
        case OSD_RGB_24:
            *byte_size = 3;
            break;

        case OSD_RGB_555:
        case OSD_RGB_565:
            *byte_size = 3;
            break;

        case OSD_RGB_32:
            *byte_size = 4;
            break;

        default:
            RGB_ERR_PRT("Input rgb_type:%d error!\n", rgb_type);
            return -1;
            break;
    }

    return 0;
}


static int get_font_size(FONT_SIZE_TYPE font_type, CHAR_CODE_TYPE code_type, unsigned int *wide, unsigned int *high)
{
    if (NULL == wide || NULL == high) {
        RGB_ERR_PRT("Input wide or high is NULL!\n");
        return -1;
    }

    switch (font_type) {
        case FONT_SIZE_16:
            if (CHAR_CODE_ASCII == code_type) {
                *wide = 8;
            }
            else {
                *wide = 16;
            }
            *high = 16;
            break;
        case FONT_SIZE_24:
            *wide = 16;
            *high = 24;
            break;
        case FONT_SIZE_32:
            if (CHAR_CODE_ASCII == code_type) {
                *wide = 16;
            }
            else {
                *wide = 32;
            }
            *high = 32;
            break;
        case FONT_SIZE_20:
        case FONT_SIZE_40:
        case FONT_SIZE_48:
            if (CHAR_CODE_ASCII == code_type) {
                *wide = 24;
            }
            else {
                *wide = 48;
            }
            *high = 48;
            break;
        case FONT_SIZE_56:
            if (CHAR_CODE_ASCII == code_type) {
                *wide = 28;
            }
            else {
                *wide = 56;
            }
            *high = 56;
            break;

        case FONT_SIZE_64:
            if (CHAR_CODE_ASCII == code_type) {
                *wide = 32;
            }
            else {
                *wide = 64;
            }
            *high = 64;
            break;

        default:
            RGB_ERR_PRT("Input font_type:%d error!\n", font_type);
            return -1;
            break;
    }

    return 0;
}


int load_font_file(FONT_SIZE_TYPE font_type)
{
    char *path[6] = {NULL};
    unsigned char **load_font[6] = {NULL};
    int   i = 0;
    int   file_cnt = 0;
    int   ret = 0;
    struct stat file_stat;
    FILE *pf = NULL;

    switch (font_type) {
        case FONT_SIZE_16:
            file_cnt = 1;
            path[1] = GB2312_FONT_16_PATH;
            path[0] = ASCII_FONT_16_PATH;
            load_font[1] = &g_gb2312_16;
            load_font[0] = &g_asc_16;
            break;

        case FONT_SIZE_32:
            file_cnt = 1;
            path[1] = GB2312_FONT_32_PATH;
            path[0] = ASCII_FONT_32_PATH;
            load_font[1] = &g_gb2312_32;
            load_font[0] = &g_asc_32;
            break;

        case FONT_SIZE_20:
        case FONT_SIZE_24:
        case FONT_SIZE_40:
        case FONT_SIZE_48:
            RGB_ERR_PRT("Input font_type:%d error!\n", font_type);
            break;

        case FONT_SIZE_64:
            file_cnt = 1;
            path[1] = GB2312_FONT_64_PATH;
            path[0] = ASCII_FONT_64_PATH;
            load_font[1] = &g_gb2312_64;
            load_font[0] = &g_asc_64;
            break;

        default:
            RGB_ERR_PRT("Input font_type:%d error!\n", font_type);
            return -1;
            break;
    }

    /* load font file in memory */
    for (i = 0; i < file_cnt; i++) {
#if 0
        ret = stat(path[i], &file_stat);
        if (ret) {
            RGB_ERR_PRT("Do stat fail! errno:%d  %s\n", errno, strerror(errno));
            return -1;
        }

        RGB_DB_PRT("file:%s  size:%ld\n", path[i], (long )file_stat.st_size);
        *load_font[i] = NULL;
        *load_font[i] = malloc(file_stat.st_size);
        if (NULL == *load_font[i]) {
            RGB_ERR_PRT("Do malloc size:%ld fail! error:%d  %s\n", (long )file_stat.st_size, errno, strerror(errno));
            return -1;
        }
#endif

        pf = fopen(path[i], "r");
        if (NULL == pf) {
            RGB_ERR_PRT("fopen %s fail! errno:%d  %s\n", path[i], errno, strerror(errno));
            return -1;
        }
#if 0
        ret = fread(*load_font[i], (size_t) file_stat.st_size, 1, pf);
        if (ret <= 0) {
            RGB_ERR_PRT("fread %s fail! errno:%d  %s\n", path[i], errno, strerror(errno));
            fclose(pf);
            return -1;
        }
#endif
        decompress_file_to_mem(pf, (char**)load_font[i]);
        if (*load_font[i] == NULL) {
            RGB_ERR_PRT("decompress file to mem failed");
        }

        fclose(pf);
    }

    return 0;
}

int unload_font_file(FONT_SIZE_TYPE font_type)
{
	switch (font_type) {
	case FONT_SIZE_16: {
		if (NULL != g_asc_16) {
			free(g_asc_16);
			g_asc_16 = NULL;
		}
		if (NULL != g_gb2312_16) {
			free(g_gb2312_16);
			g_gb2312_16 = NULL;
		}
	}
		break;

	case FONT_SIZE_32: {
		if (NULL != g_asc_32) {
			free(g_asc_32);
			g_asc_32 = NULL;
		}

		if (NULL != g_gb2312_32) {
			free(g_gb2312_32);
			g_gb2312_32 = NULL;
		}
	}
		break;

	case FONT_SIZE_64: {
		if (NULL != g_asc_64) {
			free(g_asc_64);
			g_asc_64 = NULL;
		}

		if (NULL != g_gb2312_64) {
			free(g_gb2312_64);
			g_gb2312_64 = NULL;
		}
	}
		break;

	case FONT_SIZE_20:
	case FONT_SIZE_24:
	case FONT_SIZE_40:
	case FONT_SIZE_48:
	case FONT_SIZE_56:
	default:
		RGB_ERR_PRT("Input font_type:%d not exist!\n", font_type);
		return -1;
		break;
	}

	return 0;
}

int unload_gb2312_font(void)
{
    if (NULL != g_gb2312_16) {
        free(g_gb2312_16);
        g_gb2312_16 = NULL;
    }

    if (NULL != g_gb2312_32) {
        free(g_gb2312_32);
        g_gb2312_32 = NULL;
    }

    if (NULL != g_gb2312_64) {
        free(g_gb2312_64);
        g_gb2312_64 = NULL;
    }

    if (NULL != g_asc_16) {
        free(g_asc_16);
        g_asc_16 = NULL;
    }

    if (NULL != g_asc_32) {
        free(g_asc_32);
        g_asc_32 = NULL;
    }

    if (NULL != g_asc_64) {
        free(g_asc_64);
        g_asc_64 = NULL;
    }

    return 0;
}


static CHAR_CODE_TYPE check_char_code(const char *code)
{
    if (NULL == code) {
        RGB_ERR_PRT("Input code is NULL!\n");
        return CHAR_CODE_BUTT;
    }

    /* check bit code is  ASCII in 0x0 < x < 0x7F */
    if (code[0] < 0x7f) {
        return CHAR_CODE_ASCII;
    }

    /* check bit code is in GB2312 0xa1 < x < 0xfe */
    if (0xa1 < code[1] && code[1] < 0xfe) {
        if (0xb0 < code[0] && code[0] < 0xf7) {
            return CHAR_CODE_GB2312;
        } else {
            RGB_ERR_PRT("Input code[1] Sector_code:0x%x error! ok:(0xb0 < x < 0xf7)\n", code[1]);
            return CHAR_CODE_BUTT;
        }
    } else {
        RGB_ERR_PRT("Input code[0] bit_code:0x%x error! ok:(0xa1 < x < 0xfe)\n", code[0]);
        return CHAR_CODE_BUTT;
    }

    return CHAR_CODE_BUTT;
}


static int get_font_bitmap(const char *code, FONT_SIZE_TYPE font_type, unsigned char *font)
{
    int ret = 0;
    int cnt = 0;
    int bitmap_num = 0;
    int offset = 0;
    unsigned char bit = 0, sector = 0;
    unsigned char *pfont_addr = NULL;
    CHAR_CODE_TYPE code_type  = 0;

    if (NULL == code) {
        RGB_ERR_PRT("Input code is NULL!\n");
        return -1;
    }

    if (NULL == font) {
        RGB_ERR_PRT("Input font is NULL!\n");
        return -1;
    }

    code_type = check_char_code(code);
    if (CHAR_CODE_BUTT == code_type) {
        RGB_ERR_PRT("check_char_code code[0]:0x%x error!\n", code[0]);
        return -1;
    }

    switch (font_type) {
        case FONT_SIZE_16:
            {
                switch (code_type) {
                    case CHAR_CODE_ASCII:
                        if (NULL == g_asc_16) {
                            RGB_ERR_PRT("g_asc_16 is NULL! Don't load g_asc_16!\n");
                            return -1;
                        }
                        pfont_addr = g_asc_16;
                        bitmap_num = 16 * 1;
                        offset = code[0] * 1 * 16;
                        //RGB_DB_PRT("The FONT_SIZE_asc_16 code[0]:0x%x  offset:0x%x\n", code[0], offset);
                        break;

                    case CHAR_CODE_GB2312:
                        if (NULL == g_gb2312_16) {
                            RGB_ERR_PRT("g_gb2312_16 is NULL! Don't load g_gb2312_16!\n");
                            return -1;
                        }
                        pfont_addr = g_gb2312_16;
                        bitmap_num = 16 * 2;
                        offset = (94 * (code[0] - 0xa0 - 1) + (code[1] - 0xa0 -1)) * bitmap_num;
                        //RGB_DB_PRT("The FONT_SIZE_gb2312_16 code:0x%x-0x%x  offset:0x%x\n",
                        //                                        code[1], code[0], offset);
                        break;

                    default:
                        RGB_ERR_PRT("Don't support this code type:%d\n", code_type);
                        return -1;
                        break;
                }
            }
            break;

        case FONT_SIZE_32:
            {
                switch (code_type) {
                    case CHAR_CODE_ASCII:
                        if (NULL == g_asc_32) {
                            RGB_ERR_PRT("g_asc_32 is NULL! Don't load g_asc_32!\n");
                            return -1;
                        }
                        pfont_addr = g_asc_32;
                        bitmap_num = 32 * 2;
                        offset = code[0] * 2 * 32;
                        //RGB_DB_PRT("The FONT_SIZE_asc_32 code[0]:0x%x  offset:0x%x\n", code[0], offset);
                        break;

                    case CHAR_CODE_GB2312:
                        if (NULL == g_gb2312_32) {
                            RGB_ERR_PRT("g_gb2312_32 is NULL! Don't load g_gb2312_32!\n");
                            return -1;
                        }
                        pfont_addr = g_gb2312_32;
                        bitmap_num = 32 * 4;
                        offset = (94 * (code[0] - 0xa0 - 1) + (code[1] - 0xa0 -1)) * bitmap_num;
                        //RGB_DB_PRT("The FONT_SIZE_gb2312_32 code:0x%x-0x%x  offset:0x%x\n",
                        //                                        code[1], code[0], offset);
                        break;

                    default:
                        RGB_ERR_PRT("Don't support this code type:%d\n", code_type);
                        return -1;
                        break;
                }
            }
            break;

        case FONT_SIZE_20:
        case FONT_SIZE_24:
        case FONT_SIZE_40:
        case FONT_SIZE_48:
        case FONT_SIZE_56:
            RGB_ERR_PRT("Input font_type:%d error!\n", font_type);
            break;

        case FONT_SIZE_64:
            {
                switch (code_type) {
                    case CHAR_CODE_ASCII:
                        if (NULL == g_asc_64) {
                            RGB_ERR_PRT("g_asc_56 is NULL! Don't load g_asc_56!\n");
                            return -1;
                        }
                        pfont_addr = g_asc_64;
                        bitmap_num = 64 * 4;
                        offset = code[0] * bitmap_num;
                        //RGB_DB_PRT("The FONT_SIZE_asc_32 code[0]:0x%x  offset:0x%x\n", code[0], offset);
                        break;

                    case CHAR_CODE_GB2312:
                        if (NULL == g_gb2312_64) {
                            RGB_ERR_PRT("g_gb2312_56 is NULL! Don't load g_gb2312_56!\n");
                            return -1;
                        }
                        pfont_addr = g_gb2312_64;
                        bitmap_num = 64 * 8;
                        offset = (94 * (code[0] - 0xa0 - 1) + (code[1] - 0xa0 -1)) * bitmap_num;
                        //RGB_DB_PRT("The FONT_SIZE_gb2312_32 code:0x%x-0x%x  offset:0x%x\n",
                        //                                        code[1], code[0], offset);
                        break;

                    default:
                        RGB_ERR_PRT("Don't support this code type:%d\n", code_type);
                        return -1;
                        break;
                }
            }
            break;


        default:
            RGB_ERR_PRT("Input font_type:%d error!\n", font_type);
            return -1;
            break;
    }

    for (cnt = 0; cnt < bitmap_num; cnt++) {
        font[cnt] = pfont_addr[offset + cnt];
    }

    return 0;
}


int draw_pic_in_pic(const RGB_PIC_S *src_pic, const RGB_PIC_S *dst_pic, int top, int left)
{
    if (NULL == src_pic) {
        RGB_ERR_PRT("Input src_pic is NULL!\n");
        return -1;
    }

    if (NULL == dst_pic) {
        RGB_ERR_PRT("Input dst_pic is NULL!\n");
        return -1;
    }

    if (NULL == src_pic->pic_addr) {
        RGB_ERR_PRT("Input src_pic.pic_addr is NULL!\n");
        return -1;
    }

    if (NULL == dst_pic->pic_addr) {
        RGB_ERR_PRT("Input dst_pic.pic_addr is NULL!\n");
        return -1;
    }

    /* check src dst picture rgb type */
    if (src_pic->rgb_type != dst_pic->rgb_type) {
        RGB_ERR_PRT("src_pic rgb_type:%d different dst_pic rgb_type:%d !\n",
                                            src_pic->rgb_type, dst_pic->rgb_type);
        return -1;
    }

    /* check draw region is in dst picure? */
    if ((top + src_pic->high > dst_pic->high) || (left+ src_pic->wide > dst_pic->wide)) {
        RGB_ERR_PRT("src_pic draw out of dst_pic region!\n");
        RGB_ERR_PRT("src_pic(w:%d h:%d) top:%d left:%d dst_pic(w:%d h:%d)\n",
                    src_pic->wide, src_pic->high, top, left, dst_pic->wide, dst_pic->high);
        return -1;
    }

    int ret = 0;
    int byte_size = 0;
    int offset = 0, src_cnt = 0, byte_cnt = 0;
    int w_cnt = 0, h_cnt = 0;

    ret = get_rgb_bytesize(src_pic->rgb_type, &byte_size);
    if (ret < 0) {
        RGB_ERR_PRT("Do get_rgb_bytesize fail! ret:%d\n", ret);
        return -1;
    }

    src_cnt = 0;
    /* loop in high line number */
    for (h_cnt = 0; h_cnt < src_pic->high; h_cnt++) {
        /* update dst picture offset */
        offset = (top + h_cnt) * (dst_pic->wide * byte_size) + (left * byte_size);
        /* loop in wide line number */
        for (w_cnt = 0; w_cnt < (src_pic->wide); w_cnt++) {
            /* loop in pix byte size number */
            for (byte_cnt = 0; byte_cnt < byte_size; byte_cnt++) {
                dst_pic->pic_addr[offset++] = src_pic->pic_addr[src_cnt++];
            }
        }
    }

    RGB_DB_PRT("h_cnt:%d  w_cnt:%d  offset:%d  src_cnt:%d \n", h_cnt, w_cnt, offset, src_cnt);

    return 0;
}


int draw_font_in_pic(const char *code, const FONT_RGBPIC_S *font_pic,
                                const RGB_PIC_S *dst_pic, int top, int left)
{
    int ret = 0;
    int pic_size = 0;
    unsigned int   font_wide = 0, font_high = 0;
    unsigned char  fontmap[1024] = {0};
    CHAR_CODE_TYPE code_type;

    if (NULL == code) {
        RGB_ERR_PRT("Input code is NULL!\n");
        return -1;
    }
    if (NULL == font_pic) {
        RGB_ERR_PRT("Input font_pic is NULL!\n");
        return -1;
    }
    if (NULL == dst_pic) {
        RGB_ERR_PRT("Input dst_pic is NULL!\n");
        return -1;
    }
    if (NULL == dst_pic->pic_addr) {
        RGB_ERR_PRT("Input dst_pic->pic_addr is NULL!\n");
        return -1;
    }

    code_type = check_char_code(code);
    if (CHAR_CODE_BUTT == code_type) {
        RGB_ERR_PRT("check_char_code code[0]:0x%x error!\n", code[0]);
        return -1;
    }

    ret = get_font_size(font_pic->font_type, code_type, &font_wide, &font_high);
    if (ret < 0) {
        RGB_ERR_PRT("Do get_font_size fail! ret:%d\n", ret);
        return -1;
    }

    /* check src dst picture rgb type */
    if (font_pic->rgb_type != dst_pic->rgb_type) {
        RGB_ERR_PRT("src_pic rgb_type:%d different dst_pic rgb_type:%d !\n",
                                            font_pic->rgb_type, dst_pic->rgb_type);
        return -1;
    }

    /* check draw region is in dst picure? */
    if (((top + font_high) > dst_pic->high) || ((left+ font_wide) > dst_pic->wide)) {
        RGB_ERR_PRT("src_font draw out of dst_pic region!\n");
        RGB_ERR_PRT("src_font(w:%d h:%d) top:%d left:%d dst_pic(w:%d h:%d)\n",
                    font_wide, font_high, top, left, dst_pic->wide, dst_pic->high);
        return -1;
    }

    ret = get_font_bitmap(code, font_pic->font_type, fontmap);
    if (ret < 0) {
        RGB_ERR_PRT("Do get_font_bitmap fail! ret:%d\n", ret);
        return -1;
    }

    int byte_size = 0;
    int offset = 0, font_cnt = 0, byte_cnt = 0;
    int bit_cnt = 0, w_cnt = 0, h_cnt = 0;
    unsigned char tmp_font = 0;

    ret = get_rgb_bytesize(font_pic->rgb_type, &byte_size);
    if (ret < 0) {
        RGB_ERR_PRT("Do get_rgb_bytesize fail! ret:%d\n", ret);
        return -1;
    }

    //printf("\n");
    /* loop in high line number */
    font_cnt = 0;
    for (h_cnt = 0; h_cnt < font_high; h_cnt++) {

        /* update dst picture offset */
        offset = (top + h_cnt) * (dst_pic->wide * byte_size) + (left * byte_size);

        /* loop in (bit map wide) line number */
        for (w_cnt = 0; w_cnt < ((font_wide+8-1)/8); w_cnt++) {

            tmp_font = fontmap[font_cnt++];
            for (bit_cnt = 0; bit_cnt < 8; bit_cnt++) {

                /* loop in color type size number */
                if (tmp_font & (0x1 << (7-bit_cnt))) {
                    //printf("*");
                    for (byte_cnt = 0; byte_cnt < byte_size; byte_cnt++) {
                        dst_pic->pic_addr[offset++] = font_pic->foreground[byte_cnt];
                    }
                } else {
                    //printf(" ");
                    if (font_pic->enable_bg) {
                        for (byte_cnt = 0; byte_cnt < byte_size; byte_cnt++) {
                            dst_pic->pic_addr[offset++] = font_pic->background[byte_cnt];
                        }
                    }
                    else {
                        offset += byte_size;
                    }
                }
            }
        }
        //printf("\n");
    }

    return 0;
}


int create_font_rectangle(const char *code, const FONT_RGBPIC_S *font_pic,
                                RGB_PIC_S *rgb_pic)
{
    int ret = 0;
    int code_cnt = 0, i = 0;
    unsigned int wide = 0, high = 0;
    unsigned int font_wide = 0, font_high = 0;
    CHAR_CODE_TYPE code_type = CHAR_CODE_BUTT;

    code_cnt = 0;
    while ('\0' != code[code_cnt] && 0x0a != code[code_cnt]) {
        code_type = check_char_code(&code[code_cnt]);
        if (CHAR_CODE_BUTT == code_type) {
            RGB_ERR_PRT("Do check_char_code fail! ret:%d code:0x%x\n", ret, code[code_cnt]);
            return -1;
        }
        //RGB_DB_PRT("code_cnt:%d  -- 0x%x  %c \n", code_cnt, code[code_cnt], code[code_cnt]);

        switch (code_type) {
            case CHAR_CODE_ASCII:
                code_cnt++;
                break;
            case CHAR_CODE_GB2312:
                code_cnt += 2;
                break;
            default:
                RGB_ERR_PRT("Do support this type:%d code:0x%x\n", code_type, code[code_cnt]);
                return -1;
                break;
        }

        ret = get_font_size(font_pic->font_type, code_type, &font_wide, &font_high);
        if (ret < 0) {
            RGB_ERR_PRT("Do get_font_size fail! ret:%d\n", ret);
            return -1;
        }

        wide += font_wide;
        high = font_high;
    }

    rgb_pic->wide       = ALIGN(wide, 16);
    rgb_pic->high       = ALIGN(high, 16);
    rgb_pic->rgb_type   = font_pic->rgb_type;
    rgb_pic->pic_addr   = NULL;
    ret = create_rectangle_rgb(rgb_pic);
    if (ret < 0) {
        RGB_ERR_PRT("Do create_rectangle_rgb fail! ret:%d\n", ret);
        return -1;
    }

    code_cnt = 0;
    wide = 0;
    high = 0;
    while ('\0' != code[code_cnt] && 0x0a != code[code_cnt]) {
        code_type = check_char_code(&code[code_cnt]);
        if (CHAR_CODE_BUTT == code_type) {
            RGB_ERR_PRT("Do check_char_code fail! ret:%d code:0x%x\n", ret, code[code_cnt]);
            return -1;
        }

        ret = get_font_size(font_pic->font_type, code_type, &font_wide, &font_high);
        if (ret < 0) {
            RGB_ERR_PRT("Do get_font_size fail! ret:%d\n", ret);
            return -1;
        }

        //RGB_DB_PRT("src_font(w:%d h:%d) top:%d left:%d dst_pic(w:%d h:%d)\n",
        //                    font_wide, font_high, 0, wide, rgb_pic->wide, rgb_pic->high);

        ret = draw_font_in_pic(&code[code_cnt], font_pic, rgb_pic, 0, wide);
        if (ret < 0) {
            RGB_ERR_PRT("Do draw_font_in_pic fail! ret:%d\n", ret);
            return -1;
        }

        wide += font_wide;
        switch (code_type) {
            case CHAR_CODE_ASCII:
                code_cnt++;
                break;
            case CHAR_CODE_GB2312:
                code_cnt += 2;
                break;
            default:
                RGB_ERR_PRT("Do support this type:%d code:0x%x\n", code_type, code[code_cnt]);
                return -1;
                break;
        }
    }
//    if(access(0,"/tmp/test.bmp") == -1 && font_pic->font_type == FONT_SIZE_64){
//        savebmp(rgb_pic->pic_addr, "/tmp/test.bmp", rgb_pic->wide, rgb_pic->high);
//    }
    return 0;
}


int release_rgb_picture(RGB_PIC_S * rgb_pic)
{
    if (NULL == rgb_pic) {
        RGB_ERR_PRT("Input rgb_pic is NULL!\n");
        return -1;
    }

    if (NULL == rgb_pic->pic_addr) {
        return 0;
    }

    free(rgb_pic->pic_addr);
    rgb_pic->pic_addr = NULL;

    memset(rgb_pic, 0, sizeof(RGB_PIC_S));

    return 0;
}


static int display_gb2312_bitmap(FONT_SIZE_TYPE font_type, unsigned char *font)
{
    int cnt = 0, i = 0, j = 0, k = 0;
    int row = 0, col = 0;
    unsigned char tmp = 0;

    if (NULL == font) {
        RGB_ERR_PRT("Input font is NULL!\n");
        return -1;
    }

    switch (font_type) {
        case FONT_SIZE_16:
            row = 16;
            col = 2;
            break;

        case FONT_SIZE_32:
            row = 32;
            col = 4;
            break;

        case FONT_SIZE_20:
        case FONT_SIZE_24:
        case FONT_SIZE_40:
        case FONT_SIZE_48:
            row = 48;
            col = 6;
            break;

        case FONT_SIZE_56:
            row = 56;
            col = 7;
            break;

        case FONT_SIZE_64:
            row = 64;
            col = 8;
            break;

        default:
            RGB_ERR_PRT("Input font_type:%d error!\n", font_type);
            return -1;
            break;
    }

    printf("\n");
    cnt = 0;
    for (i = 0; i < row; i++) {
        for (j = 0; j < col; j++) {
            tmp = font[cnt];
            for (k = 0; k < 8; k++) {
                if (tmp & (0x1 << (7-k))) {
                    printf("*");
                } else {
                    printf(" ");
                }
            }
            cnt++;
        }
        printf("\n");
    }

    return 0;
}


static void savebmp(char * pdata, char * bmp_file, int width, int height)
{
    //int size = width*height*3*sizeof(char);       // rgb888  - rgb24
    int size = width * height * 4 * sizeof(char);   // rgb8888 - rgb32

    /* RGB head */
    BMPFILEHEADER_T bfh;
    bfh.bfType = (WORD) 0x4d42;  //bm
    /* data size + first section size + second section size */
    bfh.bfSize = size + sizeof(BMPFILEHEADER_T) + sizeof(BMPINFOHEADER_T);
    bfh.bfReserved1 = 0; // reserved
    bfh.bfReserved2 = 0; // reserved
    bfh.bfOffBits = sizeof(BMPFILEHEADER_T) + sizeof(BMPINFOHEADER_T);

    // RGB info
    BMPINFOHEADER_T bih;
    bih.biSize = sizeof(BMPINFOHEADER_T);
    bih.biWidth = width;
    bih.biHeight = -height;
    bih.biPlanes = 1;
    //bih.biBitCount = 24;  // RGB888
    bih.biBitCount = 32;    // RGB8888
    bih.biCompression = 0;
    bih.biSizeImage = size;
    bih.biXPelsPerMeter = 2835;
    bih.biYPelsPerMeter = 2835;
    bih.biClrUsed = 0;
    bih.biClrImportant = 0;
    FILE * fp = NULL;
    fp = fopen(bmp_file, "wb");
    if (!fp) {
        return;
    }

    fwrite(&bfh, 8, 1, fp);
    fwrite(&bfh.bfReserved2, sizeof(bfh.bfReserved2), 1, fp);
    fwrite(&bfh.bfOffBits, sizeof(bfh.bfOffBits), 1, fp);
    fwrite(&bih, sizeof(BMPINFOHEADER_T), 1, fp);
    fwrite(pdata, size, 1, fp);
    fclose(fp);
}

#if 0
int main_test()
{
//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
    int    ret      = 0;
    int    pic_size = 0;
    unsigned char  *prgb      = NULL;
    unsigned char  *prgb_2    = NULL;
    unsigned char  *prgb_font = NULL;
    unsigned char   color[8]  = {0};
    int    wide     = 200;
    int    high     = 200;
    RGB_PIC_S rgb_pic;
    FONT_RGBPIC_S font_pic;
    char font[256] = {0};
    char code[16]  = {0};

    ret = load_font_file(FONT_SIZE_16);
    if (ret  < 0) {
        RGB_ERR_PRT("Do load_font_file FONT_SIZE_16 fail! ret:%d\n", ret);
        return -1;
    }

    ret = load_font_file(FONT_SIZE_32);
    if (ret  < 0) {
        RGB_ERR_PRT("Do load_font_file FONT_SIZE_32 fail! ret:%d\n", ret);
        return -1;
    }

    memset(&rgb_pic, 0, sizeof(RGB_PIC_S));
    unsigned char g_alpha[8] = {0xff};
    rgb_pic.wide = 300;
    rgb_pic.high = 300;
    rgb_pic.pic_addr = NULL;
    //rgb_pic.rgb_type = OSD_RGB_24;
    rgb_pic.rgb_type = OSD_RGB_32;
    rgb_pic.background[0]   = 0xC2;
    rgb_pic.background[1]   = 0xC2;
    rgb_pic.background[2]   = 0xC2;
    rgb_pic.background[3]   = 0xC2;
    rgb_pic.enable_mosaic   = 0;
    rgb_pic.mosaic_size     = 3;
    rgb_pic.mosaic_color[0] = 0xff;
    rgb_pic.mosaic_color[1] = 0xff;
    rgb_pic.mosaic_color[2] = 0xff;
    rgb_pic.mosaic_color[3] = 0xff;
    ret = create_rectangle_rgb(&rgb_pic);
    if (ret  < 0) {
        RGB_ERR_PRT("Do create_rectangle_rgb fail! ret:%d\n", ret);
        return -1;
    }

    font_pic.font_type = FONT_SIZE_32; //FONT_SIZE_16;
    font_pic.rgb_type  = rgb_pic.rgb_type;
    font_pic.background[0] = 0xE2;
    font_pic.background[1] = 0x2B;
    font_pic.background[2] = 0x8A;
    font_pic.background[3] = 0x8A;
    font_pic.foreground[0] = 0x29;
    font_pic.foreground[1] = 0x29;
    font_pic.foreground[2] = 0x29;
    font_pic.foreground[3] = 0x29;
    font_pic.enable_bg = 0;
    code[0] = 0xC8;
    code[1] = 0xAB;  /* 全 */
    ret = draw_font_in_pic(code, &font_pic, &rgb_pic, 120, 150);
    if (ret  < 0) {
        RGB_ERR_PRT("Do draw_font_in_pic fail! ret:%d\n", ret);
        return -1;
    }

    code[0] = 'g';
    font_pic.enable_bg = 1;
    ret = draw_font_in_pic(code, &font_pic, &rgb_pic, 120, 188);
    if (ret  < 0) {
        RGB_ERR_PRT("Do draw_font_in_pic fail! ret:%d\n", ret);
        return -1;
    }

    unlink("./rectangle_test_2.bmp");
    savebmp(rgb_pic.pic_addr, "./rectangle_test_2.bmp", rgb_pic.wide, rgb_pic.high);

    ret = release_rgb_picture(&rgb_pic);
    if (ret  < 0) {
        RGB_ERR_PRT("Do release_rgb_picture fail! ret:%d\n", ret);
        return -1;
    }

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

    char string_code[256] = {0};
    char *ptime_string = NULL;
    time_t sys_time;

    memset(string_code, 0, sizeof(string_code));
    memset(&rgb_pic, 0, sizeof(RGB_PIC_S));

    rgb_pic.pic_addr = NULL;
    //rgb_pic.rgb_type = OSD_RGB_24;
    rgb_pic.rgb_type = OSD_RGB_32;
    rgb_pic.background[0]   = 0xC2;
    rgb_pic.background[1]   = 0xC2;
    rgb_pic.background[2]   = 0xC2;
    rgb_pic.enable_mosaic   = 1;
    rgb_pic.mosaic_size     = 1;
    rgb_pic.mosaic_color[0] = 0xff;
    rgb_pic.mosaic_color[1] = 0xff;
    rgb_pic.mosaic_color[2] = 0xff;
    rgb_pic.mosaic_color[3] = 0xff;

    font_pic.font_type = FONT_SIZE_32; //FONT_SIZE_16;
    font_pic.rgb_type  = rgb_pic.rgb_type;
    font_pic.background[0] = 0xE2;
    font_pic.background[1] = 0x2B;
    font_pic.background[2] = 0x8A;
    font_pic.background[3] = 0x8A;
    font_pic.foreground[0] = 0x29;
    font_pic.foreground[1] = 0x29;
    font_pic.foreground[2] = 0x29;
    font_pic.foreground[3] = 0x29;
    font_pic.enable_bg = 0;

    sys_time = time(NULL);
    ptime_string = ctime(&sys_time);
    if (NULL == ptime_string) {
        RGB_ERR_PRT("Do ctime fail! ret:%d\n", ret);
        return -1;
    }
    RGB_DB_PRT(" ptime_string:%s \n", ptime_string);
    string_code[0] = 0xB1;
    string_code[1] = 0xCA;
    string_code[2] = 0xE4;
    string_code[3] = 0xBC;
    string_code[4] = ':';
    strcpy(&string_code[5], ptime_string);

    ret = create_font_rectangle(string_code, &font_pic, &rgb_pic);
    if (ret) {
        RGB_ERR_PRT("Do create_font_rectangle fail! ret:%d\n", ret);
        return -1;
    }

    unlink("./string_rectangle.bmp");
    savebmp(rgb_pic.pic_addr, "./string_rectangle.bmp", rgb_pic.wide, rgb_pic.high);

    ret = release_rgb_picture(&rgb_pic);
    if (ret  < 0) {
        RGB_ERR_PRT("Do release_rgb_picture fail! ret:%d\n", ret);
        return -1;
    }

//////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////

    if (NULL != prgb) {
        free(prgb);
        prgb = NULL;
    }

    if (NULL != prgb_font) {
        free(prgb_font);
        prgb_font = NULL;
    }

    ret = unload_gb2312_font();
    if (ret  < 0) {
        RGB_ERR_PRT("Do unload_gb2312_font fail! ret:%d\n", ret);
        return -1;
    }

    return 0;
}
#endif
