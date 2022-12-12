//
// Created by A on 2016/6/22.
//

#ifndef LIVE_2016_05_20_IPCSOURCE_H
#define LIVE_2016_05_20_IPCSOURCE_H

#include "CameraV4l2.h"
#include "FramedSource.hh"

extern "C" {
#include "encode.h"
}

class IPCSource : public FramedSource {

  public:
    static IPCSource *createNew(UsageEnvironment &env, const char *dev);
    virtual ~IPCSource();
    virtual void doGetNextFrame() override;

    typedef void (*DidCompleteEncode)(unsigned char *data, unsigned int size);
    void setCompleteCallback(DidCompleteEncode function);
    void encode_data();

  private:
    virtual Boolean isFramedSource() const override { return true; }

    Encode *_encode;
    DidCompleteEncode _callback;
    BYL::CameraV4l2 *_camera;

  protected:
    IPCSource(UsageEnvironment &env, const char *dev);
};

#endif  //LIVE_2016_05_20_IPCSOURCE_H
