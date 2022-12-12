/******************************************************************************
 Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************/

#include "../gsoap-gen/soapStub.h"
#include "../gsoap-gen/soapH.h"
#include <string>
#include <map>
#include "time.h"

#include "../include/utils.h"
#include "dev_ctrl_adapter.h"
#include "OnvifConnector.h"

using namespace std;

int soap_send___tev__Notify(struct soap *soap, const char *soap_endpoint, const char *soap_action,
        struct _wsnt__Notify *wsnt__Notify);

void notifyClient(const char* url) {
    soap *_soap = new soap();
    soap_init(_soap);
    soap_set_namespaces(_soap, namespaces);

    _wsnt__Notify notify;
    notify.__sizeNotificationMessage = 1;
    MALLOCN(notify.NotificationMessage, struct wsnt__NotificationMessageHolderType, notify.__sizeNotificationMessage)
    MALLOC(notify.NotificationMessage->Topic, struct wsnt__TopicExpressionType)
    notify.NotificationMessage->ProducerReference = NULL;
    notify.NotificationMessage->SubscriptionReference = NULL;
    notify.NotificationMessage->Topic->Dialect = "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet";
    notify.NotificationMessage->Topic->__mixed = "tns1:RuleEngine/CellMotionDetector/Motion";

    time_t timep;
    struct tm *p;
    time(&timep);
    p = gmtime(&timep);
    char *message = NULL;
    MALLOCN(message, char, SMALL_INFO_LENGTH)
    snprintf(message, SMALL_INFO_LENGTH, "<tt:Message UtcTime=\"%d-%d-%dT%d:%d:%dZ\" PropertyOperation=\"Changed\">"
            "<tt:Source>"
            "<tt:SimpleItem Name=\"VideoSourceConfigurationToken\" Value=\"video_source_token\"/>"
            "<tt:SimpleItem Name=\"VideoAnalyticsConfigurationToken\" Value=\"VideoAnalyticsConfiguration_0\"/>"
            "<tt:SimpleItem Name=\"Rule\" Value=\"AWCellMotionDetector\"/>"
            "</tt:Source>"
            "<tt:Data>"
            "<tt:SimpleItem Name=\"IsMotion\" Value=\"true\"/>"
            "</tt:Data>"
            "</tt:Message>", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
    notify.NotificationMessage->wsnt__Message = message;
    sleep(10);
    int ret = soap_send___tev__Notify(_soap, url, "http://docs.oasis-open.org/wsn/bw-2/NotificationConsumer/Notify",
            &notify);
    printf("message:\n%s\nret: %d\nurl: %s\n", message, ret, url);
    soap_destroy(_soap);
    soap_end(_soap);
    soap_done(_soap);
    delete _soap;
}

void dumpNotifyMap(struct soap *_soap) {
    map<string, int> *notifyMap = ((OnvifConnector*)_soap->user)->GetNotifyMap();
    map<string, int>::iterator it = notifyMap->begin();
    printf("dump notify map:\n");
    while (it != notifyMap->end()) {
        printf("url: %s\n", it->first.c_str());
        ++it;
    }
}

void eventHandle(int chn, int event_type, void *context) {
    map<string, int> *notifyMap = (map<string, int>*) context;
    if (EVENT_TYPE_MD == event_type) {
        map<string, int>::iterator it = notifyMap->begin();
        while (it != notifyMap->end()) {
            notifyClient(it->first.c_str());
            ++it;
        }
    }
}

int __tev__Notify(struct soap*, struct _wsnt__Notify *wsnt__Notify) {
    printf("function: %s invoked!", __FUNCTION__);
    return SOAP_OK;
}

// TODO:取消订阅
int __tev__Unsubscribe(struct soap* _soap, struct _wsnt__Unsubscribe *req, struct _wsnt__UnsubscribeResponse *resp) {
    printf("function: %s invoked!", __FUNCTION__);
    printf("host: %s\n", _soap->host);
    map<string, int> *notifyMap = ((OnvifConnector*)_soap->user)->GetNotifyMap();
    map<string, int>::iterator it = notifyMap->begin();
    while (it != notifyMap->end()) {
        if (string::npos != it->first.find(_soap->host)) {
            notifyMap->erase(it);
        }
        ++it;
    }
    dumpNotifyMap(_soap);

    return SOAP_OK;
}

int __tev__Subscribe(struct soap* _soap, struct _wsnt__Subscribe *req, struct _wsnt__SubscribeResponse *resp) {
    printf("url: %s\n", req->ConsumerReference.Address);

    map<string, int> *notifyMap = getConnector(_soap)->GetNotifyMap();
    map<string, int>::iterator it = notifyMap->find(req->ConsumerReference.Address);
    if (it == notifyMap->end()) {
        DeviceAdapter::EventControl *eventCtl = getSystemInterface(_soap)->event_ctrl_;
        int handleId = eventCtl->RegisterEventHandle(EVENT_TYPE_MD, eventHandle, (void*) notifyMap);
        if (handleId < 0) {
            printf("register callback failed!");
        } else {
            (*notifyMap)[req->ConsumerReference.Address] = handleId;
        }
    }

    time_t timep;
    time(&timep);

    MALLOC(resp->CurrentTime, time_t);
    MALLOC(resp->TerminationTime, time_t);
    memcpy(resp->CurrentTime, &timep, sizeof(timep));
    memcpy(resp->TerminationTime, &timep, sizeof(timep));
    resp->SubscriptionReference.Address = NULL;
    resp->SubscriptionReference.Metadata = NULL;
    resp->SubscriptionReference.ReferenceParameters = NULL;
    resp->SubscriptionReference.__any = NULL;
    resp->SubscriptionReference.__anyAttribute = NULL;
    resp->SubscriptionReference.__size = 0;

    dumpNotifyMap(_soap);

    return SOAP_OK;
}

int __tev__Unsubscribe_(struct soap*, struct _wsnt__Unsubscribe *wsnt__Unsubscribe,
        struct _wsnt__UnsubscribeResponse *wsnt__UnsubscribeResponse) {
    printf("function: %s invoked!", __FUNCTION__);
    return SOAP_OK;
}

int __tev__PullMessages(struct soap*, struct _tev__PullMessages *tev__PullMessages,
        struct _tev__PullMessagesResponse *tev__PullMessagesResponse) {
    printf("function: %s invoked!", __FUNCTION__);
    return SOAP_OK;
}

int __tev__Seek(struct soap*, struct _tev__Seek *tev__Seek, struct _tev__SeekResponse *tev__SeekResponse) {
    printf("function: %s invoked!", __FUNCTION__);
    return SOAP_OK;
}

int __tev__SetSynchronizationPoint(struct soap*, struct _tev__SetSynchronizationPoint *tev__SetSynchronizationPoint,
        struct _tev__SetSynchronizationPointResponse *tev__SetSynchronizationPointResponse) {
    printf("function: %s invoked!", __FUNCTION__);
    return SOAP_OK;
}

int __tev__GetServiceCapabilities(struct soap*, struct _tev__GetServiceCapabilities *tev__GetServiceCapabilities,
        struct _tev__GetServiceCapabilitiesResponse *tev__GetServiceCapabilitiesResponse) {
    printf("function: %s invoked!", __FUNCTION__);
    return SOAP_OK;
}

int __tev__CreatePullPointSubscription(struct soap*,
        struct _tev__CreatePullPointSubscription *tev__CreatePullPointSubscription,
        struct _tev__CreatePullPointSubscriptionResponse *tev__CreatePullPointSubscriptionResponse) {
    printf("function: %s invoked!", __FUNCTION__);
    return SOAP_OK;
}

int __tev__GetEventProperties(struct soap*, struct _tev__GetEventProperties *tev__GetEventProperties,
        struct _tev__GetEventPropertiesResponse *tev__GetEventPropertiesResponse) {
    printf("function: %s invoked!", __FUNCTION__);

    return SOAP_OK;
}

int soap_send___tev__Notify(struct soap *soap, const char *soap_endpoint, const char *soap_action,
        struct _wsnt__Notify *wsnt__Notify) {
    struct __tev__Notify soap_tmp___tev__Notify;
    if (soap_action == NULL)
        soap_action = "http://docs.oasis-open.org/wsn/bw-2/NotificationConsumer/Notify";
    soap_tmp___tev__Notify.wsnt__Notify = wsnt__Notify;
    soap_begin(soap);
    soap_set_version(soap, 2); /* SOAP1.2 */
    soap->encodingStyle = NULL;
    soap_serializeheader(soap);
    soap_serialize___tev__Notify(soap, &soap_tmp___tev__Notify);
    if (soap_begin_count(soap))
        return soap->error;
    if (soap->mode & SOAP_IO_LENGTH) {
        if (soap_envelope_begin_out(soap) || soap_putheader(soap) || soap_body_begin_out(soap)
                || soap_put___tev__Notify(soap, &soap_tmp___tev__Notify, "-wsnt:Notify", "") || soap_body_end_out(soap)
                || soap_envelope_end_out(soap))
            return soap->error;
    }
    if (soap_end_count(soap))
        return soap->error;
    if (soap_connect(soap, soap_endpoint, soap_action) || soap_envelope_begin_out(soap) || soap_putheader(soap)
            || soap_body_begin_out(soap) || soap_put___tev__Notify(soap, &soap_tmp___tev__Notify, "-wsnt:Notify", "")
            || soap_body_end_out(soap) || soap_envelope_end_out(soap) || soap_end_send(soap))
        return soap_closesock(soap);
    return SOAP_OK;
}

