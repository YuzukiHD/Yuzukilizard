/*
 * Discovery.cpp
 *
 *  Created on: 2017年1月17日
 *      Author: liu
 */

#include <string.h>
#include <time.h>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "../include/dev_ctrl_adapter.h"
#include "../include/onvif/onvif_param.h"
#include "../include/OnvifMethod.h"
#include "../include/SoapService.h"
#include "../include/SoapUtils.h"
#include "../include/utils.h"
#include "../include/XmlBuilder.h"

#define BITRATE_MEASURE 1024

using namespace std;
using namespace onvif;

int onvif::Probe(const void *context, XmlBuilder &req, XmlBuilder &resp) {
    DBG("Types:%s", req["Envelope.Body.wsdd:Probe.wsdd:Types"].stringValue().c_str());
    DBG("Scopes:%s", req["Envelope.Body.wsdd:Probe.wsdd:Scopes"].stringValue().c_str());

    char IPAddr[INFO_LENGTH] = { 0 };
    char HwId[INFO_LENGTH] = { 0 };

    OnvifContext *onvifContext = (OnvifContext*) (((SoapService*) context)->getContext());
    DeviceAdapter *adaptor = (DeviceAdapter*) (onvifContext->param);
    AWNetAttr netAttr;
    adaptor->sys_ctrl_->GetNetworkAttr(netAttr);
    sprintf(HwId, "urn:uuid:2419d68a-2dd2-21b2-a205-%s", netAttr.mac);

    SoapService *service = onvifContext->service;
    sprintf(IPAddr, "http://%s:%d/onvif/device_service", service->serviceIp().c_str(), service->servicePort());
    DBG("XAddrs: %s", IPAddr);
    DBG("Probe Types: %s", req["Envelope.Body.wsdd:Probe.wsdd:Types"].stringValue().c_str());

    resp["s:Envelope.s:Body.wsdd:ProbeMatches.wsdd:ProbeMatch.wsa:EndpointReference.wsa:Address"] = HwId;
    resp["s:Envelope.s:Body.wsdd:ProbeMatches.wsdd:ProbeMatch.wsa:EndpointReference.wsa:PortType"] = "ttl";
    resp["s:Envelope.s:Body.wsdd:ProbeMatches.wsdd:ProbeMatch.wsdd:Types"] = "tdn:NetworkVideoTransmitter";
    resp["s:Envelope.s:Body.wsdd:ProbeMatches.wsdd:ProbeMatch.wsdd:Scopes"] = "onvif://www.onvif.org/type/NetworkVideoTransmitter";
    resp["s:Envelope.s:Body.wsdd:ProbeMatches.wsdd:ProbeMatch.wsdd:XAddrs"] = IPAddr;
    resp["s:Envelope.s:Body.wsdd:ProbeMatches.wsdd:ProbeMatch.wsdd:MetadataVersion"] = 1;

    resp["s:Envelope.s:Header.wsa:To"] = "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous";
    resp["s:Envelope.s:Header.wsa:Action"] = "http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches";
    resp["s:Envelope.s:Header.wsa:MessageID"] = HwId + 4;
    resp["s:Envelope.s:Header.wsa:RelatesTo"] = req["Envelope.Header.wsa:MessageID"];
//    mustUnderstand
    resp["s:Envelope.s:Header.wsa:To"].addAttrib("s:mustUnderstand", "true");
    resp["s:Envelope.s:Header.wsa:Action"].addAttrib("s:mustUnderstand", "true");

    return 0;
}

int onvif::GetSystemDataAndTime(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*) (context->param);
    tm utcTime;
    adaptor->sys_ctrl_->GetUTCDateTime(utcTime);

    resp["tds:SystemDateAndTime.tt:DateTimeType"] = "Manual";
    resp["tds:SystemDateAndTime.tt:DaylightSavings"] = "false";
    resp["tds:SystemDateAndTime.tt:TimeZone.tt:TZ"] = utcTime.tm_zone;
    resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Time.tt:Hour"] = utcTime.tm_hour;
    resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Time.tt:Minute"] = utcTime.tm_min;
    resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Time.tt:Second"] = utcTime.tm_sec;
    resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Date.tt:Year"] = utcTime.tm_year;
    resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Date.tt:Month"] = utcTime.tm_mon;
    resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Date.tt:Day"] = utcTime.tm_mday;

    return 0;
}

int onvif::SetSystemDataAndTime(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    SoapUtils::addVisitNamespace(req, "tds:tt", SoapUtils::getAllXmlNameSpace());
//    req.findNamespace("tds", req);

//    {
//    XmlBuilder& resp = req["Envelope.Body.tds:GetSystemDateAndTimeResponse.tds:SystemDateAndTime"];
//    const string& type = resp["tds:SystemDateAndTime.tt:DateTimeType"].stringValue();
//    const string& daylightSavings = resp["tds:SystemDateAndTime.tt:DaylightSavings"].stringValue();
//    const string& tz = resp["tds:SystemDateAndTime.tt:TimeZone.tt:TZ"].stringValue();
//    const int hour = resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Time.tt:Hour"].intValue();
//    const int minute = resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Time.tt:Minute"].intValue();
//    const int second = resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Time.tt:Second"].intValue();
//    const int year = resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Date.tt:Year"].intValue();
//    const int month = resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Date.tt:Month"].intValue();
//    const int day = resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Date.tt:Day"].intValue();
//    LOG("set local time, type: %s,"
//        "tz: %s, %d:%d:%d,"
//        "date: %d-%d-%d",
//        type.c_str(),
//        tz.c_str(), hour, minute, second,
//        year, month, day);
//    }

    XmlBuilder& timeRoot = req["Envelope.Body.tds:SetSystemDateAndTime"];
    const string& type = timeRoot["tds:SetSystemDateAndTime.tds:DateTimeType"].stringValue();
    const string& daylightSavings = timeRoot["tds:SetSystemDateAndTime.tds:DaylightSavings"].stringValue();
    const string& tz = timeRoot["tds:SetSystemDateAndTime.tds:TimeZone.tt:TZ"].stringValue();
    const int hour = timeRoot["tds:SetSystemDateAndTime.tds:UTCDateTime.tt:Time.tt:Hour"].intValue();
    const int minute = timeRoot["tds:SetSystemDateAndTime.tds:UTCDateTime.tt:Time.tt:Minute"].intValue();
    const int second = timeRoot["tds:SetSystemDateAndTime.tds:UTCDateTime.tt:Time.tt:Second"].intValue();
    const int year = timeRoot["tds:SetSystemDateAndTime.tds:UTCDateTime.tt:Date.tt:Year"].intValue();
    const int month = timeRoot["tds:SetSystemDateAndTime.tds:UTCDateTime.tt:Date.tt:Month"].intValue();
    const int day = timeRoot["tds:SetSystemDateAndTime.tds:UTCDateTime.tt:Date.tt:Day"].intValue();
    LOG("set utc time, type: %s," "tz: %s, %d:%d:%d," "date: %d-%d-%d", type.c_str(), tz.c_str(), hour, minute, second, year, month, day);

    DeviceAdapter *adaptor = (DeviceAdapter*) (context->param);
    tm nvrTime;
    memset(&nvrTime, 0, sizeof(tm));
    nvrTime.tm_zone = tz.c_str();
    nvrTime.tm_year = year - TIMESTAMP_YEAR_BASE;
    nvrTime.tm_mon = month - TIMESTAMP_MONTH_BASE;
    nvrTime.tm_mday = day;
    nvrTime.tm_hour = hour;
    nvrTime.tm_min = minute;
    nvrTime.tm_sec = second;

    LOG("nvr time:%s tz:%s", asctime(&nvrTime), tz.c_str());
    adaptor->sys_ctrl_->SetLocalDateTime(nvrTime);

    return 0;
}

int onvif::GetCapabilities(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    char url[INFO_LENGTH] = { 0 };
    SoapService *service = context->service;

    snprintf(url, INFO_LENGTH, "http://%s:%d/onvif/Analytics", service->serviceIp().c_str(), service->servicePort());
    resp["tds:Capabilities.tt:Analytics.tt:XAddr"] = url;
    resp["tds:Capabilities.tt:Analytics.tt:RuleSupport"] = true;
    resp["tds:Capabilities.tt:Analytics.tt:AnalyticsModuleSupport"] = true;

    snprintf(url, INFO_LENGTH, "http://%s:%d/onvif/Device", service->serviceIp().c_str(), service->servicePort());
    resp["tds:Capabilities.tt:Device.tt:XAddr"] = url;
    resp["tds:Capabilities.tt:Device.tt:Network.tt:IPFilter"] = true;
    resp["tds:Capabilities.tt:Device.tt:Network.tt:ZeroConfiguration"] = true;
    resp["tds:Capabilities.tt:Device.tt:Network.tt:IPVersion6"] = true;
    resp["tds:Capabilities.tt:Device.tt:Network.tt:DynDNS"] = true;
    resp["tds:Capabilities.tt:Device.tt:Network.tt:Extension.tt:Dot11Configuration"] = true;

    resp["tds:Capabilities.tt:Device.tt:IO.tt:InputConnection"] = 1;
    resp["tds:Capabilities.tt:Device.tt:IO.tt:RelayOutputs"] = 0;

    XmlBuilder *tls1 = new XmlBuilder("tt:TLS1.1");
    XmlBuilder *tls2 = new XmlBuilder("tt:TLS1.2");
    XmlBuilder *x509token = new XmlBuilder("tt:X.509Token");
    resp["tds:Capabilities.tt:Device.tt:Security"].append(tls1);
    resp["tds:Capabilities.tt:Device.tt:Security"].append(tls2);
    resp["tds:Capabilities.tt:Device.tt:Security"].append(x509token);
    *tls1 = false;
    *tls2 = false;
    *x509token = false;
    resp["tds:Capabilities.tt:Device.tt:Security.tt:OnboardKeyGeneration"] = false;
    resp["tds:Capabilities.tt:Device.tt:Security.tt:AccessPolicyConfig"] = false;
    resp["tds:Capabilities.tt:Device.tt:Security.tt:SAMLToken"] = false;
    resp["tds:Capabilities.tt:Device.tt:Security.tt:KerberosToken"] = false;
    resp["tds:Capabilities.tt:Device.tt:Security.tt:RELToken"] = false;

    snprintf(url, INFO_LENGTH, "http://%s:%d/onvif/Events", service->serviceIp().c_str(), service->servicePort());
    resp["tds:Capabilities.tt:Events.tt:XAddr"] = url;
    resp["tds:Capabilities.tt:Events.tt:WSSubscriptionPolicySupport"] = true;
    resp["tds:Capabilities.tt:Events.tt:WSPullPointSupport"] = false;
    resp["tds:Capabilities.tt:Events.tt:WSPausableSubscriptionManagerInterfaceSupport"] = false;

    snprintf(url, INFO_LENGTH, "http://%s:%d/onvif/Imaging", service->serviceIp().c_str(), service->servicePort());
    resp["tds:Capabilities.tt:Imaging.tt:XAddr"] = url;

    snprintf(url, INFO_LENGTH, "http://%s:%d/onvif/Media", service->serviceIp().c_str(), service->servicePort());
    resp["tds:Capabilities.tt:Media.tt:XAddr"] = url;
    resp["tds:Capabilities.tt:Media.tt:StreamingCapabilities.tt:RTPMulticast"] = false;
    resp["tds:Capabilities.tt:Media.tt:StreamingCapabilities.tt:RTP_TCP"] = true;
    resp["tds:Capabilities.tt:Media.tt:StreamingCapabilities.tt:RTP_RTSP_TCP"] = true;
    resp["tds:Capabilities.tt:Media.tt:Extension.tt:ProfileCapabilities.tt:MaximumNumberOfProfiles"] = 10;

    snprintf(url, INFO_LENGTH, "http://%s:%d/onvif/Extension", service->serviceIp().c_str(), service->servicePort());
    resp["tds:Capabilities.tt:Extension.tt:DeviceIO.tt:XAddr"] = url;
    resp["tds:Capabilities.tt:Extension.tt:DeviceIO.tt:VideoSources"] = 1;
    resp["tds:Capabilities.tt:Extension.tt:DeviceIO.tt:VideoOutputs"] = 0;
    resp["tds:Capabilities.tt:Extension.tt:DeviceIO.tt:AudioSources"] = 0;
    resp["tds:Capabilities.tt:Extension.tt:DeviceIO.tt:AudioOutputs"] = 0;
    resp["tds:Capabilities.tt:Extension.tt:DeviceIO.tt:RelayOutputs"] = 0;

    return 0;
}

int onvif::GetProfiles(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*) (context->param);
    vector<AWStreamHandle> handles;
    AWMediaConfig mediaConfig;
    adaptor->media_ctrl_->GetMediaConfig(mediaConfig);
    LOG("media config map size[%d]", mediaConfig.size());
    adaptor->media_ctrl_->GetStreamHandle(handles);
    int count = handles.size();
    LOG("stream handle count[%d]", count);

#define MAIN_PROFILE_TOKEN "profile_main_ch01"
#define SUB_PROFILE_TOKEN "profile_sub_ch01"
#define MAIN_VIDEO_ENCODER_TOKEN "video_encoder_main_ch01"
#define SUB_VIDEO_ENCODER_TOKEN "video_encoder_sub_ch01"

    string profileToken;
    string videoEncoderToken;

    for (int i = 0; i < count; i++) {
        OnvifSimpleProfile param;
        adaptor->media_ctrl_->GetOnvifSimpleProfile(handles[i], param);

        if (MAIN_CHN_STREAM == param.stream_chn_type) {
            profileToken = MAIN_PROFILE_TOKEN;
            videoEncoderToken = MAIN_VIDEO_ENCODER_TOKEN;
        } else if (SUB_CHN_STREAM == param.stream_chn_type) {
            profileToken = SUB_PROFILE_TOKEN;
            videoEncoderToken = SUB_VIDEO_ENCODER_TOKEN;
        } else {
            continue;
        }

        XmlBuilder *profiles = new XmlBuilder("trt:Profiles");
        resp.append(profiles);

        (*profiles).addAttrib("token", profileToken);
        (*profiles).addAttrib("fixed", "false");
        (*profiles)["trt:Profiles.tt:Name"] = profileToken;
        (*profiles)["trt:Profiles.tt:VideoSourceConfiguration"].addAttrib("token", "video_source_ch01");
        (*profiles)["trt:Profiles.tt:VideoSourceConfiguration"].addAttrib("fixed", "true");
        (*profiles)["trt:Profiles.tt:VideoSourceConfiguration.tt:Name"] = "video_source_ch01";
        (*profiles)["trt:Profiles.tt:VideoSourceConfiguration.tt:UseCount"] = 1;
        (*profiles)["trt:Profiles.tt:VideoSourceConfiguration.tt:SourceToken"] = "video_source_ch01";
        (*profiles)["trt:Profiles.tt:VideoSourceConfiguration.tt:Bounds"].addAttrib("x", param.video_src_cfg.bound.x);
        (*profiles)["trt:Profiles.tt:VideoSourceConfiguration.tt:Bounds"].addAttrib("y", param.video_src_cfg.bound.y);
        (*profiles)["trt:Profiles.tt:VideoSourceConfiguration.tt:Bounds"].addAttrib("width", param.video_src_cfg.bound.width);
        (*profiles)["trt:Profiles.tt:VideoSourceConfiguration.tt:Bounds"].addAttrib("height", param.video_src_cfg.bound.height);

        XmlBuilder *fake = SoapUtils::createSoapTemplate("trt", SoapUtils::getAllXmlNameSpace());
        (*fake)["Envelope.Body.trt:GetVideoEncoderConfiguration.trt:ConfigurationToken"] = videoEncoderToken;
        XmlBuilder &videoConfig = (*profiles)["trt:Profiles.tt:VideoEncoderConfiguration"];
        GetVideoEncoderConfiguration(context, *fake, videoConfig);
        delete fake;


        (*profiles)["trt:Profiles.tt:VideoAnalyticsConfiguration"].addAttrib("token", "video_analytic_ch01");
        (*profiles)["trt:Profiles.tt:VideoAnalyticsConfiguration.tt:Name"] = "video_analytic_ch01";
        XmlBuilder *fake1 = SoapUtils::createSoapTemplate("trt", SoapUtils::getAllXmlNameSpace());
        XmlBuilder &analyticsModules = (*profiles)["trt:Profiles.tt:VideoAnalyticsConfiguration.tt:AnalyticsEngineConfiguration"];
        GetAnalyticsModules(context, *fake1, analyticsModules);

        XmlBuilder &ruleConfig = (*profiles)["trt:Profiles.tt:VideoAnalyticsConfiguration.tt:RuleEngineConfiguration"];
        GetRules(context, *fake1, ruleConfig);
        delete fake1;
    }

    return 0;
}

int onvif::GetVideoEncoderConfigurationOptions(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*) (context->param);
    OnvifVideoEncodingConfigOptions options;

    const string &token = req["Envelope.Body.trt:GetVideoEncoderConfigurationOptions.trt:ConfigurationToken"].stringValue();

    findUseHandle(token, adaptor, h);
    int ret = adaptor->media_ctrl_->GetOnvifVideoEncodingConfigOptions(h, options);
    if (ret < 0) {
        return -1;
    }

    resp["trt:Options.tt:QualityRange.tt:Min"] = options.h264_quality_range.f_min;
    resp["trt:Options.tt:QualityRange.tt:Max"] = options.h264_quality_range.f_max;
    for (unsigned int i = 0; i < options.h264_reslutions.size(); i++) {
        XmlBuilder *resolution1 = new XmlBuilder("tt:ResolutionsAvailable");
        resp["trt:Options.tt:H264"].append(resolution1);
        (*resolution1)["tt:ResolutionsAvailable.tt:Width"] = options.h264_reslutions[i].width;
        (*resolution1)["tt:ResolutionsAvailable.tt:Height"] = options.h264_reslutions[i].height;
    }

    resp["trt:Options.tt:H264.tt:GovLengthRange.tt:Min"] = options.h264_gov_len_range.i_min;
    resp["trt:Options.tt:H264.tt:GovLengthRange.tt:Max"] = options.h264_gov_len_range.i_max;
    resp["trt:Options.tt:H264.tt:FrameRateRange.tt:Min"] = options.h264_fps_range.i_min;
    resp["trt:Options.tt:H264.tt:FrameRateRange.tt:Max"] = options.h264_fps_range.i_max;
    resp["trt:Options.tt:H264.tt:EncodingIntervalRange.tt:Min"] = options.h264_enc_interval_range.i_min;
    resp["trt:Options.tt:H264.tt:EncodingIntervalRange.tt:Max"] = options.h264_enc_interval_range.i_max;
    resp["trt:Options.tt:H264.tt:H264ProfilesSupported"] = "Baseline";

    for (unsigned int i = 0; i < options.h264_reslutions.size(); i++) {
        XmlBuilder *resolution1 = new XmlBuilder("tt:ResolutionsAvailable");
        resp["trt:Options.tt:Extension.tt:H264"].append(resolution1);
        (*resolution1)["tt:ResolutionsAvailable.tt:Width"] = options.h264_reslutions[i].width;
        (*resolution1)["tt:ResolutionsAvailable.tt:Height"] = options.h264_reslutions[i].height;
    }
    resp["trt:Options.tt:Extension.tt:H264.tt:GovLengthRange.tt:Min"] = options.h264_gov_len_range.i_min;
    resp["trt:Options.tt:Extension.tt:H264.tt:GovLengthRange.tt:Max"] = options.h264_gov_len_range.i_max;
    resp["trt:Options.tt:Extension.tt:H264.tt:FrameRateRange.tt:Min"] = options.h264_fps_range.i_min;
    resp["trt:Options.tt:Extension.tt:H264.tt:FrameRateRange.tt:Max"] = options.h264_fps_range.i_max;
    resp["trt:Options.tt:Extension.tt:H264.tt:EncodingIntervalRange.tt:Min"] = options.h264_enc_interval_range.i_min;
    resp["trt:Options.tt:Extension.tt:H264.tt:EncodingIntervalRange.tt:Max"] = options.h264_enc_interval_range.i_max;
    resp["trt:Options.tt:Extension.tt:H264.tt:BitrateRange.tt:Min"] = options.h264_bitrate_range.i_min / BITRATE_MEASURE;
    resp["trt:Options.tt:Extension.tt:H264.tt:BitrateRange.tt:Max"] = options.h264_bitrate_range.i_max / BITRATE_MEASURE;

    return 0;
}

int onvif::GetVideoEncoderConfiguration(const struct OnvifContext *context, XmlBuilder& req, XmlBuilder& resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*) (context->param);
    OnvifSimpleProfile param;

    const string &token = req["Envelope.Body.trt:GetVideoEncoderConfiguration.trt:ConfigurationToken"].stringValue();

    findUseHandle(token, adaptor, h);
    int ret = adaptor->media_ctrl_->GetOnvifSimpleProfile(h, param);
    if (ret < 0) {
        return -1;
    }

    resp.addAttrib("token", token);
    resp.getPath("tt:Name") = token;
    resp.getPath("tt:UseCount") = 1;
    resp.getPath("tt:Encoding") = "H264";
    resp.getPath("tt:Resolution.tt:Width") = param.video_enc_cfg.width;
    resp.getPath("tt:Resolution.tt:Height") = param.video_enc_cfg.height;
    resp.getPath("tt:Quality") = (int) param.video_enc_cfg.quality;
    resp.getPath("tt:RateControl.tt:FrameRateLimit") = param.video_enc_cfg.rate_limit.framerate;
    resp.getPath("tt:RateControl.tt:EncodingInterval") = param.video_enc_cfg.rate_limit.enc_interval;
    resp.getPath("tt:RateControl.tt:BitrateLimit") = param.video_enc_cfg.rate_limit.bitrate / BITRATE_MEASURE;
    resp.getPath("tt:H264.tt:GovLength") = param.video_enc_cfg.h264_config.gov_len;
    resp.getPath("tt:H264.tt:H264Profile") = "Baseline";
    resp.getPath("tt:SessionTimeout") = "PT10S";

    return 0;
}

int onvif::SetVideoEncoderConfiguration(const struct OnvifContext *context, XmlBuilder& req, XmlBuilder& resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*) (context->param);

    XmlBuilder &config = req["Envelope.Body.trt:SetVideoEncoderConfiguration.trt:Configuration"];
//    req.dump();
    const string &token = req["Envelope.Body.trt:SetVideoEncoderConfiguration.trt:ConfigurationToken"].stringValue();
    findUseHandle(token, adaptor, h);

    config["trt:Configuration.tt:Name"].stringValue();
    config["trt:Configuration.tt:UseCount"].stringValue();
    config["trt:Configuration.tt:Encoding"].stringValue();
    int width = config["trt:Configuration.tt:Resolution.tt:Width"].intValue();
    int height = config["trt:Configuration.tt:Resolution.tt:Height"].intValue();
    int quality = config["trt:Configuration.tt:Quality"].intValue();
    int frameRate = config["trt:Configuration.tt:RateControl.tt:FrameRateLimit"].intValue();
    config["trt:Configuration.tt:RateControl.tt:EncodingInterval"].intValue();
    int bitRate = config["trt:Configuration.tt:RateControl.tt:BitrateLimit"].intValue() * BITRATE_MEASURE;
    int govLength = config["trt:Configuration.tt:H264.tt:GovLength"].intValue();
    config["trt:Configuration.tt:H264.tt:H264Profile"].intValue();
    config["trt:Configuration.tt:H264.tt:SessionTimeout"].intValue();

    DBG("width: %d, height: %d, bitrate: %d, framerate: %d", width, height, bitRate, frameRate);
    adaptor->media_ctrl_->SetVideoQuality(h, width, height, bitRate, frameRate);

    return 0;
}

int onvif::GetNetworkInterfaces(const struct OnvifContext *context, XmlBuilder& req, XmlBuilder& resp) {
    return 0;
}

int onvif::GetDeviceInformation(const struct OnvifContext *context, XmlBuilder& req, XmlBuilder& resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*) (context->param);
    int id = 0;
    adaptor->dev_ctrl_->GetDeviceId(&id);

    resp["tds:GetDeviceInformationResponse.tds:Manufacturer"] = "123";
    resp["tds:GetDeviceInformationResponse.tds:Model"] = "heheda";
    resp["tds:GetDeviceInformationResponse.tds:FirmwareVersion"] = "1.0";
    resp["tds:GetDeviceInformationResponse.tds:SerialNumber"] = id;
    resp["tds:GetDeviceInformationResponse.tds:HardwareId"] = id;

    return 0;
}

int onvif::GetOptions(const struct OnvifContext *context, XmlBuilder& req, XmlBuilder& resp) {

    resp["timg:ImagingOptions.tt:BacklightCompensation.tt:Mode"] = "ON";
    resp["timg:ImagingOptions.tt:BacklightCompensation.tt:Level.tt:Min"] = 0;
    resp["timg:ImagingOptions.tt:BacklightCompensation.tt:Level.tt:Max"] = 100;

    resp["timg:ImagingOptions.tt:Brightness.tt:Min"] = 0;
    resp["timg:ImagingOptions.tt:Brightness.tt:Max"] = 100;

    resp["timg:ImagingOptions.tt:ColorSaturation.tt:Min"] = 0;
    resp["timg:ImagingOptions.tt:ColorSaturation.tt:Max"] = 100;

    resp["timg:ImagingOptions.tt:Contrast.tt:Min"] = 0;
    resp["timg:ImagingOptions.tt:Contrast.tt:Max"] = 100;

    resp["timg:ImagingOptions.tt:WideDynamicRange.tt:Min"] = 0;
    resp["timg:ImagingOptions.tt:WideDynamicRange.tt:Max"] = 100;
    resp["timg:ImagingOptions.tt:WideDynamicRange.tt:Mode"] = "ON";

    resp["timg:ImagingOptions.tt:WhiteBalance.tt:Mode"] = "MANUAL";
    resp["timg:ImagingOptions.tt:WhiteBalance.tt:YrGain.tt:Min"] = 0;
    resp["timg:ImagingOptions.tt:WhiteBalance.tt:YrGain.tt:Max"] = 100;
    resp["timg:ImagingOptions.tt:WhiteBalance.tt:YbGain.tt:Min"] = 0;
    resp["timg:ImagingOptions.tt:WhiteBalance.tt:YbGain.tt:Max"] = 100;

    return 0;
}

int onvif::SetImagingSettings(const struct OnvifContext *context, XmlBuilder& req, XmlBuilder& resp) {
    XmlBuilder &settings = req["Envelope.Body.timg:SetImagingSettings"];

    const string& sourceToken = settings["timg:SetImagingSettings.timg:VideoSourceToken"].stringValue();
    int brightness = settings["timg:SetImagingSettings.timg:ImagingSettings.tt:Brightness"].intValue();
    int saturation = settings["timg:SetImagingSettings.timg:ImagingSettings.tt:ColorSaturation"].intValue();
    int contrast = settings["timg:SetImagingSettings.timg:ImagingSettings.tt:Contrast"].intValue();

    DeviceAdapter *adaptor = (DeviceAdapter*) (context->param);
    AWImageColour color;
    const string fakeToken("main");
    findUseHandle(fakeToken, adaptor, h);

    int ret = adaptor->image_ctrl_->GetImageColour(h, color);
    if (ret != 0)
        return -1;
    color.brightness = brightness;
    color.saturation = saturation;
    color.contrast = contrast;
    ret = adaptor->image_ctrl_->SetImageColour(h, color);
    if (ret != 0)
        return -1;

    ret = adaptor->image_ctrl_->SaveImageConfig();
    if (ret != 0)
        return -1;

    DBG("source token: %s", sourceToken.c_str());

    return 0;
}

int onvif::GetImagingSettings(const struct OnvifContext *context, XmlBuilder& req, XmlBuilder& resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*) (context->param);
    AWImageColour color;
    const string fakeToken("main");
    findUseHandle(fakeToken, adaptor, h);
    int ret = adaptor->image_ctrl_->GetImageColour(h, color);
    if (ret != 0)
        return -1;

    resp["timg:ImagingSettings.tt:Brightness"] = color.brightness;
    resp["timg:ImagingSettings.tt:ColorSaturation"] = color.saturation;
    resp["timg:ImagingSettings.tt:Contrast"] = color.contrast;

    return 0;
}

int onvif::GetStreamUri(const struct OnvifContext *context, XmlBuilder& req, XmlBuilder& resp) {
    SoapUtils::addVisitNamespace(req, "trt", SoapUtils::getAllXmlNameSpace());
    const string &profileStr = req["Envelope.Body.trt:GetStreamUri.trt:ProfileToken"].stringValue();
    DeviceAdapter *adaptor = (DeviceAdapter*) (context->param);

    findUseHandle(profileStr, adaptor, h);

    char url[INFO_LENGTH] = { 0 };
    adaptor->media_ctrl_->GetStreamUrl(h, url, INFO_LENGTH);

    resp["trt:MediaUri.tt:Uri"] = url;
    resp["trt:MediaUri.tt:InvalidAfterConnect"] = true;
    resp["trt:MediaUri.tt:InvalidAfterReboot"] = true;
    resp["trt:MediaUri.tt:Timeout"] = "PT10S";

    return 0;
}
