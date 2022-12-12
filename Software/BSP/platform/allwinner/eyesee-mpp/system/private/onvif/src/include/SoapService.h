/*
 * DiscoveryService.h
 *
 *  Created on: 2017年1月17日
 *      Author: liu
 */

#pragma once

#include <string>
#include <map>
#include "EventLoop.h"

namespace onvif {

class XmlBuilder;
class SoapService;
class HttpCodec;

struct OnvifContext {
    SoapService *service;
    void *param;
};

class SoapService : EventLoopHandler {
public:
    SoapService(EventLoop &loop, const std::string &bindIp, const int port);
    ~SoapService();

    void setContext(struct OnvifContext *context);
    struct OnvifContext* getContext();
    std::string serviceIp();
    int servicePort();

    //redefine virtual function
    void onRead(int fd);
    const char* info();

private:
    int createListenSocket();
    int setNoblock(int fd);
    int invokeOnvifImpl(const std::string &method, XmlBuilder &req, XmlBuilder &resp);

    static void fdReclaCallback(int fd, EventLoopHandler *handler);

private:
    EventLoop &_loop;
    std::string _bindIp;
    int _port;
    int _listenFd;

    OnvifContext *_context;
    typedef std::map<int, HttpCodec*> HttpCodecTable;
    HttpCodecTable _httpCodecTbl;
};

} /* namespace onvif */

