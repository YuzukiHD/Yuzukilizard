#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include "yolo_layer.h"

#include "bmp.h"
#include "image_utils.h"

char *coco_names[] = {"person", "bicycle", "car", "motorbike", "aeroplane", "bus", "train", "truck", "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "sofa", "pottedplant", "bed", "diningtable", "toilet", "tvmonitor", "laptop", "mouse", "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"};

float colors[6][3] = {{1, 0, 1}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}};

float get_color(int c, int x, int max)
{
    float ratio = ((float)x / max) * 5;
    int i = floor(ratio);
    int j = ceil(ratio);
    float r = 0;
    ratio -= i;
    r = (1 - ratio) * colors[i][c] + ratio * colors[j][c];
    return r;
}

void draw_box(unsigned char *data, int x1, int y1, int x2, int y2, float r, float g, float b, int w, int h)
{
    // normalize_image(a);
    unsigned char ur;
    unsigned char ug;
    unsigned char ub;
    int i;
    if (x1 < 0)
        x1 = 0;
    if (x1 >= w)
        x1 = w - 1;
    if (x2 < 0)
        x2 = 0;
    if (x2 >= w)
        x2 = w - 1;

    if (y1 < 0)
        y1 = 0;
    if (y1 >= h)
        y1 = h - 1;
    if (y2 < 0)
        y2 = 0;
    if (y2 >= h)
        y2 = h - 1;

    ur = (unsigned char)r * 255;
    ug = (unsigned char)g * 255;
    ub = (unsigned char)b * 255;

    for (i = x1; i <= x2; ++i)
    {
        data[i + y1 * w + 0 * w * h] = ur;
        data[i + y2 * w + 0 * w * h] = ur;

        data[i + y1 * w + 1 * w * h] = ug;
        data[i + y2 * w + 1 * w * h] = ug;

        data[i + y1 * w + 2 * w * h] = ub;
        data[i + y2 * w + 2 * w * h] = ub;
    }
    for (i = y1; i <= y2; ++i)
    {
        data[x1 + i * w + 0 * w * h] = ur;
        data[x2 + i * w + 0 * w * h] = ur;

        data[x1 + i * w + 1 * w * h] = ug;
        data[x2 + i * w + 1 * w * h] = ug;

        data[x1 + i * w + 2 * w * h] = ub;
        data[x2 + i * w + 2 * w * h] = ub;
    }
}

void draw_box_width(unsigned char *a, int x1, int y1, int x2, int y2, int w, float r, float g, float b, int width, int height)
{
    int i;
    for (i = 0; i < w; ++i)
    {
        draw_box(a, x1 + i, y1 + i, x2 - i, y2 - i, r, g, b, width, height);
    }
}

int writeBMP(FILE *desFile, unsigned char *inputBuf, unsigned int width, unsigned int height, int cn)
{
    if (cn != 1 && cn != 4)
    {
        printf("only support channel 1 and 4\n");
        return -1;
    }

    if (cn == 1)
    {
        FileHeader *vFileHeader = NULL;
        InfoHeader *vInfoHeader = NULL;
        unsigned char *palleteData = NULL;
        unsigned char *imageData = NULL;
        unsigned char *bmpBuffer = NULL;

        unsigned int imgSize = width * height;
        unsigned int headerSize = 1024 + sizeof(FileHeader) + sizeof(InfoHeader);
        unsigned int fileSize = headerSize + imgSize;

        unsigned int i, val, length = 1 << 8;

        bmpBuffer = (unsigned char *)malloc(fileSize);
        if (NULL == bmpBuffer)
        {
            printf("invalid buffer\n");
            return -1;
        }
        else
        {
            vFileHeader = (FileHeader *)bmpBuffer;
            vInfoHeader = (InfoHeader *)(bmpBuffer + sizeof(FileHeader));
            palleteData = bmpBuffer + sizeof(FileHeader) + sizeof(InfoHeader);
            imageData = bmpBuffer + headerSize;
        }

        // assign vFileHeader & vInfoHeader
        vFileHeader->fileType = 0x4d42;
        vFileHeader->fileSize1 = (unsigned short)((fileSize << 16) >> 16);
        vFileHeader->fileSize2 = (unsigned short)(fileSize >> 16);
        vFileHeader->fileReserved1 = 0;
        vFileHeader->fileReserved2 = 0;
        vFileHeader->fileOffset1 = (unsigned short)((headerSize << 16) >> 16);
        vFileHeader->fileOffset2 = (unsigned short)(headerSize >> 16);

        vInfoHeader->infoSize = sizeof(InfoHeader);
        vInfoHeader->imageWidth = width;
        vInfoHeader->imageHeight = height;
        vInfoHeader->imagePlane = 1;
        vInfoHeader->imageCount = 8;
        vInfoHeader->imageCompression = 0;
        vInfoHeader->imageSize = 0;
        vInfoHeader->hResolution = 0;
        vInfoHeader->vResolution = 0;
        vInfoHeader->clrUsed = 0;
        vInfoHeader->clrImportant = 0;

        for (i = 0; i < length; i++)
        {
            val = (i * 255 / (length - 1)) ^ 0;
            palleteData[0] = (unsigned char)val;
            palleteData[1] = (unsigned char)val;
            palleteData[2] = (unsigned char)val;
            palleteData[3] = (unsigned char)0;
            palleteData += 4;
        }
        // assign image data
        memcpy(imageData, inputBuf, imgSize);

        // write desFile
        if (desFile == NULL)
        {
            if (bmpBuffer != NULL)
            {
                free(bmpBuffer);
                bmpBuffer = NULL;
            }

            printf("desFile is NULL\n");
            return -1;
        }
        else
        {
            fwrite(bmpBuffer, fileSize, 1, desFile);

            if (bmpBuffer != NULL)
            {
                free(bmpBuffer);
                bmpBuffer = NULL;
            }
        }
    }
    else if (cn == 4)
    {
        FileHeader *vFileHeader = NULL;
        InfoHeader *vInfoHeader = NULL;
        unsigned char *imageData = NULL;
        unsigned char *bmpBuffer = NULL;

        unsigned int imgSize = width * height;
        unsigned int fileSize = sizeof(FileHeader) + sizeof(InfoHeader) + imgSize * 4;

        bmpBuffer = (unsigned char *)malloc(fileSize);
        if (bmpBuffer == NULL)
        {
            printf("malloc bmpBuffer failed\n");
            return -1;
        }
        else
        {
            vFileHeader = (FileHeader *)bmpBuffer;
            vInfoHeader = (InfoHeader *)(bmpBuffer + sizeof(FileHeader));
            imageData = bmpBuffer + sizeof(FileHeader) + sizeof(InfoHeader);
        }

        // assign vFileHeader & vInfoHeader
        vFileHeader->fileType = 0x4d42;
        vFileHeader->fileSize1 = (unsigned short)((fileSize << 16) >> 16);
        vFileHeader->fileSize2 = (unsigned short)(fileSize >> 16);
        vFileHeader->fileReserved1 = 0;
        vFileHeader->fileReserved2 = 0;
        vFileHeader->fileOffset1 = (unsigned short)(((sizeof(FileHeader) + sizeof(InfoHeader)) << 16) >> 16);
        vFileHeader->fileOffset2 = (unsigned short)((sizeof(FileHeader) + sizeof(InfoHeader)) >> 16);

        vInfoHeader->infoSize = sizeof(InfoHeader);
        vInfoHeader->imageWidth = width;
        vInfoHeader->imageHeight = -((int)height);
        vInfoHeader->imagePlane = 1;
        vInfoHeader->imageCount = 32;
        vInfoHeader->imageCompression = 0;
        vInfoHeader->imageSize = imgSize * 4;
        vInfoHeader->hResolution = 0;
        vInfoHeader->vResolution = 0;
        vInfoHeader->clrUsed = 0;
        vInfoHeader->clrImportant = 0;

        // assign image data
        memcpy(imageData, inputBuf, imgSize * 4);

        // write desFile
        if (desFile == NULL)
        {
            if (bmpBuffer != NULL)
            {
                free(bmpBuffer);
                bmpBuffer = NULL;
            }

            printf("desFile is NULL\n");
            return -1;
        }
        else
        {
            fwrite(bmpBuffer, fileSize, 1, desFile);

            if (bmpBuffer != NULL)
            {
                free(bmpBuffer);
                bmpBuffer = NULL;
            }
        }
    }

    return 0;
}

int writeBMToFile(char *fileName, unsigned char *inputBuf, unsigned int width, unsigned int height, int channel, int pixelFormat)
{
    FILE *dstFile = NULL;
    unsigned char *pDstBuff = NULL;
    int cn = channel;
    int status = 0;
    int x = 0, y = 0;

    if (channel != 1 && channel != 4 && channel != 3)
    {
        printf("only support channel 1 3 and 4 @ %s(%d)\n", __FUNCTION__, __LINE__);
        return -1;
    }

    dstFile = fopen(fileName, "wb");
    if (dstFile == NULL)
    {
        printf("desFile is NULL @ %s(%d)\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (channel == 3)
    {
        cn = 4;
        pDstBuff = (unsigned char *)malloc(width * height * 4);
        if (pDstBuff == NULL)
        {
            printf("malloc buffer failed @ %s(%d)\n", __FUNCTION__, __LINE__);
            if (dstFile)
                fclose(dstFile);
            return -1;
        }
        if (pixelFormat == 0)
        { // RGBRGB
            for (y = 0; y < height; y++)
            {
                for (x = 0; x < width; x++)
                {
                    int idx = y * width + x;
                    pDstBuff[idx * cn + 0] = inputBuf[idx * channel + 0];
                    pDstBuff[idx * cn + 1] = inputBuf[idx * channel + 1];
                    pDstBuff[idx * cn + 2] = inputBuf[idx * channel + 2];
                    pDstBuff[idx * cn + 3] = 0xCD;
                }
            }
        }
        else if (pixelFormat == 1)
        { // ��֯ģʽ BGRBGR
            for (y = 0; y < height; y++)
            {
                for (x = 0; x < width; x++)
                {
                    int idx = y * width + x;
                    pDstBuff[idx * cn + 0] = inputBuf[idx * channel + 2];
                    pDstBuff[idx * cn + 1] = inputBuf[idx * channel + 1];
                    pDstBuff[idx * cn + 2] = inputBuf[idx * channel + 0];
                    pDstBuff[idx * cn + 3] = 0xCD;
                }
            }
        }
        else if (pixelFormat == 2)
        { // plantģʽ R...RG....GB....B
            int pixIdx = 0;
            for (y = 0; y < height; y++)
            {
                for (x = 0; x < width; x++)
                {
                    int idx = y * width + x;
                    pDstBuff[idx * cn + 0] = inputBuf[0 * width * height + idx];
                    pDstBuff[idx * cn + 1] = inputBuf[1 * width * height + idx];
                    pDstBuff[idx * cn + 2] = inputBuf[2 * width * height + idx];
                    pDstBuff[idx * cn + 3] = 0xCD;
                }
            }
        }
        else if (pixelFormat == 3)
        { // plantģʽ B...BG....GR....R
            int pixIdx = 0;
            for (y = 0; y < height; y++)
            {
                for (x = 0; x < width; x++)
                {
                    int idx = y * width + x;
                    pDstBuff[idx * cn + 0] = inputBuf[2 * width * height + idx];
                    pDstBuff[idx * cn + 1] = inputBuf[1 * width * height + idx];
                    pDstBuff[idx * cn + 2] = inputBuf[0 * width * height + idx];
                    pDstBuff[idx * cn + 3] = 0xCD;
                }
            }
        }
        status = writeBMP(dstFile, pDstBuff, width, height, cn);
    }
    else
    {
        status = writeBMP(dstFile, inputBuf, width, height, cn);
    }

    if (pDstBuff)
    {
        free(pDstBuff);
        pDstBuff = NULL;
    }
    if (dstFile)
        fclose(dstFile);

    return status;
}

int run(blob blobs[], char *image_path)
{
    int img_w = 416;
    int img_h = 416;
    
    int net_w = 416;
    int net_h = 416;

    int classes = 80;
    float thresh = 0.8;
    float nms = 0.45;
    int nboxes = 0;

    blobs[0].w = 13;
    blobs[1].w = 26;
    blobs[2].w = 52;

    unsigned char *orgPixel = NULL;
    orgPixel = (unsigned char *)prepareImageDataForDisplay(image_path, img_w, img_h, 3);

    detection *dets = get_detections(blobs, img_w, img_h, net_w, net_h, &nboxes, classes, thresh, nms);

    int i, j;
    for (i = 0; i < nboxes; ++i)
    {
        int cls = -1;
        for (j = 0; j < 80; ++j)
        {
            if (dets[i].prob[j] > 0.5)
            {
                if (cls < 0)
                {
                    cls = j;
                    break;
                }
            }
        }
        if (cls >= 0)
        {
            box b = dets[i].bbox;

            int left = (b.x - b.w / 2.) * img_w;
            int right = (b.x + b.w / 2.) * img_w;
            int top = (b.y - b.h / 2.) * img_h;
            int bot = (b.y + b.h / 2.) * img_h;

            if (left < 0)
                left = 0;
            if (right > img_w - 1)
                right = img_w - 1;
            if (top < 0)
                top = 0;
            if (bot > img_h - 1)
                bot = img_h - 1;

            printf("%s  %.0f%% %d %d %d %d\n", coco_names[cls], dets[i].prob[cls] * 100, left, right, top, bot);

            int offset = cls * 123457 % classes;
            float red = get_color(2, offset, classes);
            float green = get_color(1, offset, classes);
            float blue = get_color(0, offset, classes);

            draw_box_width(orgPixel, left, top, right, bot, img_h * .012, red, green, blue, img_w, img_h);
        }
    }

    writeBMToFile("yolo_v3_output.bmp", orgPixel, img_w, img_h, 3, 2);

    if (NULL != orgPixel)
    {
        free(orgPixel);
        orgPixel = NULL;
    }

    return 0;
}
