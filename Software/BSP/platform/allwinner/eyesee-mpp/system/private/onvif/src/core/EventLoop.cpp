/*
 * EventLoop.cpp
 *
 *  Created on: 2017年1月17日
 *      Author: liu
 */

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/eventfd.h>
#include <utils.h>
#include "EventLoop.h"

#define DEFAULT_TIMEOUT 65535

using namespace onvif;

EventLoop::EventLoop() :
        _isRuning(true) {
    memset(_events, 0, sizeof(_events));
    _epollFd = createEpoll();
    _eventFd = initEventFd();
}

EventLoop::~EventLoop() {
    stop();
    close(_eventFd);
    close(_epollFd);

    CallbackDataList::iterator it = _callbackList.begin();
    while (it != _callbackList.end()) {
        releaseCallbackData(*it);
        it++;
    }
}

int EventLoop::scheduleEvent(int fd, EventType type, EventLoopHandler* handler) {
    return scheduleEvent(fd, type, handler, DEFAULT_TIMEOUT);
}

int EventLoop::scheduleEvent(int fd, EventType type, EventLoopHandler* handler, int timeout) {
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    CallbackData *cb = new CallbackData();
    cb->fd = fd;
    cb->handler = handler;
    cb->countDown = timeout;
    cb->beforeReclaFunc = NULL;
    _callbackList.push_back(cb);
    event.data.ptr = cb;
    switch (type) {
        case EventTypeRead:
            event.events = EPOLLIN | EPOLLET;
//            event.events = EPOLLIN;
            break;
        case EventTypeWrite:
            event.events = EPOLLOUT | EPOLLET;
            break;
        case EventTypeError:
            event.events = EPOLLERR | EPOLLET;
            break;
        default: {
            DBG("unknown event type!");
            return -1;
        }
    }

    return epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &event);
}

int EventLoop::unscheduleEvent(int fd) {
//    struct epoll_event event;
//    memset(&event, 0, sizeof(event));
    int ret = epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
    if (ret < 0) {
        LOG("socket fd: %d epoll control delete failed!", fd);
    }

    CallbackDataList::iterator it = _callbackList.begin();
    while (it != _callbackList.end()) {
        if (fd == (*it)->fd) {
            // TODO: 剩余socket的数量多了一个，尚未定位是那里多出来的
//            DBG("unschedule socket, fd:%d, remain fd size: %d", (*it)->fd, _callbackList.size());
            releaseCallbackData(*it);
            _callbackList.erase(it);

            return 0;
        }
        it++;
    }

    return -1;
}

int EventLoop::registBeforeReclaFunc(int fd, BeforeReclaFunc *func) {
    CallbackDataList::iterator it = _callbackList.begin();
    while (it != _callbackList.end()) {
        if (fd == (*it)->fd) {
            (*it)->beforeReclaFunc = func;

            return 0;
        }
        it++;
    }

    return -1;
}

void EventLoop::run() {
    _isRuning = true;
    while (_isRuning) {
        oneStep();
    }
}

void EventLoop::stop() {
    _isRuning = false;
    uint64_t dummy = 0;
    write(_eventFd, &dummy, sizeof(dummy));
}

void EventLoop::oneStep() {
    int ret = epoll_wait(_epollFd, _events, MAX_EVENTS, 1000);
    if (ret < 0) {
        if (errno != EAGAIN)
            DBG("epoll_wait failed! ret: %d errno:%d, message:%s", ret, errno, strerror(errno));
        return;
    }

    for (int i = 0; i < ret; ++i) {
        struct epoll_event *ev = &(_events[i]);
        CallbackData *cb = (CallbackData*)_events[i].data.ptr;
        if (!validCallbackData(cb)) continue;
        // note fd is living
        cb->countDown = cb->countDown == -1 ? -1 : DEFAULT_TIMEOUT;
        int fd = cb->fd;
        EventLoopHandler *handler = cb->handler;
        if (NULL == handler) continue;
        if (ev->events == EPOLLIN) {
//            DBG("EPOLLIN occur, fd: %d", fd);
            handler->onRead(fd);
        } else if (ev->events == EPOLLOUT) {
            handler->onWrite(fd);
        } else if (ev->events == EPOLLERR) {
            LOG("the fd: %d, occur error!", fd);
            handler->onError(fd);
        } else {
            DBG("unknown events! event: %d, fd:%d, info:%s", ev->events, ev->data.fd, handler->info());
        }
    }

    CallbackDataList::iterator it = _callbackList.begin();
    while (it != _callbackList.end()) {
        CallbackData *cb = *it;
        if (cb->countDown != -1) {
            cb->countDown--;
        }

        if (cb->countDown == 0) {
            LOG("recalm the timeout socket, fd:%d, remain file descriptor size: %d", cb->fd, _callbackList.size());
            releaseCallbackData(cb);
            it = _callbackList.erase(it);
        } else {
            it++;
        }
    }
}

bool EventLoop::validCallbackData(EventLoop::CallbackData *cb) {
    CallbackDataList::iterator it = _callbackList.begin();
    while (it != _callbackList.end()) {
        if ((*it) == cb) {
            return true;
        }
        it++;
    }

    return false;
}

void EventLoop::releaseCallbackData(EventLoop::CallbackData *cb) {
    if (cb->beforeReclaFunc != NULL)
        cb->beforeReclaFunc(cb->fd, cb->handler);
    delete cb;
}

int EventLoop::createEpoll() {
    int fd = epoll_create(65535);
    if (fd < 0) {
        LOG("create epoll failed! reason: %s", strerror(errno));
        return -1;
    }

    return fd;
}

int EventLoop::initEventFd() {
   int fd = eventfd(0, EFD_NONBLOCK);
   if (fd < 0) {
      LOG("create eventfd failed! reason: %s", strerror(errno));
      return -1;
   }
   scheduleEvent(fd, EventTypeRead, NULL);

   return fd;
}
