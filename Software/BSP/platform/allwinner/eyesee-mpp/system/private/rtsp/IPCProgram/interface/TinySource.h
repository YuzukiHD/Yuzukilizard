/*
 * TinySource.h
 *
 *  Created on: 2016年8月30日
 *      Author: liu
 */

#ifndef IPCPROGRAM_INTERFACE_TINYSOURCE_H_
#define IPCPROGRAM_INTERFACE_TINYSOURCE_H_

#include "pthread.h"
#include "UsageEnvironment.hh"
#include "FramedSource.hh"
#include <sys/eventfd.h>
#include <stdint.h>
#include <string>

class MediaBufferList;

class TinySource: public FramedSource {
public:
    enum PayloadType {
        PayloadTypeH264, PayloadTypeH265, PayloadTypeAAC
    };

    static TinySource* createNew(UsageEnvironment &env, PayloadType payloadType);
    virtual ~TinySource();

    PayloadType getPayloadType() const;
    void setData(unsigned char*data, unsigned int size, uint64_t pts);
    void setFrameRate(double rate);
    double getFrameRate() const;
    virtual void doGetNextFrame();
    void addRef();
    void minRef();
    void setComment(const std::string &comm);

    void setSps(const std::string &sps) {
        _sps = sps;
    }

    void getSps(std::string &sps) const {
        sps = _sps;
    }

    void setPps(const std::string &pps) {
        _pps = pps;
    }

    void getPps(std::string &pps) const {
        pps = _pps;
    }

    void setVps(const std::string &vps) {
        _vps = vps;
    }

    void getVps(std::string &vps) const {
        vps = _vps;
    }

private:
    TinySource(UsageEnvironment &env, PayloadType type);
    static void doGetNextFrame1(TinySource *source);
    static void eventFdHandler(void* clientData, int mask);
    void clearContext();
    void checkDurationTime();

    double _frameRate;
    PayloadType _type;
    int32_t _refCount;
    uint64_t _lastPts;
    EventTriggerId _eventId;
    struct timeval _lastGetFrameTime;
    int _eventFd;

    MediaBufferList *_list;
    bool _isContinueRun;

    std::string _sps;
    std::string _pps;
    std::string _vps;
};

#endif /* IPCPROGRAM_INTERFACE_TINYSOURCE_H_ */
