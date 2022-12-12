/*
 * testDiscovery.cpp
 *
 *  Created on: 2017年1月17日
 *      Author: liu
 */
#include "EventLoop.h"
#include "DiscoveryService.h"
#include "SoapService.h"

using namespace onvif;

int main(int argc, char **argv) {
    EventLoop loop;

    SoapService soap(loop, argv[1], 8080);
    DiscoveryService discovery(loop, soap);
    SoapService::OnvifContext context;

    context.service = &soap;
    context.param = NULL;
    soap.setContext(&context);

    loop.run();

    return 0;
}




