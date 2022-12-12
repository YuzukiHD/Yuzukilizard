#include "image_utils.h"

int convertJpegToBmpData(FILE *inputFile, unsigned char *bmpData, unsigned int *bmpWidth,
                         unsigned int *bmpHeight)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPARRAY buffer;
    unsigned char *point = NULL;
    unsigned long width, height;
    unsigned short depth = 0;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, inputFile);
    jpeg_read_header(&cinfo, TRUE);

    cinfo.dct_method = JDCT_IFAST;

    if (bmpData == NULL)
    {
        width = cinfo.image_width;
        height = cinfo.image_height;
    }
    else
    {
        jpeg_start_decompress(&cinfo);

        width = cinfo.output_width;
        height = cinfo.output_height;
        depth = cinfo.output_components;

        buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, width * depth, 1);

        point = bmpData;

        while (cinfo.output_scanline < height)
        {
            jpeg_read_scanlines(&cinfo, buffer, 1);
            memcpy(point, *buffer, width * depth);
            point += width * depth;
        }

        jpeg_finish_decompress(&cinfo);
    }

    jpeg_destroy_decompress(&cinfo);

    if (bmpWidth != NULL)
        *bmpWidth = width;
    if (bmpHeight != NULL)
        *bmpHeight = height;
    return depth;
}

// jpg file --> BMP data(dataformat: RGBRGBRGB...) --> BBBGGGRRR
unsigned int decode_jpeg(const char *name, unsigned char *bmpData)
{
    FILE *bmpFile = NULL;
    unsigned int width, height, depth, sz;
    unsigned char *tmpData = NULL;
    unsigned int i, j, offset;

    bmpFile = fopen(name, "rb");
    if (bmpFile == NULL)
        goto final;

    depth = convertJpegToBmpData(bmpFile, bmpData, &width, &height);
    sz = width * height * depth;

    if (sz > 0)
    {
        tmpData = (unsigned char *)malloc(sz * sizeof(unsigned char));
        if (tmpData == NULL)
            goto final;
        memcpy(tmpData, bmpData, sz);
        // trans RGBRGBRGB to BBBGGGRRR
        for (i = 0; i < depth; i++)
        {
            offset = width * height * (depth - 1 - i);
            for (j = 0; j < width * height; j++)
            {
                bmpData[j + offset] = tmpData[j * depth + i]; // FIXME
            }
        }
    }

final:
    if (tmpData)
        free(tmpData);
    if (bmpFile)
        fclose(bmpFile);
    return sz;
}

void *prepareImageDataForDisplay(char *name, unsigned int width, unsigned int height,
                                 unsigned int channels)
{
    unsigned char *bmpData;

    bmpData = (unsigned char *)malloc(width * height * channels * sizeof(unsigned char));
    if (bmpData == NULL)
        return NULL;
    memset(bmpData, 0, sizeof(unsigned char) * width * height * channels);

    decode_jpeg(name, bmpData);

    return bmpData;
}