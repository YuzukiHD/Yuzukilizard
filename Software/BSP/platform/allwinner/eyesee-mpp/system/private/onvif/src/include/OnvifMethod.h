/*
 * OnvifMethod.h
 *
 *  Created on: 2017年1月17日
 *      Author: liu
 */

#pragma once

namespace onvif {
// forward
class XmlBuilder;
struct OnvifContext;

// discovery
int Probe(const void *context, XmlBuilder &req, XmlBuilder &resp);

// device management
int GetSystemDataAndTime(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int SetSystemDataAndTime(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int GetCapabilities(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int GetProfiles(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int GetNetworkInterfaces(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int GetDeviceInformation(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);

// imaging
int GetImagingSettings(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int SetImagingSettings(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int GetOptions(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);

// media
int GetVideoEncoderConfiguration(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int SetVideoEncoderConfiguration(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int GetVideoEncoderConfigurationOptions(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int GetStreamUri(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);

// OSD
int GetOSDs(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int GetOSD(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int GetOSDOptions(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int SetOSD(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int SetOSDs(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int CreateOSD(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int DeleteOSD(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);

// analytics
int GetRules(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int CreateRules(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int ModifyRules(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int CreateAnalyticsModules(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int DeleteAnalyticsModules(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int GetAnalyticsModules(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int ModifyAnalyticsModules(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int Subscribe(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);
int UnSubscribe(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp);

}

#define findHandleType(adaptor, main, sub) \
    vector<AWStreamHandle> handles; \
    adaptor->media_ctrl_->GetStreamHandle(handles); \
    OnvifSimpleProfile profile; \
    \
    for (unsigned i = 0; i < handles.size(); i++) { \
        adaptor->media_ctrl_->GetOnvifSimpleProfile(handles[i], profile); \
        if (profile.stream_chn_type == MAIN_CHN_STREAM) { \
            main = handles[i]; \
            LOG("main handle[%d, %d, %d]", main.cam_id, main.vi_chn, main.enc_chn); \
        } else if (profile.stream_chn_type == SUB_CHN_STREAM) { \
            sub = handles[i]; \
            LOG("sub handle[%d, %d, %d]", sub.cam_id, sub.vi_chn, sub.enc_chn); \
        } \
    }

#define findUseHandle(token, adaptor, h) \
    AWStreamHandle mainHandle; \
    AWStreamHandle subHandle; \
    findHandleType(adaptor, mainHandle, subHandle); \
    \
    AWStreamHandle h = mainHandle; \
    if (string::npos != token.find("main")) { \
        h = mainHandle; \
    } else if (string::npos != token.find("sub")) { \
        h = subHandle; \
    } else { \
        LOG("unknown token! token: %s", token.c_str()); \
    } \
    LOG("handle[%d, %d, %d]", h.cam_id, h.vi_chn, h.enc_chn);
