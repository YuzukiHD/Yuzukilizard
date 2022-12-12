#ifndef __IMAGE_UTILS_H_
#define __IMAGE_UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _BASETSD_H
#include "jpeglib.h"

int convertJpegToBmpData(FILE * inputFile, unsigned char* bmpData, unsigned int *bmpWidth, unsigned int *bmpHeight);

unsigned int decode_jpeg(const char *name, unsigned char* bmpData);

void* prepareImageDataForDisplay(char *name, unsigned int width, unsigned int height, unsigned int channels);

#endif