/*
 * Discovery.cpp
 *
 *  Created on: 2017年1月17日
 *      Author: liu
 */
#include <utils.h>
#include <iostream>
#include <time.h>
#include <string.h>
#include "OnvifMethod.h"
#include "XmlBuilder.h"
#include "SoapService.h"
#include "SoapUtils.h"
#include "dev_ctrl_adapter.h"
#include "OnvifConnector.h"

#define BITRATE_MEASURE 1024

using namespace std;
using namespace onvif;

int onvif::Probe(const void *context, XmlBuilder &req, XmlBuilder &resp) {
    DBG("Types:%s", req["Envelope.Body.wsdd:Probe.wsdd:Types"].stringValue().c_str());
    DBG("Scopes:%s", req["Envelope.Body.wsdd:Probe.wsdd:Scopes"].stringValue().c_str());

    char IPAddr[INFO_LENGTH] = {0};
    char HwId[INFO_LENGTH]   = {0};

    OnvifContext *onvifContext = (OnvifContext*)(((SoapService*) context)->getContext());
    DeviceAdapter *adaptor = (DeviceAdapter*)(onvifContext->param);
    AWNetAttr netAttr;
    adaptor->sys_ctrl_->GetNetworkAttr(netAttr);
    sprintf(HwId, "urn:uuid:2419d68a-2dd2-21b2-a205-%s", netAttr.mac);

    SoapService *service = onvifContext->service;
    sprintf(IPAddr, "http://%s:%d/onvif/device_service", service->serviceIp().c_str(), service->servicePort());
    DBG("XAddrs: %s", IPAddr);
    DBG("Probe Types: %s", req["Envelope.Body.wsdd:Probe.wsdd:Types"].stringValue().c_str());

    resp["s:Envelope.s:Body.wsdd:ProbeMatches.wsdd:ProbeMatch.wsa:EndpointReference.wsa:Address"]  = HwId;
    resp["s:Envelope.s:Body.wsdd:ProbeMatches.wsdd:ProbeMatch.wsa:EndpointReference.wsa:PortType"] = "ttl";
    resp["s:Envelope.s:Body.wsdd:ProbeMatches.wsdd:ProbeMatch.wsdd:Types"]                         = "tdn:NetworkVideoTransmitter";
    resp["s:Envelope.s:Body.wsdd:ProbeMatches.wsdd:ProbeMatch.wsdd:Scopes"]                        = "onvif://www.onvif.org/type/NetworkVideoTransmitter";
    resp["s:Envelope.s:Body.wsdd:ProbeMatches.wsdd:ProbeMatch.wsdd:XAddrs"]                        = IPAddr;
    resp["s:Envelope.s:Body.wsdd:ProbeMatches.wsdd:ProbeMatch.wsdd:MetadataVersion"]               = 1;

    resp["s:Envelope.s:Header.wsa:To"]        = "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous";
    resp["s:Envelope.s:Header.wsa:Action"]    = "http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches";
    resp["s:Envelope.s:Header.wsa:MessageID"] = HwId + 4;
    resp["s:Envelope.s:Header.wsa:RelatesTo"] = req["Envelope.Header.wsa:MessageID"];
//    mustUnderstand
    resp["s:Envelope.s:Header.wsa:To"]    .addAttrib("s:mustUnderstand", "true");
    resp["s:Envelope.s:Header.wsa:Action"].addAttrib("s:mustUnderstand", "true");

    return 0;
}

int onvif::GetSystemDataAndTime(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*)(context->param);
    tm utcTime;
    adaptor->sys_ctrl_->GetUTCDateTime(utcTime);

    resp["tds:SystemDateAndTime.tt:DateTimeType"]                  = "Manual";
    resp["tds:SystemDateAndTime.tt:DaylightSavings"]               = "false";
    resp["tds:SystemDateAndTime.tt:TimeZone.tt:TZ"]                = utcTime.tm_zone;
    resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Time.tt:Hour"]   = utcTime.tm_hour;
    resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Time.tt:Minute"] = utcTime.tm_min;
    resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Time.tt:Second"] = utcTime.tm_sec;
    resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Date.tt:Year"]   = utcTime.tm_year;
    resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Date.tt:Month"]  = utcTime.tm_mon;
    resp["tds:SystemDateAndTime.tt:UTCDateTime.tt:Date.tt:Day"]    = utcTime.tm_mday;

    return 0;
}

int onvif::SetSystemDataAndTime(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    SoapUtils::addVisitNamespace(req, SoapUtils::getAllXmlNameSpace());
    req.findNamespace("tds", req);

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
    const string& type            = timeRoot["tds:SetSystemDateAndTime.tds:DateTimeType"].stringValue();
    const string& daylightSavings = timeRoot["tds:SetSystemDateAndTime.tds:DaylightSavings"].stringValue();
    const string& tz              = timeRoot["tds:SetSystemDateAndTime.tds:TimeZone.tt:TZ"].stringValue();
    const int hour                = timeRoot["tds:SetSystemDateAndTime.tds:UTCDateTime.tt:Time.tt:Hour"].intValue();
    const int minute              = timeRoot["tds:SetSystemDateAndTime.tds:UTCDateTime.tt:Time.tt:Minute"].intValue();
    const int second              = timeRoot["tds:SetSystemDateAndTime.tds:UTCDateTime.tt:Time.tt:Second"].intValue();
    const int year                = timeRoot["tds:SetSystemDateAndTime.tds:UTCDateTime.tt:Date.tt:Year"].intValue();
    const int month               = timeRoot["tds:SetSystemDateAndTime.tds:UTCDateTime.tt:Date.tt:Month"].intValue();
    const int day                 = timeRoot["tds:SetSystemDateAndTime.tds:UTCDateTime.tt:Date.tt:Day"].intValue();
    LOG("set utc time, type: %s," "tz: %s, %d:%d:%d," "date: %d-%d-%d", type.c_str(), tz.c_str(), hour, minute, second,
            year, month, day);

    DeviceAdapter *adaptor = (DeviceAdapter*)(context->param);
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
    char url[INFO_LENGTH] = {0};
    SoapService *service = context->service;

    snprintf(url, INFO_LENGTH, "http://%s:%d/onvif/Analytics", service->serviceIp().c_str(), service->servicePort());
    resp["tds:Capabilities.tt:Analytics.tt:XAddr"]                  = url;
    resp["tds:Capabilities.tt:Analytics.tt:RuleSupport"]            = true;
    resp["tds:Capabilities.tt:Analytics.tt:AnalyticsModuleSupport"] = true;

    snprintf(url, INFO_LENGTH, "http://%s:%d/onvif/Device", service->serviceIp().c_str(), service->servicePort());
    resp["tds:Capabilities.tt:Device.tt:XAddr"]                                      = url;
    resp["tds:Capabilities.tt:Device.tt:Network.tt:IPFilter"]                        = true;
    resp["tds:Capabilities.tt:Device.tt:Network.tt:ZeroConfiguration"]               = true;
    resp["tds:Capabilities.tt:Device.tt:Network.tt:IPVersion6"]                      = true;
    resp["tds:Capabilities.tt:Device.tt:Network.tt:DynDNS"]                          = true;
    resp["tds:Capabilities.tt:Device.tt:Network.tt:Extension.tt:Dot11Configuration"] = true;

    resp["tds:Capabilities.tt:Device.tt:IO.tt:InputConnection"] = 1;
    resp["tds:Capabilities.tt:Device.tt:IO.tt:RelayOutputs"] = 0;

    XmlBuilder *tls1      = new XmlBuilder("tt:TLS1.1");
    XmlBuilder *tls2      = new XmlBuilder("tt:TLS1.2");
    XmlBuilder *x509token = new XmlBuilder("tt:X.509Token");
    resp["tds:Capabilities.tt:Device.tt:Security"].append(tls1);
    resp["tds:Capabilities.tt:Device.tt:Security"].append(tls2);
    resp["tds:Capabilities.tt:Device.tt:Security"].append(x509token);
    *tls1      = false;
    *tls2      = false;
    *x509token = false;
    resp["tds:Capabilities.tt:Device.tt:Security.tt:OnboardKeyGeneration"] = false;
    resp["tds:Capabilities.tt:Device.tt:Security.tt:AccessPolicyConfig"]   = false;
    resp["tds:Capabilities.tt:Device.tt:Security.tt:SAMLToken"]            = false;
    resp["tds:Capabilities.tt:Device.tt:Security.tt:KerberosToken"]        = false;
    resp["tds:Capabilities.tt:Device.tt:Security.tt:RELToken"]             = false;

    snprintf(url, INFO_LENGTH, "http://%s:%d/onvif/Events", service->serviceIp().c_str(), service->servicePort());
    resp["tds:Capabilities.tt:Events.tt:XAddr"]                                         = url;
    resp["tds:Capabilities.tt:Events.tt:WSSubscriptionPolicySupport"]                   = true;
    resp["tds:Capabilities.tt:Events.tt:WSPullPointSupport"]                            = false;
    resp["tds:Capabilities.tt:Events.tt:WSPausableSubscriptionManagerInterfaceSupport"] = false;

    snprintf(url, INFO_LENGTH, "http://%s:%d/onvif/Imaging", service->serviceIp().c_str(), service->servicePort());
    resp["tds:Capabilities.tt:Imaging.tt:XAddr"]                                                     = url;

    snprintf(url, INFO_LENGTH, "http://%s:%d/onvif/Media", service->serviceIp().c_str(), service->servicePort());
    resp["tds:Capabilities.tt:Media.tt:XAddr"]                                                       = url;
    resp["tds:Capabilities.tt:Media.tt:StreamingCapabilities.tt:RTPMulticast"]                       = true;
    resp["tds:Capabilities.tt:Media.tt:StreamingCapabilities.tt:RTP_TCP"]                            = true;
    resp["tds:Capabilities.tt:Media.tt:StreamingCapabilities.tt:RTP_RTSP_TCP"]                       = true;
    resp["tds:Capabilities.tt:Media.tt:Extension.tt:ProfileCapabilities.tt:MaximumNumberOfProfiles"] = 10;

    snprintf(url, INFO_LENGTH, "http://%s:%d/onvif/Extension", service->serviceIp().c_str(), service->servicePort());
    resp["tds:Capabilities.tt:Extension.tt:DeviceIO.tt:XAddr"]        = url;
    resp["tds:Capabilities.tt:Extension.tt:DeviceIO.tt:VideoSources"] = 1;
    resp["tds:Capabilities.tt:Extension.tt:DeviceIO.tt:VideoOutputs"] = 0;
    resp["tds:Capabilities.tt:Extension.tt:DeviceIO.tt:AudioSources"] = 0;
    resp["tds:Capabilities.tt:Extension.tt:DeviceIO.tt:AudioOutputs"] = 0;
    resp["tds:Capabilities.tt:Extension.tt:DeviceIO.tt:RelayOutputs"] = 0;

    return 0;
}

int onvif::GetProfiles(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {

    resp.addAttrib("token", "profile_token_test");
    resp.addAttrib("fixed", "true");
    resp["trt:Profiles.tt:Name"] = "profile_test";
    resp["trt:Profiles.tt:VideoSourceConfiguration"].addAttrib("token", "video_source_token");
    resp["trt:Profiles.tt:VideoSourceConfiguration"].addAttrib("fixed", "true");
    resp["trt:Profiles.tt:VideoSourceConfiguration.tt:Name"]        = "video_source_name";
    resp["trt:Profiles.tt:VideoSourceConfiguration.tt:UseCount"]    = 1;
    resp["trt:Profiles.tt:VideoSourceConfiguration.tt:SourceToken"] = "video_source_token";
    resp["trt:Profiles.tt:VideoSourceConfiguration.tt:Bounds"].addAttrib("x", 0);
    resp["trt:Profiles.tt:VideoSourceConfiguration.tt:Bounds"].addAttrib("y", 0);
    resp["trt:Profiles.tt:VideoSourceConfiguration.tt:Bounds"].addAttrib("width", 1920);
    resp["trt:Profiles.tt:VideoSourceConfiguration.tt:Bounds"].addAttrib("height", 1080);

    resp["trt:Profiles.tt:VideoEncoderConfiguration"].addAttrib("token", "video_encode_token");
    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:Name"]                            = "video_encode_name";
    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:UseCount"]                        = 1;
    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:Encoding"]                        = "H264";
    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:Resolution.tt:Width"]             = 1920;
    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:Resolution.tt:Height"]            = 1080;
    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:Quality"]                         = 1;
    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:RateControl.tt:FrameRateLimit"]   = 15;
    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:RateControl.tt:EncodingInterval"] = 1;
    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:RateControl.tt:BitrateLimit"]     = 2048;
    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:H264.tt:GovLength"]               = 25;
    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:H264.tt:H264Profile"]             = "Baseline";
//    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:Multicast"] = 1;
    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:SessionTimeout"]                  = "PT10S";

//    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:UseCount"] = 1;
//    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:UseCount"] = 1;
//    resp["trt:Profiles.tt:VideoEncoderConfiguration.tt:UseCount"] = 1;

    return 0;
}

int onvif::GetVideoEncoderConfigurationOptions(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {

    resp["trt:Options.tt:QualityRange.tt:Min"] = 0;
    resp["trt:Options.tt:QualityRange.tt:Max"] = 1;
    for (int i = 0; i < 1; i++) {
        XmlBuilder *resolution1 = new XmlBuilder("tt:ResolutionsAvailable");
        resp["trt:Options.tt:H264"].append(resolution1);
        XmlBuilder *resolution2 = new XmlBuilder("tt:ResolutionsAvailable");
        resp["trt:Options.tt:H264"].append(resolution2);
        XmlBuilder *resolution3 = new XmlBuilder("tt:ResolutionsAvailable");
        resp["trt:Options.tt:H264"].append(resolution3);
        (*resolution1)["tt:ResolutionsAvailable.tt:Width"]  = 1920;
        (*resolution1)["tt:ResolutionsAvailable.tt:Height"] = 1080;

        (*resolution2)["tt:ResolutionsAvailable.tt:Width"]  = 1280;
        (*resolution2)["tt:ResolutionsAvailable.tt:Height"] = 720;

        (*resolution3)["tt:ResolutionsAvailable.tt:Width"]  = 640;
        (*resolution3)["tt:ResolutionsAvailable.tt:Height"] = 480;
    }
    resp["trt:Options.tt:H264.tt:GovLengthRange.tt:Min"]        = 5;
    resp["trt:Options.tt:H264.tt:GovLengthRange.tt:Max"]        = 60;
    resp["trt:Options.tt:H264.tt:FrameRateRange.tt:Min"]        = 5;
    resp["trt:Options.tt:H264.tt:FrameRateRange.tt:Max"]        = 30;
    resp["trt:Options.tt:H264.tt:EncodingIntervalRange.tt:Min"] = 1;
    resp["trt:Options.tt:H264.tt:EncodingIntervalRange.tt:Max"] = 2;
    resp["trt:Options.tt:H264.tt:H264ProfilesSupported"]        = "Baseline";

    return 0;
}

int onvif::GetVideoEncoderConfiguration(const struct OnvifContext *context, XmlBuilder& req, XmlBuilder& resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*)(context->param);
    OnvifSimpleProfile param;
    adaptor->media_ctrl_->GetOnvifSimpleProfile(0, param);

    resp.addAttrib("token", "token_main_stream_01");
    resp["trt:Configuration.tt:Name"]                            = "main_stream_01";
    resp["trt:Configuration.tt:UseCount"]                        = 1;
    resp["trt:Configuration.tt:Encoding"]                        = "H264";
    resp["trt:Configuration.tt:Resolution.tt:Width"]             = param.video_enc_cfg.width;
    resp["trt:Configuration.tt:Resolution.tt:Height"]            = param.video_enc_cfg.height;
    resp["trt:Configuration.tt:Quality"]                         = (int)param.video_enc_cfg.quality;
    resp["trt:Configuration.tt:RateControl.tt:FrameRateLimit"]   = param.video_enc_cfg.rate_limit.framerate;
    resp["trt:Configuration.tt:RateControl.tt:EncodingInterval"] = param.video_enc_cfg.rate_limit.enc_interval;
    resp["trt:Configuration.tt:RateControl.tt:BitrateLimit"]     = param.video_enc_cfg.rate_limit.bitrate / BITRATE_MEASURE;
    resp["trt:Configuration.tt:H264.tt:GovLength"]               = param.video_enc_cfg.h264_config.gov_len;
    resp["trt:Configuration.tt:H264.tt:H264Profile"]             = "Baseline";
    resp["trt:Configuration.tt:SessionTimeout"]                  = "PT10S";


    return 0;
}

int onvif::GetNetworkInterfaces(const struct OnvifContext *context, XmlBuilder& req, XmlBuilder& resp) {

    return 0;
}

int onvif::GetDeviceInformation(const struct OnvifContext *context, XmlBuilder& req, XmlBuilder& resp) {

    resp["tds:GetDeviceInformationResponse.tds:Manufacturer"]    = "123";
    resp["tds:GetDeviceInformationResponse.tds:Model"]           = "heheda";
    resp["tds:GetDeviceInformationResponse.tds:FirmwareVersion"] = "1.0";
    resp["tds:GetDeviceInformationResponse.tds:SerialNumber"]    = "12345678";
    resp["tds:GetDeviceInformationResponse.tds:HardwareId"]      = "12345678";

    return 0;
}

int onvif::GetOptions(const struct OnvifContext *context, XmlBuilder& req, XmlBuilder& resp) {

    resp["timg:ImagingOptions.tt:BacklightCompensation.tt:Mode"]         = "ON";
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

    resp["timg:ImagingOptions.tt:WhiteBalance.tt:Mode"]          = "MANUAL";
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

    DeviceAdapter *adaptor = (DeviceAdapter*)(context->param);
    AWImageColour color;
    int ret = adaptor->image_ctrl_->GetImageColour(0, color);
    if (ret != 0)
      return -1;
    color.brightness = brightness;
    color.saturation = saturation;
    color.contrast = contrast;
    ret = adaptor->image_ctrl_->SetImageColour(0, color);
    if (ret != 0)
      return -1;

    ret = adaptor->image_ctrl_->SaveImageConfig();
    if (ret != 0)
      return -1;

    DBG("source token: %s", sourceToken.c_str());

    return 0;
}

int onvif::GetImagingSettings(const struct OnvifContext *context, XmlBuilder& req, XmlBuilder& resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*)(context->param);
    AWImageColour color;
    int ret = adaptor->image_ctrl_->GetImageColour(0, color);
    if (ret != 0)
      return -1;

//    resp["timg:ImagingSettings.tt:BacklightCompensation.tt:Mode"] = "ON";
//    resp["timg:ImagingSettings.tt:BacklightCompensation.tt:Level"] = 1;

    resp["timg:ImagingSettings.tt:Brightness"] = color.brightness;
    resp["timg:ImagingSettings.tt:ColorSaturation"] = color.saturation;
    resp["timg:ImagingSettings.tt:Contrast"] = color.contrast;

//    resp["timg:ImagingSettings.tt:WideDynamicRange.tt:Level"] = 1;
//    resp["timg:ImagingSettings.tt:WideDynamicRange.tt:Mode"] = "ON";
//
//    resp["timg:ImagingSettings.tt:WhiteBalance.tt:Mode"]   = "MANUAL";
//    resp["timg:ImagingSettings.tt:WhiteBalance.tt:YrGain"] = 20;
//    resp["timg:ImagingSettings.tt:WhiteBalance.tt:YbGain"] = 80;

    return 0;
}


// TODO: 通道、主子码流区分
int onvif::GetStreamUri(const struct OnvifContext *context, XmlBuilder& req, XmlBuilder& resp) {
    DeviceAdapter *adaptor = (DeviceAdapter*)(context->param);

    OnvifSimpleProfile profile;
    memset(&profile, 0, sizeof(profile));
//    adaptor->media_ctrl_->GetOnvifSimpleProfile(tokenToChannel(req->ProfileToken), profile);

    char url[INFO_LENGTH] = {0};
    adaptor->media_ctrl_->GetStreamUrl(0, url, INFO_LENGTH);
//    resp->MediaUri->Timeout = profile.video_enc_cfg.ss_timeout;

    resp["trt:MediaUri.tt:Uri"]                 = url;
    resp["trt:MediaUri.tt:InvalidAfterConnect"] = true;
    resp["trt:MediaUri.tt:InvalidAfterReboot"]  = true;
    resp["trt:MediaUri.tt:Timeout"]             = "PT60S";

    return 0;
}

int onvif::GetOSDs(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {

    return 0;
}

int onvif::GetOSD(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {
    return 0;
}

int onvif::GetOSDOptions(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {

    resp.addAttrib("Total", 2);
    resp["trt:OSDOptions.tt:Type"] = "Text";
    resp["trt:OSDOptions.tt:PositionOption"] = "Custom";

    XmlBuilder *type1 = new XmlBuilder("tt:Type", "Plain");
    XmlBuilder *type2 = new XmlBuilder("tt:Type", "DateAndTime");
    resp["trt:OSDOptions.tt:TextOption"].append(type1);
    resp["trt:OSDOptions.tt:TextOption"].append(type2);

    resp["trt:OSDOptions.tt:TextOption.tt:FontSizeRange.tt:Min"] = 1;
    resp["trt:OSDOptions.tt:TextOption.tt:FontSizeRange.tt:Max"] = 5;

    XmlBuilder *dateFormat1 = new XmlBuilder("tt:DateFormat", "yyyy/MM/dd");
    XmlBuilder *dateFormat2 = new XmlBuilder("tt:DateFormat", "MM/dd/yyyy");
    XmlBuilder *dateFormat3 = new XmlBuilder("tt:DateFormat", "yyyy-MM-dd");
    resp["trt:OSDOptions.tt:TextOption"].append(dateFormat1);
    resp["trt:OSDOptions.tt:TextOption"].append(dateFormat2);
    resp["trt:OSDOptions.tt:TextOption"].append(dateFormat3);

    XmlBuilder *timeFormat1 = new XmlBuilder("tt:TimeFormat", "HH:mm:ss");
    XmlBuilder *timeFormat2 = new XmlBuilder("tt:TimeFormat", "hh:mm:ss");
    resp["trt:OSDOptions.tt:TextOption"].append(timeFormat1);
    resp["trt:OSDOptions.tt:TextOption"].append(timeFormat2);

    return 0;
}

int onvif::SetOSD(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {

    return 0;
}

int onvif::SetOSDs(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {

    return 0;
}

int onvif::CreateOSD(const struct OnvifContext *context, XmlBuilder &req, XmlBuilder &resp) {

    return 0;
}







































