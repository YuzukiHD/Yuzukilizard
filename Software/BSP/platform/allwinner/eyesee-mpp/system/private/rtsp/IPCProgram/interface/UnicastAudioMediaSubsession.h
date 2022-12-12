#ifndef _ADTS_AUDIO_FILE_SERVER_MEDIA_SUBSESSION_HH
#define _ADTS_AUDIO_FILE_SERVER_MEDIA_SUBSESSION_HH

#include "OnDemandServerMediaSubsession.hh"

class TinySource;

class UnicastAudioMediaSubsession: public OnDemandServerMediaSubsession {
public:
    static UnicastAudioMediaSubsession*
    createNew(UsageEnvironment &env, TinySource &tinySource);

protected:
    UnicastAudioMediaSubsession(UsageEnvironment &env, TinySource &tinySource);
    // called only by createNew();
    virtual ~UnicastAudioMediaSubsession();

protected:
    // redefined virtual functions
    virtual FramedSource* createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate);
    virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
            FramedSource* inputSource);

private:
    TinySource &_tinySource;
};

#endif
