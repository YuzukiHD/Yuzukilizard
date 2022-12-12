#include <vip_lite.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#if defined(__linux__)
#include <sys/time.h>
#endif

#define CREATE_NETWORK_FROM_MEMORY 1
//#define CREATE_NETWORK_FROM_FLASH   1
//#define CREATE_NETWORK_FROM_FILE    1

#define MAX_DIMENSION_NUMBER 4
#define MAX_INPUT_OUTPUT_NUM 20
#define MATH_ABS(x) (((x) < 0) ? -(x) : (x))
#define MATH_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MATH_MIN(a, b) (((a) < (b)) ? (a) : (b))

#define MAX_SUPPORT_RUN_NETWORK 128
void *network_buffer[MAX_SUPPORT_RUN_NETWORK] = {VIP_NULL};

const char *usage =
    "vpm_run sample.txt loop_run_count device_id \n\
    sample.txt: to include one ore more network binary graph (NBG) data file resource. "
    " See sample.txt for details.\n"
    "loop_run_count: the number of loop run network.\n"
    "device_id: specify this NBG runs device.\n"
    "example: ./vpm_run sample.txt 1 1, specify the NBG runs on device 1.\n"
    "         ./vpm_run sample.txt 1000, run this network 1000 times.\n";

typedef struct _batch_item
{
    /* batch information. */
    char **base_strings;
    int string_count;
    int nbg_name;
    int input_count;
    int *input_names;
    int output_count;
    int *output_names;
    int golden_count;
    int *golden_names;
    void **golden_data;
    vip_uint32_t *golden_size;

    /* VIP lite buffer objects. */
    vip_network network;
    vip_buffer *input_buffers;
    vip_buffer *output_buffers;

    vip_uint32_t loop_count;
    vip_uint32_t infer_cycle;
    vip_uint32_t infer_time;
    vip_uint64_t total_infer_cycle;
    vip_uint64_t total_infer_time;
} batch_item;

typedef enum _file_type_e
{
    NN_FILE_NONE,
    NN_FILE_TENSOR,
    NN_FILE_BINARY,
    NN_FILE_TEXT
} file_type_e;

#if defined(__linux__)
#define TIME_SLOTS 10
vip_uint64_t time_begin[TIME_SLOTS];
vip_uint64_t time_end[TIME_SLOTS];
static vip_uint64_t GetTime()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return (vip_uint64_t)(time.tv_usec + time.tv_sec * 1000000);
}

static void TimeBegin(int id)
{
    time_begin[id] = GetTime();
}

static void TimeEnd(int id)
{
    time_end[id] = GetTime();
}

static vip_uint64_t TimeGet(int id)
{
    return time_end[id] - time_begin[id];
}
#endif

vip_status_e vip_memset(vip_uint8_t *dst, vip_uint32_t size)
{
    vip_status_e status = VIP_SUCCESS;
#if 0
    vip_uint32_t i = 0;
    for (i = 0; i < size; i++) {
        dst[i] = 0;
    }
#else
    memset(dst, 0, size);
#endif
    return status;
}

vip_status_e vip_memcpy(vip_uint8_t *dst, vip_uint8_t *src, vip_uint32_t size)
{
    vip_status_e status = VIP_SUCCESS;
#if 0
    vip_uint32_t i = 0;
    for (i = 0; i < size; i++) {
        dst[i] = src[i];
    }
#else
    memcpy(dst, src, size);
#endif
    return status;
}

typedef struct
{
    vip_uint8_t *raw_addr;
} aligned_header;

vip_uint8_t *vsi_nn_MallocAlignedBuffer(
    vip_uint32_t mem_size,
    vip_uint32_t align_start_size,
    vip_uint32_t align_block_size)
{
    vip_uint32_t sz;
    long temp;
    vip_uint8_t *raw_addr;
    vip_uint8_t *p;
    vip_uint8_t *align_addr;
    aligned_header *header;

    sz = sizeof(aligned_header) + mem_size + align_start_size + align_block_size;
    raw_addr = (vip_uint8_t *)malloc(sz * sizeof(vip_uint8_t));
    memset(raw_addr, 0, sizeof(vip_uint8_t) * sz);
    p = raw_addr + sizeof(aligned_header);

    temp = (long)(p) % align_start_size;
    if (temp == 0)
    {
        align_addr = p;
    }
    else
    {
        align_addr = p + align_start_size - temp;
    }
    header = (aligned_header *)(align_addr - sizeof(aligned_header));
    header->raw_addr = raw_addr;

    return align_addr;
} /* vsi_nn_MallocAlignedBuffer() */

void vsi_nn_FreeAlignedBuffer(
    vip_uint8_t *handle)
{
    aligned_header *header;
    header = (aligned_header *)(handle - sizeof(aligned_header));
    free(header->raw_addr);
}

unsigned int load_file(const char *name, void *dst)
{
    FILE *fp = fopen(name, "rb");
    unsigned int size = 0;

    if (fp != NULL)
    {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);

        fseek(fp, 0, SEEK_SET);
        size = fread(dst, size, 1, fp);

        fclose(fp);
    }

    return size;
}

unsigned int save_file(const char *name, void *data, unsigned int size)
{
    FILE *fp = fopen(name, "wb+");
    unsigned int saved = 0;

    if (fp != NULL)
    {
        saved = fwrite(data, size, 1, fp);

        fclose(fp);
    }
    else
    {
        printf("Saving file %s failed.\n", name);
    }

    return saved;
}

unsigned int get_file_size(const char *name)
{
    FILE *fp = fopen(name, "rb");
    unsigned int size = 0;

    if (fp != NULL)
    {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);

        fclose(fp);
    }
    else
    {
        printf("Checking file %s failed.\n", name);
    }

    return size;
}

int get_file_type(const char *file_name)
{
    int type = 0;
    const char *ptr;
    char sep = '.';
    unsigned int pos, n;
    char buff[32] = {0};

    ptr = strrchr(file_name, sep);
    pos = ptr - file_name;
    n = strlen(file_name) - (pos + 1);
    strncpy(buff, file_name + (pos + 1), n);

    if (strcmp(buff, "tensor") == 0)
    {
        type = NN_FILE_TENSOR;
    }
    else if (strcmp(buff, "dat") == 0 || !strcmp(buff, "bin"))
    {
        type = NN_FILE_BINARY;
    }
    else if (strcmp(buff, "txt") == 0)
    {
        type = NN_FILE_TEXT;
    }
    else
    {
        printf("unsupported input file type=%s.\n", buff);
    }

    return type;
}

vip_uint32_t type_get_bytes(const vip_enum type)
{
    switch (type)
    {
    case VIP_BUFFER_FORMAT_INT8:
    case VIP_BUFFER_FORMAT_UINT8:
        return 1;
    case VIP_BUFFER_FORMAT_INT16:
    case VIP_BUFFER_FORMAT_UINT16:
    case VIP_BUFFER_FORMAT_FP16:
    case VIP_BUFFER_FORMAT_BFP16:
        return 2;
    case VIP_BUFFER_FORMAT_FP32:
    case VIP_BUFFER_FORMAT_INT32:
    case VIP_BUFFER_FORMAT_UINT32:
        return 4;
    case VIP_BUFFER_FORMAT_FP64:
    case VIP_BUFFER_FORMAT_INT64:
    case VIP_BUFFER_FORMAT_UINT64:
        return 8;

    default:
        return 0;
    }
}

vip_uint32_t get_tensor_size(
    vip_int32_t *shape,
    vip_uint32_t dim_num,
    vip_enum type)
{
    vip_uint32_t sz;
    vip_uint32_t i;
    sz = 0;
    if (NULL == shape || 0 == dim_num)
    {
        return sz;
    }
    sz = 1;
    for (i = 0; i < dim_num; i++)
    {
        sz *= shape[i];
    }
    sz *= type_get_bytes(type);

    return sz;
}

vip_uint32_t get_element_num(
    vip_int32_t *sizes,
    vip_uint32_t num_of_dims,
    vip_enum data_format)
{
    vip_uint32_t num;
    vip_uint32_t sz;
    vip_uint32_t dsize;

    sz = get_tensor_size(sizes, num_of_dims, data_format);
    dsize = type_get_bytes(data_format);
    num = (vip_uint32_t)(sz / dsize);

    return num;
}

vip_int32_t type_is_integer(const vip_enum type)
{
    vip_int32_t ret;
    ret = 0;
    switch (type)
    {
    case VIP_BUFFER_FORMAT_INT8:
    case VIP_BUFFER_FORMAT_INT16:
    case VIP_BUFFER_FORMAT_INT32:
    case VIP_BUFFER_FORMAT_UINT8:
    case VIP_BUFFER_FORMAT_UINT16:
    case VIP_BUFFER_FORMAT_UINT32:
        ret = 1;
        break;
    default:
        break;
    }

    return ret;
}

vip_int32_t type_is_signed(const vip_enum type)
{
    vip_int32_t ret;
    ret = 0;
    switch (type)
    {
    case VIP_BUFFER_FORMAT_INT8:
    case VIP_BUFFER_FORMAT_INT16:
    case VIP_BUFFER_FORMAT_INT32:
    case VIP_BUFFER_FORMAT_BFP16:
    case VIP_BUFFER_FORMAT_FP16:
    case VIP_BUFFER_FORMAT_FP32:
        ret = 1;
        break;
    default:
        break;
    }

    return ret;
}

void type_get_range(vip_enum type, double *max_range, double *min_range)
{
    vip_int32_t bits;
    double from, to;
    from = 0.0;
    to = 0.0;
    bits = type_get_bytes(type) * 8;
    if (type_is_integer(type))
    {
        if (type_is_signed(type))
        {
            from = (double)(-(1L << (bits - 1)));
            to = (double)((1UL << (bits - 1)) - 1);
        }
        else
        {
            from = 0.0;
            to = (double)((1UL << bits) - 1);
        }
    }
    else
    {
        //  TODO: Add float
    }
    if (NULL != max_range)
    {
        *max_range = to;
    }
    if (NULL != min_range)
    {
        *min_range = from;
    }
}

double copy_sign(double number, double sign)
{
    double value = MATH_ABS(number);
    return (sign > 0) ? value : (-value);
}

int math_floorf(double x)
{
    if (x >= 0)
    {
        return (int)x;
    }
    else
    {
        return (int)x - 1;
    }
}

double rint(double x)
{
#define _EPSILON 1e-8
    double decimal;
    double inter;
    int intpart;

    intpart = (int)x;
    decimal = x - intpart;
    inter = (double)intpart;

    if (MATH_ABS((MATH_ABS(decimal) - 0.5f)) < _EPSILON)
    {
        inter += (vip_int32_t)(inter) % 2;
    }
    else
    {
        return copy_sign(math_floorf(MATH_ABS(x) + 0.5f), x);
    }

    return inter;
}

vip_int32_t fp32_to_dfp(const float in, const signed char fl, const vip_enum type)
{
    vip_int32_t data;
    double max_range;
    double min_range;
    type_get_range(type, &max_range, &min_range);
    if (fl > 0)
    {
        data = (vip_int32_t)rint(in * (float)(1 << fl));
    }
    else
    {
        data = (vip_int32_t)rint(in * (1.0f / (float)(1 << -fl)));
    }
    data = MATH_MIN(data, (vip_int32_t)max_range);
    data = MATH_MAX(data, (vip_int32_t)min_range);

    return data;
}

vip_int32_t fp32_to_affine(
    const float in,
    const float scale,
    const int zero_point,
    const vip_enum type)
{
    vip_int32_t data;
    double max_range;
    double min_range;
    type_get_range(type, &max_range, &min_range);
    data = (vip_int32_t)(rint(in / scale) + zero_point);
    data = MATH_MAX((vip_int32_t)min_range, MATH_MIN((vip_int32_t)max_range, data));
    return data;
}

vip_status_e integer_convert(
    const void *src,
    void *dest,
    vip_enum src_dtype,
    vip_enum dst_dtype)
{
    vip_status_e status = VIP_SUCCESS;

    unsigned char all_zeros[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char all_ones[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    vip_uint32_t src_sz = type_get_bytes(src_dtype);
    vip_uint32_t dest_sz = type_get_bytes(dst_dtype);
    unsigned char *buffer = all_zeros;
    if (((vip_int8_t *)src)[src_sz - 1] & 0x80)
    {
        buffer = all_ones;
    }
    memcpy(buffer, src, src_sz);
    memcpy(dest, buffer, dest_sz);

    return status;
}

static unsigned short fp32_to_bfp16_rtne(float in)
{
    /*
    Convert a float point to bfloat16, with round-nearest-to-even as rounding method.
    */
    vip_uint32_t fp32 = *((unsigned int *)&in);
    unsigned short out;

    vip_uint32_t lsb = (fp32 >> 16) & 1; /* Least significant bit of resulting bfloat. */
    vip_uint32_t rounding_bias = 0x7fff + lsb;

    if (0x7FC00000 == in)
    {
        out = 0x7fc0;
    }
    else
    {
        fp32 += rounding_bias;
        out = (unsigned short)(fp32 >> 16);
    }

    return out;
}

unsigned short fp32_to_fp16(float in)
{
    vip_uint32_t fp32 = 0;
    vip_uint32_t t1 = 0;
    vip_uint32_t t2 = 0;
    vip_uint32_t t3 = 0;
    vip_uint32_t fp16 = 0u;

    vip_memcpy((vip_uint8_t *)&fp32, (vip_uint8_t *)&in, sizeof(vip_uint32_t));

    t1 = (fp32 & 0x80000000u) >> 16; /* sign bit. */
    t2 = (fp32 & 0x7F800000u) >> 13; /* Exponent bits */
    t3 = (fp32 & 0x007FE000u) >> 13; /* Mantissa bits, no rounding */

    if (t2 >= 0x023c00u)
    {
        fp16 = t1 | 0x7BFF; /* Don't round to infinity. */
    }
    else if (t2 <= 0x01c000u)
    {
        fp16 = t1;
    }
    else
    {
        t2 -= 0x01c000u;
        fp16 = t1 | t2 | t3;
    }

    return (unsigned short)fp16;
}

vip_status_e float32_to_dtype(
    float src,
    unsigned char *dst,
    const vip_enum data_type,
    const vip_enum quant_format,
    signed char fixed_point_pos,
    float tf_scale,
    vip_int32_t tf_zerop)
{
    vip_status_e status = VIP_SUCCESS;

    switch (data_type)
    {
    case VIP_BUFFER_FORMAT_FP32:
        *(float *)dst = src;
        break;
    case VIP_BUFFER_FORMAT_FP16:
        *(vip_int16_t *)dst = fp32_to_fp16(src);
        break;
    case VIP_BUFFER_FORMAT_BFP16:
        *(vip_int16_t *)dst = fp32_to_bfp16_rtne(src);
        break;
    case VIP_BUFFER_FORMAT_INT8:
    case VIP_BUFFER_FORMAT_UINT8:
    case VIP_BUFFER_FORMAT_INT16:
    {
        vip_int32_t dst_value = 0;
        switch (quant_format)
        {
        case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
            dst_value = fp32_to_dfp(src, fixed_point_pos, data_type);
            break;
        case VIP_BUFFER_QUANTIZE_TF_ASYMM:
            dst_value = fp32_to_affine(src, tf_scale, tf_zerop, data_type);
            break;
        case VIP_BUFFER_QUANTIZE_NONE:
            dst_value = (vip_int32_t)src;
            break;
        default:
            break;
        }
        integer_convert(&dst_value, dst, VIP_BUFFER_FORMAT_INT32, data_type);
    }
    break;
    default:
        printf("unsupported tensor type\n");
        ;
    }

    return status;
}

unsigned char *get_binary_data(
    char *file_name,
    vip_uint32_t *file_size)
{
    unsigned char *tensorData;

    *file_size = get_file_size((const char *)file_name);
    tensorData = (unsigned char *)malloc(*file_size * sizeof(unsigned char));
    load_file(file_name, (void *)tensorData);

    return tensorData;
}

unsigned char *get_tensor_data(
    batch_item *batch,
    char *file_name,
    vip_uint32_t *file_size,
    vip_uint32_t index)
{
    vip_uint32_t sz = 1;
    vip_uint32_t stride = 1;
    vip_int32_t sizes[4];
    vip_uint32_t num_of_dims;
    vip_uint32_t i = 0;
    vip_enum data_format;
    vip_enum quant_format;
    vip_int32_t fixed_point_pos;
    float tf_scale;
    vip_int32_t tf_zerop;
    unsigned char *tensorData = NULL;
    FILE *tensorFile;
    float fval = 0.0;

    tensorFile = fopen(file_name, "rb");

    vip_query_input(batch->network, index, VIP_BUFFER_PROP_NUM_OF_DIMENSION, &num_of_dims);
    vip_query_input(batch->network, index, VIP_BUFFER_PROP_DATA_FORMAT, &data_format);
    vip_query_input(batch->network, index, VIP_BUFFER_PROP_QUANT_FORMAT, &quant_format);
    vip_query_input(batch->network, index, VIP_BUFFER_PROP_FIXED_POINT_POS, &fixed_point_pos);
    vip_query_input(batch->network, index, VIP_BUFFER_PROP_TF_SCALE, &tf_scale);
    vip_query_input(batch->network, index, VIP_BUFFER_PROP_SIZES_OF_DIMENSION, sizes);
    vip_query_input(batch->network, index, VIP_BUFFER_PROP_TF_ZERO_POINT, &tf_zerop);

    sz = get_element_num(sizes, num_of_dims, data_format);
    stride = type_get_bytes(data_format);
    tensorData = (unsigned char *)malloc(stride * sz * sizeof(unsigned char));
    memset(tensorData, 0, stride * sz * sizeof(unsigned char));
    *file_size = stride * sz * sizeof(unsigned char);

    for (i = 0; i < sz; i++)
    {
        fscanf(tensorFile, "%f ", &fval);
        float32_to_dtype(fval, &tensorData[stride * i], data_format, quant_format,
                         fixed_point_pos, tf_scale, tf_zerop);
    }

    fclose(tensorFile);

    return tensorData;
}

void destroy_network(batch_item *batch)
{
    int i = 0;

    if (batch == VIP_NULL)
    {
        printf("failed batch is NULL\n");
        return;
    }

    vip_destroy_network(batch->network);

    for (i = 0; i < batch->input_count; i++)
    {
        vip_destroy_buffer(batch->input_buffers[i]);
    }
    free(batch->input_buffers);

    for (i = 0; i < batch->output_count; i++)
    {
        vip_destroy_buffer(batch->output_buffers[i]);
    }
    free(batch->output_buffers);
    batch->output_buffers = VIP_NULL;
}

void destroy_test_resources(batch_item *batches, vip_int32_t batch_count)
{
    vip_int32_t i = 0, j = 0;
    batch_item *batch = VIP_NULL;

    if (batches == VIP_NULL)
    {
        printf("failed batch is NULL\n");
        return;
    }

    printf("destroy teset resource batch_count=%d\n", batch_count);

    for (j = 0; j < batch_count; j++)
    {
        batch = &batches[j];

        if (batch != VIP_NULL)
        {
            if (batch->input_names != VIP_NULL)
            {
                free(batch->input_names);
                batch->input_names = VIP_NULL;
            }
            if (batch->output_names != VIP_NULL)
            {
                free(batch->output_names);
                batch->output_names = VIP_NULL;
            }
            if (batch->golden_names != VIP_NULL)
            {
                free(batch->golden_names);
                batch->golden_names = VIP_NULL;
            }
        }
        else
        {
            printf("failed to destroy batch=%d\n", j);
        }
    }

    for (i = 0; i < batches->string_count; i++)
    {
        if (batches->base_strings[i] != VIP_NULL)
        {
            free(batches->base_strings[i]);
            batches->base_strings[i] = VIP_NULL;
        }
    }

    if (batches->base_strings != VIP_NULL)
    {
        free(batches->base_strings);
        batches->base_strings = VIP_NULL;
    }

    free(batches);
    batches = VIP_NULL;
}

batch_item *parse_test_file(const char *file_name, int *Count)
{
    static const char *tokens[] = {
        "[network]",
        "[input]",
        "[golden]",
        "[output]"};
    batch_item *batch = VIP_NULL, *cur_batch = VIP_NULL;
    char line_buffer[255] = {0};
    char *line_string = VIP_NULL;
    int line_count = 0;
    int line_len = 0;
    int network_count = 0;
    int i;
    int current_data = 0;
    int first_batch = 1;

    /* Load the batch file as a string buffer. */
    FILE *fp = fopen(file_name, "r");
    if (fp == VIP_NULL)
    {
        printf("failed to open file=%s\n", file_name);
        return VIP_NULL;
    }

    /* Count the networks. */
    while (fgets(line_buffer, sizeof(line_buffer), fp) > 0)
    {
        line_count++;
#if defined(_WIN32)
        if ((line_buffer[strlen(line_buffer) - 2] == '\r') ||
            (line_buffer[strlen(line_buffer) - 2] == '\n'))
        {
            line_buffer[strlen(line_buffer) - 2] = '\0';
        }
#else
        if ((line_buffer[strlen(line_buffer) - 1] == '\r') ||
            (line_buffer[strlen(line_buffer) - 1] == '\n'))
        {
            line_buffer[strlen(line_buffer) - 1] = '\0';
        }
#endif
        else
        {
            line_buffer[strlen(line_buffer) - 1] = '\0';
        }
        if (strcmp(line_buffer, tokens[0]) == 0)
        {
            network_count++;
        }
    }
    *Count = network_count;

    /* Allocate batches. */
    batch = (batch_item *)malloc(sizeof(batch_item) * network_count);
    vip_memset((void *)batch, sizeof(batch_item) * network_count);
    if (batch == VIP_NULL)
    {
        return VIP_NULL;
    }

    /* Setup batch: set up the strings. */
    batch->base_strings = (char **)malloc(sizeof(char *) * line_count);
    batch->string_count = line_count;
    fseek(fp, 0, SEEK_SET);
    line_count = 0;
    cur_batch = batch;

    /* Setup base string. */
    while (fgets(line_buffer, sizeof(line_buffer), fp) > 0)
    {
        line_len = strlen(line_buffer);
        cur_batch->base_strings[line_count] = (char *)malloc(line_len + 1);
        memset(cur_batch->base_strings[line_count], 0, line_len + 1);
        strcpy(cur_batch->base_strings[line_count], line_buffer);
#if defined(_WIN32)
        if (cur_batch->base_strings[line_count][line_len - 2] == '\n' ||
            cur_batch->base_strings[line_count][line_len - 2] == '\r')
        {
            cur_batch->base_strings[line_count][line_len - 2] = '\0';
        }
#else
        if (cur_batch->base_strings[line_count][line_len - 1] == '\n' ||
            cur_batch->base_strings[line_count][line_len - 1] == '\r')
        {
            cur_batch->base_strings[line_count][line_len - 1] = '\0';
        }
#endif
        else
        {
            cur_batch->base_strings[line_count][line_len - 1] = '\0';
        }

        line_count++;
        memset(line_buffer, 0, sizeof(line_buffer));
    }
    fclose(fp);

    /* Locate the nbg strings. */
    cur_batch = batch;
    cur_batch->output_names = NULL; /* Output name is optional. */
    cur_batch->output_count = 0;
    cur_batch->input_count = 0;
    cur_batch->golden_count = 0;
    cur_batch->output_buffers = NULL;
    cur_batch->input_buffers = NULL;
    cur_batch->input_names = NULL;
    cur_batch->golden_names = NULL;

    for (i = 0; i < line_count; i++)
    {
        line_string = batch->base_strings[i];
        /* Parse the string data. */
        if (line_string[0] == '#')
        {
            continue;
        }
        else if (line_string[0] == '[')
        {
            if (strcmp(line_string, tokens[0]) == 0)
            {
                current_data = 1;
                if (first_batch == 0)
                {
                    cur_batch++;
                    cur_batch->base_strings = batch->base_strings;
                    cur_batch->output_count = 0;
                    cur_batch->input_count = 0;
                    cur_batch->golden_count = 0;
                    cur_batch->output_buffers = NULL;
                    cur_batch->input_buffers = NULL;
                    cur_batch->output_names = NULL;
                    cur_batch->input_names = NULL;
                    cur_batch->golden_names = NULL;
                }
                else
                {
                    first_batch = 0;
                }
            }
            else if (strcmp(line_string, tokens[1]) == 0)
            {
                current_data = 2;
            }
            else if (strcmp(line_string, tokens[2]) == 0)
            {
                current_data = 3;
            }
            else if (strcmp(line_string, tokens[3]) == 0)
            {
                current_data = 4;
            }
            else
            {
                printf("Bad batch file. Wrong line @ %d.\n", i);
                free(batch->base_strings);
                free(batch);
                batch = VIP_NULL;
                break;
            }
        }
        else
        {
            switch (current_data)
            {
            case 1: /* Network */
                cur_batch->nbg_name = i;
                break;

            case 2: /* Input */
                /* Count how many inputs and assign it accordingly. */
                {
                    int iCount = 0;
                    int j;
                    for (j = i;; j++)
                    {
                        if (j >= line_count)
                            break;

                        if ((batch->base_strings[j][0] == '#') ||
                            (batch->base_strings[j][0] == '\0'))
                            continue;

                        if (batch->base_strings[j][0] != '[')
                        {
                            iCount++;
                        }
                        else
                        {
                            break;
                        }
                    }

                    cur_batch->input_count = iCount;
                    cur_batch->input_names = (int *)malloc(sizeof(int) * iCount);

                    iCount = 0;
                    for (;; i++)
                    {
                        if (i >= line_count)
                            break;

                        if ((batch->base_strings[i][0] == '#') ||
                            (batch->base_strings[i][0] == '\0'))
                            continue;

                        if (batch->base_strings[i][0] != '[')
                        {
                            cur_batch->input_names[iCount++] = i;
                        }
                        else
                        {
                            i--;
                            break;
                        }
                    }
                }
                break;

            case 3: /* Golden */
            {
                int iCount = 0;
                int j;
                for (j = i;; j++)
                {
                    if (j >= line_count)
                        break;

                    if ((batch->base_strings[j][0] == '#') ||
                        (batch->base_strings[j][0] == '\0'))
                        continue;

                    if (batch->base_strings[j][0] != '[')
                    {
                        iCount++;
                    }
                    else
                    {
                        break;
                    }
                }

                cur_batch->golden_count = iCount;
                cur_batch->golden_names = (int *)malloc(sizeof(int) * iCount);

                iCount = 0;
                for (;; i++)
                {
                    if (i >= line_count)
                        break;

                    if ((batch->base_strings[i][0] == '#') ||
                        (batch->base_strings[i][0] == '\0'))
                        continue;

                    if (batch->base_strings[i][0] != '[')
                    {
                        cur_batch->golden_names[iCount++] = i;
                    }
                    else
                    {
                        i--;
                        break;
                    }
                }
            }
            break;

            case 4: /* Output */
            {
                int iCount = 0;
                int j;
                for (j = i;; j++)
                {
                    if (j >= line_count)
                        break;

                    if ((batch->base_strings[j][0] == '#') ||
                        (batch->base_strings[j][0] == '\0'))
                        continue;

                    if (batch->base_strings[j][0] != '[')
                    {
                        iCount++;
                    }
                    else
                    {
                        break;
                    }
                }

                cur_batch->output_count = iCount;
                cur_batch->output_names = (int *)malloc(sizeof(int) * iCount);

                iCount = 0;
                for (;; i++)
                {
                    if (i >= line_count)
                        break;

                    if ((batch->base_strings[i][0] == '#') ||
                        (batch->base_strings[i][0] == '\0'))
                        continue;

                    if (batch->base_strings[i][0] != '[')
                    {
                        cur_batch->output_names[iCount++] = i;
                    }
                    else
                    {
                        i--;
                        break;
                    }
                }
            }
            break;

            default:
                break;
            }
        }
    }

    return batch;
}

void init_test_resources(batch_item **batches, const char *batchFileName, int *Count)
{
    batch_item *items = VIP_NULL;

    items = parse_test_file(batchFileName, Count);

    *batches = items;
}

vip_status_e query_hardware_info(void)
{
    vip_uint32_t version = vip_get_version();
    vip_uint32_t device_count = 0;
    vip_uint32_t cid = 0;
    vip_uint32_t *core_count = VIP_NULL;
    vip_uint32_t i = 0;

    if (version >= 0x00010601)
    {
        vip_query_hardware(VIP_QUERY_HW_PROP_CID, sizeof(vip_uint32_t), &cid);
        vip_query_hardware(VIP_QUERY_HW_PROP_DEVICE_COUNT, sizeof(vip_uint32_t), &device_count);
        core_count = (vip_uint32_t *)malloc(sizeof(vip_uint32_t) * device_count);
        vip_query_hardware(VIP_QUERY_HW_PROP_CORE_COUNT_EACH_DEVICE,
                           sizeof(vip_uint32_t) * device_count, core_count);
        printf("cid=0x%x, device_count=%d\n", cid, device_count);
        for (i = 0; i < device_count; i++)
        {
            printf("  device[%d] core_count=%d\n", i, core_count[i]);
        }
        free(core_count);
    }
    return VIP_SUCCESS;
}

/* Create the network in the batch. */
vip_status_e create_network(
    batch_item *batch,
    vip_uint32_t device_id,
    vip_uint32_t network_id)
{
    vip_status_e status = VIP_SUCCESS;
    char *file_name = VIP_NULL;
    int file_size = 0;
    int i = 0;
    int input_count = 0;
    vip_buffer_create_params_t param;

    /* Load nbg data. */
    file_name = batch->base_strings[batch->nbg_name];
    file_size = get_file_size((const char *)file_name);
    if (file_size <= 0)
    {
        printf("Network binary file %s can't be found.\n", file_name);
        status = VIP_ERROR_INVALID_ARGUMENTS;
        return status;
    }

#ifdef CREATE_NETWORK_FROM_MEMORY
    network_buffer[network_id] = malloc(file_size);
    load_file(file_name, network_buffer[network_id]);

#if defined(__linux__)
    TimeBegin(1);
#endif

    status = vip_create_network(network_buffer[network_id], file_size, VIP_CREATE_NETWORK_FROM_MEMORY,
                                &batch->network);
    free(network_buffer[network_id]);
    network_buffer[network_id] = VIP_NULL;

#elif CREATE_NETWORK_FROM_FILE
#if defined(__linux__)
    TimeBegin(1);
#endif

    status = vip_create_network(file_name, 0, VIP_CREATE_NETWORK_FROM_FILE, &batch->network);

#elif CREATE_NETWORK_FROM_FLASH
    /* This is a demo code for DDR-less project.
       You don't need to allocate this memory if you are in DDR-less products.
       You can use vip_create_network() function to create a network.
       network_buffer is the staring address of flash */
    network_buffer[network_id] = vsi_nn_MallocAlignedBuffer(file_size, 4096, 4096);
    load_file(file_name, network_buffer[network_id]);

#if defined(__linux__)
    TimeBegin(1);
#endif

    status = vip_create_network(network_buffer[network_id], file_size, VIP_CREATE_NETWORK_FROM_FLASH,
                                &batch->network);
#endif
    if (status != VIP_SUCCESS)
    {
        printf("Network creating failed. Please validate the content of %s.\n", file_name);
        return status;
    }

    /* Create input buffers. */
    vip_query_network(batch->network, VIP_NETWORK_PROP_INPUT_COUNT, &input_count);
    if (input_count != batch->input_count)
    {
        printf("Error: input count mismatch. Required inputs by network: %d, actually provided: %d.\n",
               input_count, batch->input_count);
        status = VIP_ERROR_MISSING_INPUT_OUTPUT;
        return status;
    }

    batch->input_buffers = (vip_buffer *)malloc(sizeof(vip_buffer) * batch->input_count);
    for (i = 0; i < batch->input_count; i++)
    {
        vip_char_t name[256];
        memset(&param, 0, sizeof(param));
        param.memory_type = VIP_BUFFER_MEMORY_TYPE_DEFAULT;
        vip_query_input(batch->network, i, VIP_BUFFER_PROP_DATA_FORMAT, &param.data_format);
        vip_query_input(batch->network, i, VIP_BUFFER_PROP_NUM_OF_DIMENSION, &param.num_of_dims);
        vip_query_input(batch->network, i, VIP_BUFFER_PROP_SIZES_OF_DIMENSION, param.sizes);
        vip_query_input(batch->network, i, VIP_BUFFER_PROP_QUANT_FORMAT, &param.quant_format);
        vip_query_input(batch->network, i, VIP_BUFFER_PROP_NAME, name);
        switch (param.quant_format)
        {
        case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
            vip_query_input(batch->network, i, VIP_BUFFER_PROP_FIXED_POINT_POS,
                            &param.quant_data.dfp.fixed_point_pos);
            break;
        case VIP_BUFFER_QUANTIZE_TF_ASYMM:
            vip_query_input(batch->network, i, VIP_BUFFER_PROP_TF_SCALE,
                            &param.quant_data.affine.scale);
            vip_query_input(batch->network, i, VIP_BUFFER_PROP_TF_ZERO_POINT,
                            &param.quant_data.affine.zeroPoint);
        default:
            break;
        }

        printf("input %d dim %d %d %d %d, data_format=%d, quant_format=%d, name=%s",
               i, param.sizes[0], param.sizes[1], param.sizes[2], param.sizes[3],
               param.data_format, param.quant_format, name);

        switch (param.quant_format)
        {
        case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
            printf(", dfp=%d\n", param.quant_data.dfp.fixed_point_pos);
            break;
        case VIP_BUFFER_QUANTIZE_TF_ASYMM:
            printf(", scale=%f, zero_point=%d\n", param.quant_data.affine.scale,
                   param.quant_data.affine.zeroPoint);
            break;
        default:
            printf(", none-quant\n");
        }

        status = vip_create_buffer(&param, sizeof(param), &batch->input_buffers[i]);
        if (status != VIP_SUCCESS)
        {
            printf("fail to create input %d buffer, status=%d\n", i, status);
            return status;
        }
    }

    /* Create output buffer. */
    vip_query_network(batch->network, VIP_NETWORK_PROP_OUTPUT_COUNT, &batch->output_count);
    batch->output_buffers = (vip_buffer *)malloc(sizeof(vip_buffer) * batch->output_count);
    for (i = 0; i < batch->output_count; i++)
    {
        vip_char_t name[256];
        memset(&param, 0, sizeof(param));
        param.memory_type = VIP_BUFFER_MEMORY_TYPE_DEFAULT;
        vip_query_output(batch->network, i, VIP_BUFFER_PROP_DATA_FORMAT, &param.data_format);
        vip_query_output(batch->network, i, VIP_BUFFER_PROP_NUM_OF_DIMENSION, &param.num_of_dims);
        vip_query_output(batch->network, i, VIP_BUFFER_PROP_SIZES_OF_DIMENSION, param.sizes);
        vip_query_output(batch->network, i, VIP_BUFFER_PROP_QUANT_FORMAT, &param.quant_format);
        vip_query_output(batch->network, i, VIP_BUFFER_PROP_NAME, name);
        switch (param.quant_format)
        {
        case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
            vip_query_output(batch->network, i, VIP_BUFFER_PROP_FIXED_POINT_POS,
                             &param.quant_data.dfp.fixed_point_pos);
            break;
        case VIP_BUFFER_QUANTIZE_TF_ASYMM:
            vip_query_output(batch->network, i, VIP_BUFFER_PROP_TF_SCALE,
                             &param.quant_data.affine.scale);
            vip_query_output(batch->network, i, VIP_BUFFER_PROP_TF_ZERO_POINT,
                             &param.quant_data.affine.zeroPoint);
            break;
        default:
            break;
        }

        printf("ouput %d dim %d %d %d %d, data_format=%d, name=%s",
               i, param.sizes[0], param.sizes[1], param.sizes[2], param.sizes[3],
               param.data_format, name);

        switch (param.quant_format)
        {
        case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
            printf(", dfp=%d\n", param.quant_data.dfp.fixed_point_pos);
            break;
        case VIP_BUFFER_QUANTIZE_TF_ASYMM:
            printf(", scale=%f, zero_point=%d\n", param.quant_data.affine.scale,
                   param.quant_data.affine.zeroPoint);
            break;
        default:
            printf(", none-quant\n");
        }

        status = vip_create_buffer(&param, sizeof(param), &batch->output_buffers[i]);
        if (status != VIP_SUCCESS)
        {
            printf("fail to create output %d buffer, status=%d\n", i, status);
            return status;
        }
    }

#if defined(__linux__)
    TimeEnd(1);
    printf("nbg name=%s\n", file_name);
    printf("create network %d: %lu us.\n", i, (unsigned long)TimeGet(1));
#endif

    {
        vip_uint32_t mem_pool_size = 0;
        vip_query_network(batch->network, VIP_NETWORK_PROP_MEMORY_POOL_SIZE, &mem_pool_size);
        printf("memory pool size=%dbyte\n", mem_pool_size);
    }

    /* the defalt device id is 0. we need chang it when not use device 0.*/
    if (device_id > 0)
    {
        printf("vpm run start set device id.\n");
        status = vip_set_network(batch->network, VIP_NETWORK_PROP_SET_DEVICE_ID, &device_id);
        if (status != VIP_SUCCESS)
        {
            printf("vpm run set device id fail, id = %d.\n", device_id);
        }
        printf("vpm run set device id success, id = %d.\n", device_id);
    }

    return status;
}

vip_status_e load_golden_data(batch_item *batch)
{
    vip_status_e status = VIP_SUCCESS;
    vip_int32_t i = 0;
    char *golden_name = VIP_NULL;

    if (batch->golden_count > 0)
    {
        if (VIP_NULL == batch->golden_data)
        {
            batch->golden_data = malloc(sizeof(void *) * batch->golden_count);
            memset(batch->golden_data, 0, (sizeof(void *) * batch->golden_count));
        }
        if (VIP_NULL == batch->golden_size)
        {
            batch->golden_size = malloc(sizeof(vip_uint32_t *) * batch->golden_count);
        }
    }

    for (i = 0; i < batch->golden_count; i++)
    {
        if (batch->base_strings[batch->golden_names[i]] != VIP_NULL)
        {
            golden_name = batch->base_strings[batch->golden_names[i]];
            printf("read golden file %s\n", golden_name);
            batch->golden_size[i] = get_file_size(golden_name);
            if (0 == batch->golden_size[i])
            {
                printf("    Test output %d failed: data mismatch.\n", i);
                continue;
            }

            batch->golden_data[i] = malloc(batch->golden_size[i]);
            load_file(golden_name, (void *)batch->golden_data[i]);
        }
    }

    return status;
}

vip_status_e free_golden_data(batch_item *batch)
{
    int i = 0;

    for (i = 0; i < batch->golden_count; i++)
    {
        if (batch->golden_data[i] != VIP_NULL)
        {
            free(batch->golden_data[i]);
            batch->golden_data[i] = VIP_NULL;
        }
    }

    if (batch->golden_data != VIP_NULL)
    {
        free(batch->golden_data);
        batch->golden_data = VIP_NULL;
    }
    if (batch->golden_size != VIP_NULL)
    {
        free(batch->golden_size);
        batch->golden_size = VIP_NULL;
    }

    return VIP_SUCCESS;
}

vip_status_e load_input_data(batch_item *batch)
{
    vip_status_e status = VIP_SUCCESS;
    void *data;
    void *file_data = VIP_NULL;
    char *file_name;
    vip_uint32_t file_size;
    vip_uint32_t buff_size;
    int i;

    /* Load input buffer data. */
    for (i = 0; i < batch->input_count; i++)
    {
        file_type_e file_type;
        file_name = batch->base_strings[batch->input_names[i]];
        printf("input %d name: %s\n", i, file_name);
        file_type = get_file_type(file_name);

        switch (file_type)
        {
        case NN_FILE_TENSOR:
            file_data = (void *)get_tensor_data(batch, file_name, &file_size, i);
            break;
        case NN_FILE_BINARY:
            file_data = (void *)get_binary_data(file_name, &file_size);
            break;
        case NN_FILE_TEXT:
            file_data = (void *)get_tensor_data(batch, file_name, &file_size, i);
            break;
        default:
            printf("error input file type\n");
            break;
        }

        data = vip_map_buffer(batch->input_buffers[i]);
        buff_size = vip_get_buffer_size(batch->input_buffers[i]);
        vip_memcpy(data, file_data, buff_size > file_size ? file_size : buff_size);
        vip_unmap_buffer(batch->input_buffers[i]);

        if (file_data != VIP_NULL)
        {
            free(file_data);
            file_data = VIP_NULL;
        }
    }

    return status;
}

/* Create buffers, and configure the netowrk in the batch. */
vip_status_e set_network_input_output(batch_item *batch)
{
    vip_status_e status = VIP_SUCCESS;
    int i = 0;

    /* Load input buffer data. */
    for (i = 0; i < batch->input_count; i++)
    {
        /* Set input. */
        status = vip_set_input(batch->network, i, batch->input_buffers[i]);
        if (status != VIP_SUCCESS)
        {
            printf("fail to set input %d\n", i);
            goto ExitFunc;
        }
    }

    for (i = 0; i < batch->output_count; i++)
    {
        if (batch->output_buffers[i] != VIP_NULL)
        {
            status = vip_set_output(batch->network, i, batch->output_buffers[i]);
            if (status != VIP_SUCCESS)
            {
                printf("fail to set output\n");
                goto ExitFunc;
            }
        }
        else
        {
            printf("fail output %d is null. output_counts=%d\n", i, batch->output_count);
            status = VIP_ERROR_FAILURE;
            goto ExitFunc;
        }
    }

ExitFunc:
    return status;
}

static float int8_to_fp32(signed char val, signed char fixedPointPos)
{
    float result = 0.0f;

    if (fixedPointPos > 0)
    {
        result = (float)val * (1.0f / ((float)(1 << fixedPointPos)));
    }
    else
    {
        result = (float)val * ((float)(1 << -fixedPointPos));
    }

    return result;
}

static float int16_to_fp32(vip_int16_t val, signed char fixedPointPos)
{
    float result = 0.0f;

    if (fixedPointPos > 0)
    {
        result = (float)val * (1.0f / ((float)(1 << fixedPointPos)));
    }
    else
    {
        result = (float)val * ((float)(1 << -fixedPointPos));
    }

    return result;
}
static vip_float_t affine_to_fp32(vip_int32_t val, vip_int32_t zeroPoint, vip_float_t scale)
{
    vip_float_t result = 0.0f;
    result = ((vip_float_t)val - zeroPoint) * scale;
    return result;
}

static vip_float_t uint8_to_fp32(vip_uint8_t val, vip_int32_t zeroPoint, vip_float_t scale)
{
    vip_float_t result = 0.0f;
    result = (val - (vip_uint8_t)zeroPoint) * scale;
    return result;
}

typedef union
{
    unsigned int u;
    float f;
} _fp32_t;

static float fp16_to_fp32(const short in)
{
    const _fp32_t magic = {(254 - 15) << 23};
    const _fp32_t infnan = {(127 + 16) << 23};
    _fp32_t o;
    // Non-sign bits
    o.u = (in & 0x7fff) << 13;
    o.f *= magic.f;
    if (o.f >= infnan.f)
    {
        o.u |= 255 << 23;
    }
    // Sign bit
    o.u |= (in & 0x8000) << 16;
    return o.f;
}

static vip_bool_e get_top(
    float *pf_prob,
    float *pf_max_prob,
    unsigned int *max_class,
    unsigned int out_put_count,
    unsigned int top_num)
{
    unsigned int i, j;

    if (top_num > 10)
        return vip_false_e;

    memset(pf_max_prob, 0, sizeof(float) * top_num);
    memset(max_class, 0xff, sizeof(float) * top_num);
    for (j = 0; j < top_num; j++)
    {
        for (i = 0; i < out_put_count; i++)
        {
            if ((i == *(max_class + 0)) || (i == *(max_class + 1)) || (i == *(max_class + 2)) ||
                (i == *(max_class + 3)) || (i == *(max_class + 4)))
                continue;
            if (pf_prob[i] > *(pf_max_prob + j))
            {
                *(pf_max_prob + j) = pf_prob[i];
                *(max_class + j) = i;
            }
        }
    }

    return vip_true_e;
}

void show_result(
    void *buffer,
    unsigned int count,
    signed int data_type,
    vip_int32_t quant_format,
    unsigned char fix_pos,
    vip_int32_t zeroPoint,
    vip_float_t scale)
{
    unsigned int i;
    unsigned int max_class[5];
    float fMaxProb[5];
    float *outBuf = VIP_NULL;
    short *ptr_fp16 = VIP_NULL;
    vip_int8_t *ptr_int8 = VIP_NULL;
    vip_uint8_t *ptr_uint8 = VIP_NULL;
    vip_int16_t *ptr_int16 = VIP_NULL;
    vip_int32_t *ptr_int32 = VIP_NULL;

    outBuf = (float *)malloc(count * sizeof(float));
    memset(outBuf, 0, count * sizeof(float));
    if (outBuf == NULL)
    {
        printf("Can't malloc space \n");
    }
    if (data_type == VIP_BUFFER_FORMAT_INT8)
    {
        ptr_int8 = (vip_int8_t *)buffer;
        for (i = 0; i < count; i++)
        {
            if (quant_format == VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT)
            {
                outBuf[i] = int8_to_fp32(ptr_int8[i], fix_pos);
            }
            else if (quant_format == VIP_BUFFER_QUANTIZE_TF_ASYMM)
            {
                vip_int32_t src_value = 0;
                integer_convert(&ptr_int8[i], &src_value, VIP_BUFFER_FORMAT_INT8, VIP_BUFFER_FORMAT_INT32);
                outBuf[i] = affine_to_fp32(src_value, zeroPoint, scale);
            }
            else
            {
                outBuf[i] = *((float *)ptr_int8);
            }
        }
    }
    else if (data_type == VIP_BUFFER_FORMAT_FP16)
    {
        ptr_fp16 = (short *)buffer;
        for (i = 0; i < count; i++)
        {
            outBuf[i] = fp16_to_fp32(ptr_fp16[i]);
        }
    }
    else if (data_type == VIP_BUFFER_FORMAT_UINT8)
    {
        ptr_uint8 = (vip_uint8_t *)buffer;
        for (i = 0; i < count; i++)
        {
            outBuf[i] = affine_to_fp32(ptr_uint8[i], zeroPoint, scale);
        }
    }
    else if (data_type == VIP_BUFFER_FORMAT_INT16)
    {
        ptr_int16 = (vip_int16_t *)buffer;
        for (i = 0; i < count; i++)
        {
            outBuf[i] = int16_to_fp32(ptr_int16[i], fix_pos);
        }
    }
    else if (data_type == VIP_BUFFER_FORMAT_INT32)
    {
        ptr_int32 = (vip_int32_t *)buffer;
        if (quant_format == VIP_BUFFER_QUANTIZE_NONE)
        {
            for (i = 0; i < count; i++)
            {
                outBuf[i] = (float)ptr_int32[i];
            }
        }
    }
    else if (data_type == VIP_BUFFER_FORMAT_FP32)
    {
        memcpy(outBuf, buffer, count * sizeof(float));
    }
    else
    {
        printf("not support this format TOP5\n");
        return;
    }

    if (!get_top((float *)outBuf, fMaxProb, max_class, count, 5))
    {
        printf("Fail to show result.\n");
    }

    printf(" --- Top5 ---\n");

    for (i = 0; i < 5; i++)
    {
        printf("%3d: %8.6f\n", max_class[i], (float)fMaxProb[i]);
    }

    free(outBuf);
}

int save_txt_file(
    void *buffer,
    unsigned int ele_size,
    signed int data_type,
    vip_int32_t quant_format,
    unsigned char fix_pos,
    vip_int32_t zeroPoint,
    vip_float_t scale,
    vip_uint32_t index)
{
#define TMPBUF_SZ (512)
    char filename[255] = {'\0'};
    vip_uint32_t i = 0;
    FILE *fp;
    float fp_data = 0.0;
    vip_uint8_t *data = (vip_uint8_t *)buffer;
    vip_uint32_t type_size = type_get_bytes(data_type);
    vip_uint8_t buf[TMPBUF_SZ];
    vip_uint32_t count = 0;

    sprintf(filename, "output_%d.txt", index);
    fp = fopen(filename, "w");

    for (i = 0; i < ele_size; i++)
    {
        if (data_type == VIP_BUFFER_FORMAT_INT8)
        {
            if (quant_format == VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT)
            {
                fp_data = int8_to_fp32(*data, fix_pos);
            }
            else if (quant_format == VIP_BUFFER_QUANTIZE_TF_ASYMM)
            {
                vip_int32_t src_value = 0;
                integer_convert(data, &src_value, VIP_BUFFER_FORMAT_INT8, VIP_BUFFER_FORMAT_INT32);
                fp_data = affine_to_fp32(src_value, zeroPoint, scale);
            }
            else
            {
                fp_data = *((float *)data);
            }
        }
        else if (data_type == VIP_BUFFER_FORMAT_FP16)
        {
            fp_data = fp16_to_fp32(*((short *)data));
        }
        else if (data_type == VIP_BUFFER_FORMAT_UINT8)
        {
            fp_data = uint8_to_fp32(*data, zeroPoint, scale);
        }
        else if (data_type == VIP_BUFFER_FORMAT_INT16)
        {
            fp_data = int16_to_fp32(*((short *)data), fix_pos);
        }
        else if (data_type == VIP_BUFFER_FORMAT_FP32)
        {
            fp_data = *((float *)data);
        }
        else if (data_type == VIP_BUFFER_FORMAT_INT32)
        {
            if (quant_format == VIP_BUFFER_QUANTIZE_NONE)
            {
                fp_data = (float)(*((vip_int32_t *)data));
            }
        }
        else
        {
            printf("not support this format into output.txt file\n");
        }

        data += type_size;

        count += sprintf((char *)&buf[count], "%f%s", fp_data, "\n");

        if ((count + 50) > TMPBUF_SZ)
        {
            fwrite(buf, count, 1, fp);
            count = 0;
        }
    }

    fwrite(buf, count, 1, fp);
    fflush(fp);
    fclose(fp);

    return 0;
}

vip_int32_t inference_profile(
    batch_item *batch,
    vip_uint32_t count)
{
    vip_inference_profile_t profile;
    vip_int32_t ret = 0;
    vip_uint32_t tolerance = 1000; /* 1000us */
    vip_uint32_t time_diff = 0;

    vip_query_network(batch->network, VIP_NETWORK_PROP_PROFILING, &profile);
    printf("profile inference time=%dus, cycle=%d\n", profile.inference_time,
           profile.total_cycle);
    if (1 == count)
    {
        batch->infer_cycle = profile.total_cycle;
        batch->infer_time = profile.inference_time;
    }
    else
    {
        vip_float_t rate = (vip_float_t)batch->infer_cycle / (vip_float_t)profile.total_cycle;
        time_diff = (batch->infer_time > profile.inference_time) ? (batch->infer_time - profile.inference_time) : (profile.inference_time - batch->infer_time);
        if (((rate > 1.05) || (rate < 0.95)) && (time_diff > tolerance))
        {
            ret = -1;
        }
    }

    batch->total_infer_cycle += (vip_uint64_t)batch->infer_cycle;
    batch->total_infer_time += (vip_uint64_t)batch->infer_time;

    return ret;
}

vip_int32_t check_result(batch_item *batch)
{
    char *out_name = VIP_NULL;
    void *out_data = VIP_NULL;
    vip_int32_t j = 0;
    vip_int32_t ret = 0;
    vip_uint32_t k = 0;
    vip_int32_t data_format = 0;
    vip_int32_t output_fp = 0;
    vip_int32_t quant_format = 0;
    vip_int32_t output_counts = 0;
    vip_uint32_t output_size = 0;
    vip_int32_t zeroPoint = 0;
    vip_float_t scale;
    vip_buffer_create_params_t param;

    vip_query_network(batch->network, VIP_NETWORK_PROP_OUTPUT_COUNT, &output_counts);

    for (j = 0; j < output_counts; j++)
    {
        unsigned int output_element = 1;
        memset(&param, 0, sizeof(param));
        vip_query_output(batch->network, j, VIP_BUFFER_PROP_QUANT_FORMAT, &quant_format);
        vip_query_output(batch->network, j, VIP_BUFFER_PROP_TF_SCALE,
                         &param.quant_data.affine.scale);
        scale = param.quant_data.affine.scale;
        vip_query_output(batch->network, j, VIP_BUFFER_PROP_TF_ZERO_POINT,
                         &param.quant_data.affine.zeroPoint);
        zeroPoint = param.quant_data.affine.zeroPoint;
        vip_query_output(batch->network, j, VIP_BUFFER_PROP_DATA_FORMAT,
                         &param.data_format);
        data_format = param.data_format;
        vip_query_output(batch->network, j, VIP_BUFFER_PROP_NUM_OF_DIMENSION,
                         &param.num_of_dims);
        vip_query_output(batch->network, j, VIP_BUFFER_PROP_FIXED_POINT_POS,
                         &param.quant_data.dfp.fixed_point_pos);
        output_fp = param.quant_data.dfp.fixed_point_pos;
        vip_query_output(batch->network, j, VIP_BUFFER_PROP_SIZES_OF_DIMENSION, param.sizes);

        for (k = 0; k < param.num_of_dims; k++)
        {
            output_element *= param.sizes[k];
        }
        output_size = output_element * type_get_bytes(data_format);

        out_data = vip_map_buffer(batch->output_buffers[j]);
        /* save output to binary file */
        if ((batch->output_names != NULL) && (batch->base_strings[batch->output_names[j]] != VIP_NULL))
        {
            out_name = batch->base_strings[batch->output_names[j]];
            save_file(out_name, out_data, batch->golden_size[j]);
        }

#ifdef SAVE_OUTPUT_TXT_FILE
        /* save output to .txt file */
        save_txt_file(out_data, output_element, data_format, quant_format, output_fp, zeroPoint, scale, j);
#endif

        if ((batch->golden_data != VIP_NULL) && (batch->golden_data[j] != VIP_NULL))
        {
            if (output_size <= batch->golden_size[j])
            {
                printf("******* golden TOP5 ********\n");
                show_result(batch->golden_data[j], output_element, data_format,
                            quant_format, output_fp, zeroPoint, scale);
            }
        }
        printf("******* nb TOP5 ********\n");
        show_result(out_data, output_element, data_format, quant_format, output_fp, zeroPoint, scale);

        if ((batch->golden_count) && (batch->golden_data != VIP_NULL) && (batch->golden_data[j] != VIP_NULL))
        {
            /* Check result. */
            if (memcmp(out_data, batch->golden_data[j], batch->golden_size[j]) != 0)
            {
                char name[255] = {'\0'};
                if (batch->output_names != NULL &&
                    batch->output_names[j] > 0)
                {
                    out_name = batch->base_strings[batch->output_names[j]];
                }
                else
                {
                    sprintf(name, "failed_output_%d.bin", j);
                    out_name = name;
                }
                save_file(out_name, out_data, batch->golden_size[j]);
                printf("    Test output %d failed: data mismatch. Output saved in file %s "
                       "for further analysis.\n",
                       j, out_name);
                ret = -1;
                vip_memset(out_data, batch->golden_size[j]);
            }
            else
            {
                vip_memset(out_data, batch->golden_size[j]);
                printf("    Test output %d passed.\n\n", j);
                if ((vip_flush_buffer(batch->output_buffers[j], VIP_BUFFER_OPER_TYPE_FLUSH)) != VIP_SUCCESS)
                {
                    printf("flush output%d cache failed.\n", j);
                }
            }
        }

        vip_unmap_buffer(batch->output_buffers[j]);
    }

    return ret;
}

int main(int argc, char *argv[])
{
    vip_status_e status = VIP_SUCCESS;
    vip_char_t *file_name = VIP_NULL;
    vip_int32_t batch_count = 0;
    vip_int32_t i = 0, k = 0;
    vip_uint32_t loop_count = 1;
    vip_uint32_t count = 0;
    vip_int32_t ret = 0;
    vip_uint32_t version = 0;
    vip_uint32_t device_id = 0;
    batch_item *batchs = VIP_NULL;

    if (argc < 2)
    {
        printf("%s\n", usage);
        return -1;
    }

    printf("%s\n", usage);
    file_name = argv[1];
    printf("test started.\n\n");

    version = vip_get_version();
    printf("init vip lite, driver version=0x%08x...\n", version);

    status = vip_init(1 * 1024 * 1024);
    if (status != VIP_SUCCESS)
    {
        printf("failed to init vip\n");
        ret = -1;
        goto exit;
    }
    printf("vip lite init OK.\n\n");

    query_hardware_info();

    init_test_resources(&batchs, file_name, &batch_count);
    printf("init test resources, batch_count: %d ...\n", batch_count);

    if ((argc >= 3) && (argv[2] != VIP_NULL))
    {
        loop_count = atoi(argv[2]);
        if (loop_count == 0)
        {
            loop_count = 1; /* the number of inference times */
        }
    }
    if ((argc >= 4) && (argv[3] != VIP_NULL))
    {
        device_id = atoi(argv[3]);
    }

    for (i = 0; i < batch_count; i++)
    {
        batchs[i].loop_count = loop_count;
    }

    printf("create/prepare networks ...\n");
    if (batchs != VIP_NULL)
    {
        for (i = 0; i < batch_count; i++)
        {
            printf("batch i=%d, binary name: %s\n", i, batchs[i].base_strings[batchs[i].nbg_name]);
            status = create_network(&batchs[i], device_id, i);
            if (status != VIP_SUCCESS)
            {
                printf("create network %d failed.\n", i);
                ret = -1;
                break;
            }

#if defined(__linux__)
            TimeBegin(2);
#endif
            /* Prepare network. */
            status = vip_prepare_network(batchs[i].network);
            if (status != VIP_SUCCESS)
            {
                printf("fail prpare network, status=%d\n", status);
                ret = -1;
                break;
            }

            load_golden_data(&batchs[i]);
            load_input_data(&batchs[i]);

#if defined(__linux__)
            TimeEnd(2);
            printf("prepare network %d: %lu us.\n", i, (unsigned long)TimeGet(2));
#endif
        }

        /* run network */
        while (count < loop_count)
        {
            count++;
            for (i = 0; i < batch_count; i++)
            {
                printf("batch: %d, loop count: %d\n", i, count);
                status = set_network_input_output(&batchs[i]);
                if (status != VIP_SUCCESS)
                {
                    printf("set network input/output %d failed.\n", i);
                    ret = -1;
                    goto exit;
                }

                printf("start to run network=%s\n", batchs[i].base_strings[batchs[i].nbg_name]);
#if defined(__linux__)
                TimeBegin(0);
#endif
                /* it is only necessary to call vip_flush_buffer() after set vpmdENABLE_FLUSH_CPU_CACHE to 2 */
                for (k = 0; k < batchs[i].input_count; k++)
                {
                    if ((vip_flush_buffer(batchs[i].input_buffers[k], VIP_BUFFER_OPER_TYPE_FLUSH)) != VIP_SUCCESS)
                    {
                        printf("flush input%d cache failed.\n", k);
                    }
                }

                status = vip_run_network(batchs[i].network);
                if (status != VIP_SUCCESS)
                {
                    if (status == VIP_ERROR_CANCELED)
                    {
                        printf("network is canceled.\n");
                        ret = VIP_ERROR_CANCELED;
                        goto exit;
                    }
                    printf("fail to run network, status=%d, batchCount=%d\n", status, i);
                    ret = -2;
                    goto exit;
                }

                for (k = 0; k < batchs[i].output_count; k++)
                {
                    if ((vip_flush_buffer(batchs[i].output_buffers[k], VIP_BUFFER_OPER_TYPE_INVALIDATE)) != VIP_SUCCESS)
                    {
                        printf("flush output%d cache failed.\n", k);
                    }
                }

#if defined(__linux__)
                TimeEnd(0);
                printf("run time for this network %d: %lu us.\n", i, (unsigned long)TimeGet(0));
#endif
                printf("run network done...\n");

                inference_profile(&batchs[i], count);

                ret = check_result(&batchs[i]);
                if (ret != 0)
                {
                    goto exit;
                }
            }
        };

        if (loop_count > 1)
        {
            for (i = 0; i < batch_count; i++)
            {
                printf("batch %d, profile avg inference time=%dus, cycle=%d\n", i,
                       (vip_uint32_t)(batchs[i].total_infer_time / batchs[i].loop_count),
                       (vip_uint32_t)(batchs[i].total_infer_cycle / batchs[i].loop_count));
            }
        }
    }
    else
    {
        printf("failed to read %s\n", file_name);
    }

exit:
    if (batchs != VIP_NULL)
    {
        for (i = 0; i < batch_count; i++)
        {
            free_golden_data(&batchs[i]);

            vip_finish_network(batchs[i].network);

            destroy_network(&batchs[i]);

            if (network_buffer[i] != VIP_NULL)
            {
#if CREATE_NETWORK_FROM_FLASH
                vsi_nn_FreeAlignedBuffer((vip_uint8_t *)network_buffer[i]);
#else
                free(network_buffer[i]);
#endif
                network_buffer[i] = VIP_NULL;
            }
        }
    }

    for (i = 0; i < batch_count; i++)
    {
        if (network_buffer[i] != VIP_NULL)
        {
            free(network_buffer[i]);
            network_buffer[i] = VIP_NULL;
        }
    }

    destroy_test_resources(batchs, batch_count);

    status = vip_destroy();
    if (status != VIP_SUCCESS)
    {
        printf("fail to destory vip\n");
    }

    return ret;
}
