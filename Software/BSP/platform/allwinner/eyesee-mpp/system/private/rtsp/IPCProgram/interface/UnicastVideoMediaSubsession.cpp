/*
 * UnicastVideoMediaSubsession.cpp
 *
 *  Created on: 2016年8月30日
 *      Author: liu
 */

#include "UnicastVideoMediaSubsession.h"
#include <iostream>
#include <string.h>
#include <errno.h>
#include "BasicUsageEnvironment.hh"
#include "H264VideoStreamDiscreteFramer.hh"
#include "H265VideoStreamDiscreteFramer.hh"
#include "H264VideoRTPSink.hh"
#include "H265VideoRTPSink.hh"
#include "TinySource.h"

using namespace std;

class BridgeSource: FramedSource {
  public:
    BridgeSource(UsageEnvironment &env, TinySource &tinySource);
    ~BridgeSource();
    TinySource& getTinySource() {
        return _tinySource;
    }

  private:
    virtual void doGetNextFrame();
    static void afterGettingFrame(void *clientData, unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime,
        unsigned durationInMicroseconds);
    void afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned durationInMicroseconds);

  private:
    TinySource &_tinySource;
};

UnicastVideoMediaSubsession::UnicastVideoMediaSubsession(UsageEnvironment &env, TinySource &tinySource) :
    OnDemandServerMediaSubsession(env, true), _tinySource(tinySource) {
}

UnicastVideoMediaSubsession::~UnicastVideoMediaSubsession() {
  // _tinySource为外部创建，生命周期不归subsession管理
}

UnicastVideoMediaSubsession *UnicastVideoMediaSubsession::createNew(UsageEnvironment &env, TinySource &source) {
  return new UnicastVideoMediaSubsession(env, source);
}

FramedSource *UnicastVideoMediaSubsession::createNewStreamSource(unsigned int clientSessionId, unsigned int &estBitrate) {
  estBitrate = 1024*1024;
  FramedSource *s = NULL;
  BridgeSource *source = new BridgeSource(envir(), _tinySource);
  if (_tinySource.getPayloadType() == TinySource::PayloadTypeH265) {
      s = H265VideoStreamDiscreteFramer::createNew(envir(), (FramedSource*)source);
  } else {
      s = H264VideoStreamDiscreteFramer::createNew(envir(), (FramedSource*)source);
  }

  return s;
}

RTPSink *UnicastVideoMediaSubsession::createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtspPayloadTypeIfDynamic, FramedSource *inputSource) {
#define DYNAMIC_PAYLOAD_TYPE 96
  MultiFramedRTPSink *sink = NULL;
  string sps, pps, vps;
  _tinySource.getSps(sps);
  _tinySource.getPps(pps);
  _tinySource.getVps(vps);
  switch (_tinySource.getPayloadType()) {
  case TinySource::PayloadTypeH264:
    sink = H264VideoRTPSink::createNew(envir(), rtpGroupsock, DYNAMIC_PAYLOAD_TYPE
            , (uint8_t*)sps.c_str(), sps.size()
            , (uint8_t*)pps.c_str(), pps.size());
    break;
  case TinySource::PayloadTypeH265:
    sink = H265VideoRTPSink::createNew(envir(), rtpGroupsock, DYNAMIC_PAYLOAD_TYPE
            , (uint8_t*)sps.c_str(), sps.size()
            , (uint8_t*)pps.c_str(), pps.size()
            , (uint8_t*)vps.c_str(), vps.size());
    break;
  default:
    sink = H264VideoRTPSink::createNew(envir(), rtpGroupsock, DYNAMIC_PAYLOAD_TYPE
            , (uint8_t*)sps.c_str(), sps.size()
            , (uint8_t*)pps.c_str(), pps.size());
    break;
  }

  sink->setOnSendErrorFunc([](void *clientData) {
	  MultiFramedRTPSink *s = (MultiFramedRTPSink*)clientData;
	  printf("send rtp data failed! msg:%s\n", strerror(errno));
  }, sink);

  sink->setComm(_tinySource.getComm());

  return sink;
#undef DYNAMIC_PAYLOAD_TYPE
}

/*
 *  BridgeSource class
 */
BridgeSource::BridgeSource(UsageEnvironment &env, TinySource &tinySource) :
    FramedSource(env), _tinySource(tinySource) {
    _tinySource.addRef();
}

BridgeSource::~BridgeSource() {
  _tinySource.minRef();
}

static void onClose(void *data) {
  std::cout << "TinySource is closed!  report by [" << __FUNCTION__ << "]" << std::endl;
}

void BridgeSource::afterGettingFrame(void *clientData, unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime,
    unsigned durationInMicroseconds) {
  BridgeSource *source = (BridgeSource *) clientData;
  source->afterGettingFrame1(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void BridgeSource::afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime,
    unsigned durationInMicroseconds) {
  fFrameSize = frameSize;
  fNumTruncatedBytes = numTruncatedBytes;
  fPresentationTime = presentationTime;
  fDurationInMicroseconds = durationInMicroseconds;
  afterGetting(this);
}

void BridgeSource::doGetNextFrame() {
  _tinySource.getNextFrame(fTo, fMaxSize, (FramedSource::afterGettingFunc *) afterGettingFrame, this, onClose, NULL);
}
