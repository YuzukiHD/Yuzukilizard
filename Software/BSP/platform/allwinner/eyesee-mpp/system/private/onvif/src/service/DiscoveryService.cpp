/*
 * DiscoveryService.cpp
 *
 *  Created on: 2017年1月17日
 *      Author: liu
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <utils.h>

#include "DiscoveryService.h"
#include "SoapService.h"
#include "HttpCodec.h"
#include "SoapUtils.h"
#include "XmlBuilder.h"
#include "OnvifMethod.h"

using namespace onvif;
using namespace std;

#define MULTICAST_PORT 3702
#define MULTICAST_GROUP "239.255.255.250"

// TODO: set udp socket fd no block
DiscoveryService::DiscoveryService(EventLoop &loop, SoapService &service) :
        _loop(loop), _onvifService(service), _udpFd(-1) {
    _udpFd = createMultiCastSocket();
    if (_udpFd < 0) {
        LOG("create multicast socket failed!");
    } else {
        int ret = _loop.scheduleEvent(_udpFd, EventLoop::EventTypeRead, this, -1);
        if (ret < 0) {
            LOG("scheduleEvent failed! return value: %d", ret);
        }
    }
}

DiscoveryService::~DiscoveryService() {
    _loop.unscheduleEvent(_udpFd);
    close(_udpFd);
}

void DiscoveryService::onRead(int fd) {
    DBG("fd:%d  udpFd:%d", fd, _udpFd);

    for (;;) {
        char buffer[665535];
        struct sockaddr_in peerAddr;
        socklen_t peerAddrLen = sizeof(peerAddr);
        memset(&peerAddr, 0, sizeof(peerAddr));
        memset(&buffer, 0, sizeof(buffer));

        int recvSize = recvfrom(_udpFd, buffer, sizeof(buffer), 0, (struct sockaddr*) &peerAddr, &peerAddrLen);
        if (recvSize < 0) {
            LOG("recv data failed!");
            return;
        } else if (recvSize == sizeof(buffer)) {
            LOG("recv too mach data, content: %s", buffer);
            return;
        } else if (recvSize == 0) {
            return;
        } else {
//        DBG("recv data: %s", buffer);
        }

        XmlBuilder req;
        int ret = req.parse(buffer);
        if (ret < 0) {
            LOG("recv data is not xml, buffer:\n%s", buffer);
            return;
        }

        SoapUtils::addVisitNamespace(req, "wsdd", SoapUtils::getAllXmlNameSpace());
        if ("Probe" != SoapUtils::soapFunctionName(req)) {
            LOG("recv data is not Probe message!");
            req.dump();
            return;
        }

        LOG("[**************** Probe method invoked! ***************]");
        XmlBuilder *resp = SoapUtils::createSoapTemplate("wsdd:wsa:tdn", SoapUtils::getAllXmlNameSpace());
//        SoapUtils::addVisitNamespace(*resp, "wsdd:wsa:tdn", SoapUtils::getAllXmlNameSpace());
        ret = Probe(&_onvifService, req, *resp);
        if (ret < 0) {
            LOG("Probe method invoke failed!");
            SoapUtils::buildFailedMessage(*resp);
        }
//        resp->dump();

        ret = sendto(_udpFd, resp->toString().c_str(), resp->toString().size(), 0, (struct sockaddr*) &peerAddr,
                peerAddrLen);
        if (ret < 0) {
            LOG("sendto function invoke failed!");
        }

        delete resp;
    }
}

int DiscoveryService::createMultiCastSocket() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    int reused = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reused, sizeof(reused)) < 0) {
        LOG("set socket reused address failed!");
        return -1;
    }

    int op = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, op | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
//    addr.sin_addr.s_addr = inet_addr(_onvifService.serviceIp().c_str());
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(MULTICAST_PORT);
    if (0 > bind(fd, (struct sockaddr*) &addr, sizeof(addr))) {
        LOG("socket bind failed!");
        return -1;
    }
    DBG("Discovery socket bind address on %s port:%d", _onvifService.serviceIp().c_str(), MULTICAST_PORT);

    struct ip_mreq mcast;
    memset(&mcast, 0, sizeof(mcast));
    mcast.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
    mcast.imr_interface.s_addr = inet_addr(_onvifService.serviceIp().c_str());
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mcast, sizeof(mcast)) < 0) {
        LOG("setsockopt error!\n");
        return -1;
    }

    return fd;
}

const char* DiscoveryService::info() {
    return __FILE__;
}
