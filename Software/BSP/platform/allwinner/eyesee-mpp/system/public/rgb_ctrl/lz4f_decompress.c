// LZ4frame API example : compress a file
// Based on sample code from Zbigniew Jędrzejewski-Szmek

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <lz4frame.h>

#define BUF_SIZE 16*1024
#define LZ4_HEADER_SIZE 19
#define LZ4_FOOTER_SIZE 4

static size_t get_block_size(const LZ4F_frameInfo_t* info) {
    switch (info->blockSizeID) {
        case LZ4F_default:
        case LZ4F_max64KB:  return 1 << 16;
        case LZ4F_max256KB: return 1 << 18;
        case LZ4F_max1MB:   return 1 << 20;
        case LZ4F_max4MB:   return 1 << 22;
        default:
                            printf("Impossible unless more block sizes are allowed\n");
                            exit(1);
    }
}

size_t decompress_file_to_mem(FILE *in, char **outMem) {
    void* const src = malloc(BUF_SIZE);
    void* dst = NULL;
    size_t dstCapacity = 0;
    LZ4F_dctx *dctx = NULL;
    size_t ret;

    /* Initialization */
    if (!src) { perror("decompress_file(src)"); goto cleanup; }
    ret = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
    if (LZ4F_isError(ret)) {
        printf("LZ4F_dctx creation error: %s\n", LZ4F_getErrorName(ret));
        goto cleanup;
    }

    /* Decompression */
    ret = 1;
    while (ret != 0) {
        /* Load more input */
        size_t srcSize = fread(src, 1, BUF_SIZE, in);
        void* srcPtr = src;
        void* srcEnd = srcPtr + srcSize;
        if (srcSize == 0 || ferror(in)) {
            printf("Decompress: not enough input or error reading file\n");
            goto cleanup;
        }
        /* Allocate destination buffer if it isn't already */
        if (!dst) {
            LZ4F_frameInfo_t info;
            ret = LZ4F_getFrameInfo(dctx, &info, src, &srcSize);
            if (LZ4F_isError(ret)) {
                printf("LZ4F_getFrameInfo error: %s\n", LZ4F_getErrorName(ret));
                goto cleanup;
            }
            /* Allocating enough space for an entire block isn't necessary for
             * correctness, but it allows some memcpy's to be elided.
             */
            dstCapacity = get_block_size(&info);
            dst = malloc(dstCapacity);
            if (!dst) { perror("decompress_file(dst)"); goto cleanup; }
            srcPtr += srcSize;
            srcSize = srcEnd - srcPtr;
        }
        /* Decompress:
         * Continue while there is more input to read and the frame isn't over.
         * If srcPtr == srcEnd then we know that there is no more output left in the
         * internal buffer left to flush.
         */
        while (srcPtr != srcEnd && ret != 0) {
            /* INVARIANT: Any data left in dst has already been written */
            size_t dstSize = dstCapacity;
            ret = LZ4F_decompress(dctx, dst, &dstSize, srcPtr, &srcSize, /* LZ4F_decompressOptions_t */ NULL);
            if (LZ4F_isError(ret)) {
                printf("Decompression error: %s\n", LZ4F_getErrorName(ret));
                goto cleanup;
            }
            /* Flush output */
            if (dstSize != 0){
                /* size_t written = fwrite(dst, 1, dstSize, out); */
                /* printf("Writing %zu bytes\n", dstSize); */
                /* if (written != dstSize) { */
                /* printf("Decompress: Failed to write to file\n"); */
                /* goto cleanup; */
                /* } */
                *outMem = malloc(dstSize);
                memcpy(*outMem, dst, dstSize);
                printf("Copying %zu bytes\n", dstSize);
            }
            /* Update input */
            srcPtr += srcSize;
            srcSize = srcEnd - srcPtr;
        }
    }
    /* Check that there isn't trailing input data after the frame.
     * It is valid to have multiple frames in the same file, but this example
     * doesn't support it.
     */
    ret = fread(src, 1, 1, in);
    if (ret != 0 || !feof(in)) {
        printf("Decompress: Trailing data left in file after frame\n");
        goto cleanup;
    }

cleanup:
    free(src);
    free(dst);
    return LZ4F_freeDecompressionContext(dctx);   /* note : free works on NULL */
}

