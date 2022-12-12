/******************************************************************************
 Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************/
#include <iostream>
#include "../gsoap-gen/soapStub.h"
#include "../include/utils.h"
#include "dev_ctrl_adapter.h"
#include "OnvifConnector.h"

#define POS_MULTIPLE 5000.0f

// 目前不考虑channel为两位数的情况
int tokenToChannel(char* str) {
    return strtoul(&(str[strlen(str) - 1]), NULL, 10);
}

void channelToToken(char* str, int strSize, int chn, enum TokenType type) {
    switch (type) {
        case TokenTypeProfile:
            sprintf(str, "profile_token_%d", chn);
            break;
        case TokenTypeEncoder:
            sprintf(str, "encoder_token_%d", chn);
            break;
        case TokenTypeVideoSource:
            sprintf(str, "video_source_token");
            break;
    }
}

void channelToName(char* str, int strSize, int chn, enum TokenType type) {
    switch (type) {
        case TokenTypeProfile:
            sprintf(str, "profile_name_%d", chn);
            break;
        case TokenTypeEncoder:
            sprintf(str, "encoder_name_%d", chn);
            break;
        case TokenTypeVideoSource:
            sprintf(str, "video_source_name");
            break;
    }
}

void concatToken(char *dstStr, int dstStrSize, const char *prefix, int chn) {
    snprintf(dstStr, dstStrSize, "%s_%d", prefix, chn);
}

int __trt__GetProfiles(struct soap* _soap, struct _trt__GetProfiles *req, struct _trt__GetProfilesResponse *resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
    /* if(check_username_password(soap)) */
    /* { */
    /* return soap_sender_fault(soap, "Sender Not Authorized", NULL); */
    /* } */
    /* else */
    /* { */
    /* wldebug("passed!\n"); */
    /* } */
    /* soap_wsse_add_Security(_soap); */
#endif

    int i;
    char IPCAddr[INFO_LENGTH];
    OnvifConnector *connector = (OnvifConnector*) _soap->user;
    DeviceAdapter *interface = connector->GetAdaptor();
    sprintf(IPCAddr, "http://%s/onvif/device_service", connector->GetServerIp().c_str());
    int channelNum = 0;
    interface->media_ctrl_->GetVideoChannelCnt(channelNum);
    resp->__sizeProfiles = channelNum;
    resp->Profiles = (struct tt__Profile *) soap_malloc(_soap, sizeof(struct tt__Profile) * channelNum);
    for (int i = 0; i < channelNum; i++) {
        OnvifSimpleProfile profile;
        interface->media_ctrl_->GetOnvifSimpleProfile(i, profile);
        resp->Profiles[i].Name = (char *) soap_malloc(_soap, sizeof(char) * INFO_LENGTH);
        resp->Profiles[i].token = (char *) soap_malloc(_soap, sizeof(char) * INFO_LENGTH);
        channelToName(resp->Profiles[i].Name, INFO_LENGTH, i, TokenTypeProfile);
        channelToToken(resp->Profiles[i].token, INFO_LENGTH, i, TokenTypeProfile);

        resp->Profiles[i].fixed = (enum xsd__boolean *) soap_malloc(_soap, sizeof(enum xsd__boolean));
        *resp->Profiles[i].fixed = xsd__boolean__true_;

        /*VideoEncoderConfiguration*/
        _trt__GetVideoEncoderConfiguration getVideoEncoderConfiguration;
        _trt__GetVideoEncoderConfigurationResponse getVideoEncoderConfigurationResponse;
        MALLOCN(getVideoEncoderConfiguration.ConfigurationToken, char, INFO_LENGTH);
        channelToToken(getVideoEncoderConfiguration.ConfigurationToken,
        INFO_LENGTH, i, TokenTypeEncoder);
        __trt__GetVideoEncoderConfiguration(_soap, &getVideoEncoderConfiguration,
                &getVideoEncoderConfigurationResponse);
        resp->Profiles[i].VideoEncoderConfiguration = getVideoEncoderConfigurationResponse.Configuration;

        /* VideoSourceConfiguration 初始化过程 */
        resp->Profiles[i].VideoSourceConfiguration = (struct tt__VideoSourceConfiguration *) soap_malloc(_soap,
                sizeof(struct tt__VideoSourceConfiguration));
        resp->Profiles[i].VideoSourceConfiguration->Name = (char *) soap_malloc(_soap, sizeof(char) * INFO_LENGTH);
        channelToName(resp->Profiles[i].VideoSourceConfiguration->Name,
        INFO_LENGTH, i, TokenTypeVideoSource);
        resp->Profiles[i].VideoSourceConfiguration->token = (char *) soap_malloc(_soap, sizeof(char) * INFO_LENGTH);
        channelToToken(resp->Profiles[i].VideoSourceConfiguration->token,
        INFO_LENGTH, i, TokenTypeVideoSource);
        resp->Profiles[i].VideoSourceConfiguration->SourceToken = (char *) soap_malloc(_soap,
                sizeof(char) * INFO_LENGTH);
        /*下面的必须与__tmd__GetVideoSources中的token相同*/
        channelToToken(resp->Profiles[i].VideoSourceConfiguration->SourceToken,
        INFO_LENGTH, i, TokenTypeVideoSource);

        resp->Profiles[i].VideoSourceConfiguration->Bounds = (struct tt__IntRectangle *) soap_malloc(_soap,
                sizeof(struct tt__IntRectangle));
        resp->Profiles[i].VideoSourceConfiguration->Bounds->x = profile.video_src_cfg.bound.x;
        resp->Profiles[i].VideoSourceConfiguration->Bounds->y = profile.video_src_cfg.bound.y;
        resp->Profiles[i].VideoSourceConfiguration->Bounds->height = profile.video_src_cfg.bound.height;
        resp->Profiles[i].VideoSourceConfiguration->Bounds->width = profile.video_src_cfg.bound.width;
        resp->Profiles[i].VideoSourceConfiguration->UseCount = 0;
        resp->Profiles[i].VideoSourceConfiguration->Extension = NULL;

        // get VideoAnalyticsConfiguration
        _tad__GetVideoAnalyticsConfiguration getVideoAnalyticsConfiguration;
        char *analyticsToken = NULL;
        MALLOCN(analyticsToken, char, INFO_LENGTH)
        concatToken(analyticsToken, INFO_LENGTH, "VideoAnalyticsConfiguration", i);
        getVideoAnalyticsConfiguration.ConfigurationToken = analyticsToken;
        _tad__GetVideoAnalyticsConfigurationResponse getVideoAnalyticsConfigurationResp;
        __tad__GetVideoAnalyticsConfiguration(_soap, &getVideoAnalyticsConfiguration,
                &getVideoAnalyticsConfigurationResp);
        resp->Profiles[i].VideoAnalyticsConfiguration = getVideoAnalyticsConfigurationResp.Configuration;

        /* 其余的配置设为NULL */
        resp->Profiles[i].AudioEncoderConfiguration = NULL;
        resp->Profiles[i].AudioSourceConfiguration = NULL;
        resp->Profiles[i].PTZConfiguration = NULL;
        resp->Profiles[i].MetadataConfiguration = NULL;
        resp->Profiles[i].Extension = NULL;
    }

    return SOAP_OK;
}

int __trt__GetVideoEncoderConfiguration(struct soap* _soap, struct _trt__GetVideoEncoderConfiguration *req,
        struct _trt__GetVideoEncoderConfigurationResponse *resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
    printf("video encoder token: %s!\n", req->ConfigurationToken);
    /* soap_wsse_add_Security(soap); */
#endif

    MALLOC(resp->Configuration, struct tt__VideoEncoderConfiguration);

    DeviceAdapter *context = getSystemInterface(_soap);
    OnvifSimpleProfile param;
    context->media_ctrl_->GetOnvifSimpleProfile(tokenToChannel(req->ConfigurationToken), param);
    resp->Configuration->token = req->ConfigurationToken;
    resp->Configuration->Name = req->ConfigurationToken;
    resp->Configuration->UseCount = 1;
    resp->Configuration->Encoding = tt__VideoEncoding__H264;
    resp->Configuration->Resolution = (struct tt__VideoResolution *) soap_malloc(_soap,
            sizeof(struct tt__VideoResolution));
    resp->Configuration->Resolution->Width = param.video_enc_cfg.width;
    resp->Configuration->Resolution->Height = param.video_enc_cfg.height;
    resp->Configuration->Quality = param.video_enc_cfg.quality;
    resp->Configuration->RateControl = (struct tt__VideoRateControl *) soap_malloc(_soap,
            sizeof(struct tt__VideoRateControl));
    resp->Configuration->RateControl->FrameRateLimit = param.video_enc_cfg.rate_limit.framerate;
    resp->Configuration->RateControl->EncodingInterval = param.video_enc_cfg.rate_limit.enc_interval;
    resp->Configuration->RateControl->BitrateLimit = param.video_enc_cfg.rate_limit.bitrate / BITRATE_MEASURE;
    resp->Configuration->MPEG4 = NULL;
    resp->Configuration->H264 = (struct tt__H264Configuration *) soap_malloc(_soap,
            sizeof(struct tt__H264Configuration));
    resp->Configuration->H264->GovLength = param.video_enc_cfg.h264_config.gov_len;

    switch (param.video_enc_cfg.h264_config.h264_profile) {
        case H264_PROFILE_BASELINE:
            resp->Configuration->H264->H264Profile = tt__H264Profile__Baseline;
            break;
        case H264_PROFILE_MAIN:
            resp->Configuration->H264->H264Profile = tt__H264Profile__Main;
            break;
        case H264_PROFILE_EXTENDED:
            resp->Configuration->H264->H264Profile = tt__H264Profile__Extended;
            break;
        case H264_PROFILE_HIGH:
            resp->Configuration->H264->H264Profile = tt__H264Profile__High;
            break;
        default:
            resp->Configuration->H264->H264Profile = tt__H264Profile__Main;
            break;
    }
    resp->Configuration->SessionTimeout = param.video_enc_cfg.ss_timeout;

    resp->Configuration->Multicast = NULL;

    return SOAP_OK;
}

int __trt__SetVideoEncoderConfiguration(struct soap* _soap, struct _trt__SetVideoEncoderConfiguration *req,
        struct _trt__SetVideoEncoderConfigurationResponse *resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
    printf("set encode configuration:\n"
            "BitrateLimit : %d\n"
            "EncodingInterval : %d\n"
            "FramedRateLimit : %d\n"
            "Height : %d\n"
            "Width: %d\n"
            "Name : %s\n", req->Configuration->RateControl->BitrateLimit / BITRATE_MEASURE, req->Configuration->RateControl->EncodingInterval,
            req->Configuration->RateControl->FrameRateLimit, req->Configuration->Resolution->Height, req->Configuration->Resolution->Width,
            req->Configuration->Name);
    printf("Quality : %f\n"
            "SessionTimeout : %lld\n"
            "UseCount : %d\n"
            "token : %s\n"
            "Encoding : %d\n", req->Configuration->Quality, req->Configuration->SessionTimeout, req->Configuration->UseCount, req->Configuration->token,
            req->Configuration->Encoding);
#endif

    DeviceAdapter *encoderContext = getSystemInterface(_soap);
    long bitrate = req->Configuration->RateControl->BitrateLimit * BITRATE_MEASURE;
    encoderContext->media_ctrl_->SetVideoQuality(CTRL_TYPE_ONVIF, tokenToChannel(req->Configuration->token),
            req->Configuration->Resolution->Width, req->Configuration->Resolution->Height,
            req->Configuration->RateControl->FrameRateLimit, (int) bitrate);

    return SOAP_OK;
}

int __trt__GetVideoEncoderConfigurationOptions(struct soap* _soap,
        struct _trt__GetVideoEncoderConfigurationOptions* req,
        struct _trt__GetVideoEncoderConfigurationOptionsResponse* resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
    printf("configure token: %s\nprofileToken: %s\n", req->ConfigurationToken, req->ProfileToken);
#endif

    MALLOC(resp->Options, struct tt__VideoEncoderConfigurationOptions);
    resp->Options->JPEG = NULL;
    resp->Options->MPEG4 = NULL;
    MALLOC(resp->Options->QualityRange, struct tt__IntRange);

    int max = 0;
    int min = 0;
    DeviceAdapter *adaptor = getSystemInterface(_soap);
    OnvifVideoEncodingConfigOptions options;
    // hik NVR using Profile token, but dahua NVR don't have this field
    int chn = tokenToChannel(NULL == req->ProfileToken ? req->ConfigurationToken : req->ProfileToken);
    adaptor->media_ctrl_->GetOnvifVideoEncodingConfigOptions(chn, options);
    int resolutionSize = options.h264_reslutions.size();
    resp->Options->QualityRange->Max = options.h264_quality_range.i_max;
    resp->Options->QualityRange->Min = options.h264_quality_range.i_min;
    MALLOC(resp->Options->H264, struct tt__H264Options);

    resp->Options->H264->__sizeResolutionsAvailable = resolutionSize;
    MALLOCN(resp->Options->H264->ResolutionsAvailable, struct tt__VideoResolution, resolutionSize);
    for (int i = 0; i < resolutionSize; i++) {
        resp->Options->H264->ResolutionsAvailable[i].Height = options.h264_reslutions[i].height;
        resp->Options->H264->ResolutionsAvailable[i].Width = options.h264_reslutions[i].width;
    }

    MALLOC(resp->Options->H264->FrameRateRange, struct tt__IntRange);
    resp->Options->H264->FrameRateRange->Max = options.h264_fps_range.i_max;
    resp->Options->H264->FrameRateRange->Min = options.h264_fps_range.i_min;

    MALLOC(resp->Options->H264->GovLengthRange, struct tt__IntRange);
    resp->Options->H264->GovLengthRange->Max = options.h264_gov_len_range.i_max;
    resp->Options->H264->GovLengthRange->Min = options.h264_gov_len_range.i_min;

    MALLOC(resp->Options->H264->EncodingIntervalRange, struct tt__IntRange);
    resp->Options->H264->EncodingIntervalRange->Max = options.h264_enc_interval_range.i_max;
    resp->Options->H264->EncodingIntervalRange->Min = options.h264_enc_interval_range.i_min;

    int h264ProfilesSupported = options.h264_profiles.size();
    printf("h264profile supported: %d\n", h264ProfilesSupported);
    resp->Options->H264->__sizeH264ProfilesSupported = h264ProfilesSupported;
    MALLOCN(resp->Options->H264->H264ProfilesSupported, enum tt__H264Profile, h264ProfilesSupported);
    for (int i = 0; i < h264ProfilesSupported; i++) {
        resp->Options->H264->H264ProfilesSupported[i] = tt__H264Profile__Main;
        switch (options.h264_profiles[i]) {
            case H264_PROFILE_BASELINE:
                resp->Options->H264->H264ProfilesSupported[i] = tt__H264Profile__Baseline;
                break;
            case H264_PROFILE_MAIN:
                resp->Options->H264->H264ProfilesSupported[i] = tt__H264Profile__Main;
                break;
            case H264_PROFILE_EXTENDED:
                resp->Options->H264->H264ProfilesSupported[i] = tt__H264Profile__Extended;
                break;
            case H264_PROFILE_HIGH:
                resp->Options->H264->H264ProfilesSupported[i] = tt__H264Profile__High;
                break;
            default:
                resp->Options->H264->H264ProfilesSupported[i] = tt__H264Profile__Baseline;
                break;
        }
    }

    // Extension part
    MALLOC(resp->Options->Extension, struct tt__VideoEncoderOptionsExtension);
    resp->Options->Extension->Extension = NULL;
    resp->Options->Extension->JPEG = NULL;
    resp->Options->Extension->MPEG4 = NULL;
    MALLOC(resp->Options->Extension->H264, struct tt__H264Options2);
    MALLOC(resp->Options->Extension->H264->BitrateRange, struct tt__IntRange);
    resp->Options->Extension->H264->BitrateRange->Max = options.h264_bitrate_range.i_max / BITRATE_MEASURE;
    resp->Options->Extension->H264->BitrateRange->Min = options.h264_bitrate_range.i_min / BITRATE_MEASURE;

    resp->Options->Extension->H264->__sizeResolutionsAvailable = resolutionSize;
    MALLOCN(resp->Options->Extension->H264->ResolutionsAvailable, struct tt__VideoResolution, resolutionSize);
    for (int i = 0; i < resolutionSize; i++) {
        resp->Options->Extension->H264->ResolutionsAvailable[i].Height = options.h264_reslutions[i].height;
        resp->Options->Extension->H264->ResolutionsAvailable[i].Width = options.h264_reslutions[i].width;
    }

    MALLOC(resp->Options->Extension->H264->GovLengthRange, struct tt__IntRange);
    resp->Options->Extension->H264->GovLengthRange->Max = options.h264_gov_len_range.i_max;
    resp->Options->Extension->H264->GovLengthRange->Min = options.h264_gov_len_range.i_min;
    MALLOC(resp->Options->Extension->H264->FrameRateRange, struct tt__IntRange);
    resp->Options->Extension->H264->FrameRateRange->Max = options.h264_fps_range.i_max;
    resp->Options->Extension->H264->FrameRateRange->Min = options.h264_fps_range.i_min;
    MALLOC(resp->Options->Extension->H264->EncodingIntervalRange, struct tt__IntRange);
    resp->Options->Extension->H264->EncodingIntervalRange->Max = options.h264_enc_interval_range.i_max;
    resp->Options->Extension->H264->EncodingIntervalRange->Min = options.h264_enc_interval_range.i_min;
    resp->Options->Extension->H264->__sizeH264ProfilesSupported = 0;
    resp->Options->Extension->H264->H264ProfilesSupported = NULL;
//    trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264->__sizeH264ProfilesSupported = 1;
//    MALLOCN(trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264->H264ProfilesSupported,
//            enum tt__H264Profile, 1);
//    trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264->H264ProfilesSupported[0] = tt__H264Profile__Main;

    return SOAP_OK;
}

int __trt__GetStreamUri(struct soap* _soap, struct _trt__GetStreamUri *req, struct _trt__GetStreamUriResponse *resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
#endif

    MALLOC(resp->MediaUri, struct tt__MediaUri);
    resp->MediaUri->Uri = (char *) soap_malloc(_soap, sizeof(char) * INFO_LENGTH);
    DeviceAdapter *adaptor = getSystemInterface(_soap);
    OnvifSimpleProfile profile;
    memset(&profile, 0, sizeof(profile));
    adaptor->media_ctrl_->GetOnvifSimpleProfile(tokenToChannel(req->ProfileToken), profile);
    adaptor->media_ctrl_->GetStreamUrl(tokenToChannel(req->ProfileToken), resp->MediaUri->Uri,
    INFO_LENGTH);
    resp->MediaUri->InvalidAfterConnect = xsd__boolean__true_;
    resp->MediaUri->InvalidAfterReboot = xsd__boolean__true_;
    resp->MediaUri->Timeout = profile.video_enc_cfg.ss_timeout;

    return SOAP_OK;
}

//    int time_format;    // 时间显示格式. 0：24小时制， 1：12小时制AM/PM
//    int date_format;    // 日期显示格式. 0：YYYY/MM/DD, 1:MM/DD/YYYY, 2:YYYY-MM-DD, 3:MM-DD-YYYY
static char *convertTimeFormat(int type) {
    char *result = "HH:mm:ss";

    switch (type) {
        case 0:
            result = "HH:mm:ss";
            break;
        case 1:
            result = "hh:mm:ss";
            break;
        default:
            break;
    }

    return result;
}

static char *convertDateFormat(int type) {
    char *result = "yyyy-MM-dd";

    switch (type) {
        case 0:
            result = "yyyy/MM/dd";
            break;
        case 1:
            result = "MM/dd/yyyy";
            break;
        case 2:
            result = "yyyy-MM-dd";
            break;
        case 3:
            result = "yyyy-MM-dd";
            break;
        case 4:
            result = "dd/MM/yyyy";
            break;
        default:
            break;
    }

    return result;
}

enum OSDType {
    OSD_TYPE_NAME, OSD_TYPE_DATE, OSD_TYPE_UNKNOW
};

int getOsdTokenSuffix(enum OSDType type) {
    switch (type) {
        case OSD_TYPE_NAME:
            return 0;
            break;
        case OSD_TYPE_DATE:
            return 1;
            break;
        default:
            return 2;
    }
}

enum OSDType parseOsdTokenType(char *osdToken) {
    int id = tokenToChannel(osdToken);
    switch (id) {
        case 0:
            return OSD_TYPE_NAME;
            break;
        case 1:
            return OSD_TYPE_DATE;
            break;
        default:
            return OSD_TYPE_UNKNOW;
    }
}

int __trt__GetOSDs(struct soap* _soap, struct _trt__GetOSDs *req, struct _trt__GetOSDsResponse *resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
    printf("get OSDs token: %s\n", req->ConfigurationToken);
#endif

    //按照最多的osd数量分配结构，能简化逻辑，但浪费内存
    MALLOCN(resp->OSDs, struct tt__OSDConfiguration, 3);
    for (int i = 0; i < 3; i++) {
        MALLOCN(resp->OSDs[i].token, char, INFO_LENGTH);
        MALLOC(resp->OSDs[i].VideoSourceConfigurationToken, struct tt__OSDReference)
        MALLOC(resp->OSDs[i].TextString, struct tt__OSDTextConfiguration)
        //    MALLOC(trt__GetOSDsResponse->OSDs[i].TextString->BackgroundColor, struct tt__OSDColor)
        MALLOC(resp->OSDs[i].TextString->FontSize, int)
        MALLOC(resp->OSDs[i].Position, struct tt__OSDPosConfiguration)
        MALLOC(resp->OSDs[i].Position->Pos, struct tt__Vector)
        MALLOC(resp->OSDs[i].Position->Pos->x, float)
        MALLOC(resp->OSDs[i].Position->Pos->y, float)
    }

    do {
        int osdCount = 0;
        DeviceAdapter::OverlayControl *overlayCtrl = getSystemInterface(_soap)->overlay_ctrl_;
        int chn = tokenToChannel(req->ConfigurationToken);
        local(AWTimeOsd, timeOsd);
        int ret = overlayCtrl->GetTimeOsd(chn, timeOsd);
        if (ret != 0) {
            break;
        }

        char *sourceToken = NULL;
        if (1 == timeOsd.osd_enable) {
            MALLOCN(sourceToken, char, INFO_LENGTH);
            concatToken(sourceToken, INFO_LENGTH, "VideoSource", chn);
            concatToken(resp->OSDs[osdCount].token, INFO_LENGTH, "OSDToken", getOsdTokenSuffix(OSD_TYPE_DATE));
            resp->OSDs[osdCount].VideoSourceConfigurationToken->__item = sourceToken;
            resp->OSDs[osdCount].Type = tt__OSDType__Text;
            resp->OSDs[osdCount].TextString->BackgroundColor = NULL;
            resp->OSDs[osdCount].TextString->DateFormat = convertDateFormat(timeOsd.date_format);
            resp->OSDs[osdCount].TextString->Extension = NULL;
            resp->OSDs[osdCount].TextString->FontColor = NULL;
            *resp->OSDs[osdCount].TextString->FontSize = 10;
            resp->OSDs[osdCount].TextString->PlainText = NULL;
            resp->OSDs[osdCount].TextString->TimeFormat = convertTimeFormat(timeOsd.time_format);
            resp->OSDs[osdCount].TextString->Type = "DateAndTime";
            resp->OSDs[osdCount].Image = NULL;
            resp->OSDs[osdCount].Extension = NULL;
            resp->OSDs[osdCount].Position->Extension = NULL;
            *resp->OSDs[osdCount].Position->Pos->x = timeOsd.left / POS_MULTIPLE - 1;
            *resp->OSDs[osdCount].Position->Pos->y = timeOsd.top / POS_MULTIPLE - 1;
            resp->OSDs[osdCount].Position->Type = "Custom";

            osdCount++;
        }

        local(AWDeviceOsd, deviceOsd);
        int ret2 = overlayCtrl->GetDeviceOsd(chn, deviceOsd);
        if (ret2 != 0) {
            break;
        }

        if (1 == deviceOsd.osd_enable) {
            concatToken(resp->OSDs[osdCount].token, INFO_LENGTH, "OSDToken", getOsdTokenSuffix(OSD_TYPE_NAME));
            resp->OSDs[osdCount].VideoSourceConfigurationToken->__item = sourceToken;
            resp->OSDs[osdCount].Type = tt__OSDType__Text;
            resp->OSDs[osdCount].TextString->BackgroundColor = NULL;
            resp->OSDs[osdCount].TextString->DateFormat = NULL;
            resp->OSDs[osdCount].TextString->Extension = NULL;
            resp->OSDs[osdCount].TextString->FontColor = NULL;
            *resp->OSDs[osdCount].TextString->FontSize = 10;
            int len = sizeof(deviceOsd.device_name);
            MALLOCN(resp->OSDs[osdCount].TextString->PlainText, char, len);
            strncpy(resp->OSDs[osdCount].TextString->PlainText, deviceOsd.device_name, len);
            resp->OSDs[osdCount].TextString->TimeFormat = NULL;
            resp->OSDs[osdCount].TextString->Type = "Plain";
            resp->OSDs[osdCount].Image = NULL;
            resp->OSDs[osdCount].Extension = NULL;
            resp->OSDs[osdCount].Position->Extension = NULL;
            *resp->OSDs[osdCount].Position->Pos->x = deviceOsd.left / POS_MULTIPLE - 1;
            *resp->OSDs[osdCount].Position->Pos->y = deviceOsd.top / POS_MULTIPLE - 1;
            resp->OSDs[osdCount].Position->Type = "Custom";

            osdCount++;
        }
        resp->__sizeOSDs = osdCount;

        return SOAP_OK;
    } while (0);

    resp->__sizeOSDs = 0;
    resp->OSDs = NULL;
    return SOAP_ERR;
}

int __trt__GetOSD(struct soap* _soap, struct _trt__GetOSD *req, struct _trt__GetOSDResponse *resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
    printf("get osd use token: %s\n", req->OSDToken);
#endif

//    MALLOC(trt__GetOSDResponse->OSD, struct tt__OSDConfiguration)
//    MALLOC(trt__GetOSDResponse->OSD->VideoSourceConfigurationToken, struct tt__OSDReference)
//    MALLOC(trt__GetOSDResponse->OSD->TextString, struct tt__OSDTextConfiguration)
//    MALLOC(trt__GetOSDResponse->OSD->Position, struct tt__OSDPosConfiguration)
//    MALLOC(trt__GetOSDResponse->OSD->Position->Pos, struct tt__Vector)
//    MALLOC(trt__GetOSDResponse->OSD->Position->Pos->x, float)
//    MALLOC(trt__GetOSDResponse->OSD->Position->Pos->y, float)
//
//    trt__GetOSDResponse->OSD->token = trt__GetOSD->OSDToken;
//    trt__GetOSDResponse->OSD->Type = tt__OSDType__Text;
//    trt__GetOSDResponse->OSD->VideoSourceConfigurationToken->__item = "VideoSourceToken";
//    trt__GetOSDResponse->OSD->TextString->TimeFormat = "HH:mm:ss";
//    trt__GetOSDResponse->OSD->TextString->DateFormat = "yyyy-MM-dd";
//    trt__GetOSDResponse->OSD->TextString->PlainText = "heheda";
//    *trt__GetOSDResponse->OSD->Position->Pos->x = -0.3;
//    *trt__GetOSDResponse->OSD->Position->Pos->y = 0.4;
//    trt__GetOSDResponse->OSD->Position->Type = "Custom";
//
//    trt__GetOSDResponse->OSD->Extension = NULL;
//    trt__GetOSDResponse->OSD->Image = NULL;

    return SOAP_OK;
}

int __trt__GetOSDOptions(struct soap* _soap, struct _trt__GetOSDOptions *req,
        struct _trt__GetOSDOptionsResponse *resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
#endif
    MALLOC(resp->OSDOptions, struct tt__OSDConfigurationOptions);

    MALLOC(resp->OSDOptions->Type, enum tt__OSDType);
    resp->OSDOptions->__sizeType = 1;
    resp->OSDOptions->Type[0] = tt__OSDType__Text;

    MALLOC(resp->OSDOptions->TextOption, struct tt__OSDTextOptions);
    MALLOC(resp->OSDOptions->TextOption->FontSizeRange, struct tt__IntRange);
//    MALLOC(trt__GetOSDOptionsResponse->OSDOptions->TextOption->FontColor, int);
//    MALLOC(trt__GetOSDOptionsResponse->OSDOptions->TextOption->Extension, int);
    resp->OSDOptions->TextOption->BackgroundColor = NULL;

    resp->OSDOptions->TextOption->__sizeType = 2;
    MALLOCN(resp->OSDOptions->TextOption->Type, char*, resp->OSDOptions->TextOption->__sizeType);
    resp->OSDOptions->TextOption->Type[0] = "Plain";
    resp->OSDOptions->TextOption->Type[1] = "DateAndTime";

    resp->OSDOptions->TextOption->__sizeTimeFormat = 2;
    MALLOCN(resp->OSDOptions->TextOption->TimeFormat, char*, resp->OSDOptions->TextOption->__sizeTimeFormat);
    resp->OSDOptions->TextOption->TimeFormat[0] = convertTimeFormat(0);
    resp->OSDOptions->TextOption->TimeFormat[1] = convertTimeFormat(1);

    resp->OSDOptions->TextOption->FontSizeRange->Max = 5;
    resp->OSDOptions->TextOption->FontSizeRange->Min = 1;
    resp->OSDOptions->TextOption->FontColor = NULL;
    resp->OSDOptions->TextOption->Extension = NULL;

    resp->OSDOptions->TextOption->__sizeDateFormat = 4;
    MALLOCN(resp->OSDOptions->TextOption->DateFormat, char*, resp->OSDOptions->TextOption->__sizeDateFormat);
    resp->OSDOptions->TextOption->DateFormat[0] = convertDateFormat(0);
    resp->OSDOptions->TextOption->DateFormat[1] = convertDateFormat(1);
    resp->OSDOptions->TextOption->DateFormat[2] = convertDateFormat(2);
    resp->OSDOptions->TextOption->DateFormat[3] = convertDateFormat(3);

    resp->OSDOptions->ImageOption = NULL;
    resp->OSDOptions->Extension = NULL;

    MALLOCN(resp->OSDOptions->PositionOption, char*, 1);
    resp->OSDOptions->__sizePositionOption = 1;
    resp->OSDOptions->PositionOption[0] = "Custom";

    MALLOC(resp->OSDOptions->MaximumNumberOfOSDs, struct tt__MaximumNumberOfOSDs);
    MALLOC(resp->OSDOptions->MaximumNumberOfOSDs->PlainText, int);
    MALLOC(resp->OSDOptions->MaximumNumberOfOSDs->Time, int);
    MALLOC(resp->OSDOptions->MaximumNumberOfOSDs->Image, int);
    MALLOC(resp->OSDOptions->MaximumNumberOfOSDs->Date, int);
    MALLOC(resp->OSDOptions->MaximumNumberOfOSDs->DateAndTime, int);
    resp->OSDOptions->MaximumNumberOfOSDs->Total = 2;
    *resp->OSDOptions->MaximumNumberOfOSDs->PlainText = 1;
    *resp->OSDOptions->MaximumNumberOfOSDs->Time = 0;
    *resp->OSDOptions->MaximumNumberOfOSDs->Image = 0;
    *resp->OSDOptions->MaximumNumberOfOSDs->DateAndTime = 1;
    *resp->OSDOptions->MaximumNumberOfOSDs->Date = 0;

    return SOAP_OK;
}

int __trt__SetOSD(struct soap* _soap, struct _trt__SetOSD *req, struct _trt__SetOSDResponse *resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
    printf("set osd: \n"
            "type: %s\n"
            "token: %s\n"
            "time format: %s\n"
            "date format: %s\n"
            "plain text: %s\n"
            "x: %f\n"
            "y: %f\n"
            "position type: %s\n", req->OSD->Type == tt__OSDType__Text ? "tt__OSDType__Text" : "tt__OSDType__Extended",
            req->OSD->VideoSourceConfigurationToken->__item, req->OSD->TextString->TimeFormat, req->OSD->TextString->DateFormat,
            req->OSD->TextString->PlainText, *req->OSD->Position->Pos->x, *req->OSD->Position->Pos->y, req->OSD->Position->Type);
#endif

    DeviceAdapter::OverlayControl *overlayCtrl = getSystemInterface(_soap)->overlay_ctrl_;

    enum OSDType type = parseOsdTokenType(req->OSD->token);
    switch (type) {
        case OSD_TYPE_DATE:
            local(AWTimeOsd, timeOsd)
            ;
            overlayCtrl->GetTimeOsd(0, timeOsd);
            timeOsd.left = (*req->OSD->Position->Pos->x + 1) * POS_MULTIPLE;
            timeOsd.top = (*req->OSD->Position->Pos->y + 1) * POS_MULTIPLE;
            overlayCtrl->SetTimeOsd(0, timeOsd);
            printf("set time osd use token: %s\n", req->OSD->token);
            break;
        case OSD_TYPE_NAME:
            local(AWDeviceOsd, deviceOsd)
            ;
            overlayCtrl->GetDeviceOsd(0, deviceOsd);
            strncpy(deviceOsd.device_name, req->OSD->TextString->PlainText, OSD_MAX_TEXT_LEN);
            deviceOsd.left = (*req->OSD->Position->Pos->x + 1) * POS_MULTIPLE;
            deviceOsd.top = (*req->OSD->Position->Pos->y + 1) * POS_MULTIPLE;
            overlayCtrl->SetDeviceOsd(0, deviceOsd);
            printf("set device osd use token: %s\n", req->OSD->token);
            break;
        case OSD_TYPE_UNKNOW:
            printf("osd token is unknow type!\n");
            break;
    }

    overlayCtrl->SaveOverlayConfig();
    return SOAP_OK;
}

int __trt__CreateOSD(struct soap* _soap, struct _trt__CreateOSD *req, struct _trt__CreateOSDResponse *resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
    printf("create osd: \n"
            "type: %s\n"
            "token: %s\n"
            "time format: %s\n"
            "date format: %s\n"
            "plain text: %s\n"
            "x: %f\n"
            "y: %f\n"
            "position type: %s\n", req->OSD->Type == tt__OSDType__Text ? "tt__OSDType__Text" : "tt__OSDType__Extended",
            req->OSD->VideoSourceConfigurationToken->__item, req->OSD->TextString->TimeFormat, req->OSD->TextString->DateFormat,
            req->OSD->TextString->PlainText, *req->OSD->Position->Pos->x, *req->OSD->Position->Pos->y, req->OSD->Position->Type);
#endif

    DeviceAdapter::OverlayControl *overlayCtrl = getSystemInterface(_soap)->overlay_ctrl_;

    enum OSDType type;

    if (strcmp(req->OSD->TextString->Type, "Plain") == 0) {
        type = OSD_TYPE_NAME;
    } else if (strcmp(req->OSD->TextString->Type, "DateAndTime") == 0) {
        type = OSD_TYPE_DATE;
    } else {
        type = OSD_TYPE_UNKNOW;
    }

    switch (type) {
        case OSD_TYPE_DATE:
            local(AWTimeOsd, timeOsd)
            ;
            overlayCtrl->GetTimeOsd(0, timeOsd);
            timeOsd.osd_enable = 1;
            overlayCtrl->SetTimeOsd(0, timeOsd);
            break;
        case OSD_TYPE_NAME:
            local(AWDeviceOsd, deviceOsd)
            ;
            overlayCtrl->GetDeviceOsd(0, deviceOsd);
            deviceOsd.osd_enable = 1;
            overlayCtrl->SetDeviceOsd(0, deviceOsd);
            break;
        case OSD_TYPE_UNKNOW:
            printf("osd token is unknow type!\n");
            break;
    }

    overlayCtrl->SaveOverlayConfig();
    return SOAP_OK;
}

int __trt__DeleteOSD(struct soap* _soap, struct _trt__DeleteOSD *req, struct _trt__DeleteOSDResponse *resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
#endif

    DeviceAdapter::OverlayControl *overlayCtrl = getSystemInterface(_soap)->overlay_ctrl_;

    enum OSDType type = parseOsdTokenType(req->OSDToken);
    switch (type) {
        case OSD_TYPE_DATE:
            local(AWTimeOsd, timeOsd)
            ;
            overlayCtrl->GetTimeOsd(0, timeOsd);
            timeOsd.osd_enable = 0;
            overlayCtrl->SetTimeOsd(0, timeOsd);
            break;
        case OSD_TYPE_NAME:
            local(AWDeviceOsd, deviceOsd)
            ;
            overlayCtrl->GetDeviceOsd(0, deviceOsd);
            deviceOsd.osd_enable = 0;
            overlayCtrl->SetDeviceOsd(0, deviceOsd);
            break;
        case OSD_TYPE_UNKNOW:
            printf("osd token is unknow type!\n");
            break;
    }

    overlayCtrl->SaveOverlayConfig();
    return SOAP_OK;
}

int __trt__GetVideoAnalyticsConfigurations(struct soap* _soap, struct _trt__GetVideoAnalyticsConfigurations *req,
        struct _trt__GetVideoAnalyticsConfigurationsResponse *resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
#endif
    resp->__sizeConfigurations = 1;
    MALLOCN(resp->Configurations, struct tt__VideoAnalyticsConfiguration, 1)

    struct _tad__GetVideoAnalyticsConfiguration getVideoAnalyticsConfiguration;
    struct _tad__GetVideoAnalyticsConfigurationResponse getVideoAnalyticsConfigurationResponse;

    for (int i = 0; i < resp->__sizeConfigurations; i++) {
        char *analyticsToken = NULL;
        MALLOCN(analyticsToken, char, INFO_LENGTH)
        concatToken(analyticsToken, INFO_LENGTH, "VideoAnalyticsConfiguration", i);
        getVideoAnalyticsConfiguration.ConfigurationToken = analyticsToken;
        __tad__GetVideoAnalyticsConfiguration(_soap, &getVideoAnalyticsConfiguration,
                &getVideoAnalyticsConfigurationResponse);
        resp->Configurations[i] = *(getVideoAnalyticsConfigurationResponse.Configuration);
    }

    return SOAP_OK;
}
