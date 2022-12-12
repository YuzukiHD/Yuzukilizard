/*
 * UnicastVideoMediaSubsession.h
 *
 *  Created on: 2016年8月30日
 *      Author: liu
 */

#ifndef IPCPROGRAM_UnicastVideoMediaSubsession_H_
#define IPCPROGRAM_UnicastVideoMediaSubsession_H_

#include "OnDemandServerMediaSubsession.hh"

class TinySource; //forward
class UsageEnvironment;

class UnicastVideoMediaSubsession : public OnDemandServerMediaSubsession {
public:
    static UnicastVideoMediaSubsession *createNew(UsageEnvironment &env, TinySource &tinySource);
    virtual ~UnicastVideoMediaSubsession();

protected:
  virtual FramedSource* createNewStreamSource(unsigned clientSessionId,
                          unsigned& estBitrate);
  virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
                    unsigned char rtpPayloadTypeIfDynamic,
                    FramedSource* inputSource) ;

private:
    UnicastVideoMediaSubsession(UsageEnvironment &env, TinySource &tinySource);

private:
    TinySource &_tinySource;
};

#endif /* IPCPROGRAM_UnicastVideoMediaSubsession_H_ */
