/*
 * DiscoveryService.h
 *
 *  Created on: 2017年1月17日
 *      Author: liu
 */

#ifndef SRC_DISCOVERYSERVICE_H_
#define SRC_DISCOVERYSERVICE_H_

#include <string>
#include "EventLoop.h"

namespace onvif {

class SoapService;

class DiscoveryService : EventLoopHandler {
public:
    DiscoveryService(EventLoop &loop, SoapService &service);
    ~DiscoveryService();
    void setOnvifService(SoapService &service);

    //redefine virtual function
    void onRead(int fd);
    const char* info();

private:
    int createMultiCastSocket();

private:
    EventLoop &_loop;
    SoapService &_onvifService;
    int _udpFd;
};

} /* namespace onvif */

#endif /* SRC_DISCOVERYSERVICE_H_ */
