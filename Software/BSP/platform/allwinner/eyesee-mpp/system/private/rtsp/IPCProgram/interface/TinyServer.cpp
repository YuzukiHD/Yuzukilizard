/*
 * TinyServer.cpp
 *
 *  Created on: 2016骞�6鏈�29鏃�
 *      Author: A
 */

#include "../interface/TinyServer.h"

#include <BasicUsageEnvironment.hh>
#include <FramedSource.hh>
#include <GroupsockHelper.hh>
#include <GroupsockHelper.hh>
#include <Media.hh>
#include <iostream>
#include <string.h>
#include <errno.h>
#include <sys/prctl.h>
#include "MediaStream.h"
#include "RTSPServer.hh"
#include "TinySource.h"

#define SERVER_RUN_FLAG 0
#define SERVER_STOP_FLAG 1

#ifndef AW_RTSP_LIB_VERSION
#define AW_RTSP_LIB_VERSION "UnKnown                                 "
#endif

#ifndef BUILD_BY_WHO
#define BUILD_BY_WHO "UnKnown"
#endif

#ifndef BUILD_TIME
#define BUILD_TIME __DATE__
#endif

using namespace std;

static void printVersion() {
    fprintf(stderr, "\t\t\t\t*********************************************************************************************************\n");
    fprintf(stderr, "\t\t\t\t*\t\tAW RTSP LIB VERSION: \t%s\t\t\t*\n", AW_RTSP_LIB_VERSION);
    fprintf(stderr, "\t\t\t\t*\t\tBUILD BY WHO: \t\t%s\t\t\t\t\t\t\t\t*\n", BUILD_BY_WHO);
    fprintf(stderr, "\t\t\t\t*\t\tBUILD TIME: \t\t%s\t\t\t\t*\n", BUILD_TIME);
    fprintf(stderr, "\t\t\t\t*********************************************************************************************************\n");
}

TinyServer* TinyServer::createServer() { return new TinyServer("", 8554); }

TinyServer* TinyServer::createServer(const std::string& ip, int port) { return new TinyServer(ip, port); }

MediaStream* TinyServer::createMediaStream(string const& name, MediaStream::MediaStreamAttr attr)
{
    return new MediaStream(name, *this, attr);
}

RTSPServer* TinyServer::getRTSPServer() { return _rtspServer; }

TinyServer::TinyServer(const std::string ip, int port)
    : _scheduler(NULL),
      _env(NULL),
      _rtspServer(NULL),
      _serverIp(ip),
      _port(port),
      _runFlag(SERVER_RUN_FLAG),
      _loopRunInThread(false),
      _loopThreadId(-1)
{
    printVersion();
    postConstructor();
}

TinyServer::~TinyServer()
{
    Medium::close(_rtspServer);
    _rtspServer = NULL;

    if (_env->reclaim()) {
        _env = NULL;
    } else {
        printf("\n\n!!!!!!==============envirment relaim failed!====================\n\n");
    }

    delete _scheduler;
    _scheduler = NULL;
}

void TinyServer::postConstructor()
{
    _scheduler = BasicTaskScheduler::createNew();
    _env = BasicUsageEnvironment::createNew(*_scheduler);

    // TODO:校验ip地址的有效性，是否有必要？
    if (_serverIp.length() > 0) {
        ReceivingInterfaceAddr = inet_addr(_serverIp.c_str());
    }
    _rtspServer = RTSPServer::createNew(*_env, _port);

    if (_rtspServer == NULL) {
        *_env << "Failed to create RTSP server: " << _env->getResultMsg() << "\n";
        exit(1);
    }
}

void TinyServer::stop()
{
    _runFlag = SERVER_STOP_FLAG;
    if (_loopRunInThread == true) {
        pthread_join(_loopThreadId, NULL);
    }
}

int TinyServer::runWithNewThread()
{
    _runFlag = SERVER_RUN_FLAG;
    int ret = pthread_create(&_loopThreadId, NULL, loopThreadFunc, this);
    if (ret != 0) {
        printf("TinyServer run with new thread failed! message: %s\n", strerror(errno));
        return -1;
    }
    _loopRunInThread = true;

    return 0;
}

void TinyServer::run()
{
    _runFlag = SERVER_RUN_FLAG;
    loopThreadFunc(this);
}

void* TinyServer::loopThreadFunc(void* server)
{
    prctl(PR_SET_NAME, "RtspTinyServerLoop", 0, 0, 0);
    TinyServer* tinyServer = (TinyServer*)server;
    tinyServer->_env->taskScheduler().doEventLoop(&tinyServer->_runFlag);  // does not return
    return NULL;
}
