#ifndef __RESAMPLE_H__
#define __RESAMPLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"//wuying
#include "speex_resampler.h"
#include "arch.h"
#include "os_support.h"


typedef int(*resampler_basic_func)(SpeexResamplerState *, spx_uint32_t, const spx_word16_t *, spx_uint32_t *, spx_word16_t *, spx_uint32_t *);

struct SpeexResamplerState_ {
    spx_uint32_t in_rate;
    spx_uint32_t out_rate;
    spx_uint32_t num_rate;
    spx_uint32_t den_rate;

    int    quality;
    spx_uint32_t nb_channels;
    spx_uint32_t filt_len;
    spx_uint32_t mem_alloc_size;
    spx_uint32_t buffer_size;
    int          int_advance;
    int          frac_advance;
    float  cutoff;
    spx_uint32_t oversample;
    int          initialised;
    int          started;

    /* These are per-channel */
    spx_int32_t  *last_sample;
    spx_uint32_t *samp_frac_num;
    spx_uint32_t *magic_samples;

    spx_word16_t *mem;
    spx_word16_t *sinc_table;
    spx_uint32_t sinc_table_length;
    resampler_basic_func resampler_ptr;

    int    in_stride;
    int    out_stride;
};

EXPORT SpeexResamplerState *speex_resampler_init(spx_uint32_t nb_channels, spx_uint32_t in_rate, spx_uint32_t out_rate, int quality, int *err);
EXPORT int speex_resampler_set_rate(SpeexResamplerState *st, spx_uint32_t in_rate, spx_uint32_t out_rate);
EXPORT int speex_resampler_process_float(SpeexResamplerState *st, spx_uint32_t channel_index, const float *in, spx_uint32_t *in_len, float *out, spx_uint32_t *out_len);
EXPORT void speex_resampler_destroy(SpeexResamplerState *st);




#ifdef __cplusplus
}
#endif

#endif
