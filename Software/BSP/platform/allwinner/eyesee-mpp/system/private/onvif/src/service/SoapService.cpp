/*
 * SoapService.cpp
 *
 *  Created on: 2017年1月17日
 *      Author: liu
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include "utils.h"
#include "SoapService.h"
#include "HttpCodec.h"
#include "SoapUtils.h"
#include "XmlBuilder.h"
#include "OnvifMethod.h"

using namespace onvif;
using namespace std;

SoapService::SoapService(EventLoop &loop, const string &bindIp, int port) :
        _loop(loop), _bindIp(bindIp), _port(port), _listenFd(-1), _context(NULL) {
    _listenFd = createListenSocket();
    setNoblock(_listenFd);
    if (_listenFd < 0) {
        LOG("create soap service failed!");
    } else {
        int ret = _loop.scheduleEvent(_listenFd, EventLoop::EventTypeRead, this, -1);
        if (ret < 0) {
            LOG("scheduleEvent failed! return value: %d", ret);
        }

        ret = _loop.registBeforeReclaFunc(_listenFd, SoapService::fdReclaCallback);
        if (ret < 0) {
            LOG("registBeforeReclaFunc failed! return value: %d", ret);
        }
    }
}

SoapService::~SoapService() {
    DBG("SoapService is deinit!");
    _loop.unscheduleEvent(_listenFd);
    close(_listenFd);
}

string SoapService::serviceIp() {
    return _bindIp;
}

int SoapService::servicePort() {
    return _port;
}

// TODO: 未处理epoll ET的特殊性，待处理
void SoapService::onRead(int fd) {
    if (fd == _listenFd) {
        do {
            int socketFd = accept(_listenFd, NULL, NULL);
            if (socketFd == -1) {
                if (errno != EAGAIN) {
                    LOG("accept new connect failed! ret: %d errno:%d, message:%s", socketFd, errno, strerror(errno));
                }
                break;
            } else if (socketFd == 0) {
                DBG("accept socket: %d", socketFd);
                break;
            }
            // DBG("accept socket: %d", socketFd);

            setNoblock(socketFd);
            int ret = _loop.scheduleEvent(socketFd, EventLoop::EventTypeRead, this, 6);
            if (ret < 0) {
                LOG("schedule event failed! return value: %d", ret);
            }

            ret = _loop.registBeforeReclaFunc(socketFd, SoapService::fdReclaCallback);
            if (ret < 0) {
                LOG("registBeforeReclaFunc failed! return value: %d", ret);
            }
        } while (true);

        return;
    }

#define BUFFER_SIZE 65535
    char buffer[BUFFER_SIZE];
    do {
        memset(buffer, 0, sizeof(buffer));
        int recvSize = recv(fd, buffer, sizeof(buffer), 0);
        if (recvSize < 0) {
            if (errno == EAGAIN) {
                return;
            }
            LOG("recv failed! ret: %d errno:%d, message:%s", recvSize, errno, strerror(errno));
            break;
        } else if (recvSize >= BUFFER_SIZE) {
            LOG("recv buffer size is too slow! recv data size: %d", recvSize);
            continue;
        } else if (recvSize == 0) {
            DBG("remote is close the connect , fd: %d", fd);
            break;
        }

//    DBG("recv data from fd %d:\n%s", fd, buffer);
//    return;

        HttpCodec *codec = NULL;
        HttpCodecTable::iterator res = _httpCodecTbl.find(fd);
        if (_httpCodecTbl.end() == res){
            codec = new HttpCodec();
        } else {
            codec = res->second;
        }

        int ret = codec->parse(buffer);
        if (ret == HttpCodec::ParseFailed) {
            LOG("parse request failed! buffer content:\n%s", buffer);
            break;
        } else if (ret == HttpCodec::ParseContinue) {
            _httpCodecTbl[fd] = codec;
            continue;
        }
        const HttpRequest &httpReq = codec->req();

//    httpReq.dump();

        XmlBuilder req;
        ret = req.parse(httpReq.content.c_str());
        if (ret < 0) {
            LOG("recv data is not xml, http content:\n%s", httpReq.content.c_str());
            break;
        }

        if (req.noDefaultNamespace()) {
            SoapUtils::addVisitNamespace(req, "", SoapUtils::getAllXmlNameSpace());
        }
//    DBG("request dump:\n%s", req.toFormatString().c_str());

        XmlBuilder *resp = SoapUtils::createSoapTemplate("", SoapUtils::getAllXmlNameSpace());
        ret = invokeOnvifImpl(SoapUtils::soapFunctionName(req), req, *resp);
        if (ret < 0)
            DBG("fd: %d, request dump:\n%s", fd, buffer);

        HttpResponse httpResp;
        httpResp.content = resp->toString();
        httpResp.statusCode = ret == 0 ? 200 : 500;
        const string& httpRespStr = HttpCodec::buildMessage(httpResp);
        send(fd, httpRespStr.c_str(), httpRespStr.size(), 0);
//    DBG("response:\n%s", resp->toString().c_str());
//    DBG("response:\n%s", httpRespStr.c_str());

        delete resp;
    } while (true);

    _loop.unscheduleEvent(fd);
}

void SoapService::setContext(struct OnvifContext *context) {
    _context = context;
}

struct OnvifContext* SoapService::getContext() {
    return _context;
}

int SoapService::invokeOnvifImpl(const string &method, XmlBuilder &req, XmlBuilder &resp) {
    LOG("[***method %s invoke!***]", method.c_str());

    int ret = 0;
    const SoapXmlNsMap &ns = SoapUtils::getAllXmlNameSpace();

    /*******************DeviceManagment*************************/
    if ("GetSystemDateAndTime" == method) {
        SoapUtils::addVisitNamespace(resp, "tds:tt", ns);
        XmlBuilder& timeRoot = resp["Envelope.Body.tds:GetSystemDateAndTimeResponse.tds:SystemDateAndTime"];
        ret = GetSystemDataAndTime(_context, req, timeRoot);

    } else if ("SetSystemDateAndTime" == method) {
        SoapUtils::addVisitNamespace(resp, "tds:tt", ns);
        XmlBuilder& response = resp["Envelope.Body.tds:SetSystemDateAndTimeResponse"];
        ret = SetSystemDataAndTime(_context, req, response);

    } else if ("GetDeviceInformation" == method) {
        SoapUtils::addVisitNamespace(resp, "tds", ns);
        XmlBuilder &deviceInfo = resp["Envelope.Body.tds:GetDeviceInformationResponse"];
        ret = GetDeviceInformation(_context, req, deviceInfo);

    } else if ("GetCapabilities" == method) {
        SoapUtils::addVisitNamespace(resp, "tds:tt", ns);
        XmlBuilder &cap = resp["Envelope.Body.tds:GetCapabilitiesResponse.tds:Capabilities"];
        ret = GetCapabilities(_context, req, cap);

    } else if ("GetNetworkInterfaces" == method) {
        SoapUtils::addVisitNamespace(resp, "tds", ns);
        XmlBuilder &interface = resp["Envelope.Body.tds:GetNetworkInterfacesResponse"];
        ret = GetNetworkInterfaces(_context, req, interface);

        /*******************Media*************************/
    } else if ("GetProfiles" == method) {
        SoapUtils::addVisitNamespace(resp, "trt:tt:tan", ns);
        XmlBuilder &profiles = resp["Envelope.Body.trt:GetProfilesResponse"];
        ret = GetProfiles(_context, req, profiles);

//    } else if ("GetVideoAnalyticsconfiguration" == method) {
    } else if ("GetVideoEncoderConfiguration" == method) {
        SoapUtils::addVisitNamespace(resp, "trt:tt", ns);
        XmlBuilder &config = resp["Envelope.Body.trt:GetVideoEncoderConfigurationResponse.trt:Configuration"];
        ret = GetVideoEncoderConfiguration(_context, req, config);

    } else if ("SetVideoEncoderConfiguration" == method) {
        SoapUtils::addVisitNamespace(resp, "trt:tt", ns);
        XmlBuilder &setConfig = resp["Envelope.Body.trt:SetVideoEncoderConfigurationResponse"];
        ret = SetVideoEncoderConfiguration(_context, req, setConfig);

    } else if ("GetVideoEncoderConfigurationOptions" == method) {
        SoapUtils::addVisitNamespace(resp, "trt:tt", ns);
        XmlBuilder &options = resp["Envelope.Body.trt:GetVideoEncoderConfigurationOptionsResponse.trt:Options"];
        ret = GetVideoEncoderConfigurationOptions(_context, req, options);

    } else if ("GetStreamUri" == method) {
        SoapUtils::addVisitNamespace(resp, "trt:tt", ns);
        XmlBuilder &mediaUri = resp["Envelope.Body.trt:GetStreamUriResponse.trt:MediaUri"];
        ret = GetStreamUri(_context, req, mediaUri);

        /*******************OSD*************************/
    } else if ("GetOSDs" == method) {
        SoapUtils::addVisitNamespace(resp, "trt:tt", ns);
        XmlBuilder &getOsds = resp["Envelope.Body.trt:GetOSDsResponse"];
        ret = GetOSDs(_context, req, getOsds);

    } else if ("GetOSD" == method) {
        SoapUtils::addVisitNamespace(resp, "trt", ns);
        XmlBuilder &getOsd = resp["Envelope.Body.trt:GetOSDResponse.trt:OSD"];
        ret = GetOSD(_context, req, getOsd);

    } else if ("GetOSDOptions" == method) {
        SoapUtils::addVisitNamespace(resp, "trt:tt", ns);
        XmlBuilder &options = resp["Envelope.Body.trt:GetOSDOptionsResponse.trt:OSDOptions"];
        ret = GetOSDOptions(_context, req, options);

    } else if ("SetOSD" == method) {
        SoapUtils::addVisitNamespace(resp, "trt:tt", ns);
        XmlBuilder &setOsd = resp["Envelope.Body.trt:SetOSDResponse"];
        ret = SetOSD(_context, req, setOsd);

    } else if ("SetOSDs" == method) {
        SoapUtils::addVisitNamespace(resp, "trt", ns);
        XmlBuilder &setOsds = resp["Envelope.Body.trt:SetOSDsResponse"];
        ret = SetOSDs(_context, req, setOsds);

    } else if ("CreateOSD" == method) {
        SoapUtils::addVisitNamespace(resp, "trt:tt", ns);
        XmlBuilder &createOsd = resp["Envelope.Body.trt:CreateOSDResponse"];
        ret = CreateOSD(_context, req, createOsd);

    } else if ("DeleteOSD" == method) {
        SoapUtils::addVisitNamespace(resp, "trt", ns);
        XmlBuilder &deleteOsd = resp["Envelope.Body.trt:DeleteOSDResponse"];
        ret = DeleteOSD(_context, req, deleteOsd);

        /*******************Analytics*************************/
    } else if ("GetRules" == method) {
        SoapUtils::addVisitNamespace(resp, "tan:tt", ns);
        XmlBuilder &getRules = resp["Envelope.Body.tan:GetRulesResponse"];
        ret = GetRules(_context, req, getRules);

    } else if ("CreateRules" == method) {
        SoapUtils::addVisitNamespace(resp, "tan:tt", ns);
        XmlBuilder &create = resp["Envelope.Body.tan:CreateRulesResponse"];
        ret = CreateRules(_context, req, create);

    } else if ("ModifyRules" == method) {
        SoapUtils::addVisitNamespace(resp, "tan:tt", ns);
        XmlBuilder &modifyRules = resp["Envelope.Body.tan:ModifyRulesResponse"];
        ret = ModifyRules(_context, req, modifyRules);

    } else if ("CreateAnalyticsModules" == method) {
        SoapUtils::addVisitNamespace(resp, "tan:tt", ns);
        XmlBuilder &create = resp["Envelope.Body.tan:CreateAnalyticsModulesResponse"];
        ret = CreateAnalyticsModules(_context, req, create);

    } else if ("DeleteAnalyticsModules" == method) {
        SoapUtils::addVisitNamespace(resp, "tan", ns);
        XmlBuilder &deleteModules = resp["Envelope.Body.tan:DeleteAnalyticsModulesResponse"];
        ret = DeleteAnalyticsModules(_context, req, deleteModules);

    } else if ("GetAnalyticsModules" == method) {
        SoapUtils::addVisitNamespace(resp, "tan:tt", ns);
        XmlBuilder &getModules = resp["Envelope.Body.tan:GetAnalyticsModulesResponse"];
        ret = GetAnalyticsModules(_context, req, getModules);

    } else if ("ModifyAnalyticsModules" == method) {
        SoapUtils::addVisitNamespace(resp, "tan:tt", ns);
        XmlBuilder &modifyModules = resp["Envelope.Body.tan:ModifyAnalyticsModulesResponse"];
        ret = ModifyAnalyticsModules(_context, req, modifyModules);

        /*******************Imaging*************************/
    } else if ("GetImagingSettings" == method) {
        SoapUtils::addVisitNamespace(resp, "timg:tt", ns);
        XmlBuilder &settings = resp["Envelope.Body.timg:GetImagingSettingsResponse.timg:ImagingSettings"];
        ret = GetImagingSettings(_context, req, settings);

    } else if ("SetImagingSettings" == method) {
        SoapUtils::addVisitNamespace(resp, "timg:tt", ns);
        XmlBuilder &settings = resp["Envelope.Body.timg:SetImagingSettingsResponse"];
        ret = SetImagingSettings(_context, req, settings);

    } else if ("GetOptions" == method) {
        SoapUtils::addVisitNamespace(resp, "timg:tt", ns);
        XmlBuilder &options = resp["Envelope.Body.timg:GetOptionsResponse.timg:ImagingOptions"];
        ret = GetOptions(_context, req, options);

        /*******************MotionDetection*************************/
    } else if ("Subscribe" == method) {
        SoapUtils::addVisitNamespace(resp, "wsnt:tt", ns);
        XmlBuilder &options = resp["Envelope.Body.wsnt:SubscribeResponse"];
        ret = Subscribe(_context, req, options);

    } else if ("UnSubscribe" == method) {
        SoapUtils::addVisitNamespace(resp, "wsnt:tt", ns);
        XmlBuilder &options = resp["Envelope.Body.wsnt:UnSubscribeResponse"];
        ret = UnSubscribe(_context, req, options);

    } else {
        ret = -1;
        LOG("%s method not implement!", method.c_str());
    }

    if (ret < 0) {
        LOG("method invoke failed!");
        SoapUtils::buildFailedMessage(resp);
    }

    return ret;
}

int SoapService::createListenSocket() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    int on = 1;
    if ((setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0) {
        LOG("set socket reused address failed! reason: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(_bindIp.c_str());
    addr.sin_port = htons(_port);
    if (-1 == bind(fd, (struct sockaddr*) &addr, sizeof(addr))) {
        LOG("socket bind failed! reason: %s", strerror(errno));
        return -1;
    }

    if (-1 == listen(fd, 100)) {
        LOG("socket listen failed! reason: %s", strerror(errno));
        return -1;
    }
    DBG("socket listen on address %s, port:%d", _bindIp.c_str(), _port);

    return fd;
}

void SoapService::fdReclaCallback(int fd, EventLoopHandler *handler) {
    HttpCodecTable &httpCodecTbl = ((SoapService*)handler)->_httpCodecTbl;
    HttpCodecTable::iterator it = httpCodecTbl.find(fd);
    if (it != httpCodecTbl.end()) {
        httpCodecTbl.erase(it);
    }
    // shutdown(fd, SHUT_RDWR);
    close(fd);
}

int SoapService::setNoblock(int fd) {
    int op;

    op = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, op | O_NONBLOCK);

    return op;
}

const char* SoapService::info() {
    return __FILE__;
}
