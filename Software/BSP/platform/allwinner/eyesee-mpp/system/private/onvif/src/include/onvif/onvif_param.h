/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file onvif_param.h
 * @brief 定义onvif 设置参数描述定义
 * @author id:826
 * @version v0.3
 * @date 2016-09-10
 */

#ifndef ONVIF_PARAM_H
#define ONVIF_PARAM_H

#include <vector>

struct Size {
    int width;
    int height;

    Size() {
        width = 0;
        height = 0;
    }

    Size(int w, int h) {
        width = w;
        height = h;
    }
};

enum RotateMode {
    ROTATE_MODE_OFF = 0,
    ROTATE_MODE_ON,
    ROTATE_MODE_AUTO
};

enum VideoEncodingType {
    ENCODING_JPEG = 0,
    ENCODING_MPEG4,
    ENCODING_H264
};

enum H264Profile {
    H264_PROFILE_BASELINE = 0,
    H264_PROFILE_MAIN,
    H264_PROFILE_EXTENDED,
    H264_PROFILE_HIGH,
};

struct OnvifVideoSourceConfigExt {
    RotateMode rotate_mode;
    void       *ext;
};

struct OnvifVideoBoundRetangle {
    int x;
    int y;
    int width;
    int height;
};

struct OnvifVideoRateLimit {
    int framerate;      /**< the max output framerate in fps */
    int bitrate;        /**< the max output bitrate in kbps */
    int enc_interval;   /**< Interval at which images are encoded and transmitted */
};

struct OnvifVideoSourceConfig {
    OnvifVideoBoundRetangle   bound;
    OnvifVideoSourceConfigExt ext_conf;
};

struct OnvifH264Config {
    int         gov_len;
    H264Profile h264_profile;
};

struct OnvifVideoEncodingConfig {
    VideoEncodingType   enc_type;
    int                 width;
    int                 height;
    float               quality;        /**< encoding quality */
    OnvifVideoRateLimit rate_limit;     /** video rate control */
    OnvifH264Config     h264_config;
    int                 ss_timeout;     /**< session timeout */
};

enum StreamChnType {
    MAIN_CHN_STREAM = 0,
    SUB_CHN_STREAM = 1,
    INVALID_CHN_STREAM = -1
};

struct OnvifSimpleProfile {
    StreamChnType            stream_chn_type;       /**< 0: main chn stream, 1: sub chn stream, -1: invalid */
    OnvifVideoSourceConfig   video_src_cfg;
    OnvifVideoEncodingConfig video_enc_cfg;
};

struct Range {
    union {
        float f_max;
        int   i_max;
    };

    union {
        float f_min;
        int   i_min;
    };
};

struct OnvifVideoEncodingConfigOptions {
    StreamChnType            stream_chn_type;       /**< 0: main chn stream, 1: sub chn stream, -1: invalid */
    Range                    h264_quality_range;
    std::vector<Size>        h264_reslutions;
    Range                    h264_gov_len_range;
    Range                    h264_fps_range;
    Range                    h264_bitrate_range;
    Range                    h264_enc_interval_range;
    std::vector<H264Profile> h264_profiles;
};

#endif // onvif_param.h
