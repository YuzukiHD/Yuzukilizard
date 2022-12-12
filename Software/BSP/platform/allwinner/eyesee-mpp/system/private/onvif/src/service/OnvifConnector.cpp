/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file remote_connector.h
 * @brief Onvif SDK连接适配器接口类
 * @author
 * @version v0.1
 * @date 2016-08-31
 */

#include <signal.h>
#include "OnvifConnector.h"
#include "dev_ctrl_adapter.h"
#include "SoapService.h"
#include "EventLoop.h"
#include "DiscoveryService.h"
#include "utils.h"

#ifndef AW_ONVIF_LIB_VERSION
#define AW_ONVIF_LIB_VERSION "UnKnown                                 "
#endif

#ifndef BUILD_BY_WHO
#define BUILD_BY_WHO "UnKnown"
#endif

#ifndef BUILD_TIME
#define BUILD_TIME __DATE__
#endif

using namespace std;
using namespace onvif;

static void printVersion() {
    fprintf(stderr, "\t\t\t\t*********************************************************************************************************\n");
    fprintf(stderr, "\t\t\t\t*\t\tAW ONVIF LIB VERSION: \t%s\t\t\t*\n", AW_ONVIF_LIB_VERSION);
    fprintf(stderr, "\t\t\t\t*\t\tBUILD BY WHO: \t\t%s\t\t\t\t\t\t\t\t*\n", BUILD_BY_WHO);
    fprintf(stderr, "\t\t\t\t*\t\tBUILD TIME: \t\t%s\t\t\t\t*\n", BUILD_TIME);
    fprintf(stderr, "\t\t\t\t*********************************************************************************************************\n");
}

OnvifConnector::OnvifConnector(DeviceAdapter *adaptor) :
		_adaptor(adaptor), _discoverService(NULL),
		_soapService(NULL), _onvifContext(NULL),
		_loop(NULL), _state(STOP) {
}

OnvifConnector::~OnvifConnector() {
    Stop();

    if (_soapService != NULL)
        delete _soapService;
    if (_onvifContext != NULL)
        delete _onvifContext;
    if (_discoverService != NULL)
        delete _discoverService;
    if (_loop != NULL)
        delete _loop;
}

int OnvifConnector::Init(const InitParam &param) {
    printVersion();

    _loop = new EventLoop();
    _soapService = new SoapService(*_loop, param.arg1, 80);
    _discoverService = new DiscoveryService(*_loop, *_soapService);
    _onvifContext = new OnvifContext();
    _onvifContext->service = _soapService;
    _onvifContext->param = (void*)_adaptor;
    _soapService->setContext(_onvifContext);

	return 0;
}

int OnvifConnector::Start() {
    lock_guard<mutex> lock(_serviceStateMutex);

    _serviceThread = thread(ServiceThread, this);
    _state = RUNING;

	return 0;
}

int OnvifConnector::Stop() {
    lock_guard<mutex> lock(_serviceStateMutex);

    if (_state != RUNING) {
        LOG("onvif module is not runing, current state[%d]", _state);
        return -1;
    }

    _loop->stop();
    Join();
    _state = STOP;
    LOG("onvif module is stop complete");

	return 0;
}

int OnvifConnector::Join() {
    _serviceThread.join();

	return 0;
}

void* OnvifConnector::ServiceThread(OnvifConnector *connector) {
    prctl(PR_SET_NAME, "OnvifServiceThread", 0, 0, 0);
    DBG("OnvifServiceThread thread running: tid[%d], connector[%p]\n", gettid(), connector);
    connector->_loop->run();

	return NULL;
}
