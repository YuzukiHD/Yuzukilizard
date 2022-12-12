#include "stdio.h"
#include "time.h"
#include "sys/time.h"
#include "dev_ctrl_adapter.h"
#include "OnvifConnector.h"
#include <vector>
#include <string>

#include "../include/utils.h"

/*
 * 设备发现完成后，NVR将通过onvif依次调用如下函数获取信息
 * function: [GetSystemDateAndTime]
 * function: [GetCapabilities]
 * function: [__trt__GetProfiles]
 * function: [GetNetworkInterfaces]
 * function: [GetDeviceInformation]
 * function: [__trt__GetOSDOptions]
 * function: [__trt__GetVideoEncoderConfigurationOptions]
 * function: [__trt__GetVideoEncoderConfiguration]
 * function: [GetSystemDateAndTime]
 * function: [__trt__GetStreamUri]
 * function: [__trt__GetVideoEncoderConfiguration]
 */

using namespace std;

int GetDeviceInformation(struct soap* _soap, struct _tds__GetDeviceInformation *tds__GetDeviceInformation,
        struct _tds__GetDeviceInformationResponse *tds__GetDeviceInformationResponse) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
#endif

    return SOAP_OK;
}

int convertUtcTime(struct soap *_soap, char *timezone) {
    struct tm *pLocalTime, localTime;
    time_t nowTime = 0;
    getSystemInterface(_soap)->sys_ctrl_->GetSysDataTime(nowTime);
    pLocalTime = ::localtime_r(&nowTime, &localTime);

    char *ptr = timezone + sizeof(pLocalTime->tm_zone);
    if (NULL == ptr) {
        return 0;
    }

    string intStr = string(ptr);
    return str2int(intStr);
//    int ret = 0;
//    if(strcmp(ptr, "+") == 0) {
//        ret += str2int(intStr);
//    } else if (strcmp(ptr, "-") == 0) {
//        ret -= str2int(intStr);
//    } else {
//        printf("unknown int string! content: %s\n", ptr);
//        ret = 0;
//    }
//    return ret;
}

int SetSystemDateAndTime(struct soap* _soap, struct _tds__SetSystemDateAndTime *req,
        struct _tds__SetSystemDateAndTimeResponse *resp) {
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);

    tm nvrTime;
    memset(&nvrTime, 0, sizeof(tm));
    nvrTime.tm_zone = req->TimeZone->TZ;
    nvrTime.tm_year = req->UTCDateTime->Date->Year - TIMESTAMP_YEAR_BASE;
    nvrTime.tm_mon = req->UTCDateTime->Date->Month - TIMESTAMP_MONTH_BASE;
    nvrTime.tm_mday = req->UTCDateTime->Date->Day;
    nvrTime.tm_hour = req->UTCDateTime->Time->Hour + convertUtcTime(_soap, req->TimeZone->TZ);
    nvrTime.tm_min = req->UTCDateTime->Time->Minute;
    nvrTime.tm_sec = req->UTCDateTime->Time->Second;

    printf("%s %s\n", asctime(&nvrTime), req->TimeZone->TZ);
    time_t setTime = ::mktime(&nvrTime);
    getSystemInterface(_soap)->sys_ctrl_->SetSysDataTime(setTime);

    return SOAP_OK;
}

int GetSystemDateAndTime(struct soap* _soap, struct _tds__GetSystemDateAndTime *req,
        struct _tds__GetSystemDateAndTimeResponse *resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);

#endif

#if 0
    if(check_username_password(soap))
    {
        printf("illegal user!\n");
        return -1;
    }
    else
    {
        wldebug("passed!\n");
    }
#endif
    /* soap_wsse_add_Security(_soap); */

    DeviceAdapter *systemContext = getSystemInterface(_soap);

    struct tm *pLocalTime, localTime;
    struct tm *pUtcTime, utcTime;
    time_t nowTime = 0;
    systemContext->sys_ctrl_->GetSysDataTime(nowTime);
    pLocalTime = ::localtime_r(&nowTime, &localTime);
    pUtcTime = ::gmtime_r(&nowTime, &utcTime);

    /* IPC_Time                                                                          = getTime();获取系统时间 */
    resp->SystemDateAndTime = (struct tt__SystemDateTime *) soap_malloc(_soap, sizeof(struct tt__SystemDateTime));
    resp->SystemDateAndTime->DateTimeType = tt__SetDateTimeType__Manual;
    resp->SystemDateAndTime->DaylightSavings = xsd__boolean__false_;
    resp->SystemDateAndTime->TimeZone = (struct tt__TimeZone *) soap_malloc(_soap, sizeof(struct tt__TimeZone));
    resp->SystemDateAndTime->TimeZone->TZ = (char *) soap_malloc(_soap, sizeof(char) * INFO_LENGTH);
    strcpy(resp->SystemDateAndTime->TimeZone->TZ, pLocalTime->tm_zone);
    resp->SystemDateAndTime->UTCDateTime = NULL;
//    resp->SystemDateAndTime->UTCDateTime = (struct tt__DateTime *) soap_malloc(_soap, sizeof(struct tt__DateTime));
//    resp->SystemDateAndTime->UTCDateTime->Time = (struct tt__Time *) soap_malloc(_soap, sizeof(struct tt__Time));
//    resp->SystemDateAndTime->UTCDateTime->Time->Hour = pUtcTime->tm_hour;
//    resp->SystemDateAndTime->UTCDateTime->Time->Minute = pUtcTime->tm_min;
//    resp->SystemDateAndTime->UTCDateTime->Time->Second = pUtcTime->tm_sec;
//    resp->SystemDateAndTime->UTCDateTime->Date = (struct tt__Date *) soap_malloc(_soap, sizeof(struct tt__Date));
//    resp->SystemDateAndTime->UTCDateTime->Date->Year = pUtcTime->tm_year + TIMESTAMP_YEAR_BASE;
//    resp->SystemDateAndTime->UTCDateTime->Date->Month = pUtcTime->tm_mon + TIMESTAMP_MONTH_BASE;
//    resp->SystemDateAndTime->UTCDateTime->Date->Day = pUtcTime->tm_mday + TIMESTAMP_DAY_BASE;
    resp->SystemDateAndTime->LocalDateTime = (struct tt__DateTime *) soap_malloc(_soap, sizeof(struct tt__DateTime));
    resp->SystemDateAndTime->LocalDateTime->Time = (struct tt__Time *) soap_malloc(_soap, sizeof(struct tt__Time));
    resp->SystemDateAndTime->LocalDateTime->Time->Hour = pLocalTime->tm_hour;
    resp->SystemDateAndTime->LocalDateTime->Time->Minute = pLocalTime->tm_min;
    resp->SystemDateAndTime->LocalDateTime->Time->Second = pLocalTime->tm_sec;
    resp->SystemDateAndTime->LocalDateTime->Date = (struct tt__Date *) soap_malloc(_soap, sizeof(struct tt__Date));
    resp->SystemDateAndTime->LocalDateTime->Date->Year = pLocalTime->tm_year + TIMESTAMP_YEAR_BASE;
    resp->SystemDateAndTime->LocalDateTime->Date->Month = pLocalTime->tm_mon + TIMESTAMP_MONTH_BASE;
    resp->SystemDateAndTime->LocalDateTime->Date->Day = pLocalTime->tm_mday + TIMESTAMP_DAY_BASE;
    resp->SystemDateAndTime->Extension = NULL;

    printf("local: %s %s\n", asctime(pLocalTime), resp->SystemDateAndTime->TimeZone->TZ);
    printf("utc: %s\n", asctime(pUtcTime));

    return SOAP_OK;
}

int GetCapabilities(struct soap* _soap, struct _tds__GetCapabilities* req,
        struct _tds__GetCapabilitiesResponse* resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
#endif

    char IPCAddr[INFO_LENGTH];
    int result;
    OnvifConnector *connector = (OnvifConnector*) _soap->user;
    sprintf(IPCAddr, "http://%s/onvif/device_service", connector->GetServerIp().c_str());

    MALLOC(resp->Capabilities, struct tt__Capabilities);
    memset(resp->Capabilities, 0, sizeof(struct tt__Capabilities));

    // ============================= Analytics part =================================================
    MALLOC(resp->Capabilities->Analytics, struct tt__AnalyticsCapabilities);
    MALLOCN(resp->Capabilities->Analytics->XAddr, char, INFO_LENGTH);
    strncpy(resp->Capabilities->Analytics->XAddr, IPCAddr, INFO_LENGTH);
    resp->Capabilities->Analytics->RuleSupport = xsd__boolean__true_;
    resp->Capabilities->Analytics->AnalyticsModuleSupport = xsd__boolean__true_;

    // ============================= Events part =================================================
    MALLOC(resp->Capabilities->Events, struct tt__EventCapabilities);
    MALLOCN(resp->Capabilities->Events->XAddr, char, INFO_LENGTH);
    strncpy(resp->Capabilities->Events->XAddr, IPCAddr, INFO_LENGTH);
    resp->Capabilities->Events->WSPausableSubscriptionManagerInterfaceSupport = xsd__boolean__true_;
    resp->Capabilities->Events->WSPullPointSupport = xsd__boolean__true_;
    resp->Capabilities->Events->WSSubscriptionPolicySupport = xsd__boolean__true_;

    // ============================= Imaging part =================================================
    MALLOC(resp->Capabilities->Imaging, struct tt__ImagingCapabilities);
    MALLOCN(resp->Capabilities->Imaging->XAddr, char, INFO_LENGTH);
    strncpy(resp->Capabilities->Imaging->XAddr, IPCAddr, INFO_LENGTH);

    // ============================= Device part =================================================
    resp->Capabilities->Device = (struct tt__DeviceCapabilities*) (soap_malloc(_soap,
            sizeof(struct tt__DeviceCapabilities)));
    resp->Capabilities->Device->XAddr = (char *) soap_malloc(_soap, sizeof(char) * INFO_LENGTH);
    strncpy(resp->Capabilities->Device->XAddr, IPCAddr, INFO_LENGTH);
    resp->Capabilities->Device->Network = (struct tt__NetworkCapabilities*) (soap_malloc(_soap,
            sizeof(struct tt__NetworkCapabilities)));
    resp->Capabilities->Device->Network->IPFilter =
            (enum xsd__boolean*) (soap_malloc(_soap, sizeof(enum xsd__boolean)));
    resp->Capabilities->Device->Network->ZeroConfiguration = (enum xsd__boolean*) (soap_malloc(_soap,
            sizeof(enum xsd__boolean)));
    resp->Capabilities->Device->Network->IPVersion6 = (enum xsd__boolean*) (soap_malloc(_soap,
            sizeof(enum xsd__boolean)));
    resp->Capabilities->Device->Network->DynDNS = (enum xsd__boolean*) (soap_malloc(_soap, sizeof(enum xsd__boolean)));
    *resp->Capabilities->Device->Network->IPFilter = xsd__boolean__false_;
    *resp->Capabilities->Device->Network->ZeroConfiguration = xsd__boolean__false_;
    *resp->Capabilities->Device->Network->IPVersion6 = xsd__boolean__false_;
    *resp->Capabilities->Device->Network->DynDNS = xsd__boolean__false_;
    resp->Capabilities->Device->Network->Extension = (struct tt__NetworkCapabilitiesExtension*) (soap_malloc(_soap,
            sizeof(struct tt__NetworkCapabilitiesExtension)));
    resp->Capabilities->Device->Network->Extension->Dot11Configuration = (enum xsd__boolean*) (soap_malloc(_soap,
            sizeof(enum xsd__boolean)));
    *resp->Capabilities->Device->Network->Extension->Dot11Configuration = xsd__boolean__false_;
    resp->Capabilities->Device->Network->Extension->Extension = NULL;
    resp->Capabilities->Device->System = NULL;
    MALLOC(resp->Capabilities->Device->IO, struct tt__IOCapabilities);
    MALLOC(resp->Capabilities->Device->IO->RelayOutputs, int);
    MALLOC(resp->Capabilities->Device->IO->InputConnectors, int);
    *resp->Capabilities->Device->IO->InputConnectors = 1;
    *resp->Capabilities->Device->IO->RelayOutputs = 0;
    resp->Capabilities->Device->IO->Extension = NULL;
    MALLOC(resp->Capabilities->Device->Security, struct tt__SecurityCapabilities);
    resp->Capabilities->Device->Security->TLS1_x002e1 = xsd__boolean__false_;
    resp->Capabilities->Device->Security->TLS1_x002e2 = xsd__boolean__false_;
    resp->Capabilities->Device->Security->OnboardKeyGeneration = xsd__boolean__false_;
    resp->Capabilities->Device->Security->AccessPolicyConfig = xsd__boolean__false_;
    resp->Capabilities->Device->Security->X_x002e509Token = xsd__boolean__false_;
    resp->Capabilities->Device->Security->SAMLToken = xsd__boolean__false_;
    resp->Capabilities->Device->Security->KerberosToken = xsd__boolean__false_;
    resp->Capabilities->Device->Security->RELToken = xsd__boolean__false_;
    resp->Capabilities->Device->Security->Extension = NULL;
    resp->Capabilities->Device->Extension = NULL;

    // ============================= Device part =================================================
    resp->Capabilities->PTZ = NULL;

    // ========================================================================================
    // ============================= Media part =================================================
    // ========================================================================================
    /* 想要对接RTSP视频，必须要设置Media */
    MALLOC(resp->Capabilities->Media, struct tt__MediaCapabilities);
    MALLOCN(resp->Capabilities->Media->XAddr, char, INFO_LENGTH);
    strcpy(resp->Capabilities->Media->XAddr, IPCAddr);
    MALLOC(resp->Capabilities->Media->StreamingCapabilities, struct tt__RealTimeStreamingCapabilities);
    MALLOC(resp->Capabilities->Media->StreamingCapabilities->RTPMulticast, enum xsd__boolean);
    MALLOC(resp->Capabilities->Media->StreamingCapabilities->RTP_USCORETCP, enum xsd__boolean);
    MALLOC(resp->Capabilities->Media->StreamingCapabilities->RTP_USCORERTSP_USCORETCP, enum xsd__boolean);
    *resp->Capabilities->Media->StreamingCapabilities->RTPMulticast = xsd__boolean__false_;
    *resp->Capabilities->Media->StreamingCapabilities->RTP_USCORETCP = xsd__boolean__false_;
    *resp->Capabilities->Media->StreamingCapabilities->RTP_USCORERTSP_USCORETCP = xsd__boolean__false_;
    resp->Capabilities->Media->StreamingCapabilities->Extension = NULL;
    MALLOC(resp->Capabilities->Media->Extension, struct tt__MediaCapabilitiesExtension);
    MALLOC(resp->Capabilities->Media->Extension->ProfileCapabilities, struct tt__ProfileCapabilities);
    resp->Capabilities->Media->Extension->ProfileCapabilities->MaximumNumberOfProfiles = 10;

    // ================ EXtension part ==================================
    MALLOC(resp->Capabilities->Extension, struct tt__CapabilitiesExtension);
    MALLOC(resp->Capabilities->Extension->DeviceIO, struct tt__DeviceIOCapabilities);
    MALLOCN(resp->Capabilities->Extension->DeviceIO->XAddr, char, INFO_LENGTH);
    /* TODO: DeviceIO service server address */
    strcpy(resp->Capabilities->Extension->DeviceIO->XAddr, IPCAddr);
    resp->Capabilities->Extension->DeviceIO->VideoSources = 1;
    resp->Capabilities->Extension->DeviceIO->VideoOutputs = 0;
    resp->Capabilities->Extension->DeviceIO->AudioSources = 0;
    resp->Capabilities->Extension->DeviceIO->AudioOutputs = 0;
    resp->Capabilities->Extension->DeviceIO->RelayOutputs = 0;
    resp->Capabilities->Extension->Display = NULL;
    resp->Capabilities->Extension->Recording = NULL;
    resp->Capabilities->Extension->Search = NULL;
    resp->Capabilities->Extension->Replay = NULL;
    resp->Capabilities->Extension->Receiver = NULL;
    resp->Capabilities->Extension->AnalyticsDevice = NULL;
    resp->Capabilities->Extension->Extensions = NULL;

    return SOAP_OK;
}

int GetNetworkInterfaces(struct soap* _soap, struct _tds__GetNetworkInterfaces *req,
        struct _tds__GetNetworkInterfacesResponse *resp) {
#ifdef DEBUG
    printf("function: [%s] ======================================= invoked!\n", __FUNCTION__);
#endif

    return SOAP_OK;
}
