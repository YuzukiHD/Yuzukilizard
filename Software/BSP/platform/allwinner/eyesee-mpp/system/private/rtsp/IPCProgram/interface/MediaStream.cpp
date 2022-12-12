/*
 * MediaStream.cpp
 *
 *  Created on: 2016年8月31日
 *      Author: liu
 */

#include "MediaStream.h"
#include <RTSPServer.hh>
#include <iostream>
#include <string>
#include "GroupsockHelper.hh"
#include "H264VideoRTPSink.hh"
#include "H264or5VideoNaluFramer.hh"
#include "H265VideoRTPSink.hh"
#include "MPEG4GenericRTPSink.hh"
#include "PassiveServerMediaSubsession.hh"
#include "TinyServer.h"
#include "TinySource.h"
#include "UnicastVideoMediaSubsession.h"
#include "UnicastAudioMediaSubsession.h"
#include "FrameNaluParser.h"

#define METHOD_NOT_IMPL "this media stream type is not implement!"

using namespace std;

MediaStream::MediaStream(const string &name, TinyServer &server, MediaStreamAttr attr)
        : _server(server), _streamName(name)
        , _mediaStreamAttr(attr), _rtpGroupsock(NULL)
        , _rtcpGroupsock(NULL), _videoSource(NULL)
        , _audioSource(NULL), _videoSubsession(NULL)
        , _naluFramer(NULL), _videoSink(NULL)
        , _rtcp(NULL) , _nal_parser(NULL)
        , _disconn_token(0), _construct_token(0)
        , _newRecvCallback(NULL) {
    {
        unique_lock<mutex> lock(_task_mutex);
        switch (_mediaStreamAttr.streamType) {
            case MediaStreamAttr::STREAM_TYPE_UNICAST: {
//                _construct_token = _server.getRTSPServer()->envir().taskScheduler().createEventTrigger(unicastConstructor);
//                _server.getRTSPServer()->envir().taskScheduler().triggerEvent(_construct_token, this);
                unicastConstructor(this);
            } break;
            case MediaStreamAttr::STREAM_TYPE_MULTICAST: {
//                _construct_token = _server.getRTSPServer()->envir().taskScheduler().createEventTrigger(multicastConstructor);
//                _server.getRTSPServer()->envir().taskScheduler().triggerEvent(_construct_token, this);
                multicastConstructor(this);
            }
            break;
        }
//        _task_complete.wait(lock);
    }

    _frameRate = 30;
    _lastGetFrameTime.tv_sec = 0;
    _lastGetFrameTime.tv_usec = 0;
    _nal_parser = new Byl::FrameNaluParser(Byl::FrameNaluParser::NotManagerBuffer);
//    _disconn_token = _server.getRTSPServer()->envir().taskScheduler().createEventTrigger(disconnectAllClient);

#ifdef SAVE_FILE
    string file_name = "src_";
    file_name += _streamName;
    file_name += FILE_SUFFIX;
    _of = new ofstream(file_name);
#endif
}

MediaStream::~MediaStream() {
    {
        unique_lock<mutex> lock(_task_mutex);
//        _server.getRTSPServer()->envir().taskScheduler().triggerEvent(_disconn_token, this);
        disconnectAllClient(this);
//        _task_complete.wait(lock);
        _disconn_token = 0;
    }

    switch (_mediaStreamAttr.streamType) {
        case MediaStreamAttr::STREAM_TYPE_UNICAST:
            unicastDestructor();
            break;
        case MediaStreamAttr::STREAM_TYPE_MULTICAST:
            multicastDestructor();
            break;
    }
    delete _nal_parser;

#ifdef SAVE_FILE
    _of->flush();
    _of->close();
#endif
}

string MediaStream::streamURL() {
    RTSPServer *rtspServer = _server.getRTSPServer();
    char *uriStr = rtspServer->rtspURL(rtspServer->lookupServerMediaSession(_streamName.c_str()));
    string uri = uriStr;
    delete uriStr;
    return uri;
}

void MediaStream::appendVideoData(unsigned char *data, unsigned int size) {
    appendVideoData(data, size, FRAME_DATA_TYPE_UNKNOW);
}

void MediaStream::appendVideoData(unsigned char *data, unsigned int size, FrameDataType dataType) {
    appendVideoData(data, size, 0, dataType);
}

void MediaStream::appendVideoData(unsigned char *data, unsigned int size, uint64_t pts, FrameDataType dataType) {
#ifdef SAVE_FILE
    _of->write((char*)data, size);
#endif
    switch (dataType) {
        case FRAME_DATA_TYPE_HEADER:
            saveHeaderInfo(data, size, pts);
            break;
        case FRAME_DATA_TYPE_I:
            sendDataToQueue(data, size, pts);
            break;
        default:
            sendDataToQueue(data, size, pts);
            break;
    }
}

void MediaStream::appendAudioData(unsigned char *data, unsigned int size, uint64_t pts) {
    _audioSource->setData(data, size, pts);
}

void MediaStream::setVideoFrameRate(int frameRate) {
//    printf("videoSource[%p]", _videoSource);
    _frameRate = frameRate;
    _videoSource->setFrameRate((double)frameRate);
}

void MediaStream::checkDurationTime() {
    if ((_lastGetFrameTime.tv_sec == 0) && (_lastGetFrameTime.tv_usec == 0)) {
        gettimeofday(&_lastGetFrameTime, NULL);
        return;
    }

    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    uint64_t last_msec = _lastGetFrameTime.tv_sec * 1000 + _lastGetFrameTime.tv_usec / 1000;
    uint64_t curr_msec = currentTime.tv_sec * 1000 + currentTime.tv_usec / 1000;
    int diff = curr_msec - last_msec;
    int interval = (1000.0 / _frameRate);
    if (diff >= interval) {
        printf("[mediaStream-warning] frame push interval[%d], normal interval[%d]"
                ", last time[%llu], current time[%llu]\n", diff, interval,
                last_msec, curr_msec);
    }
    _lastGetFrameTime = currentTime;
}

void MediaStream::setNewClientCallback(OnNewClientConnectFunc *function, void *context) {
    _videoSubsession->registerPlayCallback(function, context);
}

// FIXME: 多播265未处理
void MediaStream::multicastConstructor(void *context) {
    MediaStream *self = (MediaStream*)context;

    // Create 'groupsocks' for RTP and RTCP:
    struct in_addr destinationAddress;
    destinationAddress.s_addr = chooseRandomIPv4SSMAddress(self->_server.getRTSPServer()->envir());
    // Note: This is a multicast address.  If you wish instead to stream
    // using unicast, then you should use the "testOnDemandRTSPServer"
    // test program - not this test program - as a model.

    const unsigned short rtpPortNum = 18888;
    const unsigned short rtcpPortNum = rtpPortNum + 1;
    const unsigned char ttl = 255;

    const Port rtpPort(rtpPortNum);
    const Port rtcpPort(rtcpPortNum);

    self->_rtpGroupsock = new Groupsock(self->_server.getRTSPServer()->envir(), destinationAddress, rtpPort, ttl);
    self->_rtpGroupsock->multicastSendOnly();  // we're a SSM source
    self->_rtcpGroupsock = new Groupsock(self->_server.getRTSPServer()->envir(), destinationAddress, rtcpPort, ttl);
    self->_rtcpGroupsock->multicastSendOnly();  // we're a SSM source

    // Create a 'H264 or H265 Video RTP' sink from the RTP 'groupsock':
    //	OutPacketBuffer::maxSize = 1000000;
    self->_videoSink = self->createVideoSink();

    // Create (and start) a 'RTCP instance' for this RTP sink:
    const unsigned estimatedSessionBandwidth = 500;  // in kbps; for RTCP b/w share
    const unsigned maxCNAMElen = 100;
    unsigned char CNAME[maxCNAMElen + 1];
    gethostname((char *) CNAME, maxCNAMElen);
    CNAME[maxCNAMElen] = '\0';  // just in case
    self->_rtcp = RTCPInstance::createNew(self->_server.getRTSPServer()->envir(), self->_rtcpGroupsock, estimatedSessionBandwidth, CNAME,
            self->_videoSink, NULL /* we're a server */, True /* we're a SSM source */);
    // Note: This starts RTCP running automatically

    ServerMediaSession *sms = ServerMediaSession::createNew(self->_server.getRTSPServer()->envir(), self->_streamName.c_str(),
            self->_streamName.c_str(), "Session streamed by \"testH264VideoStreamer\"", True /*SSM*/);
    sms->addSubsession(PassiveServerMediaSubsession::createNew(*self->_videoSink, self->_rtcp));
    self->_server.getRTSPServer()->addServerMediaSession(sms);

    char *url = self->_server.getRTSPServer()->rtspURL(sms);
    self->_server.getRTSPServer()->envir() << "Play this stream using the URL \"" << url << "\"\n";
    delete[] url;

    // Start the streaming:
    self->_server.getRTSPServer()->envir() << "Beginning streaming...\n";
    self->play();

//    self->_task_complete.notify_all();
}

TinySource *MediaStream::createVideoSource() {
    TinySource::PayloadType type = TinySource::PayloadTypeH264;
    switch (_mediaStreamAttr.videoType) {
        case MediaStreamAttr::VIDEO_TYPE_H264:
            type = TinySource::PayloadTypeH264;
            break;
        case MediaStreamAttr::VIDEO_TYPE_H265:
            type = TinySource::PayloadTypeH265;
            break;
        default:
            break;
    }
    TinySource *s = TinySource::createNew(_server.getRTSPServer()->envir(), type);
    s->setComment(_streamName);

    return s;
}

TinySource *MediaStream::createAudioSource() {
    return TinySource::createNew(_server.getRTSPServer()->envir(), TinySource::PayloadTypeAAC);
}

RTPSink *MediaStream::createVideoSink() {
    RTPSink *sink = NULL;
    switch (_mediaStreamAttr.videoType) {
        case MediaStreamAttr::VIDEO_TYPE_H264:
            sink = H264VideoRTPSink::createNew(_server.getRTSPServer()->envir(), _rtpGroupsock, 96);
            break;
        case MediaStreamAttr::VIDEO_TYPE_H265:
            sink = H265VideoRTPSink::createNew(_server.getRTSPServer()->envir(), _rtpGroupsock, 96);
            break;
        default:
            sink = H264VideoRTPSink::createNew(_server.getRTSPServer()->envir(), _rtpGroupsock, 96);
            break;
    }
    return sink;
}

RTPSink *MediaStream::createAudioSink() {
    return NULL;
}

void MediaStream::unicastConstructor(void *context) {
    MediaStream *self = (MediaStream*)context;

    self->_videoSource = self->createVideoSource();
//    printf("unicast videoSource[%p]\n", _videoSource);
    self->_audioSource = self->createAudioSource();
    ServerMediaSession *sms = ServerMediaSession::createNew(self->_server.getRTSPServer()->envir()
            , self->_streamName.c_str()
            , self->_streamName.c_str()
            , "Session streamed by \"testH264VideoStreamer\"", True /*SSM*/);
    self->_videoSubsession = UnicastVideoMediaSubsession::createNew(self->_server.getRTSPServer()->envir()
            , *self->_videoSource);
    sms->addSubsession(self->_videoSubsession);
    sms->addSubsession(UnicastAudioMediaSubsession::createNew(self->_server.getRTSPServer()->envir()
            , *self->_audioSource));
    self->_server.getRTSPServer()->addServerMediaSession(sms);
//    self->_task_complete.notify_all();
}

void MediaStream::unicastDestructor() {
    Medium::close(_videoSource);
    _videoSource = NULL;
    Medium::close(_audioSource);
    _audioSource = NULL;
    // ServerMediaSession 会回收subsession
    _videoSubsession = NULL;
}

void MediaStream::multicastDestructor() {
    // rtcp销毁的时候会去试用rtcpGroupsock去发送bye协议，故必须先于groupsock析构
    Medium::close(_rtcp);
    _rtcp = NULL;
    delete _rtpGroupsock;
    _rtpGroupsock = NULL;
    delete _rtcpGroupsock;
    _rtcpGroupsock = NULL;

    // sink必须在source之前销毁，因sink销毁时会去调用source去停止流
    Medium::close(_videoSink);
    _videoSink = NULL;
    Medium::close(_videoSource);
    _videoSource = NULL;
    Medium::close(_audioSource);
    _audioSource = NULL;
}

void MediaStream::disconnectAllClient(void *context) {
    MediaStream *self = (MediaStream*)context;

    RTSPServer *server = self->_server.getRTSPServer();
    ServerMediaSession *sms = server->lookupServerMediaSession(self->_streamName.c_str());
    server->closeAllClientSessionsForServerMediaSession(sms);
    server->removeServerMediaSession(sms);

//    self->_task_complete.notify_all();
}

void MediaStream::afterPlaying(MediaStream *stream) {
    stream->_server.getRTSPServer()->envir() << "...done IPC Server!\n";
}

void MediaStream::play() {
    cout << "server pthread id: [" << pthread_self() << "]" << endl;
    _videoSource = createVideoSource();

    // Create a framer for the Video Elementary Stream:
    if (TinySource::PayloadTypeH265 == _videoSource->getPayloadType()) {
        _naluFramer = H264or5VideoNaluFramer::createNew(_server.getRTSPServer()->envir(), _videoSource, 265);
    } else {
        _naluFramer = H264or5VideoNaluFramer::createNew(_server.getRTSPServer()->envir(), _videoSource, 264);
    }

    // Finally, start playing:
    _server.getRTSPServer()->envir() << "Beginning to run IPC Server...\n";
    _videoSink->startPlaying(*_videoSource, (MediaSink::afterPlayingFunc *) afterPlaying, this);
}

void MediaStream::saveHeaderInfo(uint8_t *data, uint32_t size, uint64_t pts) {
    int spsNum, ppsNum, vpsNum;
    char mask;
    if (_mediaStreamAttr.videoType == MediaStreamAttr::VIDEO_TYPE_H264) {
        spsNum = 7;
        ppsNum = 8;
        vpsNum = -1;
        mask = 0x1F;
    } else if (_mediaStreamAttr.videoType == MediaStreamAttr::VIDEO_TYPE_H265) {
        spsNum = 33 << 1;
        ppsNum = 34 << 1;
        vpsNum = 32 << 1;
        mask = 0x7E;
    } else {
        spsNum = -1;
        ppsNum = -1;
        vpsNum = -1;
        mask = 0;
        printf("warning: video type[%d] is not right\n", _mediaStreamAttr.videoType);
    }

    _nal_parser->setBuffer(data, size);

    uint8_t *ptr;
    uint32_t nal_size;
    bool dummy;
    int i_nal_type;
    while (_nal_parser->findNalu()) {
        _nal_parser->getNextNaluBuffer(ptr, nal_size, dummy);
        i_nal_type = ptr[0] & mask;
        if (i_nal_type == spsNum) {
            _videoSource->setSps(string((char*)ptr, nal_size));
        } else if (i_nal_type == ppsNum) {
            _videoSource->setPps(string((char*)ptr, nal_size));
        } else if (i_nal_type == vpsNum) {
            _videoSource->setVps(string((char*)ptr, nal_size));
        } else {
            printf("warning: the type[%d] is not sps\\pps\\vps\n", ptr[0]);
        }
        _videoSource->setData(ptr, nal_size, pts);
    }
}

void MediaStream::sendDataToQueue(uint8_t *data, uint32_t size, uint64_t pts) {
    uint8_t *ptr;
    uint32_t nal_size;
    bool dummy;

    _nal_parser->setBuffer(data, size);
    while (_nal_parser->findNalu()) {
        _nal_parser->getNextNaluBuffer(ptr, nal_size, dummy);
        _videoSource->setData(ptr, nal_size, pts);
        //printf("test...nal_size[%d], data_size[%d]\n", nal_size, size);
    }
//    printf("\n");
}