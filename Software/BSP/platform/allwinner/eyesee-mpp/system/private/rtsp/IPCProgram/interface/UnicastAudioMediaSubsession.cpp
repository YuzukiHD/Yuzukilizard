#include "UnicastAudioMediaSubsession.h"

#include "MPEG4GenericRTPSink.hh"
#include "TinySource.h"

class ADTSAudioBridgeSource: public FramedSource {
public:
    static ADTSAudioBridgeSource* createNew(UsageEnvironment& env, TinySource &tinySource);

    unsigned samplingFrequency() const {
        return fSamplingFrequency;
    }
    unsigned numChannels() const {
        return fNumChannels;
    }
    char const* configStr() const {
        return fConfigStr;
    }
    // returns the 'AudioSpecificConfig' for this stream (in ASCII form)

private:
    ADTSAudioBridgeSource(UsageEnvironment& env, TinySource &tinySource);
    // called only by createNew()

    virtual ~ADTSAudioBridgeSource();
    static void afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
            struct timeval presentationTime, unsigned durationInMicroseconds);
    void afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime,
            unsigned durationInMicroseconds);
    bool parseHeaderInfo();

private:
    // redefined virtual functions:
    virtual void doGetNextFrame();

private:
    unsigned fSamplingFrequency;
    unsigned fNumChannels;
    unsigned fuSecsPerFrame;
    char fConfigStr[5];

    TinySource &_tinySource;
    bool _needParseHeader;
};

UnicastAudioMediaSubsession*
UnicastAudioMediaSubsession::createNew(UsageEnvironment &env, TinySource &tinySource) {
    return new UnicastAudioMediaSubsession(env, tinySource);
}

UnicastAudioMediaSubsession::UnicastAudioMediaSubsession(UsageEnvironment &env, TinySource &tinySource) :
        OnDemandServerMediaSubsession(env, true), _tinySource(tinySource) {
}

UnicastAudioMediaSubsession::~UnicastAudioMediaSubsession() {
}

FramedSource* UnicastAudioMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
//    estBitrate = 96; // kbps, estimate

    return ADTSAudioBridgeSource::createNew(envir(), _tinySource);;
}

RTPSink* UnicastAudioMediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
        FramedSource* inputSource) {
    ADTSAudioBridgeSource* adtsSource = (ADTSAudioBridgeSource*) inputSource;
    return MPEG4GenericRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
            adtsSource->samplingFrequency(), "audio", "AAC-hbr", adtsSource->configStr(), adtsSource->numChannels());
}

////////// ADTSAudioBridgeSource //////////
#include <GroupsockHelper.hh>

static unsigned const samplingFrequencyTable[16] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000,
        12000, 11025, 8000, 7350, 0, 0, 0 };

ADTSAudioBridgeSource*
ADTSAudioBridgeSource::createNew(UsageEnvironment& env, TinySource &tinySource) {
    return new ADTSAudioBridgeSource(env, tinySource);
}

ADTSAudioBridgeSource::ADTSAudioBridgeSource(UsageEnvironment& env, TinySource &tinySource) :
        FramedSource(env), _tinySource(tinySource), _needParseHeader(false) {

    fSamplingFrequency = 8000;
    fNumChannels = 1;
    fuSecsPerFrame = 0;
    fConfigStr[5] = {0};

    if (_needParseHeader) {
        _needParseHeader = parseHeaderInfo() ? false : true;
    }

    _tinySource.addRef();
}

ADTSAudioBridgeSource::~ADTSAudioBridgeSource() {
    _tinySource.minRef();
}

void ADTSAudioBridgeSource::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
        struct timeval presentationTime, unsigned durationInMicroseconds) {
    ADTSAudioBridgeSource* source = (ADTSAudioBridgeSource*) clientData;
    source->afterGettingFrame1(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void ADTSAudioBridgeSource::afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes,
        struct timeval presentationTime, unsigned durationInMicroseconds) {

    fFrameSize = frameSize;
    fNumTruncatedBytes = numTruncatedBytes;
    fPresentationTime = presentationTime;
    fDurationInMicroseconds = durationInMicroseconds;

    afterGetting(this);
}

bool ADTSAudioBridgeSource::parseHeaderInfo() {
    // Now, having opened the input file, read the fixed header of the first frame,
    // to get the audio stream's parameters:
    unsigned char fixedHeader[4]; // it's actually 3.5 bytes long
    memcpy(fixedHeader, fTo, sizeof(fixedHeader));

    // Check the 'syncword':
    if (!(fixedHeader[0] == 0xFF && (fixedHeader[1] & 0xF0) == 0xF0)) {
        envir().setResultMsg("Bad 'syncword' at start of ADTS file");
        return false;
    }

    // Get and check the 'profile':
    u_int8_t profile = (fixedHeader[2] & 0xC0) >> 6; // 2 bits
    if (profile == 3) {
        envir().setResultMsg("Bad (reserved) 'profile': 3 in first frame of ADTS file");
        return false;
    }

    // Get and check the 'sampling_frequency_index':
    u_int8_t sampling_frequency_index = (fixedHeader[2] & 0x3C) >> 2; // 4 bits
    if (samplingFrequencyTable[sampling_frequency_index] == 0) {
        envir().setResultMsg("Bad 'sampling_frequency_index' in first frame of ADTS file");
        return false;
    }

    // Get and check the 'channel_configuration':
    u_int8_t channel_configuration = ((fixedHeader[2] & 0x01) << 2) | ((fixedHeader[3] & 0xC0) >> 6); // 3 bits


    channel_configuration = 1;
    sampling_frequency_index = 11;


    fSamplingFrequency = samplingFrequencyTable[sampling_frequency_index];
    fNumChannels = channel_configuration == 0 ? 2 : channel_configuration;
    fuSecsPerFrame = (1024/*samples-per-frame*/* 1000000) / fSamplingFrequency/*samples-per-second*/;

//    fuSecsPerFrame = 12200;
    profile = 1;

    // Construct the 'AudioSpecificConfig', and from it, the corresponding ASCII string:
    unsigned char audioSpecificConfig[2];
    u_int8_t const audioObjectType = profile + 1;
    audioSpecificConfig[0] = (audioObjectType << 3) | (sampling_frequency_index >> 1);
    audioSpecificConfig[1] = (sampling_frequency_index << 7) | (channel_configuration << 3);
    sprintf(fConfigStr, "%02X%02x", audioSpecificConfig[0], audioSpecificConfig[1]);

    return true;
}

// Note: We should change the following to use asynchronous file reading, #####
// as we now do with ByteStreamFileSource. #####
void ADTSAudioBridgeSource::doGetNextFrame() {
    // Begin by reading the 7-byte fixed_variable headers:
    _tinySource.getNextFrame(fTo, fMaxSize, (FramedSource::afterGettingFunc*) afterGettingFrame, this,
            FramedSource::handleClosure, this);
}
