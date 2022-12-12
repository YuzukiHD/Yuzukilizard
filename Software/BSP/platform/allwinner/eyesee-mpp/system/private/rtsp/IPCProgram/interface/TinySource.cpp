/*
 * TinySource1.cpp
 *
 *  Created on: 2016年8月30日
 *      Author: liu
 */

#include "TinySource.h"
#include <list>
#include <time.h>
#include <string>

using namespace std;

class MediaBufferList {
public:
    struct MediaBuffer {
        uint8_t *data;
        uint32_t size;
        uint64_t pts;
        struct timeval in;
    };

    MediaBufferList(uint32_t bitrate, uint8_t delayTime);
    ~MediaBufferList();

    void pushBuffer(MediaBuffer buffer);
    int popBuffer(uint8_t *to, uint32_t size, uint64_t &pts);
    void clear();
    void dump();
    int size() {
        return _bufferList.size();
    }
    void setComm(const string &comm) {
        _comm = comm;
    }

private:
    void saveBuffer(MediaBuffer &buffer);

private:
    list<MediaBuffer> _bufferList;
    uint8_t *_memStart;
    uint32_t _memSize;

    uint8_t *_readIndex;
    uint8_t *_writeIndex;

    pthread_mutex_t _lock;
    pthread_cond_t _writeComplete;
    pthread_cond_t _readComplete;
    pthread_condattr_t _condAttr;
    timespec _waitTime;
    string _comm;

    bool _writing;
};

TinySource *TinySource::createNew(UsageEnvironment &env, TinySource::PayloadType type) {
    return new TinySource(env, type);
}

TinySource::TinySource(UsageEnvironment &env, TinySource::PayloadType type) :
        FramedSource(env), _frameRate(25.0), _type(type), _refCount(0), _list(new MediaBufferList((1 << 20) * 1, 1))
        , _isContinueRun(true) {
    clearContext();
    _eventFd = eventfd(0, 0);
    env.taskScheduler().setBackgroundHandling(_eventFd, SOCKET_READABLE, eventFdHandler, this);
}

TinySource::~TinySource() {
    envir().taskScheduler().disableBackgroundHandling(_eventFd);
    delete _list;
    ::close(_eventFd);
    _eventFd = -1;
}

void TinySource::eventFdHandler(void* clientData, int mask) {
    TinySource *s = (TinySource*)clientData;
    eventfd_t count;
    eventfd_read(s->_eventFd, &count);
    s->doGetNextFrame();
}

void TinySource::doGetNextFrame1(TinySource *source) {
    source->doGetNextFrame();
}

void TinySource::doGetNextFrame() {
    if (fMaxSize == 0) {
        return;
    }

    if (_isContinueRun) {
        if (_list->size() > 0) {
            envir().taskScheduler().rescheduleDelayedTask(nextTask(), 0, (TaskFunc*)doGetNextFrame1, this);
        }
        _isContinueRun = false;
        return;
    } else {
        _isContinueRun = true;
    }

//    printf("fTo: %p, fMaxSize: %d\n", fTo, fMaxSize);
    uint64_t pts;
    int size = _list->popBuffer(fTo, (uint32_t) fMaxSize, pts);
    if (size == -1) {
//        _list->dump();
        return;
    }
    checkDurationTime();
    fFrameSize = size;

    if ((_type == TinySource::PayloadTypeAAC) || (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0)) {
        gettimeofday(&fPresentationTime, NULL);
    } else {
        int64_t diffTime = pts - _lastPts;
        double nextFraction = fPresentationTime.tv_usec / 1000000.0 + diffTime / 1000000.0;
        unsigned nextSecsIncrement = (long) nextFraction;
        fPresentationTime.tv_sec += (long) nextSecsIncrement;
        fPresentationTime.tv_usec = (long) ((nextFraction - nextSecsIncrement) * 1000000);
    }
    _lastPts = pts;

    fDurationInMicroseconds = 0;
    afterGetting(this);
}

void TinySource::setData(unsigned char *data, unsigned int size, uint64_t pts) {
    if (_refCount == 0) {
        return;
    }

    MediaBufferList::MediaBuffer buffer;
    buffer.data = data;
    buffer.size = size;
    buffer.pts  = pts;
    gettimeofday(&buffer.in, NULL);
    _list->pushBuffer(buffer);

    // wakeup event loop
    eventfd_t dummy = 1;
    eventfd_write(_eventFd, dummy);
}

TinySource::PayloadType TinySource::getPayloadType() const {
    return _type;
}

void TinySource::addRef() {
    _refCount++;
}

void TinySource::minRef() {
    if (_refCount < 0) {
        printf("warning: tiny source reference count < 1!\n");
    }
    _refCount--;
    if (0 == _refCount) {
        clearContext();
    }
}

void TinySource::setComment(const std::string &comm) {
    setComm(comm);
    _list->setComm(comm);
}

void TinySource::setFrameRate(double rate) {
    _frameRate = rate;
}

double TinySource::getFrameRate() const {
    return _frameRate;
}

void TinySource::clearContext() {
    _list->clear();
    fTo = NULL;
    fMaxSize = 0;
    fPresentationTime.tv_sec = 0;
    fPresentationTime.tv_usec = 0;

    _lastGetFrameTime.tv_sec = 0;
    _lastGetFrameTime.tv_usec = 0;
}

void TinySource::checkDurationTime() {
    if ((_lastGetFrameTime.tv_sec == 0) && (_lastGetFrameTime.tv_usec == 0)) {
        gettimeofday(&_lastGetFrameTime, NULL);
        return;
    }

    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    uint64_t last_msec = _lastGetFrameTime.tv_sec * 1000 + _lastGetFrameTime.tv_usec / 1000;
    uint64_t curr_msec = currentTime.tv_sec * 1000 + currentTime.tv_usec / 1000;
    int diff = curr_msec - last_msec;
    int interval = (1000.0 / _frameRate) * 3;
    if (diff >= interval) {
        printf("[rtsp-warning] frame send interval[%d], normal interval[%d]"
                ", list size[%d], comm[%s]\n", diff, interval,
                _list->size(), getComm().c_str());
    }
    _lastGetFrameTime = currentTime;
}

/********************************************************************
 *
 *******************************************************************/

MediaBufferList::MediaBufferList(uint32_t bitrate, uint8_t delayTime) :
        _memStart(NULL), _memSize(0), _readIndex(0), _writeIndex(0), _writing(false) {

    _memSize = (bitrate + 7) / 8 * delayTime * 10;
    _memStart = (uint8_t*) malloc(_memSize);
    if (NULL == _memStart) {
        printf("MediaBufferList: there is no memory to use!\n");
    }
    _writeIndex = _memStart;
    _readIndex = _writeIndex;
    _waitTime.tv_sec = 1;
    _waitTime.tv_nsec = 0;

    pthread_condattr_init(&_condAttr);
    pthread_condattr_setclock(&_condAttr, CLOCK_MONOTONIC);

    int ret = pthread_mutex_init(&_lock, NULL);
    if (ret < 0) {
        printf("MediaBufferList: mutex init failed!");
    }

    ret = pthread_cond_init(&_writeComplete, &_condAttr);
    if (ret < 0) {
        printf("MediaBufferList: _writeComplete init failed!");
    }
    ret = pthread_cond_init(&_readComplete, &_condAttr);
    if (ret < 0) {
        printf("MediaBufferList: _readComplete init failed!");
    }
}

MediaBufferList::~MediaBufferList() {
    clear();
    pthread_mutex_destroy(&_lock);
    pthread_cond_destroy(&_readComplete);
    pthread_cond_destroy(&_writeComplete);
    free(_memStart);
}

/**
 * @param buffer
 * @note 实现方式为buffer不会被写满，至少有1个字节的空间，防止读写指针重叠，无法判断状态
 */
void MediaBufferList::pushBuffer(MediaBuffer buffer) {
    if (buffer.size > _memSize) {
        printf("warning: MediaBufferList memory is too slow! memory size:%d, buffer size:%d\n", _memSize, buffer.size);
        return;
    }

    pthread_mutex_lock(&_lock);
    _writing = true;
    do {
        if (_writeIndex >= _readIndex) {
            if (buffer.size < (uint32_t) (_memStart + _memSize - _writeIndex)) {
                saveBuffer(buffer);
                break;
            }

            if (_readIndex == _writeIndex) {
                _readIndex = _memStart;
                _writeIndex = _readIndex;
                continue;
            }

            if (buffer.size < (uint32_t) (_readIndex - _memStart)) {
                _writeIndex = _memStart;
                saveBuffer(buffer);
                break;
            } else {
                // TODO: 读取速度太慢，等待readIndex往后移动
                pthread_cond_timedwait(&_readComplete, &_lock, &_waitTime);
                continue;
            }
        } else {
            if (buffer.size < (uint32_t) (_readIndex - _writeIndex)) {
                saveBuffer(buffer);
                break;
            } else {
                //TODO: 读取速度太慢，等待readIndex往后移动
                pthread_cond_timedwait(&_readComplete, &_lock, &_waitTime);
                continue;
            }
        }
    } while (_writing);

    _writing = false;
    pthread_mutex_unlock(&_lock);
    pthread_cond_signal(&_writeComplete);
//    dump();
}

int MediaBufferList::popBuffer(uint8_t *to, uint32_t size, uint64_t &pts) {
    if (_bufferList.size() == 0) {
        return -1;
    }

    int ret = 0;
    pthread_mutex_lock(&_lock);
    do {
        MediaBuffer b = *_bufferList.begin();
        if (b.size > size) {
            printf("dist buffer is too slow! dist size: %d, buffer, size:%d\n", size, b.size);
            ret = -1;
            break;
        }
        _bufferList.pop_front();
        memcpy(to, b.data, b.size);
        _readIndex = b.data + b.size;
        ret = b.size;
        pts = b.pts;

        struct timeval curr;
        gettimeofday(&curr, NULL);
        uint64_t last_msec = b.in.tv_sec * 1000 + b.in.tv_usec / 1000;
        uint64_t curr_msec = curr.tv_sec * 1000 + curr.tv_usec / 1000;
        int diff = curr_msec - last_msec;
        if (diff >= 60) {
            printf("[rtsp-warning] frame stay in queue time[%d], comm[%s]\n", diff, _comm.c_str());
        }

    } while (0);
    pthread_mutex_unlock(&_lock);
    pthread_cond_signal(&_readComplete);
//    dump();
    return ret;
}

void MediaBufferList::clear() {
    if (_writing) {
        _writing = false;
        pthread_cond_signal(&_readComplete);
        usleep(1);
    }
    pthread_mutex_lock(&_lock);
//        pthread_cond_wait(&_writeComplete, &_lock);
    _bufferList.clear();
    _readIndex = _memStart;
    _writeIndex = _readIndex;
    pthread_mutex_unlock(&_lock);
}

void MediaBufferList::saveBuffer(MediaBuffer &buffer) {
    memcpy(_writeIndex, buffer.data, buffer.size);
    buffer.data = _writeIndex;
    _writeIndex += buffer.size;
    _bufferList.push_back(buffer);
}

void MediaBufferList::dump() {
    static int i;
    if ((i % 20) != 0) {
        return;
    }

    printf("start address: %p\n"
            "read ptr: %p\n"
            "write ptr: %p\n"
            "list size: %d\n"
            "content:\n", _memStart, _readIndex, _writeIndex, _bufferList.size());
    for (list<MediaBuffer>::iterator it = _bufferList.begin(); it != _bufferList.end(); it++) {
        printf("%p(%d) | ", it->data, it->size);
    }
}
