/*
 * EventLoop.h
 *
 *  Created on: 2017年1月17日
 *      Author: liu
 */

#ifndef SRC_EVENTLOOP_H_
#define SRC_EVENTLOOP_H_

#include <sys/epoll.h>
#include <vector>

#define MAX_EVENTS 1024

namespace onvif {

class EventLoopHandler {
public:
    virtual ~EventLoopHandler() {};
    virtual void onRead(int fd) {};
    virtual void onWrite(int fd) {};
    virtual void onError(int fd) {};
    virtual const char* info() = 0;
};

class EventLoop {
public:
    typedef void(BeforeReclaFunc)(int fd, EventLoopHandler *handler);

    enum EventType {
        EventTypeRead,
        EventTypeWrite,
        EventTypeError
    };

    EventLoop();
    ~EventLoop();

    int scheduleEvent(int fd, EventType type, EventLoopHandler *handler);
    int scheduleEvent(int fd, EventType type, EventLoopHandler *handler, int timeout);
    int unscheduleEvent(int fd);
    int registBeforeReclaFunc(int fd, BeforeReclaFunc *func);
    void run();
    void stop();

private:
    struct CallbackData;
    bool validCallbackData(CallbackData *cb);
    void releaseCallbackData(CallbackData *cb);
    int initEventFd();

private:
    struct CallbackData {
        int fd;
        int countDown;
        EventLoopHandler *handler;
        BeforeReclaFunc *beforeReclaFunc;
    };

    typedef std::vector<CallbackData*> CallbackDataList;

    int createEpoll();
    void oneStep();

    bool _isRuning;
    int _epollFd;
    int _eventFd;
    struct epoll_event _events[MAX_EVENTS];
    CallbackDataList _callbackList;
};

} /* namespace onvif */

#endif /* SRC_EVENTLOOP_H_ */
